# Transactions

The editor transaction system: recording every persistent edit as an undoable transaction, the undo/redo stack behind Ctrl+Z / Ctrl+Shift+Z, the JSONL session log, and the automation server's `history` namespace ([automation.md](./automation.md)). The system lives in `src/Editor/Transactions` (`TransactionHistory`, `TransactionLogWriter`, `TransactionTypes`), owned by `MainWindow` and ticked once per frame. See [objects.md](./objects.md) for the definition-as-source-of-truth model this records against.

---

## Capture

Recording attaches to `ProjectConfig::OnPropertyChanged`: every definition mutation bubbles child-to-parent to the project config with the leaf definition pointer, so one subscription observes all persistent edits and no write site knows about recording. The subscription follows project loads (`ProjectManager::OnProjectLoaded` / `OnProjectPreUnload`), since loading a project replaces the `ProjectConfig` instance.

The change set carries property names only, so old values come from a shadow value map: seeded on project load from the property database crossed with the live components, updated on every recorded change and every undo/redo, extended on object create, and erased on destroy. Values are stored in their `AutomationVariantText` text encoding.

Filters applied at capture:

- `wantsConfigSerialization()` and `wantsSaveForPropertyChange(changeSet)` must both pass. These vetoes gate only the autosave timer upstream, so the recorder applies them itself, which drops runtime-driven state such as mount-driven camera transforms and transient VR devices.
- Components in the transient id range are skipped (`ProjectConfig::isTransientComponentId`).
- A notified name with no descriptor in `MikanPropertyDatabase` cannot be recorded or undone: it is skipped with a once-per-name warning. The guard test in `src/Editor/Server/Test/PropertyNotificationGuardTests.cpp` keeps this set from growing.
- Read-only descriptors are skipped: they cannot be re-applied through `setPropertyValue`, and the typed systems' component id lists notify on every create and destroy.
- A re-entrancy flag suppresses capture while undo/redo re-applies values.

Object lifecycle capture uses two object system delegates: `OnNewObjectFinalized` records a create with the definition's JSON, and `OnObjectWillBeDestroyed` (fired at the top of `removeObjectByPrimaryComponentId`, before teardown rewrites parenting) snapshots the definition for a destroy composite. The child reparent property changes that `TransformComponent::dispose` fires fold into the composite, and the destroy op is appended last so reverse-order undo recreates the object before restoring its children.

## Coalescing

A burst of changes to one target merges into one transaction while consecutive changes arrive within 0.75 s. Gesture brackets (`beginGesture`/`endGesture`) override the window and group an entire interaction:

- `TransformGizmo` brackets a handle drag as `gizmo_drag` on the edges of `ImGuizmo::IsUsing`, and `EditorObjectSystem` brackets a collider grab as `mouse_drag` on mouse button down/up
- the ImGui property panels bracket slider drags and typing via `ImGui::IsItemActive()` / `IsItemDeactivated()`, keeping per-frame value writes flowing for live preview
- a script trigger run (the script panel button or the `script trigger` automation command) is bracketed as `script:<name>`, so its property writes coalesce. Object creates and destroys still seal on their own, so a trigger that spawns objects lands as several transactions.

A merge keeps the first old value and the latest new value. Undo/redo requests, create/destroy events, project unload, and any `history` automation command seal the open transaction immediately; a destroy composite seals at the frame boundary.

## Undo and redo

The stack holds up to 256 sealed transactions with a cursor; committing a new transaction truncates the redo tail. Undo applies a transaction's ops in reverse order, redo forward.

- Property ops re-apply through `IPropertyInterface::setPropertyValue` with the stored text parsed back through `AutomationVariantText`, the same coercion path the automation server uses.
- Reparenting applies through the `parent_transform_id` case in `TransformComponent::setPropertyValue`, which routes through `attachToComponent`/`detachFromParent` so runtime links follow the definition.
- Undoing a create destroys the object; undoing a destroy recreates it under its original component id via `MikanObjectSystem::recreateObjectFromDefinitionJson`. Same-id recreation matters because scene references (parenting, camera and compositor links) are stored by component id with no fixup on destroy. The id allocators are strictly monotonic, so a freed id can never collide with a future allocation.

Ctrl+Z undoes and Ctrl+Shift+Z redoes while the project stage is up (Ctrl+Y is untaken because `Y` is the gizmo scale-mode key). Modifier-aware bindings ride `MkWindowEvent::getKeyMod()` and the `(keysym, modifier mask)` keyed bindings in `InputManager`. ImGui text fields swallow the chord before it reaches the binding, so text-edit undo stays ImGui's.

## The session log

`TransactionLogWriter` appends one JSON object per line to `<projectFolder>/logs/transactions_<timestamp>.jsonl`, opened per project load, flushed per line so a crash keeps the record, pruned to the newest 20 files. Lines:

- `{"event":"session","project":...,"started":...}`
- `{"event":"txn","seq":N,"t":<epoch ms>,"desc":...,"ops":[...]}` where a set op carries `system/class/id/prop/type/old/new` and a create/destroy op carries the definition JSON under `def`
- `{"event":"undo"|"redo","seq":N,...}` and `{"event":"clear",...}`

The log is the post-session diagnosis trail: it records what changed, in what order, and what was undone, and a set op's `old`/`new` text values re-apply directly through `property set` automation commands.

## Node graph undo

The node editor windows keep their own undo history, separate from the component transaction stack: node graphs are standalone assets outside the project config, and their edits (node property sheet writes, canvas node drags) have no property-notification chokepoint to record ops from. Each `NodeEditorWindow` owns a `NodeGraphHistory` (`src/Editor/NodeEditors/Graphs/NodeGraphHistory`), a bounded stack of whole-graph JSON snapshots seeded when a graph is created or loaded.

A snapshot commits once per quiescent frame. The graph delegates (node, pin, link, property, and asset reference create/delete), a widget interaction ending, and left-mouse release all mark a checkpoint, and the window serializes and pushes once no widget or mouse interaction is active. Identical snapshots dedup, so a burst (a delete cascade, a whole node drag) lands as one undo step. Undo and redo rebuild the graph instance from the target snapshot through `NodeGraphFactory::loadNodeGraphFromConfig` and rebind it to its owning compositor or shape component, so the compositor keeps evaluating the editor graph across undo.

Ctrl+Z / Ctrl+Shift+Z work while a node editor window has focus (the same chords as the main window), alongside the toolbar undo and redo buttons. The automation face is the `nodegraph` namespace ([automation.md](./automation.md)).

Each window session also writes a JSONL log through `NodeGraphLogWriter`, sharing the `JsonlSessionLogWriter` machinery with the component log: `<projectFolder>/logs/nodegraph_<timestamp>.jsonl`, flushed per line, pruned to the newest 20 files, path reported by `nodegraph info`. The log carries the full graph snapshot per undo step, so an edit session reconstructs exactly from the file. Lines:

- `{"event":"session","project":...,"window":...,"started":...}`
- `{"event":"baseline","graph":...,"class":...,"snapshot":{...}}` per graph create or load
- `{"event":"txn","seq":N,"t":...,"desc":...,"snapshot":{...}}` per committed undo step, `desc` holding the collection-count deltas
- `{"event":"undo"|"redo","steps":N,"cursor":C,...}` and `{"event":"restore_failed",...}`

## Known unrecordable properties

Properties whose change notifications carry names with no property descriptor are invisible to undo (and to websocket property events). The recorder warns once per name at runtime, and the guard test asserts the set does not grow silently. Standing entries include the `EditorObjectSystem` settings names and `depth_mesh_scale_correction`; `RGBSpotLightComponent` `red`/`green`/`blue` are runtime-only members with no definition backing at all.

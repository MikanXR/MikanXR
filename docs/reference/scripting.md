# Scripting

MikanXR has two programmable layers: visual node graphs (`src/Editor/NodeEditors/`) that drive rendering pipelines, and project-level Lua scripts (`src/Editor/Scripting/`) that drive scene logic. The compositor graph's rendering semantics are covered in [compositor.md](./compositor.md), the ECS objects scripts manipulate in [objects.md](./objects.md), and the client-facing script RPC in [wire-protocol.md](./wire-protocol.md).

---

## Graph types

Three `NodeGraph` subclasses exist, registered in `App::startup` via `NodeGraphFactory::registerFactory<>`:

- `CompositorNodeGraph` (`Graphs/CompositorNodeGraph.h`): composites a video frame. Entry point is the `EventNode` named `OnCompositeFrame` (`k_compositeFrameEventName`); evaluated once per composited frame by `CompositorComponent`.

- `ShapeNodeGraph` (`Graphs/ShapeNodeGraph.h`): renders a shape component. Entry point is the `OnRenderShape` event (`k_renderShapeEventName`); `renderShape()` sets a transient view-projection matrix, evaluates the chain, then clears it. Bound to a `ShapeComponent`. A shape graph draws itself rather than queueing into `MkScene`, so in the project scene view `AppStage_Project` collects these shapes while gathering and issues them after the `MkScene` pass. Drawn during the gather instead, every shape graph would land before any queued scene geometry and no stencil could ever occlude one.

Blend and depth state for a shape draw belong to the caller, not the graph. `DrawShapeMeshNode` sets neither: each node's scoped state starts as a copy of its parent, so the graph draws under whatever the pass that called `renderShapeGraph` established. The project scene view draws shape graphs after the scene geometry with the depth test on and normal alpha blending, so a shape sorts against the scene and a textured quad keeps its transparency. The compositor's `DrawShapesNode` supplies its own blend mode and `depth_test`, which apply the same way to shapes with a graph and shapes without one, and its depth test runs against the working buffer's depth ([compositor.md](./compositor.md)). Two groups of shapes that need different blending are two `DrawShapesNode`s.

- `MaterialNodeGraph` (`Graphs/MaterialNodeGraph.h`): authors a material. It has no event node and is never evaluated: `MaterialCompiler` turns it into shader source and a material file (`.compmat` or `.shapemat` by domain) beside the graph file, and the compositor and shape graphs consume the result like any hand-written material. Its node set, pin type, and compiler are in [materials.md](./materials.md).

There is no general-purpose logic graph; non-rendering logic is done in Lua.

---

## Node / pin / link model

All types live under `src/Editor/NodeEditors/`:

- `NodeGraph` owns maps of `Node`, `NodePin`, and `NodeLink` by integer id (`allocateId()`), plus graph-level `GraphProperty` objects and `AssetReference` entries. Each graph subclass declares which node, pin, property, and asset-reference factories it supports (`addNodeFactory<>`, `addPinFactory<>`, ...).

- `Node` (`Nodes/Node.h`) has input/output pin lists, `evaluateNode(NodeEvaluator&)`, and editor hooks (`editorRenderNode`, `editorRenderPropertySheet`). Factories (`TypedNodeFactory<Node, NodeConfig>`) create nodes with their default pins. A graph registers factories under `NodeFactory::getFactoryKey`, the class name by default, so one node class may register several variant factories (`<ClassName>:<variant>`) that each show as their own create menu entry; loading resolves by class name to the first factory of that class. Factories name a create menu category through `editorGetCategory`, and the menu groups them into submenus and offers a text filter.

- `NodePin` (`Pins/NodePin.h`) has a direction, connected `NodeLink` list, an optional default value (`setHasDefaultValue`, lets a node evaluate with the pin unconnected), and a dynamic flag (`setIsDynamicPin`, for pins generated from another pin's value, as when `DrawLayerNode` creates one pin per shader uniform of its bound material). Concrete pins: `FlowPin`, `FloatPin`/`Float2Pin`/`Float3Pin`/`Float4Pin`, `IntPin`, `BoolPin`, `TexturePin`, `PropertyPin`, `ArrayPin` (typed by element property class).

- Pages (`Graphs/GraphPage.h`): a graph can hold several canvases. Page 0 is the implicit root every graph has; a graph subclass that registers a page factory (`addPageFactory<>`) can create more, each a `GraphPage` object with an id and a name persisted in the graph file's `pages` array. Every node records the page it sits on (`Node::getPageId`, config key `page`, absent for the root), the editor draws only the current page's nodes and links, and `NodePin::canPinsBeConnected` refuses a link across pages. Deleting a page deletes its nodes. The material graph uses pages for material functions ([materials.md](./materials.md)); the compositor and shape graphs register no page factory today and behave as single-canvas graphs.

- `CommentNode` (`Nodes/CommentNode.h`) is a comment box every graph type registers: a colored title band over a translucent, resizable region drawn as a canvas group, so dragging it carries the nodes inside along, with a floating title hint once the canvas is zoomed out. It has no pins, so no evaluation or compile walk reaches it. Text, color, and size persist in its node config, and the Details panel edits the text and color.

- `NodeLink` (`Pins/NodeLink.h`) connects one output pin to one input pin. A link is always stored output first: connecting is allowed in either drag direction, so `NodeGraph::createLink` swaps the ends when the drag started at the input pin. Code reading a link resolves the far end by comparing against the link's own ends (`NodePin::getConnectedSourcePin`, `getConnectedTargetPin`) rather than trusting which end is stored first, so a graph saved before that normalization still walks correctly.

- Graph properties (`Properties/`) wrap referenced resources as graph-level values: `GraphMaterialProperty`, `GraphTextureProperty`, `GraphStencilProperty`, `GraphShapeProperty`, `GraphModelProperty`, `GraphBoolProperty`, array/value variants. Each carries a display name (editable in the editor's Details panel, and defaulted to the dropped asset's name when one is dragged in) plus a sort order setting its place in the Variables list, both persisted in the graph file. Names are display-only: nodes and pins reference properties by id, so renaming never breaks a link. Variable names are unique within a graph: every rename surface (the Variables row's inline field, the Details name field, the `nodegraph renamevar` automation verb) goes through `NodeGraph::renameProperty`, which suffixes a taken name with a number. The Variables row renames in place on a second click of the selected row or F2.

The editor UI is Dear ImGui plus imgui-node-editor, reached through the `MkCanvas` facade in `MikanGUI` (see [modules.md](./modules.md)). The canvas pans and wheel-zooms with text kept crisp at any zoom, and link drags give live accept/reject feedback from `NodePin::canPinsBeConnected`. Editor windows are `NodeEditorWindow` subclasses in `Windows/`: `CompositorNodeEditorWindow` (opened by `CompositorComponent::editCompositorGraph()`), `ShapeNodeEditorWindow`, and `CompositorOutputEditorWindow` (output preview, not a graph editor). The graph editors use the same dockable panel shell as the main window (Graph, Variables, Assets, and Details panels under a File/Edit/View menu bar, layout persisted per window). Graph edits are undoable through a per-window snapshot history ([transactions.md](./transactions.md)).

---

## Evaluation

`NodeEvaluator` (`Graphs/NodeEvaluator.h`) carries the graphics context and delta time. `evaluateFlowPinChain(startNode)` evaluates the start node, then follows the first link off each node's output `FlowPin` to the next node, stopping on a node with no output flow pin, on an error, or after 1000 nodes (`kInifiniteLoopThreshold`, reported as an infinite-loop error). Non-flow input pins are pulled on demand by the node being evaluated (`evaluateInputs`). Failures accumulate as `NodeEvaluationError` values on the evaluator; the owning component stores them (`getLastNodeEvalErrors()`) and the node editor windows display them.

---

## Graph persistence

Graphs are standalone assets, not part of the project config. `NodeGraphFactory::saveNodeGraph`/`loadNodeGraph` round-trip a `NodeGraphConfig` (Configuru JSON) holding the graph class name, id counter, and per-object configs for asset refs, properties, nodes, pins, and links. Each kind has its own extension and folder (`Graphs/NodeGraphFileTypes.h`): compositor graphs are `*.compgraph` under `compositors/`, shape graphs `*.shapegraph` under `shapes/`, material graphs `*.matgraph` beside their material file under `compositor_materials/` or `shape_materials/` ([materials.md](./materials.md)). The kinds are not interchangeable, so the extension is what file dialogs, the Assets panel, and the drop targets filter on. Loading dispatches on the stored class name to the registered factory, allocates the graph, then rebuilds objects through the graph's own factories; a caller that passes its expected class to `loadNodeGraph` gets a refusal naming both classes when the file is of another kind, and `peekGraphClassName` reads the class without loading. Components reference graphs by path through a `CompositorGraphAssetReference` or `ShapeGraphAssetReference` stored in their definition (e.g. `CompositorDefinition::k_compositorGraphPathPropertyId`), set by dropping a graph from the project Assets panel ([objects.md](./objects.md)); the setter refuses a file of another kind rather than storing it. Projects saved before the split, with every kind as `.graph` under one `graphs/` folder, are moved onto the current layout by `LegacyGraphMigration` when they load: each file goes to the folder its class names, material graphs are renamed beside their `.mat`, and the stored paths in the project file are rewritten.

A graph's `assetReferences` array holds the materials and textures its properties bind to, each as a class name and a stored project path, and `GraphMaterialProperty` / `GraphTextureProperty` bind by index into that array. The array is not shown anywhere: the editor's Graph Assets panel lists the project catalog filtered to the types the graph accepts, and dropping a tile onto the canvas or the variables panel calls `NodeGraph::findOrAddAssetReference`, which reuses the graph's existing entry for that class and path or appends one. A reference whose properties were later deleted stays in the file unused.

---

## Project scripts

Lua (LuaBridge3, `thirdparty/LuaBridge3`) is project-level, not per-component. `ScriptObjectSystem` (`src/Editor/ECS/Script/ScriptObjectSystem.h`) is a `MikanTypedObjectSystem` registered last in `ProjectManager::startup`, so every other system's objects already exist when scripts run. Each `ScriptComponent` (definition `ScriptDefinition`, property `script_path`) names one script file through a `ScriptAssetReference` (`*.lua`). The system owns the project's single `ProjectScriptContext`, one `lua_State` shared by every script.

On `postInit`, and on the next `update` after a script is added, removed, or has its path changed (`requestReload`), the system calls `reloadAllScripts()`: dispose the current state, create a fresh one, then run every script with a non-empty path in pool order (`runScriptFile`), and bind the result to `ScriptRequestHandler`. With no scripts in the pool there is no Lua state at all. If any file fails to run, the whole state is disposed and an error names the failing script's id and path. A single bad file takes down every script, not just its own.

The outliner's project root has a top-level Scripts folder. Its add button calls `ScriptObjectSystem::addNewScript()`, which creates `<project>/scripts/script_<timestamp>.lua` and registers it. Selecting a script row shows its path (click to pick a different `.lua` file), Edit and Reload buttons, and one button per trigger that file registered. Delete uses the outliner's Delete Component button. Reload re-runs every script in the pool, since they share one state.

`CommonScriptContext` (`Scripting/CommonScriptContext.h`) owns the `lua_State`: standard libs opened, a panic handler and detailed error reporting (`checkLuaResult` logs Lua tracebacks and disposes the state on error), and a built-in coroutine scheduler. The scheduler injects globals `start_coroutine`, `wait_frames`, `wait_next_frame`, `wait_seconds`, `get_frame_delta_seconds`. `update_scheduler()` is invoked once per frame from `updateScript()`. `runScriptFile(path, scriptId)` runs one file's chunk into the shared state. While the chunk runs, `m_loadingScriptId` holds the running script's id so any registration it makes is attributed to it.

`require` resolves against the project's own `scripts` folder, then the bundled `resources/scripts`. `setupModuleSearchPath` prepends `<project>/scripts/?.lua` and `<project>/scripts/?/init.lua` to `package.path` when the state is created, followed by the same pair under `resources/scripts`, so `require("color")` loads `<project>/scripts/color.lua`, `require("easing")` finds the bundled module without a project copy, a project module shadows a bundled one of the same name, and a dotted name walks subfolders. Lua's stock entries stay behind those, and the search path is rebuilt on every reload, along with the `package.loaded` cache that lives in the state. A required module is not a `ScriptComponent`: it registers nothing, it is loaded on demand by whichever script requires it, and it returns a table rather than defining globals. `resources/scripts/easing.lua` and `formatting.lua` are written that way. The same context also replaces `package.searchers[2]`, the stock file loader, with one that finds the file through `package.path` but loads it under the chunk name above, so a breakpoint set in a required module resolves like one set in a project script.

Editor support: the project folder is the VS Code workspace for its scripts. `resources/lua-definitions/` holds LuaLS stub files (`mikan-core.lua`, `mikan-systems.lua`, `mikan-components.lua`) describing every binding above, kept in sync by hand with the `bindLuaFunctions` implementations they name. Loading a project writes two files into the project folder, each only when missing, so delete one to regenerate it after moving the editor install:

- `.luarc.json`: points the VS Code Lua extension's `workspace.library` at the stub directory
- `.vscode/launch.json`: an lrdb attach configuration for the debugger, with the workspace as its source root

The project's `.luarc.json` lists `resources/scripts` beside the API stubs, so the language server resolves the bundled modules too, and the repo root carries its own `.luarc.json` for `resources/scripts`. The script row's Edit button and a double-click on a script tile in the Assets panel share one path, `ScriptAssetReference::editorOpen`, which runs the script editor command (an app setting, `code --reuse-window` by default) with the project folder followed by the script path, which is how VS Code opens the project workspace with that file focused. A bundled script is copied into the project first (at the same relative path, so the component's stored path now resolves to the copy) unless the "Edit bundled resources" setting is on ([objects.md](./objects.md)). A script outside both trees opens on its own.

Scripts declare their entry points through the `ScriptContext` namespace. Each registration is attributed to whichever script file was executing when it was called:

- `ScriptContext.registerTrigger(functionName)`: exposes a named global function as a trigger.
- `ScriptContext.registerMessageHandler(functionName)`: handler receives a string message, returns true if handled.
- `ScriptContext.registerHttpTrigger(routeName, functionName)`: binds a trigger to the HTTP route `/trigger/<routeName>`. The function still needs its own `registerTrigger`, which is what makes it callable; this call only maps a route onto it.
- `ScriptContext.registerVariable(name, defaultValue)`: exposes a global as an editor-editable, persisted parameter (below).
- `ScriptContext.registerComponent(name, componentClassName)`: exposes a global that references one component of that class (below).
- `ScriptContext.registerSequence(name, handlerTable)`: registers a DMX sequence handler a `DMXSequenceComponent` can pick (below).
- `ScriptContext.broadcastMessage(message)`: emits a message to connected clients.

`ScriptComponent::getTriggerNames()` and `invokeTrigger()` filter to the triggers registered by that component's own file (`CommonScriptContext::getTriggerNamesForScript(scriptId, ...)`). A trigger run from the panel button or the `script trigger` automation command is bracketed as one transaction gesture ([transactions.md](./transactions.md)).

### Trigger arguments

`CommonScriptContext::invokeScriptTrigger(triggerName, args)` always calls the Lua function with exactly one table argument, empty when the caller supplied nothing. A trigger declared with no parameters ignores it, so an argument-free trigger needs no change. Values are always strings, so a script converts with `tonumber` where it needs a number.

Each entry point fills that table from its own carrier:

- an HTTP route from the request's query string, so `/trigger/new_sub?user=bob&tier=3` arrives as `{ user = "bob", tier = "3" }`
- the `InvokeScriptTrigger` client request from its `trigger_args` map
- the `script trigger <name> [key=value ...]` automation command from its trailing tokens
- the script panel button and the HTTP triggers panel button with nothing, so both pass an empty table

Query string parsing lives in `HttpInterprocessMessageServer` (`parseQueryString`), which percent-decodes, treats `+` as a space, and keeps the last occurrence of a repeated key. Route handlers receive one `HttpRouteRequest` carrying the verb, the matched path, the decoded query args, and the body.

`resources/scripts/stream_event.lua` is the worked example: an HTTP trigger that reads its args, caches the current scene, switches to an effect scene, broadcasts to clients, waits on a coroutine, and restores. It also carries the reentrancy guard such a flow needs, since a second event arriving mid-effect would otherwise cache the effect scene as the one to restore.

### Script variables

`registerVariable` turns a Lua global into a parameter the script panel shows as a widget and the project file persists. The variable's type comes from the default value: boolean, integer, number, string, or `Vec3f` map to BOOL, INT, FLOAT, STRING, VECTOR3F. Lua keeps integer and float subtypes apart, so `30` registers INT and `30.0` registers FLOAT. Any other type, a call outside a script chunk, or a name already registered by any script (all scripts share one global table) is rejected with a logged error.

Values live in the registering script's `ScriptDefinition` (`ScriptVariableTable`, `Scripting/ScriptVariableTable.h`), persisted under `script_variables` as `{"type": name, "value": ...}` entries. On every reload the chunk re-registers its variables and `CommonScriptContext::registerVariable` resolves each one against the definition: a stored value of the same name and type wins, otherwise the default is adopted and stored. The effective value is then written to the Lua global. A stored entry the script no longer registers stays in the definition and is not shown.

The definition is the single source of truth. A panel edit, an undo, or an automation set writes the definition, and `ScriptComponent::onDefinitionMarkedDirty` pushes every stored value back into the Lua globals through `CommonScriptContext::setVariableValue`. A `Vec3f` global is a userdata copy replaced on each push, so scripts read variables inside trigger bodies rather than caching them at chunk scope. Script-side writes to a global are not persisted.

`registerComponent(name, componentClassName)` registers a component reference: an INT component id (`INVALID_MIKAN_ID` for none) tagged with a `k_componentClassName` string, persisted as `{"type": "component", "class": name, "value": id}`. The panel draws it as a dropdown of that class's live components with a leading none entry (`GuiDataSource_OptionalComponentComboBox`), finding the owning system as the one whose `getComponentIdList` answers for the class. The Lua global holds the component handle or `nil`. LuaBridge pushes a pointer by its static type, so `ProjectScriptContext` registers one push thunk per bound component class (`CommonScriptContext::registerComponentClass`) and `registerComponent` accepts only those classes. That thunk table is how any component reaches Lua as its concrete class: `CommonScriptContext::pushComponent` looks the thunk up by `getComponentClassName` and pushes exactly one value, nil for a null component or an unbound class. A binding that returns a base pointer has to go through it, since returning the base type would hand Lua the base class's metatable and hide the subclass's own fields. `DMXFixtureGroupComponent::getFixtureAtIndex` is the case that needs it, because a group holds fixtures of mixed kinds: it reaches the context through `CommonScriptContext::getFromLuaState`, which reads the owning context out of the state's registry, and returns the fixture as `RGBSpotLightComponent` or `RGBPixelGridComponent`. The id resolves through `ProjectManager::getComponentById` with an exact class check. The global is re-pushed on registration, on every write, and on object lifecycle: `ScriptObjectSystem` subscribes to every system's `OnNewObjectFinalized` and `OnObjectWillBeDestroyed` and calls `refreshComponentVariables`, excluding the id of an object about to be destroyed, so a script never holds a handle to a dead object and an undone destroy restores the handle. Scripts nil-check a reference inside trigger bodies.

The property surface is one descriptor, `script_variables`, a UI-hidden and client-API-hidden STRING carrying the table's single-line JSON. Every variable edit notifies that one name, which is what lets the transaction recorder capture it and undo re-apply the text verbatim. The panel draws the variables itself through `ScriptComponent::getScriptVariableNames` / `getScriptVariable` / `setScriptVariable`, labeled by their Lua names.

### Sequences

`registerSequence(name, handlerTable)` registers a DMX sequence handler: a table with an `update(sequence, timeSinceStart, deltaSeconds)` function and optional `start(sequence)` and `stop(sequence)`. The table is held as a Lua registry reference (`CommonScriptContext::SequenceBinding`) released before the state closes. A `DMXSequenceComponent` (objects.md) names a handler and, while playing, `DMXSequenceSystem::update` calls `update` once per frame through `CommonScriptContext::callSequenceHandler`. The handler writes into the sequence's frame buffer (`setFixtureColor`, `setPixel`, `setFixtureChannels`, `fillGroup`) and the system pushes that buffer to the group's fixtures after the callback returns, so one frame is one send per fixture. Handlers are resolved by name every frame, so a script reload that re-registers the name keeps a playing sequence going, and one that drops it stops the sequence. A Lua error inside a handler callback logs the message and traceback naming the sequence and stops that sequence only: unlike a trigger, it does not dispose the shared state, since a frame-rate callback that took every script down would make the state unusable. `resources/scripts/sequence_chase.lua` is the worked example.

A sequence whose `content_source` is not `Script` is rasterized by the editor instead, and a handler becomes optional. Only `start` and `stop` are called there, never `update`, since C++ owns the pixels for those sources. The point of `start` is dynamic content selection: `sequence:setText(s)` and `sequence:setContentPath(p)` set the text or image the run uses. Both are runtime overrides cleared on stop, so picking content never writes the definition and never records a transaction, and `sequence:getText()` / `getContentPath()` read back whichever value is in force. A handler that is named but missing stops a `Script` sequence and is ignored by a rasterized one.

`resources/scripts/generate_lights.lua` is the worked example: a `generate_lights` trigger that lays out `RGBSpotLightComponent` objects on a stage in a zig-zag grid with sequential DMX addressing, every parameter a script variable.

What is scriptable: `ProjectScriptContext::bindContextFunctions()` binds LuaBridge classes for the object systems (`CameraObjectSystem`, `SceneObjectSystem`, `StageObjectSystem`, `AnchorObjectSystem`, `CompositorObjectSystem`, `DMXObjectSystem`, `RGBSpotLightSystem`, the stencil and shape systems) and components (`MikanComponent` and subclasses: transform, scene, stage, camera, compositor, stencils, shapes, anchor, marker, DMX fixture, RGB lights). It sets these globals, one per scriptable object system:

- `CameraSystem`
- `SceneSystem`
- `StageSystem`
- `AnchorSystem`
- `CompositorSystem`
- `DMXSystem`
- `RGBSpotLightSystem`
- `DMXFixtureGroupSystem`
- `DMXPresetSystem`
- `DMXSequenceSystem`
- `ModelStencilSystem`
- `BoxStencilSystem`
- `QuadStencilSystem`
- `ModelShapeSystem`
- `BoxShapeSystem`
- `QuadShapeSystem`

The system bindings are lookups by id, name, and index. `SceneSystem` adds `setCurrentScene(scene)` and `setCurrentSceneById(id)`, which activate a scene the same way the editor's own switch does, so the change lands in the system definition and is recorded as a transaction. Both resolve the scene before acting: `SceneObjectSystem::setCurrentScene` dereferences its argument, and `setCurrentSceneById` accepts an id naming no scene, which would deactivate the current scene and leave the project with none. A nil scene or an unknown id therefore changes nothing.

`RGBSpotLightSystem` is the one system that also creates and destroys: `createLight(stageId, name)` parents a new light to the stage at its origin through `addNewObjectByTypedDefinition`, and `removeLight(lightId)` destroys one, both through the same paths the outliner uses so the transaction recorder sees them. A script removing lights collects the ids first and removes afterwards, since the component map cannot be walked while it changes. `DMXFixtureGroupSystem` follows the same shape (`createGroup(stageId, name)`, `removeGroup(groupId)`), and a group component exposes `stageId`, `getFixtureCount()`, `getFixtureAtIndex(i)`, `containsFixture(id)`, `addFixture(id)`, and `removeFixture(id)`. `DMXPresetSystem` likewise (`createPreset(groupId, name)`, `removePreset(presetId)`), and a preset exposes `groupId`, `apply()`, and `capture()`. `DMXSequenceSystem` likewise (`createSequence(groupId, name)`, `removeSequence(sequenceId)`), and a sequence exposes `groupId`, `sequenceName`, `timeSinceStart`, `isPlaying`, `getGroup()`, `play()`, `pause()`, `stop()`, and the frame buffer writers. `DMXSystem.universeChannelCount` exposes the 512-slot universe size for channel arithmetic. Every component exposes `componentId`, and a DMX fixture exposes `ownerStageId`.

There is no `ownerComponent` global: a script is not bound to a single component, so it reaches objects through the system globals above. A component handle still exposes `getCameraSystem()`, `getSceneSystem()`, `getDMXSystem()`, `getAnchorSystem()`, `getCompositorSystem()` methods for scripts that already hold a component reference. Math helpers `LuaVec3f`/`LuaQuatf` come from `Scripting/LuaMath.h`. Enum constants (e.g. `eStencilCullMode`) are registered as globals.

---

## Script RPC surface

`ScriptRequestHandler` (`src/Editor/Server/ScriptRequestHandler.h`) is the server-side bridge (see [wire-protocol.md](./wire-protocol.md)). It holds one bound `ProjectScriptContext` at a time (`bindScriptContext`/`unbindScriptContext`/`getScriptContext`):

- `InvokeScriptTrigger { trigger_name, trigger_args }` request: triggers are project-wide, so the request addresses one by name. `trigger_args` is a string-to-string map that becomes the trigger's argument table and may be empty. Result codes:
	- `MalformedParameters`: the trigger name is not registered.
	- `RequestFailed`: no project script state is loaded, or the trigger itself failed.
	- `Success`: otherwise.

- `SendScriptMessage` request: offers the message to the bound context's handlers in registration order until one returns true. An unhandled message is still `Success`.

- `MikanScriptMessagePostedEvent`: published to clients whenever a script calls `broadcastMessage`.

- HTTP triggers: `registerHttpTriggerRoute(routeName, triggerName)` installs routes under `/trigger/<name>` on the HTTP message server for every `ScriptContext.registerHttpTrigger` binding the bound context declared. The route ignores the HTTP verb and passes the request's query args to the trigger, which is what lets a GET-only caller (a Stream Deck, Streamer.bot's Fetch URL sub-action) carry a payload. Responses are JSON result codes mapped from `MikanAPIResult`.

---

## Lua debugging

`LuaDebugServer` (`Scripting/LuaDebugServer.h`) is a singleton LRDB debug server on TCP port 21110 (the vscode-lrdb extension default). `MainWindow` starts it listening before the first project loads, and calls `poll()` every frame. The project's `ProjectScriptContext` attaches itself when its Lua state is created (`createScriptState`) and detaches when the state is disposed (`disposeScriptState`), so breakpoints in any project script work without a manual attach step. Scripts can call `lrdb_break()` for a programmatic breakpoint. Chunk names are paths relative to the project folder (absolute for a script outside it), which is what the generated launch config's `sourceRoot` makes VS Code send for a gutter breakpoint, so the two match. To debug: open the project folder in VS Code, set breakpoints, and run the "Attach to MikanXR Lua" configuration while the editor is running. A trigger button, HTTP route, or `script trigger` automation command then hits them. See [debugging.md](./debugging.md).

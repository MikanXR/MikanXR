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

The editor UI is Dear ImGui plus imgui-node-editor, reached through the `MkCanvas` facade in `MikanGUI` (see [modules.md](./modules.md)). The canvas pans and wheel-zooms with text kept crisp at any zoom, and link drags give live accept/reject feedback from `NodePin::canPinsBeConnected`.

Node positions and comment box sizes are stored in 96 DPI logical units, so a graph laid out on one display opens the same on another. The canvas itself works in the scaled pixels the rest of the UI draws in, since its contents are measured ImGui items, and `MkCanvas::toCanvasSpace`/`fromCanvasSpace` are the only two places that cross between them. Everything outside those calls is logical: `Node::getNodePos`, `CommentNode::getSize`, `NodeEditorState::hangPosGridSpace`, and the grid position the `nodegraph` automation verbs take. Canvas zoom is a separate factor the canvas applies on top and needs no conversion. Editor windows are `NodeEditorWindow` subclasses in `Windows/`: `CompositorNodeEditorWindow` (opened by `CompositorComponent::editCompositorGraph()`, which rebinds and raises an already open window, and which keeps the bound compositor running while the window is open so its source nodes carry frames even when the scene displays another compositor), `ShapeNodeEditorWindow`, and `CompositorOutputEditorWindow` (output preview, not a graph editor). The graph editors use the same dockable panel shell as the main window (Graph, Variables, Assets, and Details panels under a File/Edit/View menu bar, layout persisted per window). Graph edits are undoable through a per-window snapshot history ([transactions.md](./transactions.md)). A link end is picked up with Ctrl+drag on its pin (an input pin, or an output pin holding exactly one link): the drag continues from the far pin, releasing on a compatible pin rewires it, on empty canvas deletes it, and back on the same pin restores it, all as one undo step. The editor library was patched for this (`ed::SetLinkDragAnchor` in the MikanXR fork of imgui-node-editor), since it only drags from the pressed pin otherwise. A pin's right-click menu offers Disconnect, which drops every link on the pin.

---

## Evaluation

`NodeEvaluator` (`Graphs/NodeEvaluator.h`) carries the graphics context and delta time. `evaluateFlowPinChain(startNode)` evaluates the start node, then follows the first link off each node's output `FlowPin` to the next node, stopping on a node with no output flow pin, on an error, or after 1000 nodes (`kInifiniteLoopThreshold`, reported as an infinite-loop error). Non-flow input pins are pulled on demand by the node being evaluated (`evaluateInputs`). Failures accumulate as `NodeEvaluationError` values on the evaluator; the owning component stores them (`getLastNodeEvalErrors()`) and the node editor windows display them.

---

## Graph persistence

Graphs are standalone assets, not part of the project config. `NodeGraphFactory::saveNodeGraph`/`loadNodeGraph` round-trip a `NodeGraphConfig` (Configuru JSON) holding the graph class name, id counter, and per-object configs for asset refs, properties, nodes, pins, and links. Each kind has its own extension and folder (`Graphs/NodeGraphFileTypes.h`): compositor graphs are `*.compgraph` under `compositors/`, shape graphs `*.shapegraph` under `shapes/`, material graphs `*.matgraph` beside their material file under `compositor_materials/` or `shape_materials/` ([materials.md](./materials.md)). The kinds are not interchangeable, so the extension is what file dialogs, the Assets panel, and the drop targets filter on. Loading dispatches on the stored class name to the registered factory, allocates the graph, then rebuilds objects through the graph's own factories; a caller that passes its expected class to `loadNodeGraph` gets a refusal naming both classes when the file is of another kind, and `peekGraphClassName` reads the class without loading. Components reference graphs by path through a `CompositorGraphAssetReference` or `ShapeGraphAssetReference` stored in their definition (e.g. `CompositorDefinition::k_compositorGraphPathPropertyId`), set by dropping a graph from the project Assets panel ([objects.md](./objects.md)); the setter refuses a file of another kind rather than storing it. Projects saved before the split, with every kind as `.graph` under one `graphs/` folder, are moved onto the current layout by `LegacyGraphMigration` when they load: each file goes to the folder its class names, material graphs are renamed beside their `.mat`, and the stored paths in the project file are rewritten.

A graph's `assetReferences` array holds the materials and textures its properties bind to, each as a class name and a stored project path, and `GraphMaterialProperty` / `GraphTextureProperty` bind by index into that array. The array is not shown anywhere: the editor's Graph Assets panel lists the project catalog filtered to the types the graph accepts, and dropping a tile onto the canvas or the variables panel calls `NodeGraph::findOrAddAssetReference`, which reuses the graph's existing entry for that class and path or appends one. A reference whose properties were later deleted stays in the file unused.

---

## Project scripts

Lua (LuaBridge3, `thirdparty/LuaBridge3`) is project-level. `ScriptObjectSystem` (`src/Editor/ECS/Script/ScriptObjectSystem.h`) is a `MikanTypedObjectSystem` registered last in `ProjectManager::startup`, so every other system's objects already exist when scripts run. Each `ScriptComponent` (definition `ScriptDefinition`, property `script_path`) names one script file through a `ScriptAssetReference` (`*.lua`). The system owns the project's single `ProjectScriptContext`, one `lua_State` shared by every script.

A script file defines one class derived from `ScriptBehavior`, and each `ScriptComponent` holds its own instance of its file's class. Two components can name the same file and carry different parameters, since parameters, triggers, and sequence callbacks belong to the instance rather than to the shared globals. A file's chunk runs once per state however many components share it (`CommonScriptContext::m_classRefByPath`), and the class it defines is the chunk's return value when that is a behavior class, otherwise the last class created while the chunk ran. A file that creates no class fails to load. A class left unnamed (`ScriptBehavior()` with no name) takes the file's stem.

```lua
local Rainbow = ScriptBehavior:extend("Rainbow")

function Rainbow:init()
	self.angle_speed = 2.0
	self.stage = ComponentRef("StageComponent")
	self._elapsed = 0
end

function Rainbow:Trigger_Reset(args)
	self._elapsed = 0
end

return Rainbow
```

The base class is Lua source embedded in `CommonScriptContext::addScriptBehaviorBase`, run at state creation after the coroutine scheduler and set as the global `ScriptBehavior` and as `package.loaded["ScriptBehavior"]`, so `require("ScriptBehavior")` is harmless. Classes chain through metatables and nothing is copied per instance. `ScriptBehavior:extend(name)` and `ScriptBehavior()` both make a subclass, calling a subclass constructs an instance and runs `init`, `super` reaches the parent class, and `is(cls)` tests ancestry. A class records the order its methods were declared and an instance records the order `init` assigned its public fields, which is the order the editor lists triggers and parameters in. `extend` reports each class it creates to the C++ side through `__mikan_behavior_class_created`, which records it against the loading script id so the file's class can be picked once the chunk returns.

On `postInit`, and on the next `update` after a script is added, removed, or has its path changed (`requestReload`), the system calls `reloadAllScripts()`: dispose the current state, create a fresh one, then load every script with a non-empty path in pool order (`loadBehavior`), bind the result to `ScriptRequestHandler`, and install the HTTP routes. With no scripts in the pool there is no Lua state at all. If any file fails to load, the whole state is disposed and the failing script's errors are reported. A single bad file takes down every script, not just its own. `OnScriptsReloaded(bool)` fires at the end of every reload and `getLastLoadErrors()` keeps the load errors of the last one for a listener that subscribed after it.

The outliner's project root has a top-level Scripts folder. Its add button calls `ScriptObjectSystem::addNewScript()`, which creates `<project>/scripts/script_<timestamp>.lua` and registers it. Selecting a script row shows its path (click to pick a different `.lua` file), Edit and Reload buttons, one button per trigger, the names of its HTTP trigger methods, and its parameters. Delete uses the outliner's Delete Component button. Reload re-runs every script in the pool, since they share one state.

Saving a script reloads it without the button. `ScriptFolderWatcher` (`Scripting/ScriptFolderWatcher.h`), owned by `MainWindow` and polled every frame beside the Lua debug server, watches `<project>/scripts` and the bundled `resources/scripts` through a `DirectoryWatcher` (`MikanUtility/Public/DirectoryWatcher.h`) each. The watcher wraps `FindFirstChangeNotificationW` on the folder tree, polled with a zero timeout, so no thread is involved. A signal marks a change pending, and once no further signal has arrived for the debounce window (250 ms, since editors write a file in several steps) it rescans the tree for `.lua` files, diffs the path to write time snapshot against the previous one, and reports the added, removed, and modified paths. Any change requests a reload, since a modified required module has no component of its own but is cached in `package.loaded`. An added or removed path also refreshes `ProjectAssetCatalog`, so the Assets panel follows files created, renamed, or deleted outside the editor. `MikanCmd` never constructs `MainWindow`, so it has no watcher.

`CommonScriptContext` (`Scripting/CommonScriptContext.h`) owns the `lua_State`: standard libs opened, a panic handler, error reporting (below), and a built-in coroutine scheduler. The scheduler injects globals `start_coroutine`, `wait_frames`, `wait_next_frame`, `wait_seconds`, `get_frame_delta_seconds`. `update_scheduler()` is invoked once per frame from `updateScript()`. A coroutine that fails is dropped and its error re-raised once the frame's other coroutines have run, so one broken coroutine neither starves the rest nor fails again on every later frame. `runScriptFile(path, scriptId)` runs one file's chunk with no class discovery, which the module tests use.

`require` resolves against the project's own `scripts` folder, then the bundled `resources/scripts`. `setupModuleSearchPath` prepends `<project>/scripts/?.lua` and `<project>/scripts/?/init.lua` to `package.path` when the state is created, followed by the same pair under `resources/scripts`, so `require("color")` loads `<project>/scripts/color.lua`, `require("easing")` finds the bundled module without a project copy, a project module shadows a bundled one of the same name, and a dotted name walks subfolders. Lua's stock entries stay behind those, and the search path is rebuilt on every reload, along with the `package.loaded` cache that lives in the state. A required module is not a `ScriptComponent`: it defines no behavior class, it is loaded on demand by whichever script requires it, and it returns a table rather than defining globals. `resources/scripts/easing.lua` and `formatting.lua` are written that way. A module may also define a behavior class for several files to share, and a file that wants that class as its own returns it: `return require("shared_behavior")`. The same context also replaces `package.searchers[2]`, the stock file loader, with one that finds the file through `package.path` but loads it under the chunk name above, so a breakpoint set in a required module resolves like one set in a project script.

Editor support: the project folder is the VS Code workspace for its scripts. `resources/lua-definitions/` holds LuaLS stub files (`mikan-core.lua`, `mikan-systems.lua`, `mikan-components.lua`) describing every binding above, kept in sync by hand with the `bindLuaFunctions` implementations they name. Loading a project writes two files into the project folder, each only when missing, so delete one to regenerate it after moving the editor install:

- `.luarc.json`: points the VS Code Lua extension's `workspace.library` at the stub directory
- `.vscode/launch.json`: an lrdb attach configuration for the debugger, with the workspace as its source root

The project's `.luarc.json` lists `resources/scripts` beside the API stubs, so the language server resolves the bundled modules too, and the repo root carries its own `.luarc.json` for `resources/scripts`. The script row's Edit button, a double-click on a script tile in the Assets panel, and a location in the script error dialog share one path, `ScriptAssetReference::openScriptInEditor(path, line)`, which runs the script editor command (an app setting). The command carries the placeholders `{project}`, `{file}`, and `{line}`, and the default is `code --reuse-window {project} --goto {file}:{line}`, which is how VS Code opens the project workspace with that file focused at that line. A stored command without `{file}` keeps the older behavior of receiving the project folder and the file as arguments, and a stored copy of the old default upgrades to the new one on load. A bundled script is copied into the project first (at the same relative path, so the component's stored path now resolves to the copy) unless the "Edit bundled resources" setting is on ([objects.md](./objects.md)). A script outside both trees opens on its own.

### Behavior methods

A behavior declares its entry points by method name. `CommonScriptContext::collectBehaviorMethods` walks the class chain in declaration order and files each name by prefix:

- `Trigger_<Name>(self, args)`: the trigger `<Name>`, a button in the script panel and the target of the client API, the HTTP panel, and the `script trigger` automation command
- `HttpTrigger_<Name>(self, args)`: reachable only through an HTTP route (below), listed by name in the script panel
- `OnMessage(self, message)`: receives a client script message and returns true when it handled it, which stops the message reaching later scripts
- `SequenceStart(self, sequence)`, `SequenceUpdate(self, sequence, timeSinceStart, deltaSeconds)`, `SequenceStop(self, sequence)`: drive a `DMXSequenceComponent` (below)

`ScriptContext.broadcastMessage(message)` emits a message to connected clients, and `ScriptContext.CullMode` carries the stencil cull enum. Those are the only two things left on the `ScriptContext` namespace.

Every method call goes through `CommonScriptContext::callBehaviorMethod(scriptId, methodName, argPusher, outError, kind)`, which pushes the instance as `self`, runs `lua_pcall` under a traceback message handler, and on failure fills `outError`, logs, and raises `OnScriptError` without disposing the state. A missing method is not an error. `ScriptComponent::getTriggerNames()` reads the instance's trigger list and `invokeTrigger()` fires its own instance only. A trigger run from the panel button or the `script trigger` automation command is bracketed as one transaction gesture ([transactions.md](./transactions.md)).

### Trigger arguments

`CommonScriptContext::invokeScriptTrigger(triggerName, args, targetScriptId)` calls `self:Trigger_<name>(args)` with exactly one table argument, empty when the caller supplied nothing. A method declared without the parameter ignores it. Values are always strings, so a script converts with `tonumber` where it needs a number. A target of `INVALID_MIKAN_ID` fires every instance whose class has the trigger, in pool order, and the call succeeds only when every one did. A specific target fires that instance alone, and fails when it lacks the trigger.

Each entry point fills that table from its own carrier:

- an HTTP route from the request's query string, so `/trigger/new_sub?user=bob&tier=3` arrives as `{ user = "bob", tier = "3" }`
- the `InvokeScriptTrigger` client request from its `trigger_args` map, targeted by `script_name` (the script component's object name) or broadcast when that is empty
- the `script trigger <name> [script=<scriptName>] [key=value ...]` automation command from its trailing tokens, with the `script` key reserved for the target
- the script panel button with nothing, on its own component only

Query string parsing lives in `HttpInterprocessMessageServer` (`parseQueryString`), which percent-decodes, treats `+` as a space, and keeps the last occurrence of a repeated key. Route handlers receive one `HttpRouteRequest` carrying the verb, the matched path, the decoded query args, and the body. The server has two kinds of route. Trigger routes run on the main thread inside `processRequests()`, with the connection thread parked until the answer arrives or a one second timeout elapses. Background routes run on the connection thread with the body moved in, and hop to the main thread only where they must through `runOnMainThread()`, which is how the asset upload route ([videosources.md](./videosources.md)) receives a movie without stalling a frame. The server binds loopback only unless the "Allow connections from other machines" setting in the HTTP Triggers panel is on, and a restart while a body is still arriving holds the main thread until that read ends or hits the 300 s request deadline.

### HTTP routes

Routes are project data, not script registrations. `ScriptObjectSystemDefinition` carries a `ScriptHttpRouteTable` (`Scripting/ScriptHttpRouteTable.h`) persisted under `http_routes` as `[{"route": "gift_sub", "script_component_id": 1017, "function": "GiftSub"}]`, one entry per route, with the route name unique and free of whitespace, a leading or trailing slash, or `//`. The system exposes it as one descriptor, `http_routes`, a UI-hidden and client-API-hidden STRING carrying the table's single-line JSON, the same treatment `script_variables` gets, so every edit notifies one name and undo re-applies the text verbatim. `ScriptObjectSystem::addHttpRoute`, `setHttpRoute`, and `removeHttpRoute` write the definition, and the system's definition listener pushes the table into `ScriptRequestHandler::setHttpRoutes`, which removes the installed `/trigger/<route>` handlers and installs the new set while a context is bound. A route resolves its script and method at request time, so a reload that swaps the instance or an edit that retargets the row needs no reinstall. `ScriptObjectSystem::isHttpRouteResolved` is true when the route names a loaded script whose instance has `HttpTrigger_<function>`.

The HTTP Triggers panel (`AppStages/Project/GuiPanel_HttpTriggers`) is the editing surface: the server port and reach above, then a table with the route text (committed on focus loss when valid and changed), a script component dropdown in pool order, a function dropdown of that instance's `HttpTrigger_` names, a fire button that invokes the route with empty args, and a remove button. A row whose script or method does not resolve draws its fire button disabled with an unresolved tooltip, and the route answers 400 until it resolves. Each edit is bracketed as an `http_route:<route>` gesture. `resources/scripts/stream_event.lua` is the worked example: a `HttpTrigger_GiftSub` that reads its args, caches the current scene, switches to an effect scene, broadcasts to clients, waits on a coroutine, and restores, plus a `Trigger_GiftSub` forwarding to it so the panel button still fires the event. It also carries the reentrancy guard such a flow needs, since a second event arriving mid-effect would otherwise cache the effect scene as the one to restore.

### Script parameters

Every field `init` assigns whose name does not start with `_` becomes a parameter the script panel shows as a widget and the project file persists, in declaration order. The type comes from the value: boolean, integer, number, string, or `Vec3f` map to BOOL, INT, FLOAT, STRING, VECTOR3F. Lua keeps integer and float subtypes apart, so `30` makes an INT and `30.0` a FLOAT. A `ComponentRef("StageComponent")` value makes a component reference of that class. A field holding a table, a function, or a foreign userdata is skipped with one logged warning, so a script keeps such state private by prefixing the name with `_`. `CommonScriptContext::reflectParameters` reads the instance's `_paramOrder` list after `init` returns and resolves each field against the component's definition.

Values live in the component's `ScriptDefinition` (`ScriptVariableTable`, `Scripting/ScriptVariableTable.h`), persisted under `script_variables` as `{"type": name, "value": ...}` entries keyed by field name. On every reload the instance declares its fields again and the context resolves each one against the definition: a stored value of the same name and type wins, otherwise the default is adopted and stored. The effective value is then written to `self.<name>`. A stored entry the script no longer declares stays in the definition and is not shown.

The definition is the single source of truth. A panel edit, an undo, or an automation set writes the definition, and `ScriptComponent::onDefinitionMarkedDirty` pushes every stored value back into the instance through `CommonScriptContext::setVariableValue(scriptId, name, value)`. A `Vec3f` field is a userdata copy replaced on each push, so scripts read parameters off `self` inside method bodies rather than caching them in `init`. Script-side writes to a field are not persisted.

A component reference is an INT component id (`INVALID_MIKAN_ID` for none) tagged with a `k_componentClassName` string, persisted as `{"type": "component", "class": name, "value": id}`. The panel draws it as a dropdown of that class's live components with a leading none entry (`GuiDataSource_OptionalComponentComboBox`), finding the owning system as the one whose `getComponentIdList` answers for the class. The field holds the component handle or `nil`. LuaBridge pushes a pointer by its static type, so `ProjectScriptContext` registers one push thunk per bound component class (`CommonScriptContext::registerComponentClass`) and a `ComponentRef` naming any other class is a load error. That thunk table is how any component reaches Lua as its concrete class: `CommonScriptContext::pushComponent` looks the thunk up by `getComponentClassName` and pushes exactly one value, nil for a null component or an unbound class. A binding that returns a base pointer has to go through it, since returning the base type would hand Lua the base class's metatable and hide the subclass's own fields. `DMXFixtureGroupComponent::getFixtureAtIndex` is the case that needs it, because a group holds fixtures of mixed kinds: it reaches the context through `CommonScriptContext::getFromLuaState`, which reads the owning context out of the state's registry, and returns the fixture as `RGBSpotLightComponent` or `RGBPixelGridComponent`. The id resolves through `ProjectManager::getComponentById` with an exact class check. The field is written on load, on every panel write, and on object lifecycle: `ScriptObjectSystem` subscribes to every system's `OnNewObjectFinalized` and `OnObjectWillBeDestroyed` and calls `refreshComponentVariables`, excluding the id of an object about to be destroyed, so a script never holds a handle to a dead object and an undone destroy restores the handle. Scripts nil-check a reference inside method bodies.

The property surface is one descriptor, `script_variables`, a UI-hidden and client-API-hidden STRING carrying the table's single-line JSON. Every parameter edit notifies that one name, which is what lets the transaction recorder capture it and undo re-apply the text verbatim. The panel draws the parameters itself through `ScriptComponent::getScriptVariableNames` / `getScriptVariable` / `setScriptVariable`, labeled by their field names.

### Sequences

A `DMXSequenceComponent` (objects.md) names a script component through `script_component_id` (`INVALID_MIKAN_ID` for none). The panel offers only script components whose instance has a `SequenceUpdate` method, through a component dropdown with a none entry. While playing, `DMXSequenceSystem::update` ticks the component, which calls `SequenceUpdate(sequence, timeSinceStart, deltaSeconds)` once per frame through `CommonScriptContext::callBehaviorMethod`, with `SequenceStart(sequence)` on play and `SequenceStop(sequence)` on stop when the class has them. The method writes into the sequence's frame buffer (`setFixtureColor`, `setPixel`, `setFixtureChannels`, `fillGroup`) and the system pushes that buffer to the group's fixtures after the callback returns, so one frame is one send per fixture. The script resolves by component id every frame, so a script reload that keeps `SequenceUpdate` keeps a playing sequence going, and one that drops it stops the sequence. A Lua error inside a callback is reported naming the sequence method and stops that sequence only. Like every other runtime error it leaves the shared state alive, since a frame-rate callback that took every script down would make the state unusable. `resources/scripts/sequence_chase.lua` is the worked example. A project saved with the older `sequence_name` handler string loads its sequences unassigned, since a handler name maps to no component, and they are picked again in the panel.

A sequence whose `content_source` is not `Script` is rasterized by the editor instead, and a script becomes optional. Only `SequenceStart` and `SequenceStop` are called there, never `SequenceUpdate`, since C++ owns the pixels for those sources. The point of `SequenceStart` is dynamic content selection: `sequence:setText(s)` and `sequence:setContentPath(p)` set the text or image the run uses. Both are runtime overrides cleared on stop, so picking content never writes the definition and never records a transaction, and `sequence:getText()` / `getContentPath()` read back whichever value is in force. A script that is named but missing stops a `Script` sequence and is ignored by a rasterized one.

### Script errors

Every Lua failure becomes a `ScriptError` (`Scripting/ScriptError.h`): its kind (load, trigger, HTTP trigger, message, sequence, coroutine), the script id, the chunk name and line parsed from the message, the file that chunk name resolves to, the message with its location prefix stripped, the traceback, and the method or trigger that was running. `CommonScriptContext::reportLuaError` builds it from the message the traceback handler left on the stack, logs it, and raises `OnScriptError`. Chunk names are project-relative paths (or absolute for a script outside the project), which `resolveScriptErrorPath` tries against the project folder, then the bundled resources. Lua shortens a long source name to `...<tail>`, which the resolver matches by suffix against the loaded scripts and then the two script folders. `parseLuaErrorLocation` skips a Windows drive colon, since a line number is digits between two colons.

A load error (a chunk that fails to compile or run, a file with no class, an `init` that throws, a `ComponentRef` of an unbound class) fails the reload and disposes the state. Every other kind leaves the state running. `ScriptObjectSystem` forwards `OnScriptError` and holds the load errors of the last reload, and `AppStage_Project` subscribes on enter and drains those, since the first reload happens while the project loads, before the stage exists. Errors open `ModalDialog_ScriptErrors` (`AppStages/ModalMessageBox/`), one dialog that collects every error reported while it is open, so a burst never stacks dialogs. An entry whose file and line resolved draws its location as a button that opens the script editor there through `ScriptAssetReference::openScriptInEditor`. A successful reload closes a dialog that holds only load errors, so fixing the file and saving clears it. Runtime errors stay listed until the dialog is closed. The log panel receives every error as well.

`resources/scripts/generate_lights.lua` is the worked example of a trigger with parameters: a `Trigger_GenerateLights` that lays out `RGBSpotLightComponent` objects on a stage in a zig-zag grid with sequential DMX addressing, every parameter a field of `init`.

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

- `InvokeScriptTrigger { script_name, trigger_name, trigger_args }` request: `trigger_name` is the `<Name>` of a `Trigger_<Name>` method. An empty `script_name` fires every script component whose behavior has it; otherwise it names the one script component (its object name) to fire. `trigger_args` is a string-to-string map that becomes the trigger's argument table and may be empty. Result codes:
	- `MalformedParameters`: no script has the trigger, the named script does not exist, or it lacks the trigger.
	- `RequestFailed`: no project script state is loaded, or a trigger call failed.
	- `Success`: otherwise.

- `SendScriptMessage` request: offers the message to the bound context's handlers in registration order until one returns true. An unhandled message is still `Success`.

- `MikanScriptMessagePostedEvent`: published to clients whenever a script calls `broadcastMessage`.

- HTTP triggers: `setHttpRoutes(table)` installs one route under `/trigger/<route>` on the HTTP message server per entry of the project's route table, while a context is bound. The route ignores the HTTP verb and passes the request's query args to `HttpTrigger_<function>` on the named script component, which is what lets a GET-only caller (a Stream Deck, Streamer.bot's Fetch URL sub-action) carry a payload. Responses are JSON result codes mapped from `MikanAPIResult`: 400 when the script or method is missing, 422 when the method failed, 200 otherwise.

---

## Lua debugging

`LuaDebugServer` (`Scripting/LuaDebugServer.h`) is a singleton LRDB debug server on TCP port 21110 (the vscode-lrdb extension default). `MainWindow` starts it listening before the first project loads, and calls `poll()` every frame. The project's `ProjectScriptContext` attaches itself when its Lua state is created (`createScriptState`) and detaches when the state is disposed (`disposeScriptState`), so breakpoints in any project script work without a manual attach step. Scripts can call `lrdb_break()` for a programmatic breakpoint. Chunk names are paths relative to the project folder (absolute for a script outside it), which is what the generated launch config's `sourceRoot` makes VS Code send for a gutter breakpoint, so the two match. To debug: open the project folder in VS Code, set breakpoints, and run the "Attach to MikanXR Lua" configuration while the editor is running. A trigger button, HTTP route, or `script trigger` automation command then hits them. See [debugging.md](./debugging.md).

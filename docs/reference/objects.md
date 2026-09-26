# Objects

The editor's scene object system: everything under `src/Editor/ECS`. Objects are component containers, components pair with Configuru-backed definition configs, and per-type "object systems" own pools of them under a single `ProjectManager`. This doc covers the core types, lifecycle, transforms, persistence, IDs, the property/function surface that makes objects remotely controllable, and selection/gizmos. See [modules.md](./modules.md) for where `MikanEditor` sits in the build, [wire-protocol.md](./wire-protocol.md) for the client-facing property contract, and [scripting.md](./scripting.md) for the project's Lua scripting.

---

## Core types

- `MikanObject` (`src/Editor/ECS/MikanObject.h`) is a named component container. It holds a `std::vector<MikanComponentPtr>`, a weak pointer to its owning `MikanObjectSystem`, and a root `TransformComponent`. Components are added with `addComponent<T>()` and found with `getComponentOfType<T>()` / `getComponentOfTypeAndName<T>()` (dynamic-cast based, no type registry). Lifecycle is `init()`, `postInit()`, `dispose()`, called by the owning system.

- `MikanComponent` (`src/Editor/ECS/MikanComponent.h`) is the component base. It implements `IEntityAccessor` (see below), owns a `MikanComponentDefinitionPtr`, exposes `getComponentClassName()` via a per-class `k_componentClassName` string constant, and can opt into per-frame `update(float deltaSeconds)` by setting `m_bWantsUpdate` in its constructor. `customRender(...)` is invoked by the owning system for debug drawing.

- `MikanComponentDefinition` is a `CommonConfig` subclass (`src/Editor/Config/CommonConfig.h`) holding the persisted state for a component: `m_componentId`, `m_componentName`. It implements `writeToJSON()` / `readFromJSON()` (Configuru) and `readFromInitParams()` (initialization from a client-supplied `Serialization::PolymorphicObjectPtr`, the wire-protocol creation path).

- `MikanObjectSystem` (`src/Editor/ECS/MikanObjectSystem.h`) owns a `MikanObjectList` and a `MikanObjectSystemDefinition` (also a `CommonConfig`). Systems are created and ticked by `ProjectManager` (`src/Editor/Project/ProjectManager.h`), which also owns the `MikanPropertyDatabase` and `MikanFunctionDatabase`. Systems find each other with `getObjectSystemOfType<T>()`.

The design rule throughout: the definition (config) is the persistent, change-notified source of truth; the component is the runtime view of it. `CommonConfig` provides `OnPropertyChanged` with a `ConfigPropertyChangeSet`, child-config nesting via `addChildConfig()`, autosave (`updateAutoSave`), and a `wantsSaveForPropertyChange()` veto (used by `CameraDefinition` to avoid saving every frame while a tracking mount drives the camera transform).

---

## Typed object systems and object creation

Most systems derive from the template `MikanTypedObjectSystem<TComponent, TDefinition, TID, TSystem, TSystemDefinition>` (`src/Editor/ECS/MikanTypedObjectSystem.h`): one system manages one pool (`MikanTypedComponentPool`) of one primary component type keyed by a typed ID (e.g. `AnchorObjectSystem : MikanTypedObjectSystem<AnchorComponent, AnchorDefinition, MikanSpatialAnchorID, AnchorObjectSystem, AnchorObjectSystemDefinition>`). The matching `MikanTypedObjectSystemDefinition<TComponent, TDefinition, TID>` holds a `MikanTypedComponentPoolDefinition` (added as a child config) that serializes the list of component definitions.

Object construction is centralized in `MikanTypedObjectSystem::objectFactory`:

1. `newEmptyObject()` creates the `MikanObject`.
2. The primary component is added and bound to its definition (`setDefinition`).
3. If the primary component is a `TransformComponent`, it becomes the object's root component.
4. `additionalComponentFactory(...)` lets the derived system add secondary components (e.g. `AnchorObjectSystem` adds a `SelectionComponent` and a `BoxColliderComponent` attached to the root).
5. `MikanObject::init()`, then `postInit()` if the system is already initialized.
6. The definition is added to the system definition's pool (which fires the config property-change event), and `OnNewObjectFinalized` is broadcast.

Creation entry points: `addNewObjectByTypedDefinition(initFunc)` in C++, and `addNewObjectByUntypedDefinition(primaryComponentClass, initParams)` for wire-protocol creation (matches on `TComponent::k_componentClassName`, then calls `TDefinition::readFromInitParams`). A new component is named `<prefix>_<id>` by `resolveDefaultComponentName` (`src/Editor/ECS/ComponentNaming.h`), where the prefix comes from the app settings prefix table (`AppSettingsConfig::getComponentNamePrefix`, keyed by component class name, edited in the Component Naming section of Project Settings) and defaults to the short codes listed there (`CAM`, `LIGHT`, `SRC`, `STENCIL`, and so on). A class with an empty prefix names by its class, `TrackingMountComponent_1024`. An init function that sets its own name overrides the default, and undo recreation keeps the persisted name. Names are not checked for uniqueness: every by-name lookup (`getTypedComponentByName`, the Lua `get*ByName` bindings, a shape graph's `texture_source_name`) returns the first match. Hand-given names follow the same `PREFIX_Name` shape (`CAM_Desk`, `SEQ_Rainbow`), and the bundled `generate_lights.lua` names its lights `LIGHT_gen_<n>` and finds them again by that prefix. Loading a project calls `init()` on each system, which runs `m_componentPool.initializeFromDefinitions(...)` over the saved definitions. Deletion (`removeObjectByPrimaryComponentId`) is three steps in order: dispose the object (fires `OnObjectDisposed` / `OnComponentDisposed`), remove the component from the pool, then remove the definition from the system definition.

---

## The object systems that exist

`ProjectManager::startup` (`src/Editor/Project/ProjectManager.cpp`) registers, in order: `EditorObjectSystem`, `ClientTextureSourceSystem`, `SpoutTextureSourceSystem`, `CEFTextureSourceSystem`, `NetworkVideoSourceSystem`, `USBVideoSourceSystem`, `MarkerObjectSystem`, `StageObjectSystem`, `SceneObjectSystem`, `CompositorObjectSystem`, `CameraObjectSystem`, `AnchorObjectSystem`, `QuadShapeSystem`, `BoxShapeSystem`, `ModelShapeSystem`, `QuadStencilSystem`, `BoxStencilSystem`, `ModelStencilSystem`, `VRObjectSystem`, `TrackingMountObjectSystem`, `MarkerTrackingVolumeSystem`, `VRTrackingVolumeSystem`, `DMXObjectSystem`, `RGBSpotLightSystem`, `RGBPixelGridSystem`, `DMXFixtureGroupSystem`, `DMXPresetSystem`, `LightEnvironmentSystem`, `ScriptObjectSystem`, with `DMXSequenceSystem` inserted just before `DMXObjectSystem` so a sequence frame's fixture writes flush over DMX the same frame. The order matters: `EditorObjectSystem` is first so it receives the component-creation events the later systems fire during init, and `ScriptObjectSystem` is last so its scripts can resolve every other system's objects by name when they load. `ARKitVideoSourceSystem` and `FileVideoSourceSystem` follow `USBVideoSourceSystem`.

Primary component families (each with a matching `*Definition` config class):

- Scene structure: `SceneComponent` and `StageComponent` (both extend `TransformComponent`; a scene attaches to a stage via `attachToStage`, a stage references a tracking volume and stores stage bounds).
- Spatial: `AnchorComponent`, `MarkerComponent` (ArUco id + physical length in mm), `TrackingMountComponent` (binds a VR device by path/socket), `TrackingVolumeComponent` with `VRTrackingVolumeComponent` / `MarkerTrackingVolumeComponent` subclasses, `VRDeviceComponent` (runtime-only, under `VRObjectSystem`).
- Camera/video: `CameraComponent` (stage id, tracking mount id, video source id, aperture offset, frame sync mode, and the runtime-only pose-driven flag that resolves the mode's auto setting), `VideoSourceComponent` with `USBVideoSourceComponent` / `NetworkVideoSourceComponent` / `ARKitVideoSourceComponent` / `FileVideoSourceComponent` subclasses (see [videosources.md](./videosources.md)).
- Compositing inputs: `CompositorComponent`, `TextureSourceComponent` with `ClientTextureSourceComponent` / `SpoutTextureSourceComponent` / `CEFTextureSourceComponent` (see [compositor.md](./compositor.md)).
- Stencils and shapes: `StencilComponent` base with `QuadStencilComponent` / `BoxStencilComponent` / `ModelStencilComponent`; parallel `ShapeComponent` family `QuadShapeComponent` / `BoxShapeComponent` / `ModelShapeComponent`.
- Lighting: `DMXFixtureComponent`, `RGBSpotLightComponent`, `RGBPixelGridComponent` (physical fixtures driven over DMX. Every fixture carries its emitter spec, `max_wattage` and `lumens_per_watt`, on the shared `DMXFixtureComponentDefinition`: a channel byte is only a fraction of full output, so those two are what let a client turn one into a light of the right strength rather than guessing per integration. A pixel grid also carries its physical layout, `pixel_size_mm`, `pixel_separation_mm`, `origin_pixel`, and `zig_zag`, which together map a grid cell to its position in the DMX stream through `RGBPixelGridDefinition::getPixelWireIndex`. The pixel buffer stays in wire order, so that mapping is the only place the physical arrangement is interpreted: the viewport boxes, the wiring path, and the preset and sequence swatch grids all read through it. Columns run along local +X and rows along -Y, so the panel faces +Z, and the grid gets one `BoxColliderComponent` sized to the whole panel rather than one per pixel), `DMXFixtureGroupComponent` (a named set of fixture ids on one stage, the unit presets and sequences address; membership is many-to-many, and `DMXFixtureGroupSystem` prunes a fixture from every group on the fixture systems' `OnComponentDisposed`, which fires inside the destroy composite so the membership change undoes with the destroy), `DMXPresetComponent` (a fixed set of DMX channel bytes per member fixture of one group, keyed by fixture id and persisted as a `preset_data` JSON property; `capture_preset` snapshots the live fixtures through `DMXFixtureComponent::getChannelValues` as one recorded transaction, `apply_preset` writes them back through `setChannelValues` with zeros for members the preset holds nothing for, and `DMXFixtureGroupSystem::removeGroup` destroys a group's presets and sequences before the group so undo recreates the group first), `DMXSequenceComponent` (an animation of one group: `duration_seconds` and `loop`, runtime playback state, and a frame buffer the system applies through `setChannelValues`. `content_source` picks who fills that buffer: the `SequenceUpdate` method of the script component named by `script_component_id`, or one of three sources the editor rasterizes itself, a scrolling bitmap, scrolling UTF-8 text, or a played animation. The rasterized sources draw onto the group's first `RGBPixelGridComponent` member through `DMXSequenceContent`, which owns the canvas, the stb decoders, the text rasterizer, and the two pure time-to-content mappings. `brightness` is a 0 to 1 dimmer applied in `applyFrameBuffer`, the one place the frame buffer becomes DMX, so it covers every content source including a script's writes; scaling the channel bytes is exactly what converting to HSV, scaling the value component, and converting back would produce, since each of R, G, and B is linear in V for a fixed hue and saturation. It scales every channel because every fixture kind a group can hold is RGB triples; see scripting.md), plus `LightEnvironmentComponent`, a captured lighting probe rather than a fixture (see [scene-lighting.md](./scene-lighting.md)).
- Editor-only (secondary components, not system primaries): `SelectionComponent`, the gizmo components, `ColliderComponent` variants (`BoxColliderComponent`, `DiskColliderComponent`, `MeshColliderComponent` backed by `StaticMeshKdTree`), `StaticMeshComponent`.
- Scripting: `ScriptComponent`, one per project script file, under `ScriptObjectSystem` (see [scripting.md](./scripting.md)).

A fixture belongs to the stage that created it for life. Its `stage_id` is read only and UI hidden, like a camera's, and only the outliner's add actions set it. Groups, presets, and sequences all address fixtures within one stage, so moving a fixture to another stage would leave a hole in every group holding it, and nothing offers that. Supporting it would mean deciding what happens to those memberships first.

`LightEnvironmentComponent` is worth calling out because it breaks the fixture pattern of its neighbors. It extends `TransformComponent` so a probe has a world position, which is what leaves room for multiple probes later without a wire-format change. Its definition persists 27 spherical harmonic floats plus an exposure scalar and the estimate's confidence numbers, and `customRender` draws a sphere shaded with the recovered environment so a committed probe is visible in the viewport. That sphere is opaque and, at its 10 m radius, encloses the scene from inside in the perspective view, so anything translucent that writes no depth must be drawn after it: the spot lights' additive cone volumes go through `RGBSpotLightSystem::renderConeVolumes` at the end of `AppStage_Project::render`, after the scene pass and the grid, rather than inside `customRender`.

Cross-system queries live in free-function headers: `ObjectSystemColliderQueries.h`, `ObjectSystemRenderQueries.h`, `TextureSourceQueries.h`, `TrackingVolumeQueries.h`, `VideoSourceQueries.h`.

---

## Component IDs

Component IDs are project-wide integers. `ProjectConfig` owns two allocators (`src/Editor/Project/ProjectConfig.h`): a `PersistentIDAllocator` whose counter is saved in the project file (so IDs never recycle across sessions) and a `MonotonicIDAllocator` for transient runtime-only components (e.g. `VRObjectSystem` devices) in a reserved high range (`k_transientIdStart`). The primary component's ID doubles as the domain ID: `getSceneId()`, `getCameraId()`, `getStageId()` etc. all return `getComponentId()`. `ProjectManager::getComponentById` resolves any ID by asking each system's pool.

---

## Transforms and parenting

`TransformComponent` (`src/Editor/ECS/Scene/TransformComponent.h`) holds a relative `GlmTransform` (TRS, `src/Libraries/MikanMath/Public/Transform.h`) and a cached world `glm::mat4`. Parenting is by component ID: `TransformComponentDefinition` persists `m_parentTransformId` (`k_parentTransformIdPropertyId`) alongside the relative scale/rotation/position. At runtime `attachToComponent(newParent)` / `detachFromParent(reason)` maintain parent/child weak-pointer lists, and `propogateWorldTransformChange` recomputes and pushes world transforms down the child list. `setWorldTransform` back-computes the relative transform from the parent, and `reparentPreservingWorldTransform` pairs the two so a reparent changes only which frame the component is expressed in. Math conventions (handedness, units, matrix order) are in [conventions.md](./conventions.md).

### Which parents a component accepts

The scene hierarchy is fixed by type, not by convention. `TransformComponent::canAttachToParent` states the rule per component class and `attachToComponent` is the only place it is enforced, which covers every path that sets a parent: project load through `postInit`, the outliner drag-drop, the component panels' parent pickers, and `setPropertyValue("parent_transform_id")` from the client API, Lua, or automation. A refused attach is logged and leaves the component unparented rather than repaired, where the outliner surfaces it for a manual fix.

| Component | Accepted parent |
| --- | --- |
| `StageComponent` | none, it is a root |
| `CameraComponent` | `StageComponent` |
| `SceneComponent` | `StageComponent` |
| `DMXFixtureComponent` (`RGBSpotLightComponent`, `RGBPixelGridComponent`) | `StageComponent` |
| `LightEnvironmentComponent` | `CameraComponent` |
| `AnchorComponent`, `StencilComponent`, `ShapeComponent` | `SceneComponent`, or one another |

The last row is the scene-actor rule, shared by the three types through `isValidSceneActorParent` (`src/Editor/ECS/Scene/SceneActorParenting.h`). Every other `TransformComponent` subclass keeps the permissive base: colliders, static meshes, gizmo handles and VR device sockets are attached internally to their own object's root and are not part of the authored hierarchy. Attaching into one's own subtree is refused for all of them.

---

## Which scenes draw

Every stage draws in the project viewport, so two stages can be aligned against each other. Scene actors do not: a scene draws when it is the current scene, or when its `force_render` flag is set. Without that, two scenes on one stage stack their geometry on top of each other, which is unreadable in the editor and casts inactive-scene shadows on a client. `force_render` on the current scene is a no-op.

`SceneComponent::shouldRender` states the rule and the free `isSceneGeometryRendered` (both in `src/Editor/ECS/Scene/SceneComponent.h`) applies it to one object, by walking its root component up the transform chain to the first `SceneComponent`. An object under no scene, which is every stage-parented camera, light and tracking volume, always draws.

That predicate is passed at two seams, so nothing is drawn that cannot be clicked and nothing is clicked that is not drawn:

- `addAllRenderablesToMkScene` (`src/Editor/ECS/Scene/ObjectSystemRenderQueries.h`), which `AppStage_Project::renderProjectScene` and the compositor output window's debug overlay submit through, alongside the matching `MikanTypedObjectSystem::customRender` filters
- `findClosestCollisionAlongRay` (`src/Editor/ECS/Collision/ObjectSystemColliderQueries.h`), which `EditorObjectSystem::findClosestSelectionTarget` raycasts through

Outliner selection is unfiltered: picking a hidden scene's actor from the tree stays possible, since that selection is explicit rather than spatial. The compositor node graph is unaffected either way, because `DrawLayerNode` and `DepthMaskNode` take their stencil sets from author-chosen pin arrays rather than from everything in the project.

`force_render` crosses the wire on `MikanSceneComponentValues`, beside the scene system's `current_scene_id` on `MikanSceneSystemValues` ([wire-protocol.md](./wire-protocol.md)), so a client applies the same rule to the actors it spawns.

---

## Persistence

The project file is a single Configuru JSON document with extension `.mikanproj` (`ProjectManager::k_mikanProjectFileExtension`), written by `ProjectConfig::writeToJSON` via `ProjectManager::saveProject` / loaded by `loadProject`. `ProjectConfig` holds one `*ObjectSystemDefinition` per system (created through `addTypedDefinition<TConfig, TSystem>`, keyed by the system's `k_objectSystemClassName`), each of which serializes its component-definition pool under `k_componentPoolPropertyId`. So the persistence pattern per object type is: component class, definition (config) class, system definition holding the pool of definitions, all nested as `CommonConfig` children of `ProjectConfig`. `CommonConfig::readVector2f` / `readVector3f` zero their output when the key is absent rather than leaving the member alone, so a new vector property added to an existing definition must guard its read with `has_key` or every project saved before it existed loads zeroes instead of the default. Definition property setters call the change-notification helpers, which mark the config dirty and drive the autosave cooldown. The same notification chain feeds the editor transaction recorder ([transactions.md](./transactions.md)).

---

## Asset references and the project Assets panel

Every file a project uses (graphs, models, scripts, materials, textures, fonts) is named through an `AssetReference` (`src/Editor/Asset/AssetReference.h`) or through a string property tagged with `AssetReferenceFactoryMetaData`. Paths are stored in project form: forward slashes, relative to the project directory when the file sits under it (`PathUtils::makeStoredProjectPath`), and resolved through `PathUtils::resolveProjectResource`, which looks in the project first and then in the bundled `resources/` folder. `AssetReference::setAssetPath` and every tagged definition setter normalize into that form, so an absolute path handed in from anywhere stores relative. A project therefore owns only the assets it adds or edits: everything bundled with the application resolves in place, and a project file at the same stored path shadows the bundled one.

`ProjectAssetCatalog` (`src/Editor/Asset/ProjectAssetCatalog.h`, owned by `MainWindow`, reached through `IEditorWindow::getAssetCatalog`) is the scanned view of the project's asset folders. Each folder is a `ProjectAssetFolderDesc`: a project subfolder, the bundled subfolder of the same kind under `resources/` that shows through behind it, and the asset factories whose file types it holds. The folders are:

- `compositors` (`*.compgraph`), `shapes` (`*.shapegraph`), `models`, `scripts`, `textures`, `movies`: one entry per matching file. `movies` holds recorded takes and their `*.pose.json` pose tracks, and `textures` lists pose tracks too so a photo's sidecar sits beside it
- `compositor_materials` (`*.compmat`), `shape_materials` (`*.shapemat`): one entry per material with its sibling graph and shader sources hidden
- `fonts`: bundled-only, `resources/font/`, with no Add button

Every folder is scanned project first, then bundled, and a bundled file whose stored path a project file already holds is not listed. Bundled entries carry `bBundled` and are read-only (`bReadOnly`) unless the app setting "Edit bundled resources" is on, a developer switch in the Settings panel (`AppSettingsConfig::getEditBundledResources`, mirrored into the catalog by `setBundledResourcesEditable`) that lets the editors save and delete bundled files in place. The catalog stores paths only and creates `AssetReference` instances (which may own a GL preview) lazily on a panel's request, so it refreshes headless and before any window exists. It rescans on project load, after an import or delete, after a graph or material save, and when the setting flips. Every asset folder exists in a loaded project: `ProjectManager::loadProject` creates any that is missing and logs it, so a new project starts with empty folders and copies nothing, a project saved before a folder kind existed gains that folder on its next load, and an Add button's file dialog always has a folder to open in.

The project stage's Assets panel (`GuiPanel_Assets`) browses the catalog. Its Add buttons are the only import path (the two material folders also carry a New button that authors a material in the material editor): the picked file is copied into the folder (a material's whole folder, under its domain's folder), a name collision gets a numeric suffix and a duplicate material is refused, and the copy is selected in the panel. A bundled tile's right-click menu offers Copy to Project, which imports it at its own relative path so the copy shadows it. Right-click delete runs `ProjectAssetCatalog::findReferences` first, which reads every graph file and every material file under the project and under `resources/` as raw JSON plus every tagged component property in the loaded project, and refuses with the referrer list when anything still names the asset. The compositor, shape, and material editors carry a Graph Assets panel that is the same catalog filtered to the types their graph accepts. Drag and drop never crosses OS windows, since each editor window has its own ImGui context: the project panel feeds the component property rows in the main window, and each editor's own panel feeds its canvas.

Editing a bundled asset without the switch never writes into `resources/`. A graph or material opened from a bundled file saves through the Save As dialog, opened at the project path that shadows it (`ProjectAssetCatalog::makeProjectShadowPath`), and a build-only compile of a bundled material graph is refused until it is saved into the project. The script row's Edit button and a double-click on a script tile copy a bundled script into the project first and open the copy. Since the copies keep the bundled stored path, components that referenced the bundled file follow the project copy with no change.

A property row tagged with `AssetReferenceFactoryMetaData` draws as the type's glyph and the asset's file name and accepts a drop of a matching asset (`AssetPropertyGui::drawAssetReferenceProperty`). There is no file browser on the row. The project panel's payload is a `ProjectAssetDragPayload` matched by the factory's file patterns, the editor panels' payload is the `AssetReferencePtr` matched by class name. A pattern matches as a lowercase suffix of the file name rather than against `path::extension()`, so a two-part pattern such as `*.pose.json` works.

---

## Remote control and the property system

Every component and system implements `IEntityAccessor` (`src/Editor/ECS/IEntityAccessor.h`), which is `IPropertyInterface` + `IFunctionInterface`:

- Static `getPropertyDescriptors` / `getFunctionDescriptors` declare the schema (`PropertyDescriptor` with `MikanVariantType`, flags like `setReadOnly()`, `setUIHidden()`, `setClientAPIHidden()`).
- Virtual `getPropertyValue` / `setPropertyValue` (`MikanVariant`) and `invokeFunction` implement it.
- `getClientAPIValuesStructType()` returns the matching client-API values struct as a `Serialization::StructTypeHandle`, the opaque reflected-struct handle ([wire-protocol.md](./wire-protocol.md)).

`MikanTypedObjectSystem::registerPropertyDescriptors` registers both system and component schemas into the `ProjectManager`-owned `MikanPropertyDatabase` / `MikanFunctionDatabase`; each typed system also exposes its ID list via `k_componentIdListPropertyId` (served by `GetComponentListRequest`). The websocket side is `PropertyRequestHandler` / `FunctionRequestHandler` in `src/Editor/Server`; the values-struct/descriptor/`getPropertyValue` consistency is enforced by the schema-guard test run through `MikanCmd -runTests`. Details in [wire-protocol.md](./wire-protocol.md).

Note that `IRemoteControllable` / `RemoteControlManager` (`src/Editor/Server`) is a separate, string-command mechanism for remote-controlling `AppStage` UI screens (push/pop stage, `handleRemoteControlCommand`), not scene objects. Scene objects are remotely controlled through the property/function databases above.

A fixture's live channel bytes are the exception: they are not properties and never cross as component values. `RGBSpotLightComponent`'s `red`/`green`/`blue` are `setClientAPIHidden` runtime members, not even persisted, so a project loads with every fixture dark until a preset, sequence or script writes one. A client learns the colour only from the DMX stream in `LightRequestHandler`, which sends two ways:

- subscribing answers with the current data, so a client that connects between writes is not left holding zeros
- `DMXObjectSystem::update` broadcasts afterwards, once per tick in which a universe was written

`DMXObjectSystem::extractUniverseData` answers for any universe; the per-tick filter lives in the broadcast caller through `getDirtyDMXUniverseIdSet`. Keep it that way. Folding the dirty check back into the extract silently empties both the subscribe snapshot and the `GetDMXData` request, which is a bug that presents as a client rendering black lights rather than as an error.

Reaching the physical fixtures is a separate path, `MikanDMX` (`src/Libraries/MikanDMX`). `DMXObjectSystem::writeUniverseData` hands channel bytes to `DMXSendThread`, which transmits E1.31 from its own thread at `transmit_rate_hz`. Two rules there are easy to get wrong:

- **Every known universe transmits every tick**, not only the ones that changed. A receiver times its stream out after a couple of seconds of silence and falls back to its idle effect, so change-only output strands any look that is deliberately held. A universe only exists once something has written it, so an idle project still sends nothing.
- **A universe listed in `dmx_destinations` is unicast to each of its addresses; anything else goes to its `239.255.x.y` group.** Multicast is the E1.31 default and right on a wired LAN, but a controller on Wi-Fi frequently never receives it, because consumer access points drop multicast they hold no IGMP state for and often expose no setting to change that. The table is keyed by universe alone: E1.31 sends a universe whole and the receiver selects its own channel window, so a channel range here could not affect what is sent.

The table is editor-only (`setUIHidden`, `setClientAPIHidden`), persisted as a configuru array and exchanged as JSON text by the DMX panel and the automation channel. LAN routing is not a client's concern.

---

## Selection, interaction, and gizmos

`EditorObjectSystem` (`src/Editor/ECS/Editor/EditorObjectSystem.h`) is the interaction hub. Viewports feed it mouse rays (`onMouseRayChanged`, `onMouseRayButtonDown/Up`); it raycasts against `ColliderComponent`s (`findClosestSelectionTarget`, `ColliderRaycastHitResult`) filtered by an object-system selection filter. Hit objects interact through their `SelectionComponent` (`src/Editor/ECS/Editor/SelectionComponent.h`), which broadcasts hover/grab/move/release/selected delegates; `setSelection` fires `OnSelectionChanged`. The gizmo object is built once per activated scene, by `createSceneTransformGizmo` from `onSceneActivated`, out of `GizmoTransformComponent` plus `GizmoTranslateComponent` / `GizmoRotateComponent` / `GizmoScaleComponent`, hit-tested through generated box/disk colliders. Selecting an object with a transform snaps that existing gizmo to it rather than spawning one. A project with no activated scene therefore has no gizmo, which a newly created project is until the first scene is added, so every use of `m_gizmoComponentWeakPtr` is null checked. `EditorObjectSystem` also owns the orthographic ruler/measurement tool and the debug-draw flags, grid, and snap settings persisted in `EditorObjectSystemDefinition` (`EditorSettings`).

A raycast hit carries a priority as well as a distance, and priority decides first (`ColliderRaycastHitResult::isHigherPriorityThan`). Gizmo handle colliders carry priorities 1 to 3 (planar handles above the view-plane disk above the axis arrows) and every other collider is 0, so a handle under the cursor always wins over the object behind it. While a grab is held, `onMouseRayChanged` forwards only the move and leaves hover alone, so dragging a handle never lights up other objects. A selection filter change keeps the current selection when its object system is still in the new filter, which is what lets a stage object picked from the scene view keep its gizmo through the switch to the stage view. The gizmo is drawn by `renderGizmo` after all scene geometry: the translate arrows are solid meshes (a unit cylinder and cone scaled per frame) and they, like the line-drawn handles, get an occluded pass with the depth test off at reduced brightness and then a depth-tested pass at full brightness, so the part behind geometry reads dimmed.

`EditorSettings` also carries the viewport camera pose (`EditorCameraState`): the perspective fly pose, a pan and zoom per axis-aligned orthographic viewpoint, and which view was last active. `AppStage_Project` restores it on enter and writes it back once the camera has been still for 0.75 s, so a flight lands as one config change rather than one per frame, and flushes it on exit because unloading a project does not save. The whole block notifies under one property name, `editor_camera_state`, which has no property descriptor and so stays out of undo like the other editor settings. The outliner's fold state persists the same way under `outliner_open_state`: a map from row key (`ProjectOutlinerModel::getNodeStateKey`) to open flag holding only the rows that differ from their kind's default, which the outliner feeds to ImGui every frame so a tree rebuild cannot reset it. A component row renames in place: a second click on the selected row, released without dragging, or F2 swaps the label for a text field that commits on Enter or focus loss and cancels on Escape, through the same `setName` path as the property row. The tree's height persists beside it under `outliner_tree_height`: the splitter bar between the tree and the action strip drives a local height while held and writes it back once on release, and the panel clamps it so neither the tree nor the strip below can collapse.

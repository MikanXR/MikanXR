# Pipeline

The Pipeline panel: the chain that feeds the outputs from the current scene, drawn as a strip of boxes with a state dot and one value each, so a scene that renders wrong can be traced to the faulty link without walking the outliner. The object model it reads is in [objects.md](./objects.md), the compositor and outputs in [compositor.md](./compositor.md), and its automation commands in [automation.md](./automation.md).

---

## What it shows

The strip follows the current scene only (`SceneObjectSystem::getCurrentScene`), since that scene's compositor is the one that runs and publishes. With no current scene the six stages draw faded and Scene reads not configured.

- Row 1, joined by arrows: Volume, Stage, Scene, Camera, Compositor, Output.

- Row 2, the sub-stages of the open stage:
	- Volume, marker volume: Origin Marker
	- Volume, VR volume: SteamVR, Origin Marker, Mat Mount, Tracking Mounts
	- Stage: DMX, Light Groups
	- Scene: Environment Light, Depth Mesh, Lighting Preset
	- Camera: Tracking Mount, Video Source, Lens Calibration
	- Compositor: Texture Sources, Stencils, Shapes
	- Output: one box per output component, then Return Feed

- Row 3, the members of the open kind box. A kind box stands for a set (Tracking Mounts, Light Groups, Depth Mesh, Texture Sources, Stencils, Shapes) and shows its member count.

- A status line under the rows: the selected box's state and reason.

The rows keep a fixed height, so the panels docked below never move when the selection changes. A box's width comes from its name and a fixed value slot, never from its value, so a changing number never resizes it. A value that does not fit ends in an ellipsis and reads in full in the hover text.

The strip and the outliner share one selection. A click on a box highlights it and selects its component in the outliner, where its properties show. A box that stands for no component (a kind box, SteamVR, Return Feed) changes only the strip. A selection made anywhere else (the outliner, the scene list, a viewport pick) moves the highlight to the box that stands for that component, preferring one in a row already open, and a component the chain does not hold drops the highlight. The outliner raises its selection again after the strip's own click, and that echo is ignored while the highlighted box already stands for the component, so a click on Lens Calibration stays there rather than jumping to Video Source, which stands for the same video source.

A double-click on a box with a tool selects it and runs the tool, the same function call a property panel's button makes. The hover text says when a box has one:

- Compositor: `edit_compositor_graph`
- Lens Calibration: `calibrate_intrinsics` on the video source
- Origin Marker of a marker volume: `align_camera` on the scene's camera
- Mat Mount: `align_tracking_volume` on the VR volume
- Environment Light: `capture_scene_lighting` on the probe, or on the scene when there is no probe, since the capture creates one
- Depth Mesh: `capture_depth_mesh` on the scene
- A Depth Mesh member: `capture_depth_mesh` on the scene for a depth proxy, `build_occlusion_mesh` on the scene camera's video source for an occlusion mesh when that source is a recorded take, and none otherwise
- A CEF texture source: `show_texture_source_settings` on the source

## Membership

The compositor rows come from one walk of the active compositor graph per snapshot (`CompositorNodeGraph::gatherReferencedSceneObjects` on `CompositorComponent::getActiveCompositorNodeGraph`, the graph an open editor window holds ahead of the saved one). The walk is static, as [compositor.md](./compositor.md) describes, so a graph edit shows on the next frame.

- Stencils and Shapes list what `CompositorComponent::ownsSceneObject` accepts: the scene's own, plus the stage-level ones every scene on that stage shares. A member the graph pulls reads as usual, and one it does not reads inactive, "unused", with the reason "not used by the compositor graph". With no graph, whether a member is used is unknown and none reads unused.

- Texture Sources lists every texture source the graph reads, of any kind, including those read only by the shape graph of a shape the graph pulls:
	- a client source reads waiting until its client publishes, then its publish rate
	- a Spout source reads waiting until a sender is received, then receiving
	- a CEF source reads no browser, loading, or ready
	- a node naming a source the project no longer has adds one Missing Source member, not configured
	- a node set to the first client adds one First Client member, waiting until any client publishes for the compositor's camera

- Depth Mesh lists the model stencils a capture made, tagged by their editor-only `capture_kind`, that the compositor owns, so a proxy parented to the stage shows too. A member reads like a stencil (disabled, model failed to load, unused), and the same stencil also shows under Stencils. A stencil made before the tag existed is untagged and not listed.

## States

| State | Meaning | Dot |
| --- | --- | --- |
| `running` | A live signal says it is working | green |
| `configured` | Assigned and usable, with no live signal to say more | ring |
| `degraded` | Running, but part of what it produces is missing | amber |
| `notConfigured` | The chain needs it and it is missing or failed | red |
| `inactive` | Optional and absent, switched off, or idle | faded ring |

A box never shows running on a guess: a node with no real signal reads configured. An optional thing that is simply absent (no lighting preset, no environment light, no tracking mount on a static camera, a disabled output) is inactive, and a missing required link (no camera, no video source, no graph) is not configured. A texture source the graph uses with no client connected is degraded, "waiting for client", since that is the usual reason a composite is video only.

A state rolls up to the parent and never downstream. A stage takes the worst of itself and its sub-stages, and a kind box the worst of its members, but only a degraded or not configured child marks its parent. A failed video source turns Camera red and leaves a running Compositor green, so the leftmost marked box is the cause. The parent keeps its own value and carries the child's reason, with the child named.

## Code

Everything is in `src/Editor/AppStages/Project/`.

- `ProjectPipelineModel` is pure: no ImGui and no object systems. A `PipelineSnapshot` is a flat list of `PipelineNode`, each with a stable id, a kind (stage, box, kind box, member), an `ePipelineBox` that selects its status rule, the component a click selects, and a `PipelineFacts` of plain values. `computeOwnStatus` turns one node's facts into a state, a value and a reason, and `computeStatus` adds the roll-up. Text leaves as a localization key with numeric arguments, or as a component's own name. `PipelineSelection` holds the highlighted box and the two open rows by node id, so it survives a snapshot rebuild.

- `ProjectPipelineGather` is the only reader of the object systems. `gatherProjectPipeline` fills a snapshot once per frame, and `formatPipelineText` expands a `PipelineText` for display.

- `GuiPanel_Pipeline` draws the strip from a snapshot and owns the selection, which is session-only. It subscribes to `GuiPanel_ProjectOutliner::OnSelectedComponentChanged` and applies a changed component id at the top of its next frame, against that frame's snapshot. Its outliner selections and tool calls run as deferred GUI events, outside the frame.

Adding a box means an `ePipelineBox` value, its rule in `computePresentStatus`, its `pipeline.box*` label when it is not labeled by a component name, the node in the gather, and a case in `ProjectPipelineModelTests`. The rules are tested on hand-built snapshots, faulted one fact at a time, by the `project_pipeline_model` module of `MikanCmd -runTests`.

Node ids are untranslated and stable: a stage's name (`camera`), a sub-stage under it (`camera.video`, `volume.origin`), and a member with its component id appended (`compositor.stencils.1042`).

## Signals not read yet

These boxes report configured or not configured only, because nothing measures the live state today:

- Origin Marker: no state says whether a marker volume currently sees its origin marker
- Mat Mount, Tracking Mounts, the camera's Tracking Mount: a mount knows whether its device path resolves, but a VR device pose carries no validity flag
- Return Feed: whether the feed is streaming is read, but the destination count is collected per frame and not kept, so the box cannot say how many clients take it

A saved dock layout predates the panel, so the window floats until View > Reset Layout docks it across the top.

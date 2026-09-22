# Compositor

How MikanXR produces the mixed-reality composite: undistorted camera video layered with client-rendered frames and masked by stencils, evaluated by a node graph every video frame. Video capture itself is covered in [videosources.md](./videosources.md), the client protocol in [wire-protocol.md](./wire-protocol.md), the ECS object model in [objects.md](./objects.md), and the node graph machinery in [scripting.md](./scripting.md).

---

## What the compositor produces

The compositor is an ECS component: `CompositorComponent` in `src/Editor/ECS/Compositor/`, owned by `CompositorObjectSystem`. Each compositor binds one camera (`getCameraComponent()`, which supplies the video source) and one owner scene. Its output is an 8-bit sRGB RGBA texture, sized to the video frame, exposed via `CompositorComponent::getCompositedFrameTexture()`.

Compositing is driven by a `CompositorNodeGraph` (`src/Editor/NodeEditors/Graphs/CompositorNodeGraph.h`), a node graph asset referenced by path from the compositor's definition. There is no fixed layer stack; the "layer model" is whatever chain of draw nodes the graph author wires off the graph's `OnCompositeFrame` event node. A typical graph draws the camera video first, then one or more client layers on top.

`CompositorDefinition` (the persisted config) stores: `camera_id`, `owner_scene_id`, `compositor_graph_path` (a `NodeGraphAssetReference`), `spout_enable_output`, and `spout_output_name`. It round-trips through Configuru JSON (`writeToJSON`/`readFromJSON`) with the rest of the project config, and every field is also remotely readable/writable through the property system (`getPropertyValue`/`setPropertyValue`, see [wire-protocol.md](./wire-protocol.md)).

---

## Frame pacing and the frame event queue

`CompositorComponent::update()` runs during `ProjectManager::update()` each tick and does two things, in order:

- `tryCompositeOldestFrame()`: composites the frame at the front of `m_frameEventQueue`, but only once the queue is full (`VideoFrameDistortionView::getMaxFrameQueueSize()`, from the video source's frame queue size setting) and a new video frame has arrived. Filling the queue first maximizes the time clients have to render each frame; gating on new video frames locks the composite rate to the source frame rate. The node editor's pause (`setEditorEvaluationPaused`) skips graph evaluation here while frames keep popping, freezing the composite at the last output; closing the editor window always resumes.

- `tryEnqueueNewFrame()`: calls `VideoFrameDistortionView::readAndProcessVideoFrame()` to pull and undistort the next camera frame, builds a `MikanCameraNewFrameEvent` (frame index plus camera pose/intrinsics), pushes it on the queue, and publishes it to all clients via `CameraRequestHandler::publishCameraNewFrameEvent()`.

So each frame makes a round trip: the editor announces frame N with a camera pose, clients render their scene from that pose and submit textures, and the editor composites frame N several ticks later when the queue cycles around. `getPendingCompositedFrameIndex()` is the frame currently awaiting composite; `OnNewFrameComposited` fires after each composite. If the queue overflows, oldest frames are dropped with an error log.

The announcement takes the form the camera's frame sync mode asks for (`CameraComponent::getEffectiveFrameSyncMode`, see [wire-protocol.md](./wire-protocol.md)). A video-frame synced camera gets the frame event per video frame. A free-running camera gets a `MikanCameraNewPropertiesEvent` from `publishCameraPropertiesIfChanged` only when the camera state differs from the last one published, with a forced publish when the compositor starts and when the mode flips to free running. A client that joins between changes pulls the same state through `GetCameraProperties`, answered by `CameraRequestHandler` from the camera and the running compositors. `start()` and `stop()` also publish the compositor started and stopped events. The local frame event queue is filled either way, since it also paces the video texture the composite reads.

Video always comes through a `VideoFrameDistortionView` in `eVideoFrameProcessorMode::COMPOSITOR` mode with `eVideoDisplayMode::mode_undistored`. Compositing always uses the undistorted frame (see [videosources.md](./videosources.md)).

---

## How client frames arrive

Client-rendered frames never travel over the websocket. Clients allocate a shared GPU texture (Spout2-based, `MikanSharedTexture` library, see [modules.md](./modules.md)) and the editor reads it with a `SharedTextureReadAccessor` (`src/Editor/Interprocess/SharedTextureReader`). `ClientSourceManager` (`src/Editor/ECS/TextureSource/ClientSourceManager.h`) listens for `MikanServer` render-target allocate/update/release events and keeps a per-client `ClientTextureFrameQueue` keyed by client id and camera id, so a texture for frame N is still available when the compositor gets around to compositing frame N. The ring is sized at allocation to at least the camera's video source `video_frame_queue_size`, through the resolver `MainWindow` installs, because a ring shallower than the compositor's own queue can never hold the frame about to be composited. A lookup that finds no slot for the requested frame serves the newest slot instead, and `ClientTextureFrameQueue` counts those misses and logs the first and every hundredth, which is the only trace of a client running behind the compositor. A free-running client publishes with frame index -1, which the lookup never counts as a miss.

In the scene, a `ClientTextureSourceComponent` (a `TextureSourceComponent` subclass) names a client source; in the graph, `ColorTextureSourceNode` and `DepthTextureSourceNode` reference a texture source component and fetch the client texture for the pending frame index. `ColorTextureSourceNode` also carries `eTextureSourceColorType` (colorRGB/colorRGBA plus multiplicative shadowRGB/shadowRGBA variants) and an author-selected fallback (`eColorTextureFallbackMode`: transparent black, opaque black, opaque white, or auto by type) emitted when no client texture is available yet. The fallback is author-selected per node rather than inferred because the correct identity depends on the downstream layer's material and blend mode, which C++ cannot introspect (materials are GLSL shaders): an inverted-alpha "normal" layer needs opaque black so the inverted output alpha is zero, while a multiply layer needs white. A wrong choice paints over the video (a transparent-black fallback through an inverted-alpha shader composites as opaque black). `autoByType` preserves the legacy per-color-type behavior and is the default for graphs saved before the option existed. `CompositorNodeGraph::gatherAllReferencedClientSourceIDs()` walks the graph to report which client sources it consumes.

---

## The interactive browser window

`CEFTextureSourceComponent` runs a windowless CEF browser and uploads each `OnPaint` into a scene texture. Off-screen rendering is fixed at `CefBrowserHost::CreateBrowser` and a windowed browser never calls `OnPaint`, so making the page interactive cannot mean giving the browser a real window. Instead `CEFBrowserEditorWindow` (`src/Editor/ECS/TextureSource/`) opens as a satellite editor window, draws that same texture, and forwards input back into the browser through `CefBrowserHost::SendMouseClickEvent` and friends. The page stays live in the scene the whole time, and the main window keeps running whatever app stage it was on. The window is created by the component's `show_texture_source_settings` function, one per component, matched by component id through `App::getWindowsOfType`.

The definition's width and height are the scene texture resolution, so the window letterboxes the page rather than driving `WasResized`: dragging a preview window must not change what the compositor outputs. The `match_resolution_to_browser_window` function copies the window's page area, toolbar excluded, into the definition when that is what the author wants. Two rectangles come out of one letterbox calculation, because SDL reports the cursor top-down from the window's upper left while the GL viewport is bottom-up.

Two details of the texture path only surface once the page is more than a static poster. The color texture uploads without a PBO, because PBO streaming uploads the previous call's buffer while staging the current one: free for a video source that pushes a frame every tick, but a browser only paints on damage, so the last paint of a settled page would sit in the PBO unshown and the texture would hold the page before it. And a navigation can swap in a new render widget that did not inherit the old one's visibility and size, so `OnLoadingStateChange` re-asserts both and calls `Invalidate` once the load settles. Without that the address bar moves to the new page while the scene keeps showing the old one.

Two things about the browser only matter once the page is clickable. Widget popups (`<select>` dropdowns, autofill) paint into a separate `PET_POPUP` surface, so the component keeps the view buffer pristine and composites the popup over a scratch copy at upload time, which lets a popup disappear without waiting for the browser to repaint underneath it. And `OnBeforePopup` navigates the existing browser instead of allowing a new one, since a popup browser would be windowed and could never reach the scene texture.

`url_writeback` is an editor-only property, off by default: when set, `OnAddressChange` records where browsing ended up back into the definition's `url`. It is `setClientAPIHidden` because it governs editor interaction and has no meaning to a client application, so it stays out of `MikanCEFTextureSourceValues` and off the wire.

---

## The compositor node graph

`CompositorNodeGraph::compositeFrame(NodeEvaluator&)` is the per-frame entry point, called from `CompositorComponent::evaluateCompositorNodeGraph()`. It:

- resizes and (re)creates two framebuffers to the video dimensions: `m_compositingFrameBuffer` (RGBA16, a linear working buffer, since compositing in linear space keeps blend math correct and the extra depth avoids banding) and `m_outputFrameBuffer` (8-bit sRGB output);
- binds the working framebuffer, clears its depth buffer to far, disables the depth test, and runs `NodeEvaluator::evaluateFlowPinChain()` from the `OnCompositeFrame` `EventNode`;
- runs `encodeLinearFrameToSRGB()`, a fullscreen pass with the internal `Internal_PT_LinearToSRGB` material, into the output buffer.

Node types the graph can spawn (registered in the `CompositorNodeGraph` constructor): `EventNode`, `DrawLayerNode`, `DrawShapesNode`, `DepthMaskNode`, `WriteDepthNode`, `ApplyMaterialNode`, `ColorTextureSourceNode`, `DepthTextureSourceNode`, `VideoTextureNode`, `TextureNode`, `MaterialNode`, `StencilNode`, `StencilSelectNode`, `ShapeNode`, `ShapeSelectNode`, `ArrayNode`, `MousePosNode`, `TimeNode`. `VideoTextureNode` yields the camera texture (raw or distortion-corrected, `eVideoTextureSource`). Evaluation errors are collected as `NodeEvaluationError` values; `CompositorComponent::getLastNodeEvalErrors()` exposes them and the node editor window displays them.

`DrawLayerNode` is the core layer primitive. It takes a material (`GraphMaterialProperty`) and an optional stencil array, then draws a fullscreen quad (`getLayerMesh()`, or the v-flipped variant for sources with inverted texture coordinates) with that material. Its input pins are rebuilt dynamically from the bound material's shader uniforms: each float/vec2/vec3/vec4/texture uniform becomes a pin, with per-node default values persisted in `DrawLayerNodeConfig`. Per-node options: `eCompositorBlendMode` (off, normal `SRC_ALPHA/ONE_MINUS_SRC_ALPHA`, multiply `DST_COLOR/ZERO`), `eCompositorStencilMode` (none/inside/outside), vertical flip, and invert-when-camera-inside.

Depth in the working buffer is opt-in per node. The chain runs with the depth test off and `DrawLayerNode` never depth tests, so a layer chain composites exactly as before. The buffer's depth (a `DEPTH24_STENCIL8` renderbuffer that also carries the stencil) is cleared to far once per frame. `WriteDepthNode` takes a linear depth texture, the output of `DepthMaskNode` or `DepthTextureSourceNode`, and writes it into that depth buffer as hardware depth through the internal `Internal_PT_LinearToHardwareDepth` material, the inverse of the `Internal_P_LinearDepth` mapping, driven by the same aperture projection. It draws with color writes masked and the default LESS test against the cleared buffer, so two in sequence keep the nearer depth. A `DrawShapesNode` with `depth_test` on then sorts its shapes against whatever has been written, which is how a shape hides behind a stencil-shaped depth proxy while the client layers on top stay untouched. The node's blend mode and depth test govern every shape it draws, including shapes that render through their own shape graph ([scripting.md](./scripting.md)).

---

## Stencils

Stencils are scene objects (`QuadStencilComponent`, `BoxStencilComponent`, `ModelStencilComponent` with their object systems, see [objects.md](./objects.md)) that constrain where a layer draws. `StencilSelectNode` and `ShapeSelectNode` gather only what `CompositorComponent::ownsSceneObject` accepts: objects under the compositor's owner scene, plus stage-level objects on that scene's stage, which every scene there shares. Another scene's stencils never mask this composite, whatever the viewport shows. Before drawing its layer quad, `DrawLayerNode` rasterizes the relevant stencil geometry into the OpenGL stencil buffer with color and depth writes masked off (`evaluateQuadStencils`/`evaluateBoxStencils`/`evaluateModelStencils`), using the bound camera's aperture view-projection matrix so stencils are placed exactly as the physical camera sees them. The stencil mode then selects drawing inside or outside that mask.

Supporting machinery in `CompositorNodeGraph`: unit quad/box meshes for quad and box stencils, plus `getOrLoadStencilRenderModel()` which loads a model stencil's OBJ through `MikanModelResourceManager` and caches it per stencil id (`m_stencilMeshCache`); the cache entry is flushed when the stencil's model path changes. A parallel depth-model cache (`getOrLoadDepthRenderModel()`, `Internal_P_LinearDepth` material) supports depth masking. Quad stencils that are double-sided support the "magic portal" inversion: when the camera is behind every double-sided stencil quad, the mask logic inverts (`m_bInvertWhenCameraInside`).

---

## Materials and shaders

Layer materials are `MkMaterial` objects from the `MikanRenderer` OpenGL abstraction (see [modules.md](./modules.md)), managed by `MikanShaderCache` (`src/Editor/Renderer/MikanShaderCache.h`). Two populations exist:

- Built-in internal materials compiled from embedded GLSL in `GlShaderCache.cpp`, named by the `INTERNAL_MATERIAL_*` constants in `IMkShaderCache.h` (fullscreen texture blits, linear/sRGB conversion, linear depth, solid color, NV12 conversion, depth packing, etc.).

- User materials loaded from `*.compmat` files via `CompositorMaterialAssetReference` and `MikanShaderConfig`: a Configuru config naming the material, vertex/fragment shader file paths, a uniform-to-semantic map, and vertex attributes. These are what `MaterialNode`/`GraphMaterialProperty` feed into `DrawLayerNode`, and their uniforms become the node's dynamic pins.

A material file is either hand-written or compiled from a material node graph, which also writes the GLSL beside it; the format, the domains and vertex presets a material is checked against, and the graph editor are in [materials.md](./materials.md). `DrawLayerNode` and `ApplyMaterialNode` accept only compositor-domain materials, and rebuild their dynamic pins when the material editor saves a material they hold.

Draw state (blend, stencil, masks, viewport) is managed through the scoped `MkStateStack`/`MkScopedState` system rather than raw GL calls.

---

## Outputs

- **Editor display.** `CompositorOutputEditorWindow` (`src/Editor/NodeEditors/Windows/`) is a dedicated output window that draws the composited frame texture on a fullscreen quad over the bound scene. Because this window is meant to show what the shot looks like, the editor's authoring overlay is suppressed in it unless the `debug_render_in_compositor` editor setting ("Debug Render in Compositor", off by default) is on; the per-object "Render X" flags then still apply on top of it. Scene instances are cleared each frame either way, so toggling the overlay off cannot strand renderables belonging to deleted objects. `CompositorComponent::renderToViewportQuad()` is a helper that blits the composited texture into the current viewport with the fullscreen RGB material (no in-tree caller found, unverified use).

- **Spout.** When `spout_enable_output` is set, `startOutputStreaming()` creates an `ISharedTextureWriteAccessor` (`createSharedTextureWriteAccessor`, Spout2/`MikanSharedTexture`) named by `spout_output_name` (default `DEFAULT_SPOUT_OUTPUT_NAME`), RGBA32/OpenGL, and each composited frame is pushed via `writeColorFrameTexture()`. OBS or any Spout receiver can pick this up. Toggling the definition properties starts/stops the sender live.

- **Video recording.** No video-file recorder currently exists in the compositor path; `eSupportedCodec` (MP4V/MJPG/RGBA) in `CompositorConstants.h` is defined but unreferenced outside that file.

Which compositors run: `CompositorObjectSystem::refreshRunningCompositors` starts the current scene's display compositor (`SceneComponent::refreshActiveCompositors` hands it over) plus any compositor a graph editor window is bound to (`CompositorComponent::setEditorHeld`), and stops every other one. A stopped compositor holds no video distortion view and skips `update`, which is why its graph shows black source nodes and never evaluates. A compositor running only because an editor holds it streams its video source and evaluates the graph being edited (`tryCompositeOldestFrame` prefers the editor graph) but is denied output streaming (`setOutputStreamingAllowed`), so no Spout sender appears for it until it becomes the display compositor.

---

## Frame anatomy

Where a frame goes, from `App::exec` (`src/Editor/AppCore/App.cpp`) down:

1. `App::exec` loops `tick()` under a `FrameTimer(11)` (~90 fps cap). `App::tick` clamps `deltaSeconds` to 0.1 s, pumps the CEF message loop, polls SDL events, then calls `tickWindows()`.
2. `tickWindows` runs `update(deltaSeconds)` then `render()` for each `EditorWindow` (main window plus any node editor / compositor output windows).
3. `MainWindow::update`: `MikanServer::update()` first (websocket socket events and queued client requests, which is also where client render-target updates land in `ClientSourceManager`), then `LuaDebugServer::poll()`, SDL event handling, and `ProjectManager::update()`, which ticks the object systems, running `CompositorComponent::update()` (composite oldest frame, then enqueue/publish the next camera frame). Finally the current `AppStage` GUI and update.
4. `MainWindow::render`: renders each enabled `MikanViewport` for the app stage, then the UI, then presents. The compositor's GL work happens during the update phase (inside node graph evaluation), not here; render only displays already-composited textures.

Debug lines and points (`drawGrid`, `drawArrow`, gizmo handles, frustums) queue into the shared line renderer during the app stage render and flush once per viewport at the end of `renderStageViewport`. The `depthTest` flag on the current `MkState` at queue time routes each primitive to a depth-tested or an overlay batch, so the `MkScopedState` wrapping the draw call decides how it renders. The overlay batch draws first with the depth test off, which lets a caller queue the same geometry twice, dimmed overlay plus bright depth-tested, to render an occluded shape dimmed instead of hidden (the transform gizmo's treatment). Lines rasterize as screen-space quads through a geometry shader, with the per-vertex size selecting the width in pixels (core-profile wide lines are unreliable across drivers). Calling `IMkLineRenderer::render(true)` overrides the routing and draws everything without depth, which is what the calibration stages use over their video framebuffers. `MkScene::render` does not clear the depth buffer; a caller drawing a scene as an overlay on a video or composited layer clears depth explicitly first, as the calibration stages and `CompositorOutputEditorWindow` do.

# Depth Proxy Mesh Capture

How Mikan turns one captured video frame into a metric, camera-space proxy mesh and registers it as a model stencil (shadow catcher) - replacing hand-placed proxy geometry with a single capture from the camera's point of view.

The model measurements that justify this design (silhouette alignment, FOV sensitivity, normals quality) live in [scene-lighting.md](./scene-lighting.md); this doc covers the pipeline itself.

---

## The model

One checkpoint: **MoGe-2 ViT-L with the normal head** (`Ruicheng/moge-2-vitl-normal`), run in-process through ONNX Runtime. MIT licensed, code and weights. Unlike Marigold there is no local export step - the authors publish an official ONNX export (`Ruicheng/moge-2-vitl-normal-onnx`, opset 14, single 1.3GB `model.onnx`), downloaded on first use into `models/moge2/` (see [commands.md](./commands.md) for where that resolves to).

The graph takes plain RGB in [0,1] at **native resolution** (ImageNet normalization is baked in) plus a scalar `num_tokens` (1200..3600), and returns:

| Output | Meaning |
|---|---|
| `points` | AFFINE point map - camera space up to an unknown Z shift and global scale |
| `normal` | unit normals, directly predicted (not derived from the depth) |
| `mask` | validity probability, threshold at 0.5 |
| `metric_scale` | scalar taking the shifted geometry to metres |

The graph runs under both the CPU and DirectML execution providers with **identical output** (0.000% depth difference), DirectML roughly 7x faster (measured at 1.24s against 8.9s steady-state, 832x480 at 3600 tokens). The ONNX pipeline matches the PyTorch reference to 0.04% depth / 0.03 degrees of normal error.

`tools/moge2_onnx_pipeline.py` is the **executable specification for the C++ port** (`MoGeInference`): preprocessing, the forward pass, and the metric recovery below map 1:1. The C++ implementation was validated against it per-vertex to 0.001% at p99.

## Metric recovery needs the calibrated FOV

The point map is affine; recovering metric camera space takes three steps (`MoGeInference::run`):

1. **Focal from the calibrated horizontal FOV**, closed form. Mikan always has this (`MikanMonoIntrinsics::hfov` describes the undistorted image, which is also what the model is fed), so the reference implementation's focal-*estimation* path is deliberately not ported. This is not a nicety: metric scale rides directly on the assumed FOV - a 45 degree guess against a ~56 degree truth shifts depth 22-36%.
2. **Z shift** via a 1-D least squares `min |focal * xy/(z+shift) - uv|` over the point map nearest-downsampled to 64x64. One trap, found by unit test: every denominator `z + shift` is a depth and must be positive at the solution, but the raw affine `z` can be negative - starting the solve at zero then puts a pole between the start and the answer and the iteration stalls against it. `solveDepthShift` starts just past the largest pole instead. (The reference starts at zero unconditionally; the two agree wherever the reference converges.)
3. **Scale** by `metric_scale`, then the point map is rebuilt from depth + intrinsics so points reproject exactly (`force_projection` in the reference).

Everything `MoGeInference::Result` exposes is converted to **Mikan camera space** (+X right, +Y up, +Z toward the viewer; visible geometry at negative Z) at the API boundary - the raw model output is OpenCV convention (+Y down, +Z forward) and the Y/Z negation lives in exactly one place. Invalid pixels carry +inf depth and a ZERO normal, which downstream unit-length checks reject for free.

## Mesh generation

`DepthMeshGenerator` builds a regular grid over the depth map (default stride 4) and emits two triangles per cell, **cutting the mesh at depth discontinuities**: a triangle is dropped when any of its corners' pairwise depth ratio exceeds `discontinuityRatio` (default 1.15). Connecting a silhouette to the background would create stretched skirt triangles that catch shadows in mid-air; measured on the reference plates, the model's depth edges land within ~1px of the image silhouettes and commit to near-or-far rather than averaging, so ratio culling is sufficient and the mesh cleanly separates at object boundaries.

Geometry beyond `maxDepth` (default 20m) is dropped - distant background adds triangles but cannot receive a meaningful contact shadow.

The OBJ written per capture carries positions (camera-space metres), per-vertex normals, and video-frame UVs. **Triangle winding is counter-clockwise around the toward-camera vertex normals** (the standard OBJ front-face convention), and this is load-bearing: the Unreal client copies triangle indices verbatim through a handedness-flipping coordinate conversion, so a mesh wound the other way shows every surface as a back face in-engine - the first version of the generator did exactly that. A unit test now locks winding to the vertex normals.

The captured frame is saved as `<name>.png` next to the OBJ and referenced by a sibling `.mtl` (`map_Kd`), so the plate projects back onto the proxy through the frame UVs:

- **In the editor** this is free - the OBJ importer already loads `map_Kd` textures.

- **In Unreal** the stencil actor loads the sibling `.png` of `model_path` (same machine as the editor, which frame delivery via shared texture memory already assumes) and applies it to the placement mesh through the `TexturedPlacementMaterial` parent material - assignable per-actor or as a default on `AMikanClient` (`ModelStencilTexturedPlacementMaterial`), and it needs a Texture parameter named **`BaseTexture`**. When unset or no texture exists, the placement mesh keeps the plain color material. The actor also flips the V coordinate when building mesh sections - Mikan ships OBJ/GL-convention UVs (V up) and Unreal samples V down; the stock capture materials are untextured, so only the textured placement preview observes this.

## The capture tool

`AppStage_DepthMeshCapture`, reached from a camera's **Capture Depth Mesh** component function (registered through `IFunctionInterface`, so also reachable from Lua and remotely). Same shape as the scene-lighting capture stage:

pick camera -> live video while framing -> **Capture** runs the model -> the panel shows mesh statistics and a depth overlay (red = near, blue = far) drawn over the frame so silhouette alignment can be judged -> **Create Stencil** writes the mesh and registers it.

### Capture runs on a worker thread

The capture is not run on the UI thread, because a progress readout and a cancel button both require the window to keep painting while the model works. The stage owns one worker thread for its whole lifetime, and that thread owns the `MoGeInference`: `OnnxSession` requires a session to be created, run, and destroyed on a single thread, so a thread-per-capture would mean reloading 1.3GB every time.

Everything the worker needs is gathered on the UI thread before dispatch - the frame (cloned, since the video pipeline reuses its buffer), the calibrated FOV, and the ArUco corner detection, which reads the distortion view and so cannot move off the UI thread. The worker then does model load, inference, scale correction, and meshing over plain data, and `update()` polls for the result.

**Progress is per-step, not smooth, and deliberately so.** ONNX Runtime reports nothing from inside a `Run`, and the two long steps - model load and inference - are exactly the opaque ones. Faking a sliding bar would be inventing information, so the bar advances at step boundaries (weighted toward those two steps) and an elapsed-seconds readout ticks every frame to show the work is alive. The vendored ImGui clamps negative fractions, so its indeterminate-bar trick is not available either.

**Cancel is immediate rather than between steps.** `OnnxSession::requestTerminate()` sets ONNX Runtime's terminate flag, which aborts a `Run` already in flight; that is the one method on `OnnxSession` safe to call from another thread, and it is why cancel works during the step the operator is most likely waiting out (~10s on the CPU fallback). A terminated run throws, so `run()`/`runOutputs()` catch and return empty - callers already treat empty as failure - and the cancel flag is what distinguishes a cancelled run from a broken one. The flag is sticky, so `run()` clears it on entry. Cancelling returns to framing rather than closing the tool, keeping the loaded model and the framing.

Failure modes are loud and specific, in the order they are checked: model not installed (Capture offers to download it rather than failing), no video frame, no calibrated intrinsics, and - only at Create Stencil time - no tracked camera pose and no loaded project.

What Create Stencil does:

- Writes `<project>/models/DepthProxy_<timestamp>.obj`. The name is unique per capture because the model resource cache keys on the file path - regenerating under a reused path would serve the stale mesh.

- Creates a `ModelStencilComponent` through `ModelStencilSystem::addNewObjectByTypedDefinition` with the **absolute** OBJ path (the importer loads the stored path verbatim; a relative one silently fails to resolve), **parented under the active scene** so it lands in that scene's subtree in the project outliner. With no active scene it falls back to the camera's stage, which files it in the outliner's unparented tray.

- Sets the stencil's world transform to the capturing camera's `getStageSpaceAperturePose` (back-solved into a stage-relative transform by `setWorldTransform`). The vertices are camera-local, so the stencil rides the camera pose and the OBJ stays reusable.

## Metric scale calibration

The model's scale is its weakest output. The `metric_scale` scalar is predicted from image *appearance* (the ViT's class token) - the FOV conditioning above only resolves the Z shift - so a camera whose image "doesn't look like" its true FOV (long lens, crop) can be off by an **integer factor** while the shape stays excellent. Observed in practice: a proxy ~5x too large.

The correction is measured against an ArUco marker of known size:

- During each capture, every marker defined in the project's marker system is searched for in the captured frame (`CalibrationPatternFinder_Aruco`, the same detection + solvePnP path the camera-alignment tools use). If one is found, its four subpixel corners give ground-truth camera-space depths; each is compared against the median model depth in a 5x5 window at that pixel, and the **median ratio of at least 3 valid corners** is the correction factor.

- The **corner spread** (worst per-corner disagreement with the factor) is surfaced in the panel as a consistency check - a marker at a grazing angle or on a depth edge produces a high spread and an amber warning rather than a silently bad factor.

- The factor is applied to the geometry *before* mesh generation, so the stats, the depth overlay, and the OBJ are all corrected.

- **Creating the stencil persists the factor on the camera** (`depth_mesh_scale_correction` in the camera definition). Marker-less captures from that camera reuse it automatically - the scale error is a property of the camera/lens, not the frame - so the marker only needs to be in view when (re)calibrating, and production shots stay clean. A capture with neither a marker nor a stored factor shows an amber "No scale calibration" warning.

`MoGeInference::run` logs the FOV, shift, and metric scale it used (`fov_x=... shift=... metric_scale=...` in the log); a wrong FOV reaching the model is the first suspect when a proxy comes out the wrong size, and that line settles it.

Nothing further is needed for connected clients: creating the stencil fires the component-id-list property event, clients refetch the stencil list, and the render geometry ships over the wire via `GetModelStencilRenderGeometry` - the Unreal actor's shadow mesh component picks the proxy up automatically.

## Headless use

```
MikanCmd -depthMesh -image=<path> -fov=<degrees> [-obj=<path>] [-mogeModels=<dir>] [-stride=<n>] [-maxDepth=<m>] [-cpu]
```

Prints the mesh statistics and writes the OBJ. This is also the validation path against `tools/moge2_onnx_pipeline.py`.

Unit tests (`MikanCmd -runTests`, `depth_mesh_generator` module) cover the shift solver (including the negative-z pole case), discontinuity culling, mask/max-depth rejection, and OBJ consistency.

## Known limits

- **Thin structures and transparency** reconstruct poorly (wheat stalks fuzz, frosted glass leans toward the surface behind it) - both visible in the eval renders under `build/moge2_eval/`.

- **Single viewpoint**: surfaces the camera cannot see do not exist in the proxy. Disocclusions are holes, not hallucinated geometry - correct for a shadow catcher, insufficient for occluding a character walking behind real furniture (occlusion-grade edges are a separate problem).

- **Metric scale without calibration**: the model's raw scale guess can be off by an integer factor on unusual lenses (see "Metric scale calibration" above). With a marker calibration stored on the camera the remaining error is the marker measurement itself, well under a percent. A future zero-hardware alternative is fitting against the tracked floor plane.

Work deferred out of this feature is tracked in [plan.md](../plan.md), not here.

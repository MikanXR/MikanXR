# Occlusion mesh

The occlusion mesh is a static room mesh fused from the LiDAR depth a phone records beside a take, registered as a model stencil so a composited character is hidden by the real furniture it walks behind. This doc covers the fusion pipeline, the pose refinement that runs inside it, the editor stage and the headless command that drive it, the files they write, and the spaces everything lives in. The decision behind the design and the measurements that led to it are in [the proposal](../proposals/occlusion-mesh.md); the take format it consumes is in [videosources.md](./videosources.md) ("Pose tracks" and "Depth tracks"); the single-frame depth proxy it supersedes for occlusion is in [depth-proxy-mesh.md](./depth-proxy-mesh.md).

## Input

A take recorded by MikanARStreamer on a LiDAR phone: `<stem>.mp4`, `<stem>.pose.json` (version 2, declaring `depth_width`, `depth_height`, `depth_unit_mm`), `<stem>.depth.bin`, and `<stem>.conf.bin`, all in the project's `movies` folder. Record `i` of the two binaries belongs to frame `i` of the pose track's file order. `PoseTrack` sorts frames by time on load and keeps that position as `PoseTrackFrame::fileIndex`, which is the only key the fusion reads records by. The sidecars derive from the media stem (`DepthTrackReader::depthSidecarPathForMedia`), never from the pose track path, so a source repointed at a refined track still finds them.

The take must be aligned: `FileVideoSourceDefinition::hasPoseOffset()` must hold, since the mesh is placed in the stage through that offset and an unaligned take has nowhere to go.

## Pipeline (`src/Editor/Reconstruction`)

`OcclusionMeshBuilder::build` is pure data in and data out: a `PoseTrack`, an open `DepthTrackReader`, settings, an optional progress and cancel observer, and a result. Every other entry point calls it the same way.

1. Keyframes: every `keyframeStride`th frame in time order (default 3).
2. Bounds: every fourth keyframe's confidence-2 pixels, back-projected and transformed to world, trimmed to the 0.5 and 99.5 percentile per axis, padded by `boundsMargin` (0.3 m).
3. Volume: `TsdfVolume`, a dense truncated signed distance grid over that box. Default 1.5 cm voxels with a 6 cm truncation (the room preset); 8 mm and 3.2 cm for a tabletop. The voxel grows by ten percent until the grid fits `maxVoxels` (60 million).
4. Integration, per keyframe: confidence 0, 1, 2 becomes weight 0, 0.5, 1; pixels whose depth jumps by more than 8 percent against a 4-neighbour are culled (flying pixels at silhouettes); every voxel in the frame's frustum takes the truncated signed distance into its running weighted average. With registration on, the frame is first aligned to the model (below).
5. Extraction: unobserved voxels take the sign of their nearest observed voxel (without this every surface grows a back shell two voxels behind it), then marching cubes under the observed region dilated by one voxel. Vertices on shared edges are welded.
6. Cleanup (`MeshCleanup`): degenerate and duplicate triangles, connected components under `minComponentTriangles` (500 for a room, 1500 for a tabletop), unreferenced vertices, area-weighted vertex normals.

Threads: integration splits the frustum's voxel box over z slabs, the ray cast over rows, through `parallelFor`; marching cubes and cleanup are single-threaded. Cancellation is cooperative between frames and phases.

## Pose refinement

ARKit's poses drift along a sweep: on a 57 s room loop the held-out median error of the fused mesh was 1.07 cm against 0.39 cm for a short sweep of the same room, while the loop's own seam closed to about 3 cm. Frame-to-model registration removes the drift along the path and leaves the seam where ARKit put it.

With `bFrameToModel` on, each keyframe past `warmupFrames` (3) is registered before it is fused: `TsdfVolume::raycast` renders the volume so far from the frame's predicted pose into points and normals, and `FrameToModelRegistration::registerFrame` runs projective point-to-plane ICP against that map. The prediction is the recorded pose carried forward by the previous frame's correction, so the solve only has to find the new drift. Gates and terms, all in `RegistrationConfig`:

- correspondence distance 10 cm, tightening to a third over the 10 iterations, normal angle 30 degrees, Huber 2 cm
- the data term weighted by its own noise (5 mm) so the prior reads in the same units
- a prior on the deviation from the prediction, 3 mm and 0.2 degrees, which holds the directions a lone wall or floor leaves free
- at least 2000 correspondences, and a per-step correction under 15 cm and 5 degrees, else the frame is rejected

A rejected frame is skipped, never fused at a pose the data disagree with. The 15 cm bound is wide enough to absorb an ARKit tracking jump in one step.

Three details carry the method, each found on synthetic planes and confirmed on a real take. A trilinear sample that touches an unobserved voxel reads as unobserved (`TsdfVolume::sample` returns NaN), or the rim of every observed patch reads as a wall the solve slides toward. Model normals use a two-voxel gradient stencil, since one voxel gives them 2 to 3 percent noise that makes an unobservable direction look stiff. And the prior is what keeps a floor-only frame from drifting along the floor.

The refined pose of every frame of the take, fused or not, is the recorded pose composed with the correction of the nearest fused frame by file index. The correction is world-side, so it never crosses a camera-axis conversion.

## Spaces

| step | in | out | where |
|---|---|---|---|
| pose track load | row-major camera-to-world, ARKit world, ARKit camera axes (+Y up, -Z forward) | glm column-major, same spaces | `PoseTrack::readFrame` |
| a frame enters the fusion | ARKit camera axes | OpenCV camera axes (+Y down, +Z forward), ARKit world | `OcclusionMeshBuilder::toOpenCvCameraToWorld`, a half turn about the camera's X, once per frame |
| integrate, ray cast, register | OpenCV camera, ARKit world | ARKit world | no flips |
| refined pose | world-side correction | `correction * recordedCameraToWorld` | `OcclusionMeshBuilder::build` |
| mesh | ARKit world | ARKit world, in the `.glb` | none |
| stencil | ARKit world | editor world | `setWorldTransform(stageWorld * poseOffset)`, the composition `FileVideoSourceComponent::publishPoseForFrame` applies to a camera, lifted to the mesh |

Depth intrinsics are the frame's colour intrinsics scaled by `depth_width / image_width` and `depth_height / image_height` (`scaleIntrinsics`), and rays go through integer pixel positions. ARKit world and the editor's world share handedness and up axis, so the mesh needs no axis conversion anywhere. Triangles are wound counter-clockwise about their outward normal, which the Unreal client depends on.

## The stage

`AppStage_OcclusionMeshBuild` (`src/Editor/AppStages/OcclusionMeshBuild`) is entered from a file video source's "Build Occlusion Mesh" function (`build_occlusion_mesh`). `tryEnterBuild` refuses, with a message box, a take without a pose track, without the depth header or the sidecars, an unaligned take, a source no camera on a stage uses, or no project. The stage then runs the depth proxy capture's worker pattern: the UI thread hands a copy of the pose track, the sidecar paths, and the settings to a worker thread under a mutex, the worker opens the reader and calls the builder, and `update` polls the phase and frame counts into the panel each frame. Cancel is a flag the builder checks between frames.

The panel offers the grid preset, the keyframe stride, the registration toggle, and whether the source should play the take with the refined poses. After a build it shows the triangle count, grid, frames fused, and the registration summary, and draws the mesh as a point sample where the stencil will land while the viewport camera orbits it.

Create Stencil writes `<project>/models/OcclusionMesh_<timestamp>.glb` (`GlbMeshWriter`), creates a `ModelStencilComponent` under the active scene with that model path and the world transform above, refreshes the asset catalog, and when registration ran writes `<stem>.refined.pose.json` beside the take (version 2, same session id, image size, depth header, times, and intrinsics, frames in file order) and, with the toggle on, points the source's pose track path at it. A take without an active scene cannot be applied: stencils accept only a scene as their parent.

Drive scripts reach it with `function invoke FileVideoSourceSystem <componentId> build_occlusion_mesh`, then `stage get_state`, `get_take_info`, `set_option <voxel_preset|keyframe_stride|frame_to_model|replace_pose_track> <value>`, `build`, `get_progress`, `get_summary`, `apply`, `cancel`, and `return` ([automation.md](./automation.md)).

## The headless command

`MikanCmd.exe -occlusionMesh -take=<take.mp4> [-glb=<path>] [-refinedTrack=<path>] [-voxel=<metres>] [-stride=<n>] [-noRegistration]` runs the same builder on a take outside the editor and writes the mesh and the refined track. It is the parity check against the Python harness in the experiments repo, which fuses the same take with `recon/fusion.py` and scores both.

## Edge refinement

The fused mesh is a coarse prior, and at composite time its silhouette is decided in image space: `DepthEdgeRefineNode` ([compositor.md](./compositor.md)) takes the depth mask rendered from the stencil and the video frame and moves the mask's edges to the picture's within a band, by classifying each pixel against the colour of the near and the far side of the depth edge, keeping the mask's own depth values. The same filter exists as `tools/edge_refine_eval.py` in the experiments repo, which renders a fused mesh at a take's held-out frames, refines it against the video, and scores both against the held-out LiDAR's edges, which is how the node's defaults were chosen: a 16 px band and a 0.2 colour threshold took the room loop's held-out edge distance from 9.44 to 9.00 px. The LiDAR's edges carry the sensor's own blur, so hand-marked frames are the finer measure and are tracked in `plan.md`.

## glTF models

The mesh is written as a glTF binary rather than OBJ because a room is about a million triangles, which a text parser loads slowly and a binary buffer loads as a copy. `GltfModelImporter` (`src/Editor/Asset/Import`) reads `.glb` and `.gltf` through cgltf (`thirdparty/cgltf`): every triangle primitive in the scene becomes one triangulated mesh plus one wireframe mesh, node transforms are applied, and the base colour texture is loaded from the embedded image or its URI. glTF stores texture V down and the renderer samples V up, so the importer flips V at one point and `GlbMeshWriter` flips it back on the way out. `ModelAssetReference` lists all three extensions, and the model resource manager matches extensions case-insensitively.

## Tests

`MikanCmd -runTests` carries four modules for this: `tsdf_volume` (marching cubes on an analytic sphere with its winding, the gradient cull, a plane and a sphere fused from several poses, the absent back shell, the rim rule of `sample`, ray cast accuracy, island removal), `frame_registration` (the small-motion helpers, a displaced frame recovered on a corner scene, a floor-only frame held by the prior, rejection, the sidecar reader), `occlusion_mesh` (the file index through the sort, the camera axis conversion, a synthetic take with a scrambled file order fused end to end with drifted poses that the refinement beats), and `glb_mesh_writer` (a round trip through cgltf).

## Known limits

- Thin structures and anything that moves are absent. An open laptop lid on a desk is the measured example: the LiDAR sees it at high confidence from the front, and the views past its edge carve it away at any truncation.
- The end-to-end seam of a long loop stays where ARKit's relocalisation put it, about 3 cm on the measured take. A loop constraint on top is an open question in the proposal.
- Fusion is CPU-only. A compute shader is the obvious later step and is tracked in `plan.md`.

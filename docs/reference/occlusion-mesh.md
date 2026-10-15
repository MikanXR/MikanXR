# Occlusion mesh

The occlusion mesh is a static room mesh fused from the LiDAR depth a phone records beside a take, registered as a model stencil so a composited character is hidden by the real furniture it walks behind. This doc covers the fusion pipeline, the pose refinement that runs inside it, the editor stage and the headless command that drive it, the files they write, and the spaces everything lives in. The decision behind the design and the measurements that led to it are in [the proposal](../proposals/occlusion-mesh.md); the take format it consumes is in [videosources.md](./videosources.md) ("Pose tracks" and "Depth tracks"); the single-frame depth proxy it supersedes for occlusion is in [depth-proxy-mesh.md](./depth-proxy-mesh.md).

## Input

A `trackcamera` capture recorded by MikanARStreamer on a LiDAR phone, in the project's `captures` folder: its `movie_pose` member (version 2, declaring `depth_width`, `depth_height`, `depth_unit_mm`) and its `depth` and `confidence` members ([videosources.md](./videosources.md), "Capture archives"). Record `i` of the two members belongs to frame `i` of the pose track's file order. `PoseTrack` sorts frames by time on load and keeps that position as `PoseTrackFrame::fileIndex`, which is the only key the fusion reads records by. The depth members are read from the capture whatever track the source plays, so a source repointed at a refined track still finds them.

The take must be aligned: `FileVideoSourceDefinition::hasPoseOffset()` must hold, since the mesh is placed in the stage through that offset and an unaligned take has nowhere to go.

## Pipeline (`src/Editor/Reconstruction`)

`OcclusionMeshBuilder::build` is pure data in and data out: a `PoseTrack`, an open `DepthTrackReader`, settings, an optional progress and cancel observer, and a result. Every other entry point calls it the same way.

1. Keyframes: every `keyframeStride`th frame in time order (default 3).
2. Bounds: every fourth keyframe's confidence-2 pixels, back-projected and transformed to world, trimmed to the 0.5 and 99.5 percentile per axis, padded by `boundsMargin` (0.3 m).
3. Volume: `TsdfVolume`, a dense truncated signed distance grid over that box. Default 1.5 cm voxels with a 6 cm truncation (the room preset); 8 mm and 3.2 cm for a tabletop. The voxel grows by ten percent until the grid fits `maxVoxels` (60 million).
4. Integration, per keyframe: confidence 0, 1, 2 becomes weight 0, 0.5, 1; pixels whose depth jumps by more than 8 percent against a 4-neighbour are culled (flying pixels at silhouettes); every voxel in the frame's frustum takes the truncated signed distance into its running weighted average. With registration on, the frame is first aligned to the model (below).
5. Trim: with `cameraPathRadius` set (default 2 m, 0 for off), `CameraPathClip` masks out every voxel farther than the radius from every fused camera position, so a sweep of a corner does not carry the rest of the room into the mesh. The grid itself is never trimmed, so the registration above still had the far walls to anchor on. The test runs per block of four voxels against the block's centre, so the cut is quantised to about 6 cm at the room preset.
6. Extraction: unobserved voxels take the sign of their nearest observed voxel (without this every surface grows a back shell two voxels behind it), then marching cubes under the observed region dilated by one voxel and under the trim mask. A masked voxel is left out of the region, so no cube touching it is emitted and the surface ends there with a bare edge. Vertices on shared edges are welded.
7. Cleanup (`MeshCleanup`): degenerate and duplicate triangles, connected components under `minComponentTriangles` (500 for a room, 1500 for a tabletop), unreferenced vertices, area-weighted vertex normals.
8. Texturing (`MeshTexturing`), when the caller hands `build` a `MovieDecoder` open on the take's movie: a rough colour so the mesh reads, not photogrammetry. `collectTextureViews` treats every fused keyframe (thinned to 256 on a long take) as a candidate, scores each by the triangles it sees from its refined pose and its depth record alone, and picks up to `textureViews` (16) greedily by how many triangles each adds to the coverage, so the costly colour decode is paid only for the chosen frames. Each chosen frame is decoded by presentation time (a frame the decoder cannot land on within half a frame period is skipped) and shrunk to a `textureTileWidth` (640 px) tile. The tiles form one atlas with a flat grey tile last. Each triangle takes the view that sees it best: every vertex projects onto a measured depth pixel no more than 5 cm in front of it (the farthest of the 3x3 around the pixel, since at the LiDAR's resolution a vertex on a concave edge rounds onto the nearer wall beside it), the face normal turns toward the camera by a cosine of at least 0.2, and among those the most frontal per metre wins. A triangle no view clears that floor for is tried again at a cosine of 0.05, since a stretched glancing view reads better than grey. Its vertices' texcoords are their projections into that tile, and a vertex shared by triangles of different views is split, so the textured mesh has more vertices than the fused one. Triangles no view sees land on the grey tile. Seams between views are visible and intended. On the measured desk take about a quarter of the triangles stay grey, and most of those face away from every camera position: the closed far sides the extraction gives an object whose back was never observed, which no frame can colour.

Threads: integration splits the frustum's voxel box over z slabs, the ray cast over rows, the trim mask over cell slabs, and the view choice over triangles, through `parallelFor`; marching cubes, cleanup, and the vertex split are single-threaded. Cancellation is cooperative between frames, views, and phases.

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
| texturing | ARKit world | a view's OpenCV camera, then its tile's pixels | the view's refined pose through `toOpenCvCameraToWorld`, then the colour intrinsics scaled to the tile |
| stencil | ARKit world | editor world | `setWorldTransform(stageWorld * poseOffset)`, the composition `FileVideoSourceComponent::publishPoseForFrame` applies to a camera, lifted to the mesh |

Depth intrinsics are the frame's colour intrinsics scaled by `depth_width / image_width` and `depth_height / image_height` (`scaleIntrinsics`), and rays go through integer pixel positions. ARKit world and the editor's world share handedness and up axis, so the mesh needs no axis conversion anywhere. Triangles are wound counter-clockwise about their outward normal, which the Unreal client depends on. Texcoords are V-up in `TriangleMesh`, as `GlbMeshWriter` takes them, and the atlas image's rows run down, so the texturing flips V once when it writes a texcoord.

## The stage

`AppStage_OcclusionMeshBuild` (`src/Editor/AppStages/OcclusionMeshBuild`) is entered from a file video source's "Build Occlusion Mesh" function (`build_occlusion_mesh`). `tryEnterBuild` refuses, with a message box, media that is not a `trackcamera` capture, a pose track override that does not load, a capture without depth or movie members, an unaligned take, a source no camera on a stage uses, or no project. The track is the override when the source has one from the capture's own session and the capture's `movie_pose` otherwise: an override recorded in another session indexes nothing in this capture's depth members, so it is skipped with a log line, as playback skips it. The stage then runs the depth proxy capture's worker pattern: the UI thread hands a copy of the pose track, the capture path with its depth and movie members, and the settings to a worker thread under a mutex, the worker opens the reader and a `MovieDecoder` on the movie member (through `Capture::makeSubfileUrl`, with no GL in sight) and calls the builder, and `update` polls the phase and frame counts into the panel each frame. A movie that will not open costs the texture, not the mesh. Cancel is a flag the builder checks between frames and views.

The panel offers the grid preset, the keyframe stride, the trim radius, the registration toggle, whether the source should play the take with the refined poses, and the texture toggle. After a build it shows the triangle count, grid, frames fused, voxels trimmed, the registration summary, and the views used with the atlas size, and draws the mesh as a point sample where the stencil will land while the viewport camera orbits it.

Create Stencil writes `<project>/models/OcclusionMesh_<timestamp>.glb` (`GlbMeshWriter`) and, when the mesh was textured, `OcclusionMesh_<timestamp>.png` beside it first, referenced from the glb's material as the base colour texture. The png carries the model's own stem because that is where the Unreal client's placement texture looks. It then creates a `ModelStencilComponent` under the active scene with that model path and the world transform above, refreshes the asset catalog, and when registration ran writes `captures/<stem>.refined.pose.json` beside the capture (version 2, same session id, image size, depth header, times, and intrinsics, frames in file order) and, with the toggle on, points the source's pose track path at it. The capture itself is never rewritten. A take without an active scene cannot be applied: stencils accept only a scene as their parent.

Drive scripts reach it with `function invoke FileVideoSourceSystem <componentId> build_occlusion_mesh`, then `stage get_state`, `get_take_info`, `set_option <voxel_preset|keyframe_stride|frame_to_model|replace_pose_track|path_radius|texture> <value>`, `build`, `get_progress`, `get_summary`, `apply`, `cancel`, and `return` ([automation.md](./automation.md)).

## The headless command

`MikanCmd.exe -occlusionMesh -take=<take.trackcamera> [-glb=<path>] [-refinedTrack=<path>] [-voxel=<metres>] [-stride=<n>] [-noRegistration] [-pathRadius=<metres>] [-noTexture] [-textureViews=<n>]` runs the same builder on a capture outside the editor, reading the capture's own track, depth members, and movie, and writes the mesh, its atlas as `<glb stem>.png` beside the glb, and the refined track, by default beside the capture. It is the parity check against the Python harness in the experiments repo, which fuses the same take with `recon/fusion.py` and scores both.

## Edge refinement

The fused mesh is a coarse prior, and at composite time its silhouette is decided in image space: `DepthEdgeRefineNode` ([compositor.md](./compositor.md)) takes the depth mask rendered from the stencil and the video frame and moves the mask's edges to the picture's within a band, by classifying each pixel against the colour of the near and the far side of the depth edge, keeping the mask's own depth values. The same filter exists as `tools/edge_refine_eval.py` in the experiments repo, which renders a fused mesh at a take's held-out frames, refines it against the video, and scores both against the held-out LiDAR's edges, which is how the node's defaults were chosen: a 16 px band and a 0.2 colour threshold took the room loop's held-out edge distance from 9.44 to 9.00 px. The LiDAR's edges carry the sensor's own blur, so hand-marked frames are the finer measure and are tracked in `plan.md`.

## glTF models

The mesh is written as a glTF binary rather than OBJ because a room is about a million triangles, which a text parser loads slowly and a binary buffer loads as a copy. `GltfModelImporter` (`src/Editor/Asset/Import`) reads `.glb` and `.gltf` through cgltf (`thirdparty/cgltf`): every triangle primitive in the scene becomes one triangulated mesh plus one wireframe mesh, node transforms are applied, and the base colour texture is loaded from the embedded image or its URI. glTF stores texture V down and the renderer samples V up, so the importer flips V at one point and `GlbMeshWriter` flips it back on the way out. `ModelAssetReference` lists all three extensions, and the model resource manager matches extensions case-insensitively.

## Tests

`MikanCmd -runTests` carries five modules for this: `tsdf_volume` (marching cubes on an analytic sphere with its winding, the gradient cull, a plane and a sphere fused from several poses, the absent back shell, the rim rule of `sample`, ray cast accuracy, island removal, the camera path trim), `frame_registration` (the small-motion helpers, a displaced frame recovered on a corner scene, a floor-only frame held by the prior, rejection, the depth member reader), `occlusion_mesh` (the file index through the sort, the camera axis conversion, a synthetic take with a scrambled file order packed as a capture and fused end to end with drifted poses that the refinement beats), `mesh_texturing` (a wall and a panel seen by two synthetic views, each triangle taking the view that faces and sees it, a quad behind every camera on the grey tile, the vertex split between views, texcoords through the vertex compaction), and `glb_mesh_writer` (a round trip through cgltf).

## Known limits

- Thin structures and anything that moves are absent. An open laptop lid on a desk is the measured example: the LiDAR sees it at high confidence from the front, and the views past its edge carve it away at any truncation.
- The texture is a projection from a handful of frames, so a surface seen only at a glance takes a stretched, oblique tile, and anything that moved between the fused frames and the chosen view is painted where the view saw it. The LiDAR depth that gates visibility is coarse, so a thin occluder can leak the view behind it onto a triangle. The far side the extraction closes an object with stays grey, since no frame saw it.
- The end-to-end seam of a long loop stays where ARKit's relocalisation put it, about 3 cm on the measured take. A loop constraint on top is an open question in the proposal.
- Fusion is CPU-only. A compute shader is the obvious later step and is tracked in `plan.md`.

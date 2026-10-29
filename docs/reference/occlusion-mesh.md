# Occlusion mesh

The occlusion mesh is a static room mesh fused from the LiDAR depth a phone records beside a take, registered as a model stencil so a composited character is hidden by the real furniture it walks behind. This doc covers the fusion pipeline, the pose refinement that runs inside it, the editor stage and the headless command that drive it, the files they write, and the spaces everything lives in. The decision behind the design and the measurements that led to it are in [the proposal](../proposals/occlusion-mesh.md); the take format it consumes is in [videosources.md](./videosources.md) ("Pose tracks" and "Depth tracks"); the single-frame depth proxy it supersedes for occlusion is in [depth-proxy-mesh.md](./depth-proxy-mesh.md).

## Input

A `trackcamera` capture recorded by MikanARStreamer on a LiDAR phone, in the project's `captures` folder: its `movie_pose` member (version 2, declaring `depth_width`, `depth_height`, `depth_unit_mm`) and its `depth` and `confidence` members ([videosources.md](./videosources.md), "Capture archives"). Record `i` of the two members belongs to frame `i` of the pose track's file order. `PoseTrack` sorts frames by time on load and keeps that position as `PoseTrackFrame::fileIndex`, which is the only key the fusion reads records by. The depth members are read from the capture whatever track the source plays, so a source repointed at a refined track still finds them.

The take must be aligned: `FileVideoSourceDefinition::hasPoseOffset()` must hold, since the mesh is placed in the stage through that offset and an unaligned take has nowhere to go.

## Pipeline (`src/Editor/Reconstruction`)

`OcclusionMeshBuilder::build` is pure data in and data out: a `PoseTrack`, an open `DepthTrackReader`, settings, an optional progress and cancel observer, and a result. Every other entry point calls it the same way.

1. Keyframes: every `keyframeStride`th frame in time order (default 3).
2. Bounds: every fourth keyframe's confidence-2 pixels, back-projected and transformed to world, trimmed to the 0.5 and 99.5 percentile per axis, padded by `boundsMargin` (0.3 m). With `bBoundsOverride` set, the operator's box (`boundsOverrideLo`, `boundsOverrideHi`, in the take's ARKit world) replaces them, so a long walk around a small subject does not widen the grid.
3. Volume: `TsdfVolume`, a dense truncated signed distance grid over that box. Default 1.5 cm voxels with a 6 cm truncation (the room preset); 8 mm and 3.2 cm for a tabletop. The voxel grows by ten percent until the grid fits `maxVoxels` (60 million), and `OcclusionMeshBuildStats::bVoxelGrown` records when it did.
4. Integration, per keyframe: confidence 0, 1, 2 becomes weight 0, 0.5, 1; pixels whose depth jumps by more than 8 percent against a 4-neighbour are culled (flying pixels at silhouettes); every voxel in the frame's frustum takes the truncated signed distance into its running weighted average. With registration on, the frame is first aligned to the model (below).
5. Trim: with `cameraPathRadius` set (default 2 m, 0 for off), `CameraPathClip` masks out every voxel farther than the radius from every fused camera position, so a sweep of a corner does not carry the rest of the room into the mesh. The grid itself is never trimmed, so the registration above still had the far walls to anchor on. The test runs per block of four voxels against the block's centre, so the cut is quantised to about 6 cm at the room preset.
6. Extraction: unobserved voxels take the sign of their nearest observed voxel (without this every surface grows a back shell two voxels behind it), then marching cubes under the observed region dilated by one voxel and under the trim mask. A masked voxel is left out of the region, so no cube touching it is emitted and the surface ends there with a bare edge. Vertices on shared edges are welded.
7. Cleanup (`MeshCleanup`): degenerate and duplicate triangles, connected components under `minComponentTriangles` (500 for a room, 1500 for a tabletop), unreferenced vertices, area-weighted vertex normals.
8. Coverage (`MeshCoverage`), on every build: how well the fused keyframes observed each vertex of the clean mesh (below).
9. Texturing (`MeshTexturing`), when the caller hands `build` a `MovieDecoder` open on the take's movie: a rough colour so the mesh reads, not photogrammetry. `collectTextureViews` treats every fused keyframe (thinned to 256 on a long take) as a candidate, scores each by the triangles it sees from its refined pose and its depth record alone, and picks up to `textureViews` (16) greedily by how many triangles each adds to the coverage, so the costly colour decode is paid only for the chosen frames. Each chosen frame is decoded by presentation time (a frame the decoder cannot land on within half a frame period is skipped) and shrunk to a `textureTileWidth` (640 px) tile. The tiles form one atlas with a flat grey tile last. Each triangle takes the view that sees it best: every vertex projects onto a measured depth pixel no more than 5 cm in front of it (the farthest of the 3x3 around the pixel, since at the LiDAR's resolution a vertex on a concave edge rounds onto the nearer wall beside it), the face normal turns toward the camera by a cosine of at least 0.2, and among those the most frontal per metre wins. A triangle no view clears that floor for is tried again at a cosine of 0.05, since a stretched glancing view reads better than grey. Its vertices' texcoords are their projections into that tile, and a vertex shared by triangles of different views is split, so the textured mesh has more vertices than the fused one. `textureMesh` reports the source vertex of each split vertex, and the coverage record follows the split through it. Triangles no view sees land on the grey tile. Seams between views are visible and intended. On the measured desk take about a quarter of the triangles stay grey, and most of those face away from every camera position: the closed far sides the extraction gives an object whose back was never observed, which no frame can colour.

Threads: integration splits the frustum's voxel box over z slabs, the ray cast over rows, the trim mask over cell slabs, the coverage over vertices, and the view choice over triangles, through `parallelFor`; marching cubes, cleanup, and the vertex split are single-threaded. Cancellation is cooperative between frames, views, and phases.

## Coverage

An occlusion mesh is only as good as the take it is fused from, and a side the operator never faced comes out lumpy. The coverage pass measures that per vertex so the preview can show it ([the proposal](../proposals/occlusion-coverage-feedback.md) carries the evidence and the phone half). Every vertex of the clean mesh is projected into every fused keyframe's depth at the pose the frame was fused at (refined when registration ran), through `DepthProjection`, the projection `MeshTexturing` shares. The frame's pixel is usable when the fusion would have used it: kept by the flying pixel cull at the fusion's `gradientCull`, confidence at least 1, and inside the depth range. A frame whose camera lies behind the vertex's surface counts nothing. A usable pixel then:

- supports the vertex when its depth agrees within the band, `max(supportBand, bandVoxels * voxel)`, which is 1.5 cm or two voxels
- carves it when it lies farther than the band (the frame says the space is empty)
- is hidden from it when it lies nearer, and counts nowhere

`MeshCoverageResult` keeps one record per vertex:

- `support` and `supportHigh` (the same at confidence 2)
- `carve`
- `directions`, the distinct bins the supporting cameras sat in: one frontal bin within 30 degrees of the normal and eight azimuth bins around it
- `bestCos`, the largest cosine between the normal and a supporting camera
- `nearest`, the distance to the nearest supporting camera
- the class

A vertex is good when `support >= 8`, `directions >= 3`, `bestCos >= 0.7`, and carve is at most a third of `support + carve`. It is weak when it is supported but fails any of those, and unseen when nothing supports it. The thresholds are first values fitted to one take and live in `MeshCoverageSettings`. The stats carry the share of the mesh's area in each class, apportioned a third of each triangle to each corner.

The pass reads the depth records in turn on one thread, since `DepthTrackReader` is one stream, and splits each frame's vertices over the threads. It scores every fused keyframe.

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
| coverage | ARKit world | a fused frame's OpenCV camera, then its depth pixel | the frame's fused pose through `toOpenCvCameraToWorld`, then `DepthProjection::projectToPixel` |
| stencil | ARKit world | editor world | `setWorldTransform(stageWorld * poseOffset)`, the composition `FileVideoSourceComponent::publishPoseForFrame` applies to a camera, lifted to the mesh |

Depth intrinsics are the frame's colour intrinsics scaled by `depth_width / image_width` and `depth_height / image_height` (`scaleIntrinsics`), and rays go through integer pixel positions. ARKit world and the editor's world share handedness and up axis, so the mesh needs no axis conversion anywhere. Triangles are wound counter-clockwise about their outward normal, which the Unreal client depends on. Texcoords are V-up in `TriangleMesh`, as `GlbMeshWriter` takes them, and the atlas image's rows run down, so the texturing flips V once when it writes a texcoord.

## The stage

`AppStage_OcclusionMeshBuild` (`src/Editor/AppStages/OcclusionMeshBuild`) is entered from a file video source's "Build Occlusion Mesh" function (`build_occlusion_mesh`). `tryEnterBuild` refuses, with a message box, media that is not a `trackcamera` capture, a pose track override that does not load, a capture without depth or movie members, an unaligned take, a source no camera on a stage uses, or no project. The track is the override when the source has one from the capture's own session and the capture's `movie_pose` otherwise: an override recorded in another session indexes nothing in this capture's depth members, so it is skipped with a log line, as playback skips it. The stage then runs the depth proxy capture's worker pattern: the UI thread hands a copy of the pose track, the capture path with its depth and movie members, and the settings to a worker thread under a mutex, the worker opens the reader and a `MovieDecoder` on the movie member (through `Capture::makeSubfileUrl`, with no GL in sight) and calls the builder, and `update` polls the phase and frame counts into the panel each frame. A movie that will not open costs the texture, not the mesh. Cancel is a flag the builder checks between frames and views.

The panel offers the grid preset, the keyframe stride, the trim radius, the registration toggle, whether the source should play the take with the refined poses, and the texture toggle. After a build it shows the triangle count, grid, frames fused, voxels trimmed, the registration summary, the views used with the atlas size, the coverage shares, and a warning when the voxel grew past the preset.

The preview (`OcclusionMeshPreview`) draws the mesh solid where the stencil will land, lit through `Internal_PNT_TexturedLitColored`, and the viewport camera orbits it. On arrival and from "Frame Mesh", the orbit centres on the mesh's bounds at a distance that fits them, looking the way the fused cameras looked on average. A "View" combo picks what the mesh shows, from the views the build can show:

- Textured: the atlas, uploaded with its rows flipped since this shader samples V as given
- Shaded: plain grey
- Coverage: the class, green good, yellow weak, red unseen, the default after a build
- Observations, Directions, Viewing angle, Contradiction, Distance: one measure on a red to green ramp, grey where the measure has no value

Every view is the same geometry and material. An overlay writes its measure into the texcoord and samples a small colour table, so a switch rebuilds the vertex buffer only. A class boundary interpolates through yellow across one triangle, which is accepted. "Show the camera path" draws the fused camera positions as a polyline, which ties a weak side to where the operator walked.

The bounds box is drawn over the preview from the bounds the build used. Dragging one of its six face handles with the left mouse button moves that face along its axis (the right button orbits). "Rebuild In Box" carries the box into the next build's settings as the bounds override, and the settings page shows it with a "Use Automatic Bounds" reset. The box lasts for the stage session.

Create Stencil writes `<project>/models/OcclusionMesh_<timestamp>.glb` (`GlbMeshWriter`) and, when the mesh was textured, `OcclusionMesh_<timestamp>.png` beside it first, referenced from the glb's material as the base colour texture. Clients receive the png embedded in the model's GLB from the geometry route ([wire-protocol.md](./wire-protocol.md)), so they never read it from disk. It then creates a `ModelStencilComponent` under the active scene with that model path and the world transform above, refreshes the asset catalog, and when registration ran writes `captures/<stem>.refined.pose.json` beside the capture (version 2, same session id, image size, depth header, times, and intrinsics, frames in file order) and, with the toggle on, points the source's pose track path at it. The capture itself is never rewritten. A take without an active scene cannot be applied: stencils accept only a scene as their parent.

Drive scripts reach it with `function invoke FileVideoSourceSystem <componentId> build_occlusion_mesh`, then the stage commands in [automation.md](./automation.md): the build options and `set_option bounds` before a build, and `set_option view` and `frame_mesh` on the preview after one.

## The headless command

`MikanCmd.exe -occlusionMesh -take=<take.trackcamera> [-glb=<path>] [-refinedTrack=<path>] [-voxel=<metres>] [-stride=<n>] [-noRegistration] [-pathRadius=<metres>] [-noTexture] [-textureViews=<n>] [-bounds=<lo.x,lo.y,lo.z,hi.x,hi.y,hi.z>] [-coverage=<dir>]` runs the same builder on a capture outside the editor, reading the capture's own track, depth members, and movie, and writes the mesh, its atlas as `<glb stem>.png` beside the glb, and the refined track, by default beside the capture. It is the parity check against the Python harness in the experiments repo, which fuses the same take with `recon/fusion.py` and scores both.

`-bounds` fuses inside that box. `-coverage` writes the coverage record as one `.npy` per measure into the folder, indexed by the glb's vertices, so `numpy.load` reads each directly (`NpyWriter`, little-endian, version 1.0 header):

- `support.npy`, `support_high.npy`, `carve.npy` (`uint16`)
- `directions.npy`, `class.npy` (`uint8`, class 0 unseen, 1 weak, 2 good)
- `best_cos.npy`, `nearest.npy` (`float32`)

## glTF models

The mesh is written as a glTF binary rather than OBJ because a room is about a million triangles, which a text parser loads slowly and a binary buffer loads as a copy. `GltfModelImporter` (`src/Editor/Asset/Import`) reads `.glb` and `.gltf` through cgltf (`thirdparty/cgltf`): every triangle primitive in the scene becomes one triangulated mesh plus one wireframe mesh, node transforms are applied, and the base colour texture is loaded from the embedded image or its URI. glTF stores texture V down and the renderer samples V up, so the importer flips V at one point and `GlbMeshWriter` flips it back on the way out. `GlbMeshWriter::writeToBytes` also builds the GLB the geometry route serves to clients, from any model's loaded meshes. `ModelAssetReference` lists all three extensions, and the model resource manager matches extensions case-insensitively.

## Tests

`MikanCmd -runTests` carries six modules for this: `tsdf_volume` (marching cubes on an analytic sphere with its winding, the gradient cull, a plane and a sphere fused from several poses, the absent back shell, the rim rule of `sample`, ray cast accuracy, island removal, the camera path trim), `frame_registration` (the small-motion helpers, a displaced frame recovered on a corner scene, a floor-only frame held by the prior, rejection, the depth member reader), `occlusion_mesh` (the file index through the sort, the camera axis conversion, a synthetic take with a scrambled file order packed as a capture and fused end to end with drifted poses that the refinement beats, a coverage record per output vertex, a capped grid growing its voxel and a bounds box keeping it), `mesh_coverage` (support, carve, and hidden against a synthetic plane, unusable pixels and a camera behind the surface counting nothing, direction bins from three azimuths, the class rule, area shares, the band following the voxel, the remap, an `.npy` round trip), `mesh_texturing` (a wall and a panel seen by two synthetic views, each triangle taking the view that faces and sees it, a quad behind every camera on the grey tile, the vertex split between views and its source vertex remap, texcoords through the vertex compaction), and `glb_mesh_writer` (a round trip through cgltf).

## Known limits

- Thin structures and anything that moves are absent. An open laptop lid on a desk is the measured example: the LiDAR sees it at high confidence from the front, and the views past its edge carve it away at any truncation.
- The texture is a projection from a handful of frames, so a surface seen only at a glance takes a stretched, oblique tile, and anything that moved between the fused frames and the chosen view is painted where the view saw it. The LiDAR depth that gates visibility is coarse, so a thin occluder can leak the view behind it onto a triangle. The far side the extraction closes an object with stays grey, since no frame saw it.
- The end-to-end seam of a long loop stays where ARKit's relocalisation put it, about 3 cm on the measured take. A loop constraint on top is an open question in the proposal.
- Fusion is CPU-only. A compute shader is the obvious later step and is tracked in `plan.md`.

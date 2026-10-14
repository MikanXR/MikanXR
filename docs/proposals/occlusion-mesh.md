# Proposal: Occlusion mesh from MikanARStreamer LiDAR recordings

Design decided, Part 1 implemented. This document is the handoff: it carries the decision, the evidence behind it, what already exists on both sides, the pieces in order, and the open questions. A reader starting from it should not need the conversation that produced it.

## Goal

A character composited into the video must be occluded by real furniture it walks behind, with silhouette edges clean enough to pass as in-shot. Today's depth proxy mesh ([depth-proxy-mesh.md](../reference/depth-proxy-mesh.md)) is a shadow catcher from one MoGe-2 frame: single viewpoint, holes behind everything, metric scale from appearance. It cannot do this job, and `plan.md` carries the gap as "Depth proxy mesh: occlusion-grade silhouette edges".

## Decision

The occlusion mesh is built from the iPhone's LiDAR, recorded by MikanARStreamer alongside the video it already records: ARKit `sceneDepth` (256x192, metres), its confidence map, and the per-frame pose and intrinsics the pose track already carries. A TSDF fuses the sweep into one mesh. No learned depth model is involved. The mesh is a coarse depth prior: at composite time it is rendered from the tracked camera into a depth map, and an image-space refinement step snaps the resulting mask to the video frame's edges within a band of a few pixels. Silhouette quality is decided in that last step and by pose accuracy, not by mesh detail.

The decision rests on a comparison run in the ReconstructionExperiments repo (`D:\Github\git-MikanXR\ReconstructionExperiments`, see its README "Decision" and `NOTES.md`), on ten Stray Scanner captures (one hotel room, nine cluttered tabletops) with four candidate depth models (Prompt Depth Anything, Depth Anything 3, MapAnything, MILo) against a LiDAR-only baseline, all fused by the same TSDF and scored against held-out LiDAR frames:

- No model beat the LiDAR baseline on any capture. Means over ten: baseline 1.87 cm MAE and 8.7 px edge distance, the best model (MapAnything with LiDAR scale alignment) 1.88 cm and 8.4 px at 10 GB of VRAM and 145 s per capture, PromptDA 2.6 cm, DA3 8.0 cm, MILo worst and non-commercial.
- The models' one visible gain, crisper small-object outlines at 8 mm voxels, is a depth-resolution effect, and fine-voxel LiDAR fusion recovers part of it for free.
- At room scale the LiDAR mesh against its own held-out frames is 1.75 cm MAE and 11 px edge distance. That is ARKit pose drift across the sweep, every method inherits it, and it is now the dominant silhouette error.
- Every model needed the LiDAR as input to produce metric depth at all (MapAnything without it: 11 cm MAE). Recording the LiDAR is the prerequisite either way.
- Thin structures (chair legs) and anything that moves are absent from every static mesh. Image-space refinement cannot invent them. Per-frame depth at runtime is the only route to those and is a different architecture, out of scope here. An open laptop lid on a desk is the same case: the LiDAR sees it frontally at high confidence, and the views that look past its edge carve it away, at any truncation.

PromptDA stays in the experiments repo as an optional offline refinement for close-range hero objects. It is a Python preprocessing step, not a candidate for an in-editor port.

## What exists today

On the MikanXR side ([videosources.md](../reference/videosources.md), [automation.md](../reference/automation.md), [compositor.md](../reference/compositor.md)):

- MikanARStreamer records an H.264 take with a `<stem>.pose.json` sidecar (`mikan-pose-track` version 1: per frame `time_us`, `capture_timestamp_us`, a row-major camera-to-world transform in ARKit world space, pinhole `fx fy cx cy`), writes a pose only for frames it actually encoded, and answers `record start [marker]` / `record stop` / `capture [marker]` over the ARKit debug channel (`arkit send ...` through `tools/automate.py`).
- The phone's Captures panel uploads a take and its sidecar to the editor one file per request: `POST /assets/upload?folder=<folderId>&name=<fileName>`, owned by `AssetUploadRequestHandler` (`src/Editor/Server/AssetUploadRequestHandler.h`), with the file type judged by extension against the folder's asset factories.
- `FileVideoSourceComponent` plays a take with its pose track, matching frames to poses by nearest `time_us`, and applies the ARKit world-to-stage offset at exactly one point, `FileVideoSourceComponent::publishPoseForFrame` (the live source's point is `ARKitVideoSourceComponent::notifyFrameBundleReceived`). The offset is solved by the Align Camera action from a marker reference take and persists on the definition.
- Model stencils (`ModelStencilComponent`) are OBJ meshes placed in stage space. `DepthMaskNode` renders the stencils in a compositor graph into a linear depth texture, and `WriteDepthNode` puts that depth into the working buffer so later layers depth test against it. The depth proxy capture (`AppStage_DepthMeshCapture`) already registers a captured mesh as a model stencil, parented under the active scene, with the OBJ written to `<project>/models/`.
- Coordinate conventions, the single OpenCV-to-Mikan conversion points, and the Y-flip story are in [conventions.md](../reference/conventions.md). The depth proxy code converts model output (OpenCV camera axes) to Mikan camera space at exactly one place and documents why.

On the experiments side (`D:\Github\git-MikanXR\ReconstructionExperiments`):

- `recon/capture.py` loads Stray Scanner captures (poses are camera-to-world with OpenCV camera axes, no flip needed; verified by reprojecting LiDAR between frames to millimetres).
- `recon/fusion.py` is the fusion reference: a dense GPU TSDF over the training LiDAR's bounding box, 1.5 cm voxels and 6 cm truncation by default (8 mm in `configs/fine.yaml` for tabletop scale), per-pixel weights from confidence (0, 0.5, 1 for ARKit 0, 1, 2), a relative-gradient cull of flying pixels at depth discontinuities, nearest-sign fill of unobserved voxels before marching cubes (without it every surface grows a spurious back face two voxels behind it), and connected-component island removal.
- `recon/evaluate.py` scores a mesh against held-out frames: ray cast from the ARKit pose at LiDAR resolution, compared with confidence-2 pixels (depth error, completeness, floater hits, silhouette edge distance), plus unsupported-surface fraction against the LiDAR cloud. `run.py`, `run_all.py`, `summarize.py`, `status.py` drive it. This is the measurement the pose-refinement work needs.

## The pieces

In dependency order. Each is a separate change and a separate review.

### Part 1: MikanARStreamer records `sceneDepth` and confidence

Done. The format is documented in [videosources.md](../reference/videosources.md) ("Pose tracks" and "Depth tracks") and the streamer README. What was decided while building it:

- The session runs with `.sceneDepth` and autofocus off only for the span of a video take (`ARCaptureController.sceneDepthEnabled`, re-running the configuration without a tracking reset). Streaming and the marker reference take never ask for the LiDAR.
- The depth and confidence buffers are copied on ARKit's thread into a `DepthFrame` (uint16 millimetres, uint8 confidence) so the `ARFrame` is released at once; only the colour buffer travels on as a reference.
- The sidecar files are created by the first depth frame, not at start. Frames the movie wrote before the sensor delivered are backfilled as zero records, so record index always equals pose index, and a take whose sensor never delivers leaves no sidecar and no `depth_*` keys.
- The editor's `PoseTrack` loader accepts versions 1 and 2 (the original loader rejected anything but 1, so the bump alone would have broken playback). The editor's own recorder still writes version 1.
- The `movies` folder gained `DepthTrackAssetReference` for the two files, so the upload route accepts them.
- `record` and `record start` reply with `depth on|off`, `record stop` adds the sidecar paths and frame counts, `stats` carries `depthFramesWritten`, and `captures list` ends each line with `depth|-`.

- Verified on a device: a 20 s room take (615 frames, 612 with depth, the first three zero records from sensor warm-up) uploaded whole and reprojects LiDAR between frames thirty apart at 1.7 to 5.8 mm median per pair, 5.3 mm over the take (`tools/check_take.py` in the experiments repo). The first pair has nothing to compare because the warm-up frames carry no high-confidence depth.

### Part 2: Validate takes in the experiments harness before any C++

- [x] Add a Mikan take loader to `recon/capture.py` (mp4 plus `.pose.json` plus the two binaries), producing the same `Capture` object as the Stray Scanner loader. The transform in the pose track is row-major camera-to-world in ARKit world space with ARKit camera axes (+Y up, +Z toward the viewer), so the loader applies the one 180 degree rotation about X that Stray Scanner bakes in, and nothing else. `load_mikan_take(<take>.mp4)`, and `run.py --capture <path to the mp4>` picks it up. Verified by the reprojection check above, which would read tens of centimetres with the flip wrong.
- [x] Run `lidar_tsdf` on a few takes (tabletop and room). Three streamer takes, all recording correctly by the reprojection check, details in the experiments repo `NOTES.md`:
	- A 20 s room sweep: 1.38 cm MAE, 10.2 edge px, 0.39 cm median held-out error.
	- A tabletop (a desk with two open laptops): 0.56 cm median held-out error but 2.55 cm MAE, because the laptop lids are carved out of every fusion by the side and rear views, the thin-structure limit above with a desk example. Not a recording problem.
	- A 57 s room loop over 16.6 m: 2.35 cm MAE, 8.7 edge px, 1.07 cm median held-out error. The median doubling against the short sweep of the same room is drift within the sweep. The loop closes to 31 mm and 0.9 degrees after ARKit's own relocalisation, so the error is spread along the path rather than sitting at the seam.

### Part 3: Pose refinement of the sweep

Decided: frame-to-model ICP during integration, the KinectFusion loop. The measured drift (1.75 cm MAE and 11 px edge distance on the Stray Scanner hotel room, 2.35 cm and 8.7 px with a 1.07 cm median on the streamer's room loop, both at 256x192) is what the refinement has to remove, and the harness measures the result directly. The streamer loop settled the choice: ARKit closes the loop itself to about 3 cm and 1 degree, so the residual is distributed along the sweep rather than concentrated at the seam, and a method that corrects every keyframe against the geometry removes it where a loop-only method would not. The alternatives were keyframe bundle adjustment over video feature tracks (loop closure, but the most machinery, against motion-blurred frames at walking speed) and a pose graph with ICP loop constraints (addresses a seam that is already small).

The refinement runs offline on the recorded take, and its output is a refined pose track (the same format, a new file beside the original) so every consumer downstream is unchanged. The live camera's registration at composite time is a separate problem and is covered by the existing alignment path.

The method is built and measured in the experiments repo (`fusion.frame_to_model`, `recon/register.py`, the room loop results and the three defects found on the way in `NOTES.md`). On the room loop it takes the held-out median from 1.07 cm to 0.49 cm, the level of a short sweep, and MAE from 2.35 to 1.59 cm, with every frame accepted and an ARKit tracking jump of 56 mm absorbed in one step. It leaves the end-to-end seam where ARKit's relocalisation put it, about 3 cm and 1 degree. The parts that carry into the Part 4 port are the ones the defects forced:

- the ray cast treats any sample that touches an unobserved voxel as unobserved, or the rim of every observed patch reads as a wall
- model normals from a two-voxel gradient stencil, not one
- a prior on the deviation from the carried-forward ARKit pose (3 mm, 0.2 degrees against a 5 mm data sigma) for the directions a frame's geometry leaves free
- a rejected frame is skipped, never integrated at the stale pose, and the per-step bound (15 cm) is wide enough to absorb a tracking jump

Remaining on this part:

- [x] Write the refined poses out as a pose track file beside the original (the editor writes `<stem>.refined.pose.json`; the experiments repo keeps them as `poses_refined.npy`).
- [ ] Record ARKit's per-frame tracking state in the pose track, so a fusion can skip limited-tracking frames outright instead of discovering them as rejected registrations.

### Part 4: TSDF fusion in Mikan

Done: [occlusion-mesh.md](../reference/occlusion-mesh.md) is the reference. Two decisions changed from the list below while building it: the mesh is written as a glTF binary rather than OBJ, since a room is a million triangles and OBJ parsing is slow, which brought `.glb` import into the editor through cgltf; and the vertices stay in ARKit world space with the stencil carrying `stageWorld * poseOffset`, so a re-solved alignment moves the stencil rather than forcing a rebuild.

- [x] Entry point: an action on a file video source that carries depth sidecars, "Build occlusion mesh from take", alongside the existing recording and alignment tools, or a stage in the shape of `AppStage_DepthMeshCapture` with a worker thread and a progress readout. The capture stage's pattern (gather on the UI thread, work on a worker, poll for the result) carries over.
- [x] Algorithm as in `recon/fusion.py`: bounding box from the confidence-2 cloud, confidence weights, gradient cull, truncated signed distance with a running weighted average, nearest-sign fill of unobserved voxels, marching cubes, island removal. A CPU implementation is acceptable first (the room fused in 67 s on a GPU, mostly in marching cubes and cleanup, which were CPU anyway). A compute shader is the obvious later step.
- [x] Spaces: the take's poses are ARKit world space. The mesh is fused in that space and placed in the stage through the take's solved world-to-stage offset, so an unaligned take produces a mesh that cannot be placed and the tool says so. The vertices crossing from OpenCV camera convention into Mikan convention do so at one named point, as the depth proxy does.
- [x] Output: an OBJ under `<project>/models/` with a unique name per build, registered as a `ModelStencilComponent` under the active scene, counter-clockwise winding around outward normals (the Unreal client depends on it; the depth proxy doc records the lesson). Unlike the depth proxy the vertices are stage-space, not camera-local, so the stencil's transform is identity relative to the stage.
- [x] Unit tests in the `MikanCmd -runTests` suite for the TSDF (a synthetic plane and sphere fuse to the right surface, the back-shell case stays absent, the gradient cull drops a synthetic flying pixel).

### Part 5: Image-space edge refinement

- [ ] A compositor node that takes the depth mask (`DepthMaskNode` output), the video texture, and a band width in pixels, and returns a refined mask or depth where edges follow the video within the band: joint bilateral upsampling or a guided filter, computed in the fragment shader. This is where the silhouette is decided and it should be built and judged on real takes with a character behind furniture, not on synthetic data.
- [ ] Measure it: the harness's edge distance on held-out frames gives the prior's error; the refined mask's agreement with hand-marked edges on a handful of frames gives the step's gain. A small set of marked frames is worth making early.

## Constraints to respect

- Nothing here changes the client wire protocol. A new sidecar format, a new stencil, and a new compositor node are editor-side. If a client ever needs the occlusion mesh it already arrives through `GetModelStencilRenderGeometry`. Read [wire-protocol.md](../reference/wire-protocol.md) before adding any property.
- Coordinate conventions have single conversion points ([conventions.md](../reference/conventions.md)). Do not add a flip on a hunch; verify with the reprojection check.
- Pose track version 2 must load version 1 takes unchanged: depth sidecars are optional, and a take without them still plays and aligns.
- Code style per [standards.md](../reference/standards.md), and no doc citations in code comments.

## Open questions

- Whether a loop constraint on top of the frame-to-model refinement is worth building for the remaining end-to-end seam, about 3 cm and 1 degree on the room loop after ARKit's own relocalisation.
- Whether fusion should run on the phone instead, using ARKit's own `sceneReconstruction` mesh as a free alternative. It costs nothing to export `ARMeshAnchor`s in the same take and compare them in the harness, and it would short-circuit Part 4 if the quality is enough. Worth adding to Part 1 as an optional export.
- Voxel size per take: 1.5 cm for a room, 8 mm for a tabletop. A heuristic from the sweep's extent, or an operator setting. On the tabletop take 8 mm changed nothing but the fusion time (five times longer) and the unsupported-surface fraction, so the measured case for 8 mm is still only the Stray Scanner small-object outlines.
- Whether the fusion should weight free-space carving below surface votes, so a thin plate seen frontally many times survives the views that look past its edge. Untested; it would be an experiment in `recon/fusion.py` before it is a Part 4 decision.
- Whether the editor keeps the fused volume for incremental updates (a second sweep extends the mesh) or always rebuilds from a take.

## References

- Experiments repo: `C:\GitHub\BrendanWalker\ReconstructionExperiments`, `README.md` (decision and results), `NOTES.md` (observations, the back-shell bug, the Windows build notes), `recon/fusion.py`, `recon/evaluate.py`, `recon/capture.py`.
- MikanXR docs: [depth-proxy-mesh.md](../reference/depth-proxy-mesh.md), [videosources.md](../reference/videosources.md) (pose tracks, uploads, the offset), [automation.md](../reference/automation.md) (the `arkit` channel), [compositor.md](../reference/compositor.md) (depth nodes and stencils), [conventions.md](../reference/conventions.md), [calibration.md](../reference/calibration.md) (the marker alignment that produces the offset).
- Stray Scanner data format, the model the sidecars follow: `https://github.com/strayrobots/scanner/blob/main/docs/format.md`.

# Proposal: Occlusion mesh from MikanARStreamer LiDAR recordings

Design decided, implementation not started. This document is the handoff: it carries the decision, the evidence behind it, what already exists on both sides, the pieces in order, and the open questions. A reader starting from it should not need the conversation that produced it.

## Goal

A character composited into the video must be occluded by real furniture it walks behind, with silhouette edges clean enough to pass as in-shot. Today's depth proxy mesh ([depth-proxy-mesh.md](../reference/depth-proxy-mesh.md)) is a shadow catcher from one MoGe-2 frame: single viewpoint, holes behind everything, metric scale from appearance. It cannot do this job, and `plan.md` carries the gap as "Depth proxy mesh: occlusion-grade silhouette edges".

## Decision

The occlusion mesh is built from the iPhone's LiDAR, recorded by MikanARStreamer alongside the video it already records: ARKit `sceneDepth` (256x192, metres), its confidence map, and the per-frame pose and intrinsics the pose track already carries. A TSDF fuses the sweep into one mesh. No learned depth model is involved. The mesh is a coarse depth prior: at composite time it is rendered from the tracked camera into a depth map, and an image-space refinement step snaps the resulting mask to the video frame's edges within a band of a few pixels. Silhouette quality is decided in that last step and by pose accuracy, not by mesh detail.

The decision rests on a comparison run in the ReconstructionExperiments repo (`C:\GitHub\BrendanWalker\ReconstructionExperiments`, see its README "Decision" and `NOTES.md`), on ten Stray Scanner captures (one hotel room, nine cluttered tabletops) with four candidate depth models (Prompt Depth Anything, Depth Anything 3, MapAnything, MILo) against a LiDAR-only baseline, all fused by the same TSDF and scored against held-out LiDAR frames:

- No model beat the LiDAR baseline on any capture. Means over ten: baseline 1.87 cm MAE and 8.7 px edge distance, the best model (MapAnything with LiDAR scale alignment) 1.88 cm and 8.4 px at 10 GB of VRAM and 145 s per capture, PromptDA 2.6 cm, DA3 8.0 cm, MILo worst and non-commercial.
- The models' one visible gain, crisper small-object outlines at 8 mm voxels, is a depth-resolution effect, and fine-voxel LiDAR fusion recovers part of it for free.
- At room scale the LiDAR mesh against its own held-out frames is 1.75 cm MAE and 11 px edge distance. That is ARKit pose drift across the sweep, every method inherits it, and it is now the dominant silhouette error.
- Every model needed the LiDAR as input to produce metric depth at all (MapAnything without it: 11 cm MAE). Recording the LiDAR is the prerequisite either way.
- Thin structures (chair legs) and anything that moves are absent from every static mesh. Image-space refinement cannot invent them. Per-frame depth at runtime is the only route to those and is a different architecture, out of scope here.

PromptDA stays in the experiments repo as an optional offline refinement for close-range hero objects. It is a Python preprocessing step, not a candidate for an in-editor port.

## What exists today

On the MikanXR side ([videosources.md](../reference/videosources.md), [automation.md](../reference/automation.md), [compositor.md](../reference/compositor.md)):

- MikanARStreamer records an H.264 take with a `<stem>.pose.json` sidecar (`mikan-pose-track` version 1: per frame `time_us`, `capture_timestamp_us`, a row-major camera-to-world transform in ARKit world space, pinhole `fx fy cx cy`), writes a pose only for frames it actually encoded, and answers `record start [marker]` / `record stop` / `capture [marker]` over the ARKit debug channel (`arkit send ...` through `tools/automate.py`).
- The phone's Captures panel uploads a take and its sidecar to the editor one file per request: `POST /assets/upload?folder=<folderId>&name=<fileName>`, owned by `AssetUploadRequestHandler` (`src/Editor/Server/AssetUploadRequestHandler.h`), with the file type judged by extension against the folder's asset factories.
- `FileVideoSourceComponent` plays a take with its pose track, matching frames to poses by nearest `time_us`, and applies the ARKit world-to-stage offset at exactly one point, `FileVideoSourceComponent::publishPoseForFrame` (the live source's point is `ARKitVideoSourceComponent::notifyFrameBundleReceived`). The offset is solved by the Align Camera action from a marker reference take and persists on the definition.
- Model stencils (`ModelStencilComponent`) are OBJ meshes placed in stage space. `DepthMaskNode` renders the stencils in a compositor graph into a linear depth texture, and `WriteDepthNode` puts that depth into the working buffer so later layers depth test against it. The depth proxy capture (`AppStage_DepthMeshCapture`) already registers a captured mesh as a model stencil, parented under the active scene, with the OBJ written to `<project>/models/`.
- Coordinate conventions, the single OpenCV-to-Mikan conversion points, and the Y-flip story are in [conventions.md](../reference/conventions.md). The depth proxy code converts model output (OpenCV camera axes) to Mikan camera space at exactly one place and documents why.

On the experiments side (`C:\GitHub\BrendanWalker\ReconstructionExperiments`):

- `recon/capture.py` loads Stray Scanner captures (poses are camera-to-world with OpenCV camera axes, no flip needed; verified by reprojecting LiDAR between frames to millimetres).
- `recon/fusion.py` is the fusion reference: a dense GPU TSDF over the training LiDAR's bounding box, 1.5 cm voxels and 6 cm truncation by default (8 mm in `configs/fine.yaml` for tabletop scale), per-pixel weights from confidence (0, 0.5, 1 for ARKit 0, 1, 2), a relative-gradient cull of flying pixels at depth discontinuities, nearest-sign fill of unobserved voxels before marching cubes (without it every surface grows a spurious back face two voxels behind it), and connected-component island removal.
- `recon/evaluate.py` scores a mesh against held-out frames: ray cast from the ARKit pose at LiDAR resolution, compared with confidence-2 pixels (depth error, completeness, floater hits, silhouette edge distance), plus unsupported-surface fraction against the LiDAR cloud. `run.py`, `run_all.py`, `summarize.py`, `status.py` drive it. This is the measurement the pose-refinement work needs.

## The pieces

In dependency order. Each is a separate change and a separate review.

### Part 1: MikanARStreamer records `sceneDepth` and confidence

- [ ] Enable `frameSemantics = [.sceneDepth]` on the session when recording (raw, not `.smoothedSceneDepth`, which blends across time and lags under motion). Confirm `isAutoFocusEnabled = false` during a take so intrinsics stay stable, if it is not already.
- [ ] Per `ARFrame` the recorder accepts, copy `frame.sceneDepth.depthMap` (256x192 Float32 metres) and `frame.sceneDepth.confidenceMap` (256x192 UInt8, 0 low / 1 medium / 2 high) out of their pixel buffers, respecting `CVPixelBufferGetBytesPerRow`, and release the frame immediately. Write only for frames the encoder actually wrote, on the same path that appends the pose record, so the three stay in lockstep.
- [ ] Sidecar layout, proposed: `<stem>.depth.bin` (uint16 millimetres, 256x192, one fixed-size record per written frame, zero where invalid) and `<stem>.conf.bin` (uint8, same shape and order), with the pose track bumped to version 2 carrying `depth_width`, `depth_height`, and `depth_unit_mm`. Record index in the binaries equals the index into `frames` in the pose track. uint16 millimetres halves the Float32 size and matches Stray Scanner, so the experiments repo loader needs one small addition. About 150 KB per frame, 9 MB/s at 60 fps, which the phone writes without trouble; compression can come later if takes get long.
- [ ] Depth intrinsics are the colour intrinsics scaled by `256 / image_width` and `192 / image_height`. Store nothing extra; derive at load, as the experiments loader does.
- [ ] Captures panel uploads the two new sidecars through the existing asset route. `AssetUploadRequestHandler` judges files by extension against the folder's factories, so the `movies` folder needs to accept `.depth.bin` and `.conf.bin` beside `.pose.json`. The same-name replacement rule already keeps a re-sent take together.
- [ ] `record` over the debug channel reports whether depth is being recorded, and `stats` gains a depth-frames-written counter, so a drive script can verify a take without pulling it.
- [ ] Verification before anything downstream: pull a take, load it with the experiments repo loader (Part 2), and run `check_pose_consistency` on it. Correct recording reprojects LiDAR between frames thirty apart to a few millimetres median. This step is blocked until a new streamer build can be tested on a device.

### Part 2: Validate takes in the experiments harness before any C++

- [ ] Add a Mikan take loader to `recon/capture.py` (mp4 plus `.pose.json` plus the two binaries), producing the same `Capture` object as the Stray Scanner loader. The transform in the pose track is row-major camera-to-world in ARKit world space with ARKit camera axes (+Y up, +Z toward the viewer), so the loader applies the one 180 degree rotation about X that Stray Scanner bakes in, and nothing else. Verify with the reprojection check, not by eye.
- [ ] Run `lidar_tsdf` on a few takes (tabletop and room). Expect numbers in the range of the Stray Scanner captures: under 1 cm MAE against held-out frames at tabletop scale, 1.5 to 2 cm at room scale. A take that does worse has a recording problem, which is cheaper to find here than in the editor.

### Part 3: Pose refinement of the sweep

The largest open design item. The measured drift (1.75 cm MAE, 11 px edge distance at 256x192, room scale) is what the refinement has to remove, and the harness measures the result directly. Options, not yet chosen:

- Frame-to-model ICP during integration: register each incoming LiDAR frame against the TSDF built so far before integrating it (the KinectFusion loop). Cheap, no feature tracks, but drifts the same way ARKit does on a long loop.
- Keyframe bundle adjustment over the sweep with feature tracks from the video, poses initialised from ARKit and the LiDAR giving metric scale. Handles loop closure; more machinery.
- Pose graph with loop closure on LiDAR frame pairs (ICP constraints between keyframes that see the same geometry), then one global solve. Middle ground.

Whatever is chosen runs offline on the recorded take, and its output is a refined pose track (the same format, a new file beside the original) so every consumer downstream is unchanged. The live camera's registration at composite time is a separate problem and is covered by the existing alignment path.

### Part 4: TSDF fusion in Mikan

- [ ] Entry point: an action on a file video source that carries depth sidecars, "Build occlusion mesh from take", alongside the existing recording and alignment tools, or a stage in the shape of `AppStage_DepthMeshCapture` with a worker thread and a progress readout. The capture stage's pattern (gather on the UI thread, work on a worker, poll for the result) carries over.
- [ ] Algorithm as in `recon/fusion.py`: bounding box from the confidence-2 cloud, confidence weights, gradient cull, truncated signed distance with a running weighted average, nearest-sign fill of unobserved voxels, marching cubes, island removal. A CPU implementation is acceptable first (the room fused in 67 s on a GPU, mostly in marching cubes and cleanup, which were CPU anyway). A compute shader is the obvious later step.
- [ ] Spaces: the take's poses are ARKit world space. The mesh is fused in that space and placed in the stage through the take's solved world-to-stage offset, so an unaligned take produces a mesh that cannot be placed and the tool says so. The vertices crossing from OpenCV camera convention into Mikan convention do so at one named point, as the depth proxy does.
- [ ] Output: an OBJ under `<project>/models/` with a unique name per build, registered as a `ModelStencilComponent` under the active scene, counter-clockwise winding around outward normals (the Unreal client depends on it; the depth proxy doc records the lesson). Unlike the depth proxy the vertices are stage-space, not camera-local, so the stencil's transform is identity relative to the stage.
- [ ] Unit tests in the `MikanCmd -runTests` suite for the TSDF (a synthetic plane and sphere fuse to the right surface, the back-shell case stays absent, the gradient cull drops a synthetic flying pixel).

### Part 5: Image-space edge refinement

- [ ] A compositor node that takes the depth mask (`DepthMaskNode` output), the video texture, and a band width in pixels, and returns a refined mask or depth where edges follow the video within the band: joint bilateral upsampling or a guided filter, computed in the fragment shader. This is where the silhouette is decided and it should be built and judged on real takes with a character behind furniture, not on synthetic data.
- [ ] Measure it: the harness's edge distance on held-out frames gives the prior's error; the refined mask's agreement with hand-marked edges on a handful of frames gives the step's gain. A small set of marked frames is worth making early.

## Constraints to respect

- Nothing here changes the client wire protocol. A new sidecar format, a new stencil, and a new compositor node are editor-side. If a client ever needs the occlusion mesh it already arrives through `GetModelStencilRenderGeometry`. Read [wire-protocol.md](../reference/wire-protocol.md) before adding any property.
- Coordinate conventions have single conversion points ([conventions.md](../reference/conventions.md)). Do not add a flip on a hunch; verify with the reprojection check.
- Pose track version 2 must load version 1 takes unchanged: depth sidecars are optional, and a take without them still plays and aligns.
- Code style per [standards.md](../reference/standards.md), and no doc citations in code comments.

## Open questions

- Pose refinement method (Part 3). Decide after Part 2 has real takes to measure on.
- Whether fusion should run on the phone instead, using ARKit's own `sceneReconstruction` mesh as a free alternative. It costs nothing to export `ARMeshAnchor`s in the same take and compare them in the harness, and it would short-circuit Part 4 if the quality is enough. Worth adding to Part 1 as an optional export.
- Voxel size per take: 1.5 cm for a room, 8 mm for a tabletop. A heuristic from the sweep's extent, or an operator setting.
- Whether the editor keeps the fused volume for incremental updates (a second sweep extends the mesh) or always rebuilds from a take.

## References

- Experiments repo: `C:\GitHub\BrendanWalker\ReconstructionExperiments`, `README.md` (decision and results), `NOTES.md` (observations, the back-shell bug, the Windows build notes), `recon/fusion.py`, `recon/evaluate.py`, `recon/capture.py`.
- MikanXR docs: [depth-proxy-mesh.md](../reference/depth-proxy-mesh.md), [videosources.md](../reference/videosources.md) (pose tracks, uploads, the offset), [automation.md](../reference/automation.md) (the `arkit` channel), [compositor.md](../reference/compositor.md) (depth nodes and stencils), [conventions.md](../reference/conventions.md), [calibration.md](../reference/calibration.md) (the marker alignment that produces the offset).
- Stray Scanner data format, the model the sidecars follow: `https://github.com/strayrobots/scanner/blob/main/docs/format.md`.

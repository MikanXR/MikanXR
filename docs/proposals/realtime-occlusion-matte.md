# Proposal: A real-time occlusion matte

Measured and tabled. Nothing of it is built. The occlusion matte ([occlusion-matte.md](../reference/occlusion-matte.md)) is built once per recorded take by SAM2 small prompted from the occlusion mesh, and the question was whether the same matte can be produced live, frame by frame, for a live video source. Three measurement passes in the ReconstructionExperiments repo answered it: no lighter segmenter holds SAM2 small's edge, a SAM2 small keyframe carried through the mesh holds one or two frames, and re-anchoring the carried silhouette to the video's edges recovers little of the keyframe's quality. This document is the record of those passes, the reading, and the routes that would reopen the work. A reader starting from it should not need the conversation that produced it.

## Goal

A live video source (the ARKit stream, a USB camera) composited with a character gets the same silhouette correction a recorded take gets from its matte: the mesh's occluding contours moved onto the picture's edges within a band, so the character is clipped where the real object is and not where the mesh says it is. The scene's occluders are static objects the mesh already covers. The matte runs on the GPU the client renderer shares, inside the two-frame slot the compositor's frame queue gives a worker ([compositor.md](../reference/compositor.md)), so it has a few milliseconds per frame and not the per-frame cost the offline build spends.

ARKit's own matte generator is not a route: it mattes people from the phone's person segmentation and is silent on furniture.

## What exists today

- The recorded-take path ([occlusion-matte.md](../reference/occlusion-matte.md)): `OcclusionMatteBuilder` rasterizes the meshes to depth per frame, finds the occluding contour runs, prompts SAM2 small with positive points per run, scores the candidates by IoU against the mesh's near side, and writes a label sidecar the file source plays back and the compositor's node keys into the depth mask.

- `Sam2Segmenter` (`src/Editor/Calibration`) runs the encoder and decoder graphs through `OnnxSession` under DirectML, with CPU tensors: the frame's feature maps cross host to device on every decoder call and the encoder's outputs come back to the host once per frame.

- A live source has no CPU copy of its frames in compositor mode. The ARKit source's `VideoFrameDistortionView::readbackDirectColorTexture` is the GPU readback a worker would use, and `IOcclusionMatteSource` is the seam the compositor's matte ring reads from.

- The measurement harness in `D:\Github\git-MikanXR\ReconstructionExperiments`: `tools/occlusion_edge_eval.py` scores a predicted silhouette against hand-marked occluding edges (median and p90 distance in pixels from the marked edge to the predicted boundary, and the share of the marked edge within 5 px), `tools/sam2_edge_eval.py` runs the prompt design through any segmenter family, `tools/segmenter_sweep.py` and `tools/sam2_dml_timing.py` sweep accuracy and cost, and `tools/keyframe_reproject_eval.py` measures the keyframe carry and the re-anchoring. The marked sets are hand-marked frames from several tabletop recordings and a room walk. The accuracy tables with their reading are in that repo's `NOTES.md` ("Lighter segmenters for a live matte" and "Keyframe reprojection of the matte"), against recordings that are not checked in.

## Pass 1: lighter segmenters

The question was whether a lighter promptable encoder holds SAM2 small's edge through the unchanged prompt design. The candidates, all Apache 2.0, each in its own venv: EdgeTAM (a RepViT trunk, 4.9 M encoder parameters), EfficientTAM ti and s at 1024 and 512 (6.2 and 22.4 M), EfficientViT-SAM l0 and l1 at 512 and xl0 at 1024 (30.7, 43.6 and 112.9 M, with a SAM1-style decoder), and SAM2 tiny and small at 512. The bar set before the runs: a live candidate's DirectML encoder costs at most 10 ms, and its whole-frame accuracy sits within 1 px median and 3 px p90 of SAM2 small's on every marked set with at least 85 percent of the edge within 5 px. The whole frame is the framing a live path encodes, where the offline build crops around each run.

The accuracy of each candidate on each marked set was measured, and the table lives in the experiments repo's notes, against recordings that are not checked in.

None qualifies. Every 512 configuration degrades, worst on the whole frame, where one encoder pixel is 3.75 frame pixels. EfficientViT-SAM l0 is nearest on accuracy, inside the margin on all but one marked set, at 26 to 40 ms of encoder and a second decoder contract.

Cost on the RTX 3090 with the Unreal editor, two `Mikan.exe` and LM Studio running (53 to 97 percent utilization before each family, so a quiet machine runs these in 0.5 to 0.6 of the time: small is 75 ms fp32 under DirectML quiet, 29.9 ms under CUDA), onnxruntime-directml 1.20.1, the 1920x1440 frame as the encoder input, median ms:

| model | input | CUDA encoder | DirectML fp32 | DirectML fp16 |
|---|---|---|---|---|
| SAM2 small | 1024 | 65.4 | 126.7 | 63.1 |
| SAM2 small | 512 | 23.9 | 35.0 | 19.0 |
| SAM2 tiny | 1024 | 57.3 | 113.5 | 55.7 |
| EdgeTAM | 1024 | 45.8 | 21.4 | 13.5 |
| EfficientTAM ti | 1024 | 29.1 | 58.6 | 30.5 |
| EfficientTAM ti | 512 | 23.5 | 16.2 | 11.7 |
| EfficientViT-SAM l0 | 512 | 30.2 | 40.4 | 26.0 |
| EfficientViT-SAM xl0 | 1024 | 80.7 | 117.8 | 70.5 |

The decoder is the larger term past one run. SAM2 small's decoder with 8 points takes 11.1 ms with its three feature tensors copied from the host per call, 9.6 ms bound once as DirectML `OrtValue`s, and 10.0 ms as an fp16 graph. A frame at 1, 4 and 7 runs (encoder plus bound decoder calls) costs 72.8 / 101.7 / 130.6 ms for small fp16 at 1024, 25.7 / 55.3 / 84.8 for tiny fp16 at 512 and 23.1 / 51.7 / 80.4 for EdgeTAM fp16. Seven runs on one frame cost about 67 ms of decoder under this load whatever the encoder, so per-frame segmentation needs the runs' prompts batched into one decoder call (the mask decoder takes a batch of prompts, and the current export's batch axis is 1) as well as a lighter encoder, and neither alone fits the budget.

What the pass settled about the graphs, which any live port inherits:

- fp16 halves every encoder under DirectML. `onnxruntime.transformers.float16.convert_float_to_float16` with `keep_io_types` works where `onnxconverter_common` leaves `Cast` nodes that fail type checking. The encoder's positional-embedding `Resize` is a constant chain ORT cannot fold in fp16, and left in place it runs on the CPU at 1.2 s per call, so the fp32 graph is constant-folded offline first (a CPU session at basic optimization writing `optimized_model_filepath`), after which the whole encoder is one DirectML fused node. The converted graph loads on onnxruntime-directml 1.20.1, the editor's version, and fails on 1.24.4 with `HasExternalDataInMemory` unless graph optimization is disabled.
- The fp16 encoder's largest difference in `image_embed` against fp32 is 1.3 to 2.0 percent of the output's range. The fp16 decoder's masks are within 0.2 to 0.4 percent of the fp32 range and its `iou_scores` within 0.5 percent on tiny and 6 percent on small (the matte chooses candidates by IoU against the mesh, not by that score).
- DirectML has no cubic `Resize` kernel. EfficientViT-SAM's neck has three, which ORT ran on the CPU with a host round trip, 432 of the 421 to 484 ms the unpatched graphs took. `tools/onnx_resize_linear.py` rewrites them to linear.

## Pass 2: SAM2 small keyframes carried through the mesh

Since no lighter encoder holds the edge, the next design was SAM2 small on a keyframe with its corrections carried through the mesh depth to the frames between, so the encoder's cost is paid once per interval. The pass measured how far the carry holds on two tabletop recordings, one against a mesh built from another session and one against a mesh built from the same recording, on the recordings' ARKit poses, the ones a live path has. From a keyframe t, frame t+k is given four ways: `hold` (the keyframe's mask as it is), `mesh` (the mesh rendered at t+k, uncorrected), `mask_warp` (the keyframe's mask pixels unprojected through the keyframe's mesh depth and splatted into t+k) and `correction_warp` (the mesh re-rendered at t+k plus the keyframe's added pixels and minus its removed pixels, each carried as 3D points within 64 px of the keyframe's occluding contours, a point dropped where it lands more than 2 cm behind the mesh at t+k). The reprojection is exact: the mesh carried through its own depth lands on the mesh rendered at the target frame. Truth is the segmenter's own image-mode mask of frame t+k, for keyframes every fifth frame, and the hand-marked frames as a second check. The measurements behind this are in the experiments repo's notes, against recordings that are not checked in.

A keyframe is good for one frame against the mesh from another session and two or three against the mesh built from the same recording. The camera's motion is not the cause, since the reprojection is exact and holding the keyframe's mask unchanged measures what the motion alone would cost. The cause is that the corrections are anchored to the mesh and the mesh slides against the image. Measured as the image-mode mask's centroid against the projected mesh's, the mesh sits off the image, less so with the refined poses than the recorded ones, and that offset changes from frame to frame and grows with the interval, which is the carry's decline. The object's image-space velocity explains none of the offset's variance, so it is not a pose-to-frame timestamp delay but the mesh's own misregistration (plan.md's silhouette-shift item) projected differently as the view turns. The mesh built from the recording it is scored on sits closer to its marked frames than the mesh from another session, and its carry lasts longer in the same proportion: the interval is set by the mesh's registration, not by the camera. The refined poses pin the mesh closer to the image on average but jitter more frame to frame, so the carry degrades faster on them past the first frame.

Read against pass 1: carrying a keyframe two frames against the mesh from another session is about what EdgeTAM gives per frame on the whole frame, and EfficientViT-SAM l0 per frame is better than carrying even one. The keyframe design does not buy quality over a lighter per-frame segmenter unless the mesh is registered as well as one built from the same recording.

## Pass 3: re-anchoring the carried silhouette to the video's edges

The slide is smooth and a few pixels per frame, so the last pass asked whether a cheap per-frame image-space alignment of the carried silhouette to the video's edges recovers the keyframe's quality. The alignment is an oriented chamfer: Canny edges (20 and 60 after a 1 px blur, where the segmenter's own silhouette has an edge within 2 px on more of its points than at 40 and 120) binned by gradient orientation into eight bins with one distance transform per bin truncated at 8 px, so a contour point is measured only against edges that run its way and a point with no edge near it neither flattens nor drags the search. The carried mask's occluding outline is searched over integer translations within 16 px (40 for the bare mesh, which sits farther off), refined to sub-pixel, and a shift is rejected unless it gains half a pixel of mean edge distance over no shift. Three scopes: one translation for the whole contour, one per occluding contour run substituted within that run's band, and a chain that carries one frame, re-anchors per run within 8 px, rebuilds the keyframe from the result and carries again. The image-mode mask re-anchored to its own frame almost never moves, so the edge evidence agrees with the segmenter where nothing has moved. The carried, anchored and chained silhouettes were scored against the image-mode mask of frame t+k and against the hand-marked frames. The measurements behind this are in the experiments repo's notes, against recordings that are not checked in.

The whole-contour scope is often rejected and changes nothing where it fires, since the carried contour mixes the object's slide with the desk's much larger one and one translation serves neither. Per run, the gain is small against the mesh from another session and nothing against the mesh built from the same recording at short intervals. The chain adds a little against the mesh from another session and nothing against the other, where it rejected most of its shifts. The hand-marked frames say the same: per run at one frame both stay short of segmenting the frame itself. The bare mesh re-anchored with no segmenter at all, the silhouette alignment plan.md names for the mesh, improves against the mesh from another session with its shifts pinned at the 40 px window's edge, and barely moves against the mesh built from the same recording.

What remains after the best shift is not a translation. The carried silhouette is ragged from the splat, it carries the keyframe's own segmentation errors forward (a white object behind the subject that one keyframe's mask took in), it finds no edge where the frame is motion blurred (the segmenter's silhouette has no edge within 5 px on a blurred frame at any threshold), and the mesh's slide differs along one contour. The proxy truth also flickers (the segmenter's mask carried one frame does not fully overlap the next frame's), which is part of the one-frame residual on both sides.

## Reading

- The quality bar is SAM2 small on every frame. Per frame under load it costs 73 ms at one run and 131 at seven in fp16, against a budget of a few milliseconds on a shared GPU, so near-offline quality live is out of budget.

- A live matte today means a lighter segmenter on every frame on the whole frame, a visible step down from the offline matte, at 25 to 50 ms per frame under load before the decoder is batched. That is the one route left standing.

- SAM2 small keyframes do not help: the carry is limited by the mesh's registration to one or two frames, and image-space re-anchoring by translation does not restore it.

## What would reopen the work

- Batching the runs' prompts into one decoder call. The mask decoder takes a batch of prompts and the export's batch axis is 1. It removes 50 to 60 ms per frame on every encoder and is required by any per-frame route. Unbuilt and unmeasured.

- Registering the mesh to the image. The keyframe carry fails on registration, not reprojection. The silhouette-shift item in plan.md is the same defect, and a fix there reopens the keyframe route, which has not been measured on a well-registered mesh other than one built from the recording it was scored on.

- A local per-point edge snap after the carry, each carried contour point to its own nearest oriented edge a few pixels away, rather than one translation per run. The measured residual points at it. Unmeasured.

- A quiet-GPU rerun of the timings, one command away in the experiments repo (`tools/sam2_dml_timing.py`). Every number above was taken at 53 to 97 percent utilization from other applications.

- Keeping the segmenter's tensors on the device in the editor. `OnnxSession` builds CPU tensors, and IOBinding saves 1.5 to 2.5 ms of an 11 ms decoder call with an fp16 decoder graph saving about the same again. The recorded-take build benefits on its own, and plan.md carries it.

## Open questions

- Whether a coarser live matte is worth shipping at all, against the recorded-take matte and the mesh in the client's holdout. Answerable only from a composite with a live source, a character behind furniture, and the lighter segmenter's edge.

- Whether the live matte runs on frames or on a stride of frames with the mesh's own silhouette between, once the mesh is registered well enough that its silhouette is within the band on every frame.

## References

- Experiments repo `D:\Github\git-MikanXR\ReconstructionExperiments`: `NOTES.md` ("Lighter segmenters for a live matte", "Keyframe reprojection of the matte"), `README.md` (the per-family venvs and their `requirements-<family>.txt`), `out/segmenter_sweep.md`, `out/sam2_timing_lighter/`, `out/keyframe_reproject_recorded/summary.md` (recorded poses), `out/keyframe_reproject_duster/summary.md`, `out/keyframe_reproject/summary.md` (refined poses), with the `summary_carry_only.md` copies from before the re-anchoring rows.
- MikanXR docs: [occlusion-matte.md](../reference/occlusion-matte.md), [occlusion-mesh.md](../reference/occlusion-mesh.md), [compositor.md](../reference/compositor.md) (the frame queue and the matte ring), [videosources.md](../reference/videosources.md) (the live sources and their frame delivery).

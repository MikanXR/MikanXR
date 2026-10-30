# Proposal: A real-time occlusion matte

Measured and tabled. Nothing of it is built. The occlusion matte ([occlusion-matte.md](../reference/occlusion-matte.md)) is built once per recorded take by SAM2 small prompted from the occlusion mesh, and the question was whether the same matte can be produced live, frame by frame, for a live video source. Three measurement passes in the ReconstructionExperiments repo answered it: no lighter segmenter holds SAM2 small's edge, a SAM2 small keyframe carried through the mesh holds one or two frames, and re-anchoring the carried silhouette to the video's edges recovers half a pixel. This document is the record of those passes, the reading, and the routes that would reopen the work. A reader starting from it should not need the conversation that produced it.

## Goal

A live video source (the ARKit stream, a USB camera) composited with a character gets the same silhouette correction a recorded take gets from its matte: the mesh's occluding contours moved onto the picture's edges within a band, so the character is clipped where the real object is and not where the mesh says it is. The scene's occluders are static objects the mesh already covers. The matte runs on the GPU the client renderer shares, inside the two-frame slot the compositor's frame queue gives a worker ([compositor.md](../reference/compositor.md)), so it has a few milliseconds per frame and not the 625 to 1688 ms the offline build spends.

ARKit's own matte generator is not a route: it mattes people from the phone's person segmentation and is silent on furniture.

## What exists today

- The recorded-take path ([occlusion-matte.md](../reference/occlusion-matte.md)): `OcclusionMatteBuilder` rasterizes the meshes to depth per frame, finds the occluding contour runs, prompts SAM2 small with positive points per run, scores the candidates by IoU against the mesh's near side, and writes a label sidecar the file source plays back and the compositor's node keys into the depth mask.

- `Sam2Segmenter` (`src/Editor/Calibration`) runs the encoder and decoder graphs through `OnnxSession` under DirectML, with CPU tensors: the frame's feature maps cross host to device on every decoder call and the encoder's outputs come back to the host once per frame.

- A live source has no CPU copy of its frames in compositor mode. The ARKit source's `VideoFrameDistortionView::readbackDirectColorTexture` is the GPU readback a worker would use, and `IOcclusionMatteSource` is the seam the compositor's matte ring reads from.

- The measurement harness in `D:\Github\git-MikanXR\ReconstructionExperiments`: `tools/occlusion_edge_eval.py` scores a predicted silhouette against hand-marked occluding edges (median and p90 distance in pixels from the marked edge to the predicted boundary, and the share of the marked edge within 5 px), `tools/sam2_edge_eval.py` runs the prompt design through any segmenter family, `tools/segmenter_sweep.py` and `tools/sam2_dml_timing.py` sweep accuracy and cost, and `tools/keyframe_reproject_eval.py` measures the keyframe carry and the re-anchoring. The marked sets are the can, cup, room and duster takes, 24 frames. Every number below is median / p90 / within 5 px unless it says otherwise, and the full tables with their reading are in that repo's `NOTES.md` ("Lighter segmenters for a live matte" and "Keyframe reprojection of the matte").

## Pass 1: lighter segmenters

The question was whether a lighter promptable encoder holds SAM2 small's edge through the unchanged prompt design. The candidates, all Apache 2.0, each in its own venv: EdgeTAM (a RepViT trunk, 4.9 M encoder parameters), EfficientTAM ti and s at 1024 and 512 (6.2 and 22.4 M), EfficientViT-SAM l0 and l1 at 512 and xl0 at 1024 (30.7, 43.6 and 112.9 M, with a SAM1-style decoder), and SAM2 tiny and small at 512. The bar set before the runs: a live candidate's DirectML encoder costs at most 10 ms, and its whole-frame accuracy sits within 1 px median and 3 px p90 of SAM2 small's on every marked set with at least 85 percent of the edge within 5 px. The whole frame is the framing a live path encodes, where the offline build crops around each run.

| model | input | can | cup | room | duster |
|---|---|---|---|---|---|
| SAM2 small, crop (the offline build) | 1024 | 1.3 / 6.3 / 0.94 | 1.0 / 2.7 / 0.98 | 1.4 / 6.0 / 0.91 | 1.7 / 3.7 / 0.94 |
| SAM2 small, whole frame | 1024 | 1.9 / 7.4 / 0.87 | 1.4 / 4.6 / 0.92 | 2.0 / 18.6 / 0.83 | 1.6 / 5.6 / 0.89 |
| SAM2 small, whole frame | 512 | 3.3 / 21.1 / 0.68 | 1.8 / 5.5 / 0.88 | 3.8 / 26.6 / 0.65 | 2.9 / 8.4 / 0.76 |
| EdgeTAM, whole frame | 1024 | 2.8 / 12.2 / 0.80 | 2.5 / 5.2 / 0.86 | 1.9 / 7.6 / 0.85 | 2.1 / 5.8 / 0.86 |
| EfficientTAM ti, whole frame | 1024 | 2.3 / 8.2 / 0.81 | 2.2 / 6.4 / 0.84 | 2.2 / 6.8 / 0.85 | 1.8 / 9.8 / 0.84 |
| EfficientViT-SAM l0, whole frame | 512 | 1.6 / 6.1 / 0.89 | 1.8 / 7.7 / 0.87 | 1.8 / 17.9 / 0.83 | 1.4 / 4.2 / 0.92 |
| EfficientViT-SAM xl0, whole frame | 1024 | 1.4 / 4.8 / 0.93 | 2.9 / 10.7 / 0.76 | 1.8 / 15.3 / 0.83 | 1.4 / 3.7 / 0.92 |

None qualifies. Every 512 configuration degrades, worst on the whole frame, where one encoder pixel is 3.75 frame pixels. EfficientViT-SAM l0 is nearest on accuracy (inside the margin on three sets, the cup's p90 7.7 against 4.6) at 26 to 40 ms of encoder and a second decoder contract. The room's p90 of 18.6 for small itself on the whole frame is one frame of five.

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

The decoder is the larger term past one run. SAM2 small's decoder with 8 points takes 11.1 ms with its three feature tensors copied from the host per call, 9.6 ms bound once as DirectML `OrtValue`s, and 10.0 ms as an fp16 graph. A frame at 1, 4 and 7 runs (encoder plus bound decoder calls) costs 72.8 / 101.7 / 130.6 ms for small fp16 at 1024, 25.7 / 55.3 / 84.8 for tiny fp16 at 512 and 23.1 / 51.7 / 80.4 for EdgeTAM fp16. The seven runs a take frame carries cost about 67 ms of decoder under this load whatever the encoder, so per-frame segmentation needs the runs' prompts batched into one decoder call (the mask decoder takes a batch of prompts, and the current export's batch axis is 1) as well as a lighter encoder, and neither alone fits the budget.

What the pass settled about the graphs, which any live port inherits:

- fp16 halves every encoder under DirectML. `onnxruntime.transformers.float16.convert_float_to_float16` with `keep_io_types` works where `onnxconverter_common` leaves `Cast` nodes that fail type checking. The encoder's positional-embedding `Resize` is a constant chain ORT cannot fold in fp16, and left in place it runs on the CPU at 1.2 s per call, so the fp32 graph is constant-folded offline first (a CPU session at basic optimization writing `optimized_model_filepath`), after which the whole encoder is one DirectML fused node. The converted graph loads on onnxruntime-directml 1.20.1, the editor's version, and fails on 1.24.4 with `HasExternalDataInMemory` unless graph optimization is disabled.
- The fp16 encoder's largest difference in `image_embed` against fp32 is 1.3 to 2.0 percent of the output's range. The fp16 decoder's masks are within 0.2 to 0.4 percent of the fp32 range and its `iou_scores` within 0.5 percent on tiny and 6 percent on small (the matte chooses candidates by IoU against the mesh, not by that score).
- DirectML has no cubic `Resize` kernel. EfficientViT-SAM's neck has three, which ORT ran on the CPU with a host round trip, 432 of the 421 to 484 ms the unpatched graphs took. `tools/onnx_resize_linear.py` rewrites them to linear.

## Pass 2: SAM2 small keyframes carried through the mesh

Since no lighter encoder holds the edge, the next design was SAM2 small on a keyframe with its corrections carried through the mesh depth to the frames between, so the encoder's cost is paid once per interval. The pass measured how far the carry holds, on the can take (`take_20261027_232002` with the `erosion_V2.glb` mesh built from another session, 481 frames) and the duster take (`take_20261029_143651` with the mesh built from itself, 601 frames), on the takes' recorded ARKit poses, the ones a live path has. From a keyframe t, frame t+k is given four ways: `hold` (the keyframe's mask as it is), `mesh` (the mesh rendered at t+k, uncorrected), `mask_warp` (the keyframe's mask pixels unprojected through the keyframe's mesh depth and splatted into t+k) and `correction_warp` (the mesh re-rendered at t+k plus the keyframe's added pixels and minus its removed pixels, each carried as 3D points within 64 px of the keyframe's occluding contours, a point dropped where it lands more than 2 cm behind the mesh at t+k). The reprojection is exact: the mesh carried through its own depth lands within 0 px of the mesh rendered at the target up to five frames and 2 px at thirty. Truth is the segmenter's own image-mode mask of frame t+k, for keyframes every fifth frame:

| k | camera motion | can `hold` | can `mesh` | can `correction_warp` | duster `mesh` | duster `correction_warp` |
|---|---|---|---|---|---|---|
| 1 | 0.8 cm, 0.8 deg | 8.7 / 14.2 / 0.47 | 15.9 / 34.1 / 0.17 | 2.1 / 8.4 / 0.85 | 12.1 / 34.2 / 0.32 | 1.7 / 9.8 / 0.85 |
| 2 | 1.6 cm, 1.6 deg | 16.5 / 26.1 / 0.26 | 16.1 / 35.9 / 0.17 | 3.0 / 12.1 / 0.74 | 12.6 / 36.3 / 0.30 | 2.6 / 14.0 / 0.78 |
| 3 | 2.3 cm, 2.4 deg | 21.7 / 34.0 / 0.17 | 15.9 / 34.6 / 0.17 | 3.6 / 13.6 / 0.67 | 11.9 / 36.7 / 0.32 | 2.8 / 15.0 / 0.76 |
| 5 | 3.8 cm, 4.0 deg | 34.1 / 55.9 / 0.13 | 16.2 / 37.1 / 0.16 | 4.5 / 18.7 / 0.57 | 12.3 / 36.1 / 0.31 | 4.0 / 17.2 / 0.70 |
| 10 | 7.5 cm, 7.8 deg | 60.2 / 100.5 / 0.09 | 16.3 / 36.9 / 0.16 | 6.7 / 23.8 / 0.45 | 12.4 / 36.2 / 0.31 | 5.2 / 21.2 / 0.60 |
| 30 | 21 cm, 25 deg | 279 / 332 / 0.04 | 16.2 / 35.0 / 0.17 | 11.8 / 32.1 / 0.30 | 12.4 / 36.5 / 0.31 | 6.7 / 25.8 / 0.51 |

Against the hand-marked frames, `correction_warp` on the can is 2.4 / 9.7 / 0.81 at one frame, 3.7 / 11.9 / 0.66 at two and 6.1 / 21.4 / 0.42 at five, where those frames' own image-mode mask is 1.0 / 3.8 / 0.94. On the duster it is 2.2 / 4.6 / 0.92 at one, 2.1 / 5.1 / 0.89 at two, 2.6 / 6.5 / 0.83 at three and 3.1 / 9.3 / 0.75 at five, against 1.8 / 3.8 / 0.93 for image mode and 5.5 / 27.3 / 0.51 for the mesh.

A keyframe is good for one frame on the can and two or three on the duster. The camera's motion is not the cause, since the reprojection is exact and the `hold` row shows what the motion alone would cost. The cause is that the corrections are anchored to the mesh and the mesh slides against the image. Measured as the can's image-mode mask centroid against the projected mesh's, the mesh sits 23 px off the image on median (p90 36) with the recorded poses and 14 px with the refined ones, and that offset changes by 1.5 px per frame median (7 px p90), 3.3 px over five frames and 13.6 over thirty, which is the carried rows' decline. The can's image-space velocity explains none of the offset's variance, so it is not a pose-to-frame timestamp delay but the mesh's own misregistration (7 mm by the marked-frame measurement in plan.md's silhouette-shift item) projected differently as the view turns 0.8 degrees per frame. The duster's mesh, built from the take it is scored on, sits 5.5 px off its marked frames against the can mesh's 20, and its carry lasts longer in the same proportion: the interval is set by the mesh's registration, not by the camera. The refined can poses pin the mesh closer to the image on average but jitter four times more frame to frame (the step between consecutive poses changes by 0.13 cm median against 0.03), so the carry degrades faster on them past the first frame.

Read against pass 1: carrying a keyframe two frames on the can (3.0 / 12.1 / 0.74) is what EdgeTAM gives per frame on the whole frame (2.8 / 12.2 / 0.80), and EfficientViT-SAM l0 per frame (1.6 / 6.1 / 0.89) is better than carrying even one. The keyframe design does not buy quality over a lighter per-frame segmenter unless the mesh is registered to the duster's standard.

## Pass 3: re-anchoring the carried silhouette to the video's edges

The slide is smooth and a few pixels per frame, so the last pass asked whether a cheap per-frame image-space alignment of the carried silhouette to the video's edges recovers the keyframe's quality. The alignment is an oriented chamfer: Canny edges (20 and 60 after a 1 px blur, where the segmenter's own silhouette has an edge within 2 px on 74 to 86 percent of its points, against 60 percent at 40 and 120) binned by gradient orientation into eight bins with one distance transform per bin truncated at 8 px, so a contour point is measured only against edges that run its way and a point with no edge near it neither flattens nor drags the search. The carried mask's occluding outline is searched over integer translations within 16 px (40 for the bare mesh, which sits farther off), refined to sub-pixel, and a shift is rejected unless it gains half a pixel of mean edge distance over no shift. Three scopes: one translation for the whole contour, one per occluding contour run substituted within that run's band, and a chain that carries one frame, re-anchors per run within 8 px, rebuilds the keyframe from the result and carries again. The image-mode mask re-anchored to its own frame does not move on 98 to 100 percent of frames, so the edge evidence agrees with the segmenter where nothing has moved. Against the image-mode mask of frame t+k:

| k | can carried | can anchored per run | can chained | duster carried | duster anchored per run | duster chained |
|---|---|---|---|---|---|---|
| 1 | 2.1 / 8.4 / 0.85 | 2.1 / 8.5 / 0.86 | 1.9 / 8.1 / 0.87 | 1.7 / 9.8 / 0.85 | 2.0 / 10.6 / 0.83 | 1.7 / 10.0 / 0.84 |
| 2 | 3.0 / 12.1 / 0.74 | 2.8 / 11.5 / 0.79 | 2.5 / 11.2 / 0.82 | 2.6 / 14.0 / 0.78 | 2.8 / 14.0 / 0.76 | 2.6 / 14.0 / 0.78 |
| 3 | 3.6 / 13.6 / 0.67 | 3.0 / 12.4 / 0.73 | 2.8 / 11.6 / 0.76 | 2.8 / 15.0 / 0.76 | 2.8 / 14.8 / 0.76 | 2.8 / 15.2 / 0.76 |
| 5 | 4.5 / 18.7 / 0.57 | 3.9 / 16.8 / 0.65 | 3.6 / 16.5 / 0.68 | 4.0 / 17.2 / 0.70 | 3.8 / 17.1 / 0.71 | 3.8 / 16.9 / 0.71 |
| 10 | 6.7 / 23.8 / 0.45 | 5.2 / 21.1 / 0.54 | 4.6 / 21.4 / 0.58 | 5.2 / 21.2 / 0.60 | 4.8 / 20.6 / 0.63 | 4.6 / 20.3 / 0.64 |

The whole-contour scope rarely fires (rejected on 41 percent of the can's frames and 76 percent of the duster's) and changes nothing where it does, since the carried contour mixes the object's few pixels of slide with the desk's 18 px and one translation serves neither. Per run, the gain is half a pixel of median and 5 to 10 points of the share within 5 px on the can, and nothing on the duster below k = 5. The chain adds a little on the can and nothing on the duster, where it rejected 69 percent of its shifts. The hand-marked frames say the same: per run at one frame the can is 1.7 / 10.2 / 0.84 and the duster 1.9 / 4.7 / 0.92, against 1.0 / 3.8 / 0.94 and 1.8 / 3.8 / 0.93 for segmenting the frame itself. The bare mesh re-anchored with no segmenter at all, the silhouette alignment plan.md names for the mesh, goes from 16 to 10 px median on the can with its shifts pinned at the 40 px window's edge, and from 12 to 11 on the duster.

What remains after the best shift is not a translation. The carried silhouette is ragged from the splat, it carries the keyframe's own segmentation errors forward (a white object behind the can that one keyframe's mask took in), it finds no edge where the frame is motion blurred (the segmenter's silhouette has no edge within 5 px on a blurred frame at any threshold), and the mesh's slide differs along one contour. The proxy truth also flickers (the segmenter's mask carried one frame overlaps the next frame's at IoU 0.92), which is part of the one-frame residual on both sides.

## Reading

- The quality bar is SAM2 small on every frame. Per frame under load it costs 73 ms at one run and 131 at seven in fp16, against a budget of a few milliseconds on a shared GPU, so near-offline quality live is out of budget.

- A live matte today means a lighter segmenter on every frame at roughly 2 to 3 px median and 6 to 12 px p90 on the whole frame, a visible step down from the offline matte, at 25 to 50 ms per frame under load before the decoder is batched. That is the one route left standing.

- SAM2 small keyframes do not help: the carry is limited by the mesh's registration to one or two frames, and image-space re-anchoring by translation does not restore it.

## What would reopen the work

- Batching the runs' prompts into one decoder call. The mask decoder takes a batch of prompts and the export's batch axis is 1. It removes 50 to 60 ms per frame on every encoder and is required by any per-frame route. Unbuilt and unmeasured.

- Registering the mesh to the image. The keyframe carry fails on registration, not reprojection. The silhouette-shift item in plan.md (the mesh 23 px off the image on the can's recorded poses) is the same defect, and a fix there reopens the keyframe route, which has not been measured on a well-registered mesh other than the duster's.

- A local per-point edge snap after the carry, each carried contour point to its own nearest oriented edge a few pixels away, rather than one translation per run. The measured residual points at it. Unmeasured.

- A quiet-GPU rerun of the timings, one command away in the experiments repo (`tools/sam2_dml_timing.py`). Every number above was taken at 53 to 97 percent utilization from other applications.

- Keeping the segmenter's tensors on the device in the editor. `OnnxSession` builds CPU tensors, and IOBinding saves 1.5 to 2.5 ms of an 11 ms decoder call with an fp16 decoder graph saving about the same again. The recorded-take build benefits on its own, and plan.md carries it.

## Open questions

- Whether a coarser live matte is worth shipping at all, against the recorded-take matte and the mesh in the client's holdout. Answerable only from a composite with a live source, a character behind furniture, and the lighter segmenter's edge.

- Whether the live matte runs on frames or on a stride of frames with the mesh's own silhouette between, once the mesh is registered well enough that its silhouette is within the band on every frame.

## References

- Experiments repo `D:\Github\git-MikanXR\ReconstructionExperiments`: `NOTES.md` ("Lighter segmenters for a live matte", "Keyframe reprojection of the matte"), `README.md` (the per-family venvs and their `requirements-<family>.txt`), `out/segmenter_sweep.md`, `out/sam2_timing_lighter/`, `out/keyframe_reproject_recorded/summary.md` (the can, recorded poses), `out/keyframe_reproject_duster/summary.md`, `out/keyframe_reproject/summary.md` (the can, refined poses), with the `summary_carry_only.md` copies from before the re-anchoring rows.
- MikanXR docs: [occlusion-matte.md](../reference/occlusion-matte.md), [occlusion-mesh.md](../reference/occlusion-mesh.md), [compositor.md](../reference/compositor.md) (the frame queue and the matte ring), [videosources.md](../reference/videosources.md) (the live sources and their frame delivery).

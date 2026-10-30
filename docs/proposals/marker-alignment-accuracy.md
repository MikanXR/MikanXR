# Marker alignment accuracy

## Problem

The marker alignment is the largest error left between an occlusion mesh and the camera that plays against it from another session. Three tabletop takes of the same table were recorded in separate ARKit sessions on 2026-10-29: A (`take_20261029_143651`), B (`take_20261029_151637`), and C (`take_20261029_155059`). Each was aligned only through its own marker shot. Laying one take's mesh onto another take's LiDAR depth then needs a rigid correction of 5.0 mm on average over the six pairs, 4.1 mm of it along the table normal. The median depth error before that correction is 4.8 mm, against 2.6 mm after it. At half a metre, 4 mm is about 12 px across a 1920 px frame, and a composited character's feet sit on the table exactly where that vertical error shows.

## How the alignment is solved now

- A take: the marker shot is one frame, the first ARKit frame after the shutter press, with that frame's pose and intrinsics (`StreamingSession.capturePhoto` on the phone). The editor finds the 4 marker corners, solves them with `cv::SOLVEPNP_AP3P` (`computeOpenCVCameraRelativePatternTransform`), and converts the result to a world-to-stage offset with the shot's pose (`CaptureMarkerAlignment`).
- A live session: the same corner solve on 12 consecutive detections at 4 Hz, averaged (`ARKitMarkerAutoAligner`).

## What the measurements found

The scripts and outputs are in `D:\tmp\refine_eval`, to move into the experiments repo.

The single shot is accurate in its own world. Each shot agrees with the mean of the still frames around it within 0.4 mm and 0.4 degrees. The marker size is right: the print measures 47 mm with calipers.

ARKit's world moves over a take, and that is the error that matters. The frame's own LiDAR depth places the table, at the marker, 5.7 mm (B) and 3.8 mm (C) below the mesh's table at the start of the take. The marker solve agrees with that same frame's depth within 1.3 mm. The mesh is fused from the whole take, so it lives in the world averaged over the take, while the marker shot catches the world at one instant. The table's height from each frame's own depth wanders about 5 mm over a take (std 1.9 to 2.4 mm), with no steady trend.

The marker solve gets noisy with distance. Within each frame, the marker's height against the frame's own depth around it has a spread of:

- about 1 to 1.5 mm under 0.45 m
- 4 to 5 mm at 0.45 to 0.6 m
- 7 to 11 mm beyond 0.6 m

Viewing angle matters much less than distance. Averaging the marker over every movie frame halves the cross-session error, but mostly by averaging the world over time while taking in noisy far frames.

The solver matters at close range. At 0.43 m the AP3P and `SOLVEPNP_IPPE_SQUARE` solves agree within about 1 mm. At 0.21 m, AP3P puts the marker 2.5 mm deeper, with its normal 1.7 degrees off the table against 0.57 degrees for IPPE. That close shot also had a reprojection error of 1.5 to 1.9 px against 0.3 px elsewhere, which the measurements did not explain.

Snapping the shot's height onto the take's averaged table is the best alignment tested. Means over the six pairs:

| Alignment | Error before the fit | Along the normal | In-plane | Tilt |
|---|---|---|---|---|
| Marker shots, today | 4.84 mm | 4.08 mm | 2.91 mm | 0.29 deg |
| Mean of every movie frame | 3.19 mm | 0.85 mm | 2.61 mm | 0.35 deg |
| Shot snapped to the mesh table, height and tilt | 3.30 mm | 1.17 mm | 3.01 mm | 0.51 deg |
| Shot snapped to the mesh table, height only | 3.04 mm | 0.86 mm | 2.89 mm | 0.29 deg |

The error after the fit is 2.6 mm under every alignment, so the height-only snap leaves 0.4 mm above that floor. Taking the tilt from the mesh made tilt worse: a table plane fitted to a 10 cm patch of a 1.5 cm mesh has a noisier normal than the marker's own, and a tilt error levers into height tens of centimetres from the marker. The in-plane offset of about 3 mm does not change with any alignment, and the mean of the near movie frames leaves it too, so it is not tied to the instant of the shot. On a mostly flat tabletop the rigid fit constrains in-plane motion weakly, so part of that number is the measurement itself.

## Plan

1. Height snap. Solve the shot as today, then move the stage origin along the solved marker normal onto the take's time-averaged table height at the marker. The shot keeps its tilt, in-plane position, and heading.
	- The averaged height comes from the take's depth records, not from a mesh, so it is known before any build: for every frame whose depth sees the marker's world point, fit the table in an annulus around that point with the frame's own pose, the way the measurement did, and take a robust mean over the take. The measurement's plane fits used about 1300 to 2700 confident points per frame with a 0.8 mm rms, against a marker solve that degrades past 0.45 m, so the depth carries the height rather than the marker's movie-frame solves.
	- It runs where the shot solve runs, in `CaptureMarkerAlignment` from `FileVideoSourceSystem`, once per media open and offset, on a worker thread. The unsnapped offset applies until the snap lands, so Align Camera's verify view never waits, and the occlusion build refuses to start while a snap is pending. Reading every depth record took under a second on the three takes.
	- A take with no depth members, or whose depth never sees the marker, keeps the unsnapped shot and logs why.
	- A manual Align Camera result on a take gets the same snap, since the error it carries is the same.
2. Solver. Switch the 4-corner marker case of `computeOpenCVCameraRelativePatternTransform` from AP3P to `SOLVEPNP_IPPE_SQUARE`, and refuse a solve whose two IPPE solutions are close (an error ratio under 2). This also changes the live alignment and the other align stages, so it lands as its own change, measured against the same three takes and a live session.
3. Capture guidance on the phone: the marker shot is taken under 0.45 m from the marker, with the phone held still, and the phone shows the distance while framing the shot.

## Height snap result

The editor's height snap landed with `MikanCmd -alignTake` and was scored on the six pairs. Its table heights match the simulation's: applied to the square-marker shots they give 3.19 mm before the fit, against 3.04 mm simulated. Applied to the editor's own AP3P shots they give 4.02 mm, from 5.38 mm unsnapped. The gap is AP3P's tilt on take C's close shot, 1.37 degrees off the square-marker solve, which the snap passes through because it only translates. On the A and B pairs, where the two solvers agree, the editor's snapped result (3.41 and 3.38 mm) matches the simulation (2.96 and 3.49 mm). So the solver switch in part 2 is what carries the snap to its full gain.

## Live alignment

A live session has the same problem: the live world wanders while the alignment is solved once. Following the wander means re-solving the alignment while the marker is visible, or an ARKit image anchor that ARKit moves with its own world corrections. Either way the stage moves during a session, which needs smoothing so it never jumps and a rule for when the marker is out of view. That behaviour change is decided separately, after the take work lands.

## Options not taken

- Several frames at the shot, or depth recorded with the shot: both still freeze one instant of the world, so neither closes the gap to the take's averaged world.
- Aligning from the mean of the movie frames: it needs the marker in view during the take and filtering by distance, and the height snap reaches the same error with neither.
- Registering the shot's world to the mesh in all six degrees of freedom: the in-plane error it would remove is about the size the measurement cannot resolve on a tabletop. It stays a candidate if a scene with more structure shows a real in-plane offset.

## Testing

- Synthetic: the `capture_marker_alignment` module gains a take whose depth records place the table at a known height that differs from the marker shot's, and the snapped offset recovers it. A second case covers a take without depth, which keeps the shot.
- The solver: synthetic marker shots at 0.2 to 0.8 m with corner noise, where the IPPE solve's error is no larger than AP3P's at every distance, plus an ambiguous shot that is refused.
- End to end: the error before the fit, rerun on takes A, B, and C after each change, with the height snap expected at about 3.0 mm.

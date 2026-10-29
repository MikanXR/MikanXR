# Marker alignment accuracy

## Problem

The marker alignment is the largest error left between an occlusion mesh and the camera that plays against it from another session. Two tabletop takes of the same table, recorded in separate ARKit sessions and aligned only through their marker shots, measured on 2026-10-29:

- the rigid fit that best lays one take's mesh onto the other take's LiDAR depth is 7.6 mm and 0.39 degrees (A onto B) and 7.1 mm and 0.10 degrees (B onto A)
- take B's marker solve puts the stage origin 7.8 mm below the table surface its own LiDAR mesh measures, while take A's is 0.2 mm off
- with that fit applied, the cross-session median depth error drops from about 8.6 mm to 3.6 to 4.7 mm

So the error belongs to the individual take's alignment and varies from take to take. At half a metre, 8 mm is about 25 px across a 1920 px frame. A live phone session inherits the same failure modes through `ARKitMarkerAutoAligner`, which averages 12 detections taken from roughly one viewpoint.

## How the alignment is solved now

- A take: the marker shot is one frame, the first ARKit frame after the shutter press, with that frame's pose and intrinsics (`StreamingSession.capturePhoto` on the phone). The editor finds the 4 marker corners and solves them with `cv::SOLVEPNP_AP3P` (`computeOpenCVCameraRelativePatternTransform`), then converts the result to a world-to-stage offset with the shot's pose.
- A live session: the same corner solve on 12 consecutive detections at 4 Hz, averaged.

## Candidate causes

1. The pose of a small marker from one view. A 47 mm marker at 0.43 m spans about 130 px. Its position along the view ray and its tilt are the weakly constrained directions of a planar 4-point solve, and a tilt error grows with distance from the marker: 0.4 degrees is 3.5 mm half a metre away.
2. The solver. AP3P uses 3 of the 4 corners to solve and the fourth to pick a solution. `SOLVEPNP_IPPE_SQUARE` is the solver built for a square marker, and it also reports how ambiguous the shot is (the ratio of the two candidate solutions' errors).
3. The shot itself: motion blur at the shutter press, or a view close to head-on, where tilt is least observable.
4. ARKit's world moving after the shot. The marker shot is the earliest pose of the take. If ARKit corrects its world while the take records, the movie frames' world no longer matches the one the shot was solved in.
5. The physical marker: the printed side differing from the 47 mm the project says, paper that does not lie flat, or a glossy print.
6. Intrinsics: an error in ARKit's focal length for the shot scales the solved distance.

## Measure first

None of these needs code in the repo, and together they say which cause dominates.

- M1, the marker against the take's own surface: the distance from the solved stage origin to the fused table surface, and the angle between the solved marker normal and the surface normal. It is free on every take with depth, and it is the per-take quality number the build stage could later show.
- M2, the marker through the movie: a take that keeps the marker in view for its first few seconds, held still and then moved around the marker. Solving the marker in every frame with that frame's pose gives the spread from frame to frame (causes 1 to 3) and any trend over time (cause 4), both compared against the shot.
- M3, cross-session: the rigid fit size between two takes' meshes after marker alignment, as measured above. It is the end-to-end number every fix is judged by. The scoring scripts are in `D:\tmp\refine_eval` and would move into the experiments repo.
- The physical marker: measure its printed side with calipers, and check that it lies flat.

## Options

To be picked once the measurements say what dominates:

- A, the square-marker solver: `SOLVEPNP_IPPE_SQUARE` for the 4-corner marker case, with a shot refused when its reprojection error is high or its two solutions are close (causes 1 and 2). This is a small change in `CameraMath`, and it also affects the live alignment and the other align stages, so it needs measuring on both.
- B, a bigger target: a larger marker, or a ChArUco board (the finder family already supports one). More corners spread over a larger area constrain tilt and remove the planar ambiguity (cause 1). This is the cheapest change and needs no code beyond choosing the board.
- C, several frames: the phone records a short burst of frames with their poses while the operator moves a little around the marker, and the editor averages them through `FrameCoupledAlignment`, as the live alignment does (causes 1 and 3). The views from different angles constrain tilt. This changes the capture format: the marker members would hold several frames.
- D, depth on the shot: the phone records LiDAR depth for the marker shot. The editor fits the marker plane from depth and solves only the in-plane position and rotation from the corners (causes 1 and 6). This takes a depth member for the marker shot.
- E, align from the movie: solve the marker in every movie frame where it is visible and average the results. The alignment then lives in the same world as the frames the mesh is fused from (cause 4), and the separate marker step could become optional.
- F, an ARKit image anchor: register the marker as an `ARReferenceImage` so ARKit tracks and refines its pose itself, and send the anchor's pose with the take and the live stream. This is the strongest option for live sessions and the largest change on the phone.
- G, operator guidance on the phone: show how large the marker appears, the viewing angle (30 to 60 degrees off the marker's normal is best), and motion at the shutter press, and refuse a shot outside those limits (cause 3).

## Better captures now, with no code

- Use a bigger marker on a rigid, flat, matte backing, and measure its printed size.
- Take the marker shot from 30 to 60 degrees off the marker's normal, not head-on, with the marker filling a fifth or more of the frame width.
- Hold the phone still for the shutter press.
- Keep the marker in view for the first seconds of the take, which also gives M2 its data.

## Recommendation

Run M1, M2, and the caliper check on one new take, captured as described above. If the spread from frame to frame dominates, A plus B is the cheap fix, with C or D next. If a trend over time dominates, E is. F stays the path for live sessions in either case.

## Testing

- Synthetic: extend the `capture_marker_alignment` test module with corner noise and viewing angle, and bound the solved error for each solver.
- Per take: M1 on every build, against a threshold.
- End to end: M3 on a pair of takes from separate sessions, before and after each change.

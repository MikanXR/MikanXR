# Proposal: Occlusion capture coverage feedback

Editor parts (1 to 4) are in `main`, and the duster take has run through the headless build. Phone parts (5 and 6) were built in MikanARStreamer, measured wrong on a real take, and replaced by the live fusion of [phone-live-fusion.md](./phone-live-fusion.md); what remains of them here is the measurement that retired them. The parity check and the tuning (7) remain, tracked in [plan.md](../plan.md). This document is the record of the editor's coverage view: the evidence that motivates it, the metric, the pieces as built, and the questions still open. A reader starting from it should not need the conversation that produced it.

## As built

The editor parts are in `main`:

- `src/Editor/Reconstruction/DepthProjection.h/.cpp`: the shared vertex-to-depth-pixel projection, used by `MeshTexturing` too.
- `src/Editor/Reconstruction/MeshCoverage.h/.cpp`: the metric.
- `src/Editor/Reconstruction/NpyWriter.h/.cpp`: the `.npy` writer.
- `OcclusionMeshBuilder`: the coverage phase, the bounds box override, and the new stats. `MeshTexturing::textureMesh`: the source vertex remap. `CmdApp.cpp`: `-coverage` and `-bounds`.
- `src/Editor/AppStages/OcclusionMeshBuild/OcclusionMeshPreview.h/.cpp`: the solid preview and its views. `AppStage_OcclusionMeshBuild`: framing, the camera path, the bounds box handles, and the automation commands. `GuiPanel_OcclusionMeshBuild`: the view combo, the legend, the coverage summary, the voxel warning, and the bounds box controls.
- Tests: the `mesh_coverage` module, a remap check in `mesh_texturing`, and coverage plus bounds box checks in `occlusion_mesh`.

[occlusion-mesh.md](../reference/occlusion-mesh.md) ("Coverage", "The stage", "The headless command", "Tests"), [automation.md](../reference/automation.md) and [commands.md](../reference/commands.md) describe the work as built.

### Where it differs from the parts below

- The fusion's cull is the silhouette cull (`TsdfVolume::cullFlyingPixels` with `gradientCull`, `gradientCullKeepsNear`, `gradientCullRadius`): at a depth jump of more than 8 percent against a 4-neighbour the near pixel is kept, and every pixel farther than the nearest depth within 2 pixels is dropped. The coverage pass applies exactly that, plus the fusion's depth range. The rule arrived from `occlusion-upsample` after the coverage pass was built, which had applied the older cull that dropped both pixels of a pair.

- A frame whose camera lies on the far side of the vertex's surface (cosine between the normal and the direction to the camera at or below 0) counts as neither support nor carve. The metric section now says so. The parity check needs `tools/surface_coverage.py` to apply the same rule.

- `MeshCoverage` is an accumulator rather than a function taking the reader. The builder's `OcclusionMeshBuilder::measureCoverage` reads each fused keyframe's depth in turn, builds a `CoverageView` (fused pose in OpenCV axes, depth intrinsics, depth, confidence, keep mask, depth range), and folds it in with `accumulateView`. `finish` classifies and computes the area shares.
	- Counts saturate at 65535.
	- `bestCos` and `nearest` are 0 where nothing supports the vertex.
	- Each pixel is read at the vertex's rounded projection, with no neighbourhood.

- The overlays use the one texcoord slot. A view switch rebuilds the vertex buffer with that view's texcoords, rather than keeping a second texcoord set or a second mesh.
	- Sequential overlays sample a 256 by 1 ramp from red (poor) through yellow to green (good). Texel 0 is grey, for vertices the measure has no value for.
	- The ramp's full-green ends are constants in `OcclusionMeshPreview.cpp`: 16 supporting frames, 9 directions, and a nearest camera at 0 m against 3 m for full red.
	- Contradiction and Distance run inverted, so green is always good.
	- Textures filter linearly. No filter API was added.

- The textured view uploads the atlas converted to RGBA and flipped vertically, since `Internal_PNT_TexturedLitColored` samples V as given while the mesh's texcoords are V-up.

- Framing aims the orbit at the centre of the mesh's vertex bounds, at a distance that fits their diagonal in the narrower field of view. The view looks along the fused cameras' mean forward, with the pitch held at 20 degrees or more so the tops of things read.

- The camera path is drawn through the line renderer, which draws with depth testing off, so the path shows through the mesh.

- The bounds box starts on the bounds the build used. Dragging one of its six face handles with the left mouse button moves that face along its axis, and a face stops 5 cm short of the opposite one. The right button still orbits. Each pushed stage has its own input binding set, so a left click does not reach the paused Project viewport behind the stage. "Rebuild In Box" hands the box to the panel's build settings and returns to the settings page, which shows it with a "Use Automatic Bounds" reset.

- MikanCmd takes the box too, as `-bounds=lo.x,lo.y,lo.z,hi.x,hi.y,hi.z`, and warns when the voxel grew.

- `set_option view` is the one option accepted after a build. `set_option bounds` is accepted only before one, like the other options. `get_summary` also reports `bounds_overridden`, `voxel_grown` and the current `view`.

### Remaining

The headless build has run on the duster take. The parity check against `tools/surface_coverage.py`, the acceptance test for the metric, and a drive of the stage's views have not run:

1. Run the headless build on the cup take at the tabletop preset and write the coverage:
	- `MikanCmd.exe -occlusionMesh -take=<take_20261028_131840.trackcamera> -voxel=0.008 -glb=<out>/cup.glb -coverage=<out>/coverage`
	- The output prints the class shares and warns if the voxel grew.

2. Parity check in the experiments repo:
	- Load `class.npy` and the other arrays with `numpy.load`. They are indexed by the vertices of `cup.glb`.
	- Run `tools/surface_coverage.py` on the same mesh. A small comparison script there may be needed, if the tool does not take an external mesh and arrays.
	- Before comparing, confirm the tool applies the silhouette cull, the depth range and the camera-behind rule above.
	- The bar is at least 95% of the cup's area in the same class.
	- A mismatch is a bug in one of the two until shown otherwise. The thresholds are not tuned to make it pass.

3. Drive the stage on that take with `tools/automate.py` and look at the screenshots:
	- Enter the stage with `function invoke FileVideoSourceSystem <componentId> build_occlusion_mesh`.
	- `stage set_option voxel_preset tabletop`, then `stage build`, then wait for `verifyMesh`.
	- `stage get_summary` for the coverage shares.
	- `stage set_option view <name>` for each view, each with `screenshot window`.
	- `stage frame_mesh`.
	- What to look for:
		- The cup's 150 to 270 degree side is weak or red in the Coverage view.
		- The textured view is the right way up.
		- The mesh faces the camera on arrival.
	- Then drag the box by hand, or use `stage cancel` and `stage set_option bounds ...`, rebuild, and confirm `get_summary` reports `voxel_size` 0.008 and `voxel_grown 0`.

4. Fix what those turn up.

The checklist in [plan.md](../plan.md) ("Occlusion coverage feedback") mirrors steps 2 to 4.

### The phone's coverage grid, retired

Parts 5 and 6 landed in MikanARStreamer ahead of the parity check: `CoverageGrid`, a sparse two-level voxel grid in ARKit world judging support and carve at each voxel's surface point with the shared metric, `CoverageOverlay`, SceneKit points over the camera view (good green, weak red, only what the newest depth frame sees), and a HUD line with the shares and an arrow toward the nearest weak cluster. On the iPhone 12 Pro an update took 14 ms at 10 Hz at about 27,000 voxels and the recorder dropped no frames. The measurement below retired it, and the live fusion removed the grid, its settings, its tests and the `coverage` debug command.

### The phone against the editor on a real take

`take_20261028_211829` walks all the way around a compressed air duster, a metal can about 6.5 cm across and 24 cm tall, on a desk: 72 s, every 30 degree sector around the can visited, the camera at a median 57 degrees above horizontal and 0.6 m away. The operator saw the can stay mostly red. Two measures of the same take:

- The phone's `CoverageGrid`, replayed offline over every third frame (the live rate) by a macOS build of the same source: 35% of the can's side voxels good, 45% at the editor's thresholds. The leading failure is carve, with the median carve share between 0.4 and 0.8 depending on height. There are about twice as many voxels on the can as the editor has vertices, and the desk around it carries many voxels with one or two supporting frames carved nearly every time after, specks a single frame created in empty space.

- `MikanCmd -occlusionMesh -voxel=0.008 -coverage` on the same take, boxed to the can: 94% of the can's vertices good. Every vertex has at least 8 supporting frames, the median carve share is 0.11, and the median best viewing angle is 0.87. Registration's final correction was 7 mm.

So the walk was good and the phone's red was wrong, by far more than the margin. The phone's thresholds cost 10 of the points, and the rest comes from the voxel representation: a voxel's surface point is fixed by the single frame that created it, so one frame's depth noise leaves it off the surface that later frames, and the editor's fusion, average to. Replays isolating the causes:

- Dropping a voxel once it has been carved at least 3 times and more than twice as often as supported (the space was seen through, so the editor's fusion would hold no surface there) lifts the can to 56% good, 72% at the editor's thresholds. The top half of the can reaches 78 to 95%. The bottom 8 cm, where it meets the desk, stays mostly carved.

- Pulling each voxel's point toward the measured surface on every supporting frame adds about 5 points alone and nothing on top of the dropping.

- Replaying with the editor's refined poses instead of ARKit's raw ones does not help, so pose drift is not the cause.

[phone-live-fusion.md](./phone-live-fusion.md) replaces the points with the editor's fusion run live on the phone.

## Goal

An occlusion mesh is only as good as the take it is fused from, and today nothing tells the operator whether a take covered the scene well enough until a character walks behind a lumpy object in the composite. Two things fix that:

- After a build, the editor shows the mesh properly (centred, solid, textured) and lets the operator switch to overlays that colour every surface by how well the take observed it, so a bad side is obvious and the operator knows what to recapture.

- While recording, the phone shows what the take has captured so far, so the operator walks to the weak side before stopping the take. This half is now the live fusion mesh of [phone-live-fusion.md](./phone-live-fusion.md).

## Evidence

Measured in the ReconstructionExperiments repo (`D:\Github\git-MikanXR\ReconstructionExperiments`, `NOTES.md` and `tools/surface_coverage.py`) on `take_20261028_131840`, a 41 s walk around a table with a coffee cup on it, fused by `MikanCmd -occlusionMesh` at the tabletop preset. Every cup vertex was projected into each of the 398 fused keyframes at its refined pose and compared with that frame's own LiDAR depth:

- A frame supports a vertex when the measured depth at its pixel agrees with it within 1.5 cm, carves it when the measured depth lies farther (the frame says the space is empty), and is hidden from it when the measured depth lies nearer.

- The cup's lumpy side (vertices facing 150 to 270 degrees around its axis) had 17 to 24 supporting frames against 70 to 96 on the good side, a best viewing angle of 0.58 to 0.70 (cosine between normal and view) against 0.85 to 0.91, and more carving frames than supporting ones.

- The cause was the walk, not the sensor. Of the 49 keyframes taken from the 150 to 240 degree side only 4 had the cup in view, and nobody stood at 240 to 270 degrees, so that side was only ever seen at a glancing angle from the neighbouring sides, where rays that skim past the cup to the table carve it.

- A secondary cause: the walk made the reconstruction volume large, so the grid's voxel grew from 8 mm to 1.0 cm to fit `maxVoxels`, which faceted the cup visibly.

So the signal that separates a well-captured surface from a poorly captured one is a handful of per-surface numbers that both the editor and the phone can compute: how many frames saw it head-on, from how many directions, and how many frames contradicted it.

## The shared metric

The editor computes one per-surface record per mesh vertex after the fusion, at the refined poses. The phone computed the same record per voxel of a coarse grid while recording, at ARKit's raw poses, until the live fusion replaced the grid. It would compute it again only to colour the live mesh (that proposal's Part 4), with the one-way promise that the phone is the stricter side, so a surface shown green there is green in the editor's build.

- `support`: frames whose depth at the surface point's pixel agrees with it within the support band (1.5 cm, or two voxels when the voxel is larger), confidence at least 1, inside the fusion's depth range, and kept by the fusion's flying pixel cull (a pixel whose depth jumps by more than 8 percent against a 4-neighbour is dropped, [occlusion-mesh.md](../reference/occlusion-mesh.md)). A frame whose camera lies behind the surface counts nothing.

- `supportHigh`: the same at confidence 2.

- `carve`: frames whose kept, confident depth at the pixel lies farther than the point by more than the band.

- `directions`: how many distinct directions the supporting frames saw the point from. Directions are binned around the surface normal: one frontal bin (within 30 degrees of the normal) and eight azimuth bins around it, nine in all.

- `bestCos`: the largest cosine between the normal and the direction to a supporting camera.

- `nearest`: the smallest distance to a supporting camera, in metres.

From those, one coverage class per point:

- `good`: `support >= 8`, `directions >= 3`, `bestCos >= 0.7`, and `carve` at most a third of `support + carve`
- `weak`: seen (`support >= 1`) but failing any of the above
- `unseen`: `support == 0`

The thresholds are first guesses fitted to the cup take (its good side clears all four, its lumpy side fails `bestCos` and the carve share). The class uses these four measures only. `supportHigh` and `nearest` are shown as overlays but do not enter the class, and Part 7's tuning revisits that if either turns out to predict surface error.

The thresholds live in `MeshCoverageSettings` so tuning is a constant change.

## What exists today

The editor's pieces, which coverage colouring on the live mesh would start from:

- `MeshCoverage::accumulateView` (`src/Editor/Reconstruction/MeshCoverage.cpp`) is the per-frame rule: projection, the usable-pixel test, support, carve, or hidden, the camera-behind rule, and the direction bins.

- `MeshCoverage::classify` is the class rule, and `MeshCoverageSettings` holds the thresholds.

- The `mesh_coverage` tests (`src/Editor/Reconstruction/Test/MeshCoverageTests.cpp`) are synthetic plane cases, the check that a port of the rule agrees with it.

## The pieces

### Part 1: a real mesh preview, centred (editor, implemented)

- Replace the point preview with the fused mesh drawn solid. When the stage consumes a build result (`consumeBuildOutput`, UI thread, GL context current) it builds a GPU mesh from `m_result.mesh` (positions, normals, and texcoords when textured) with an `IMkVertexDefinition` for position, normal, texcoord and `createMkTriangulatedMesh`, bound to `Internal_PNT_TexturedLitColored`. The atlas becomes an `IMkTexture`. A room mesh is about a million triangles and draws without trouble. The mesh is released on redo, cancel and exit.

- Centre the view on the mesh as soon as the result arrives: orbit target at the centre of the mesh's world bounds (through the same `stage world * pose offset`), radius from the bounds' diagonal so the whole mesh fits, and the initial yaw and pitch looking along the take's average viewing direction, which is how the operator saw the scene. A "Frame mesh" button in the panel re-applies it.

- An optional "Camera path" checkbox draws the take's fused camera positions as a polyline through the line renderer. It is what ties a weak region to where the operator walked, the single most useful thing in the cup diagnosis.

### Part 2: the coverage pass (editor, implemented)

- New module `MeshCoverage` in `src/Editor/Reconstruction`, pure data in and out like `MeshTexturing`: the `TriangleMesh`, the per-frame poses the fusion used (refined when registration ran), the open `DepthTrackReader`, the fused keyframe list, the `TsdfVolumeConfig` (for the cull and the voxel size), and a `MeshCoverageSettings` holding the band and the class thresholds. Output: per-vertex arrays for the six measures of the shared metric and the class.

- Factor the vertex-to-depth-pixel projection that `MeshTexturing::isVertexSeen` does into a small shared helper so the two modules project identically, rather than writing it a second time.

- The pass runs on every build as a new builder phase, `eOcclusionMeshBuildPhase::coverage`, after cleanup and before texturing, with progress per keyframe and cooperative cancellation like the other phases. There is no setting to turn it off. It scores every fused keyframe, with no stride and no time cap: its work is vertices times keyframes (a room mesh at 400 keyframes is a few hundred million projections), split over vertices with `parallelFor`, and accuracy takes priority over build time.

- The pass reads each keyframe's depth through `DepthTrackReader` itself. It does not share a loop with the texture candidate scoring in `collectTextureViews`, which reads the same depth again. The two phases stay independent at the cost of one extra depth decode per keyframe.

- Texturing splits vertices per (vertex, tile) along atlas seams, so the coverage arrays computed on the clean mesh would no longer line up with the textured mesh. `MeshTexturing::textureMesh` gains an output mapping each new vertex to its source vertex, and the builder expands the coverage arrays through it. Coverage still runs once, on the clean mesh.

- `OcclusionMeshBuildResult` gains the coverage arrays, and `OcclusionMeshBuildStats` gains the area share of each class.

- `MikanCmd -occlusionMesh` gains `-coverage=<path>`, a folder receiving one raw `.npy` per measure (`support.npy`, `support_high.npy`, `carve.npy`, `directions.npy`, `best_cos.npy`, `nearest.npy`, `class.npy`), indexed by the vertices of the mesh file the same command writes. `numpy.load` reads each one directly, so the experiments repo can check the editor's numbers against `tools/surface_coverage.py` on the cup take. That parity check is the acceptance test for the metric: it passes when at least 95% of the cup's surface area gets the same class from both.

### Part 3: the view combo and the overlays (editor, implemented)

- A "View" combo box in the panel's result section selects what the mesh shows:
	- Textured (when the build textured the mesh)
	- Shaded (neutral grey, lit, the default for an untextured build)
	- Coverage (the class: green good, yellow weak, red unseen)
	- Observations (`support`)
	- Directions (`directions`, 0 to 9)
	- Viewing angle (`bestCos`)
	- Contradiction (the carve share, `carve / (support + carve)`)
	- Distance (`nearest`)

- Every overlay is the same mesh and the same material. The metric is written into a second texcoord set (or the same texcoord slot of a second GPU mesh built once per build), and the diffuse texture is a small colour ramp (a 256 by 1 texture: a sequential ramp for counts, a three-colour table for the class). The lighting stays, so the overlay still reads as a surface, and no new shader is needed. Switching views swaps the texture and the vertex buffer, not the geometry. In the Coverage view a triangle whose corners are green and red interpolates its texcoord through yellow, so a class boundary draws a narrow false weak band. At mesh resolution that band is narrow, and it is accepted rather than paying for a per-triangle overlay mesh.

- A legend under the combo names the colour range of the current view with its numeric ends, and the summary lists the share of surface area in each coverage class. The coverage view is the default after a build that ran the coverage pass, since it is what tells the operator whether to recapture.

- Automation: `set_option view <textured|shaded|coverage|observations|directions|angle|contradiction|distance>`, `frame_mesh`, and `get_summary` gains `coverage_good`, `coverage_weak` and `coverage_unseen` area shares. `screenshot window` then captures the view for a drive.

- Docs: [occlusion-mesh.md](../reference/occlusion-mesh.md) gains a coverage section (the metric, the thresholds, the phase) and its stage section the view combo. [automation.md](../reference/automation.md) gains the commands.

### Part 4: an operator bounds box (editor, implemented)

The cup take's voxel grew from 8 mm to 1.0 cm because `computeBounds` takes the bounds from every keyframe's confident depth, so a long walk enlarged the volume and the voxel grew to fit `maxVoxels`. The coverage view is where an operator notices that, so the fix lands with this work.

- An optional bounds box in the build settings. When set, it replaces the computed bounds for the volume. When unset, the automatic bounds stay the default.

- The operator places it with a box gizmo over the centred mesh preview from Part 1: build once with automatic bounds, drag the box around the subject, rebuild. The stage keeps the box across rebuilds.

- Automation: `set_option bounds <lo.x> <lo.y> <lo.z> <hi.x> <hi.y> <hi.z>` and `set_option bounds auto`, and `get_summary` reports the bounds the build used.

- The summary flags a build whose `voxelSize` came out larger than the preset's, so the operator knows when to place a box.

### Parts 5 and 6: the coverage grid and overlay on the phone (retired)

A sparse two-level voxel grid in ARKit world, updated at 10 Hz from the take's depth with the shared metric per voxel, drawn as class-coloured points over the camera view with a HUD line and an arrow toward the nearest weak cluster. Built as described under "The phone's coverage grid, retired", measured wrong on a real take, and replaced by [phone-live-fusion.md](./phone-live-fusion.md).

### Part 7: tuning and verification

- Once the parity check passes, tune the editor's thresholds on the cup and duster takes so that the coverage class predicts the surface error the experiments repo measures against hand-marked silhouettes (`tools/occlusion_edge_eval.py`, `tools/score_mesh_variants.py`). Record the result in the experiments repo's `NOTES.md` and the chosen values in [occlusion-mesh.md](../reference/occlusion-mesh.md).

- A take recorded with the live mesh on, walking until the subject reads whole, must build good all round in the editor, and a take that skips a side must come out weak or red there.

## Sequencing

The editor parts (1 to 4) have landed. The parity check against `tools/surface_coverage.py` gates Part 7's tuning, which comes last. The phone's feedback is the live fusion's own.

## Out of scope

- The cup take's table top fused as two surfaces about 2 cm apart, which points at pose drift between passes. A "disagreement" overlay (surfaces whose supporting frames disagree on depth by more than the band) would make drift visible, but drift is a registration problem and that overlay is tracked separately in [plan.md](../plan.md).

## Open questions

- The stage holds no build settings beyond the session today, so the bounds box lasts only until the stage exits. Should it persist per take (for example a sidecar beside the take's pose track) so a rebuild next session keeps it?

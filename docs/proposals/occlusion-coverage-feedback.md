# Proposal: Occlusion capture coverage feedback

Editor parts (1 to 4) implemented, phone parts (5 and 6) and tuning (7) pending. This document is the handoff for two pieces of work: a coverage view in the editor's occlusion mesh build stage, and live coverage feedback on the phone while a take records. It carries the evidence that motivates both, the metric they share, what exists today on each side, the pieces in order, and the questions still open. A reader starting from it should not need the conversation that produced it.

## Goal

An occlusion mesh is only as good as the take it is fused from, and today nothing tells the operator whether a take covered the scene well enough until a character walks behind a lumpy object in the composite. Two things fix that:

- After a build, the editor shows the mesh properly (centred, solid, textured) and lets the operator switch to overlays that colour every surface by how well the take observed it, so a bad side is obvious and the operator knows what to recapture.

- While recording, the phone colours the surfaces it has seen by the same measure, so the operator walks to the weak side before stopping the take.

## Evidence

Measured in the ReconstructionExperiments repo (`D:\Github\git-MikanXR\ReconstructionExperiments`, `NOTES.md` and `tools/surface_coverage.py`) on `take_20261028_131840`, a 41 s walk around a table with a coffee cup on it, fused by `MikanCmd -occlusionMesh` at the tabletop preset. Every cup vertex was projected into each of the 398 fused keyframes at its refined pose and compared with that frame's own LiDAR depth:

- A frame supports a vertex when the measured depth at its pixel agrees with it within 1.5 cm, carves it when the measured depth lies farther (the frame says the space is empty), and is hidden from it when the measured depth lies nearer.

- The cup's lumpy side (vertices facing 150 to 270 degrees around its axis) had 17 to 24 supporting frames against 70 to 96 on the good side, a best viewing angle of 0.58 to 0.70 (cosine between normal and view) against 0.85 to 0.91, and more carving frames than supporting ones.

- The cause was the walk, not the sensor. Of the 49 keyframes taken from the 150 to 240 degree side only 4 had the cup in view, and nobody stood at 240 to 270 degrees, so that side was only ever seen at a glancing angle from the neighbouring sides, where rays that skim past the cup to the table carve it.

- A secondary cause: the walk made the reconstruction volume large, so the grid's voxel grew from 8 mm to 1.0 cm to fit `maxVoxels`, which faceted the cup visibly.

So the signal that separates a well-captured surface from a poorly captured one is a handful of per-surface numbers that both the editor and the phone can compute: how many frames saw it head-on, from how many directions, and how many frames contradicted it.

## The shared metric

Both sides compute the same per-surface record. The editor computes it per mesh vertex after the fusion, at the refined poses. The phone computes it per voxel of a coarse grid while recording, at ARKit's raw poses. Those inputs differ, so the two cannot agree exactly near the thresholds. The promise is one-way instead: the phone is stricter by design, so a surface the phone shows green is one the editor's build will show green. A phone surface shown weak that the editor calls good costs the operator a little extra walking, which is the safe direction.

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

The thresholds live in one settings struct on each side so tuning is a constant change. The phone's defaults are the editor's thresholds with a safety margin (for example `support >= 12`, `bestCos >= 0.75`, carve share at most a quarter), which is what makes it the stricter side. Part 7 sets the margin from the disagreement measured between the two.

## What exists today

Editor side ([occlusion-mesh.md](../reference/occlusion-mesh.md), [automation.md](../reference/automation.md)):

- `AppStage_OcclusionMeshBuild` (`src/Editor/AppStages/OcclusionMeshBuild`) runs the build on a worker thread and holds the result in `m_result` (`OcclusionMeshBuildResult`: the `TriangleMesh` in the take's ARKit world, the texture atlas as a `cv::Mat`, `fusedKeyframes`, the per-frame refined poses, and `OcclusionMeshBuildStats`).

- The preview, `AppStage_OcclusionMeshBuild::renderMeshPreview`, draws at most 60,000 vertices as normal-shaded points through the line renderer, placed by `stage world * pose offset`. `enter()` switches the viewport's `MikanCamera` to orbit mode but never aims it, so the orbit centres on the stage origin rather than the mesh, which is what makes it hard to navigate.

- `MikanCamera` already has the orbit controls a centred view needs: `setOrbitTargetPosition`, `setOrbitRadius`, `setOrbitLocation(yaw, pitch, radius)`.

- `Internal_PNT_TexturedLitColored` (`GlShaderCache.cpp`) draws a position, normal, texcoord mesh lit by a directional light, sampling a diffuse texture multiplied by a model colour. Model stencils draw through `Internal_PNT_Textured` from a `MikanRenderModelResource`. `createMkTriangulatedMesh` with an `IMkVertexDefinition` builds a GPU mesh from raw buffers, as `ModelImportUtils::createTriangulatedMeshResource` does.

- `MeshTexturing` (`src/Editor/Reconstruction/MeshTexturing.cpp`) already projects mesh vertices into keyframe depth to decide which view sees a triangle (`isVertexSeen`, through the `TextureView` struct), and `DepthTrackReader::readRecord` reads a keyframe's depth and confidence. The coverage pass needs the same projection.

- `GuiPanel_OcclusionMeshBuild` shows the settings before a build and the summary after it, and the stage answers `set_option`, `get_summary` and friends over automation.

Phone side (MikanARStreamer, from the project notes, so check each name against the current code before starting):

- `ARCaptureController` delivers every ARKit frame, `onPoseObserved` fires on each frame ahead of the fps gate, and `trackcamera` takes record `sceneDepth` and its confidence through `DepthTrack.swift` (`DepthTrackWriter`) beside the movie.

- `CaptureFlowSession` drives the guided capture flows, `StatsOverlayView` is the stats overlay, `AppSettings` and `SettingsView` hold the settings.

- The project builds in Swift 6 language mode. The test target compiles only pure-logic sources, so anything that imports ARKit, Metal or SwiftUI is registered as `:app` with `tools/register_sources.py`, and logic that should be tested has to stay in files free of those imports.

## The pieces

### Part 1: a real mesh preview, centred (editor)

- Replace the point preview with the fused mesh drawn solid. When the stage consumes a build result (`consumeBuildOutput`, UI thread, GL context current) it builds a GPU mesh from `m_result.mesh` (positions, normals, and texcoords when textured) with an `IMkVertexDefinition` for position, normal, texcoord and `createMkTriangulatedMesh`, bound to `Internal_PNT_TexturedLitColored`. The atlas becomes an `IMkTexture`. A room mesh is about a million triangles and draws without trouble. The mesh is released on redo, cancel and exit.

- Centre the view on the mesh as soon as the result arrives: orbit target at the centre of the mesh's world bounds (through the same `stage world * pose offset`), radius from the bounds' diagonal so the whole mesh fits, and the initial yaw and pitch looking along the take's average viewing direction, which is how the operator saw the scene. A "Frame mesh" button in the panel re-applies it.

- An optional "Camera path" checkbox draws the take's fused camera positions as a polyline through the line renderer. It is what ties a weak region to where the operator walked, the single most useful thing in the cup diagnosis.

### Part 2: the coverage pass (editor)

- New module `MeshCoverage` in `src/Editor/Reconstruction`, pure data in and out like `MeshTexturing`: the `TriangleMesh`, the per-frame poses the fusion used (refined when registration ran), the open `DepthTrackReader`, the fused keyframe list, the `TsdfVolumeConfig` (for the cull and the voxel size), and a `MeshCoverageSettings` holding the band and the class thresholds. Output: per-vertex arrays for the six measures of the shared metric and the class.

- Factor the vertex-to-depth-pixel projection that `MeshTexturing::isVertexSeen` does into a small shared helper so the two modules project identically, rather than writing it a second time.

- The pass runs on every build as a new builder phase, `eOcclusionMeshBuildPhase::coverage`, after cleanup and before texturing, with progress per keyframe and cooperative cancellation like the other phases. There is no setting to turn it off. It scores every fused keyframe, with no stride and no time cap: its work is vertices times keyframes (a room mesh at 400 keyframes is a few hundred million projections), split over vertices with `parallelFor`, and accuracy takes priority over build time.

- The pass reads each keyframe's depth through `DepthTrackReader` itself. It does not share a loop with the texture candidate scoring in `collectTextureViews`, which reads the same depth again. The two phases stay independent at the cost of one extra depth decode per keyframe.

- Texturing splits vertices per (vertex, tile) along atlas seams, so the coverage arrays computed on the clean mesh would no longer line up with the textured mesh. `MeshTexturing::textureMesh` gains an output mapping each new vertex to its source vertex, and the builder expands the coverage arrays through it. Coverage still runs once, on the clean mesh.

- `OcclusionMeshBuildResult` gains the coverage arrays, and `OcclusionMeshBuildStats` gains the area share of each class.

- `MikanCmd -occlusionMesh` gains `-coverage=<path>`, a folder receiving one raw `.npy` per measure (`support.npy`, `support_high.npy`, `carve.npy`, `directions.npy`, `best_cos.npy`, `nearest.npy`, `class.npy`), indexed by the vertices of the mesh file the same command writes. `numpy.load` reads each one directly, so the experiments repo can check the editor's numbers against `tools/surface_coverage.py` on the cup take. That parity check is the acceptance test for the metric: it passes when at least 95% of the cup's surface area gets the same class from both.

### Part 3: the view combo and the overlays (editor)

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

### Part 4: an operator bounds box (editor)

The cup take's voxel grew from 8 mm to 1.0 cm because `computeBounds` takes the bounds from every keyframe's confident depth, so a long walk enlarged the volume and the voxel grew to fit `maxVoxels`. The coverage view is where an operator notices that, so the fix lands with this work.

- An optional bounds box in the build settings. When set, it replaces the computed bounds for the volume. When unset, the automatic bounds stay the default.

- The operator places it with a box gizmo over the centred mesh preview from Part 1: build once with automatic bounds, drag the box around the subject, rebuild. The stage keeps the box across rebuilds.

- Automation: `set_option bounds <lo.x> <lo.y> <lo.z> <hi.x> <hi.y> <hi.z>` and `set_option bounds auto`, and `get_summary` reports the bounds the build used.

- The summary flags a build whose `voxelSize` came out larger than the preset's, so the operator knows when to place a box.

### Part 5: the coverage grid on the phone (MikanARStreamer)

- A pure-logic `CoverageGrid` (no ARKit or Metal imports, so the test target can compile it): a sparse hash of voxels in ARKit world, keyed by integer voxel coordinates, each holding the shared metric's measures with the normal estimated from the depth that created it. Capped at a voxel count (500,000 at about 32 bytes each is 16 MB) with the farthest from the current camera evicted first.

- The grid has two levels. Every observation updates a coarse voxel (2 cm to start, a setting), and observations at depth under about 1 m also update a fine voxel (a finer size, a setting) so a cup's sides are more than one voxel thick. The display prefers the fine voxel where one exists. The voxel cap covers both levels.

- Voxels exist only where the phone has observed depth, so the phone has no `unseen` class: space it never saw draws no voxel at all. The phone shows good and weak, and weak plays the part of red. That covers the failure the cup showed, a side seen only at glancing angles and carved, which comes out weak.

- Per update, from one `ARFrame`'s `sceneDepth`, its confidence, the camera transform and the intrinsics scaled to the depth size, as the take recorder already has them:
	1. Insert: back-project confident depth pixels that the flying pixel cull keeps (the same rule as the editor) and create any voxel that does not exist yet, with a normal from the depth image's neighbours.
	2. Update: project every existing voxel inside the frustum into the depth image and record support, carve or hidden, the direction bin and the viewing angle, exactly as the editor's pass does per vertex.

- Updates run on a background queue at a fixed rate below the frame rate (10 Hz to start), each on the most recent frame only, so the grid never falls behind and the recorder's own work is untouched. The update is a few hundred thousand projections at most, which is a Metal compute dispatch if the CPU cannot keep 10 Hz on the oldest supported phone. Keep the CPU version as the tested reference either way.

- The grid runs in a scan preview before recording, so the operator can plan the walk, and while a `trackcamera` take records. It resets when recording starts, so the take's grid reflects only what the take itself saw, and it is discarded when the take ends. Nothing about it is written into the capture: the editor computes its own coverage from the take after the build.

### Part 6: the live overlay on the phone (MikanARStreamer)

- Draw the grid's voxels over the camera view as screen-facing points (or small quads), coloured by the same class colours as the editor's coverage view, with a toggle in the recording UI. Points are the first version: they need no mesh and read clearly at room scale.

- A later version colours ARKit's own scene reconstruction mesh (`ARMeshAnchor`, available on LiDAR phones) by looking up each mesh vertex's voxel. It reads better than points, but ARKit keeps re-meshing those anchors, which is why the statistics live in the grid and not on the mesh.

- A small HUD line beside the existing stats: the share of grid surface that is good and weak, and an arrow toward the nearest large cluster of weak voxels within a few metres, so the operator knows which way to walk. The nearest cluster rather than an azimuth sector around the subject keeps the hint working in a room as well as around a tabletop.

- Settings: overlay on or off, the two voxel sizes, update rate, and the class thresholds, defaulting to the editor's values plus the safety margin.

### Part 7: tuning and verification

- Record one take with the overlay on, walking until the cup is green all round, and one deliberately skipping a side. Build both in the editor: everything the phone showed green must be green in the editor, and the skipped side must come out lumpy, weak on the phone and weak or red in the editor. Measure where the editor calls good what the phone called weak, and set the phone's margin from it.

- Tune the thresholds on those takes so that the coverage class predicts the surface error the experiments repo measures against hand-marked silhouettes (`tools/occlusion_edge_eval.py`, `tools/score_mesh_variants.py`). Record the result in the experiments repo's `NOTES.md` and the chosen values in [occlusion-mesh.md](../reference/occlusion-mesh.md).

- Phone performance on the oldest supported LiDAR phone: the overlay must not drop the recorded frame rate.

## Sequencing

The editor parts (1 to 4) land first and pass the parity check against `tools/surface_coverage.py` before the phone work (Parts 5 and 6) starts, so the phone ports a validated metric. Part 7's tuning comes last, once both sides exist.

## Out of scope

- The cup take's table top fused as two surfaces about 2 cm apart, which points at pose drift between passes. A "disagreement" overlay (surfaces whose supporting frames disagree on depth by more than the band) would make drift visible, but drift is a registration problem and that overlay is tracked separately in [plan.md](../plan.md).

## Open questions

- The stage holds no build settings beyond the session today, so the bounds box lasts only until the stage exits. Should it persist per take (for example a sidecar beside the take's pose track) so a rebuild next session keeps it?

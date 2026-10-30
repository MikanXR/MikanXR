# Proposal: Live fusion on the phone

Accepted. Parts 1 and 2 are built in MikanARStreamer and committed on its `main`. Part 1 passes its parity gate. Part 2 runs live on the phone and fuses every frame it is meant to (below). Part 2's measurement does not call for Part 3, since integration fits well inside the 100 ms a frame allows on the iPhone 12 Pro. Part 4 is not started. This document proposes replacing the phone's coverage points with the editor's own depth fusion, run live on the phone while a Track Camera take records and drawn over the camera as a mesh, so the operator sees the surface the editor's build will produce and judges its detail directly. It carries the evidence that motivates it, the design, the pieces in order, how it is verified, and the questions still open.

## Goal

While a take records, the operator sees a shaded mesh on the real objects: the surface the editor's occlusion mesh build would fuse from the depth recorded so far. A side that comes out lumpy, a thin part that is missing, or a hole where nothing was seen is visible on the spot, and the operator walks to fix it before stopping the take. The mesh follows the editor's fusion rule, so it is a preview of the build rather than a second opinion about it.

## Evidence

The coverage points of [occlusion-coverage-feedback.md](./occlusion-coverage-feedback.md) (Parts 5 and 6) were measured against the editor on a tabletop recording, a walk all the way around a metal duster can on a desk ("The phone against the editor on a real take" in that proposal):

- The editor's build of the recording scores nearly all of the can good. The phone's `CoverageGrid`, replayed over the same frames, scores only a minority good, even at the editor's thresholds. The operator saw a can that stayed red however much they walked.

- The cause is the representation, not the walk, the thresholds, or pose drift. A voxel's surface point is fixed by the one frame that created it, so that frame's depth noise leaves it off the surface later frames agree on, and they carve it. Nothing ever removes a voxel, so specks a single frame creates in empty space stay red too. The editor's truncated signed distance volume averages every frame into each voxel and writes free space as well as surface, so noise averages out and a speck is erased by the frames that see through it.

- Patching the grid recovers part of the gap and no more: dropping voxels that are seen through closes some of it, and averaging each voxel's point adds nothing on top. The base of the can, where it meets the desk, stays red while the editor calls it good. Replaying with the editor's refined poses changes nothing, so the phone's raw ARKit poses are not what holds it back.

- ARKit's own scene mesh (`ARMeshAnchor`) was considered and rejected for this. It is built for room-scale surfaces, walls and furniture, and its coarse triangles turn a 6.5 cm can into a rounded blob however well it was scanned, so it cannot show the detail the editor's 8 mm build will have.

So the measure the phone shows has to come from the same kind of fusion the editor runs. Fusing on the phone gives that by construction, and the fused surface is itself the most direct feedback: the operator does not have to trust a colour.

## What exists today

- `src/Editor/Reconstruction/TsdfVolume.h/.cpp` is the rule to port. A dense grid over a box, one running weighted average per voxel (tsdf initialised to 1, weight to 0). Per frame: the flying pixel cull (`gradientCull`, 8%), confidence 0, 1, 2 as weight 0, 0.5, 1, the depth range (0.1 to 5 m). Every voxel in the frame's frustum projects to its rounded pixel, and with `sdf = pixelDepth - voxelDepth` above `-truncation` it folds in `min(sdf / truncation, 1)` at that pixel's weight. Free space in front of a surface is written as +1, which is what erases a speck.

- The presets ([occlusion-mesh.md](../reference/occlusion-mesh.md), "Pipeline"): room 1.5 cm voxels with a 6 cm truncation, tabletop 8 mm and 3.2 cm. The editor fuses every third frame of a take.

- `MarchingCubes` extracts the zero crossing. Before it runs, unobserved voxels take the sign of their nearest observed voxel, which closes the unseen far side of an object.

- `src/Editor/Reconstruction/Test/TsdfVolumeTests.cpp` holds the synthetic cases the port should pass.

- On the phone, `ARCaptureController.onDepthObserved` already delivers a copy of the depth, the confidence, the pose, and the depth intrinsics at a set rate while a take records. `CoverageWorker` already runs work on a queue of its own, dropping a frame while the last is still in flight. `CoverageOverlay` already puts geometry into the `ARSCNView` scene in ARKit world. The macOS replay build of the phone's Swift sources against a take (used for the measurements above) is the harness for parity.

## Part 1 as built

- `DepthObservation.swift` holds the depth observation, the editor's flying pixel cull, the confidence weights, and the single OpenCV to ARKit camera axis conversion, moved out of `CoverageGrid.swift`, which now uses them too.

- `FusionVolume.swift` holds `FusionSettings` (the tabletop and room presets), the sparse volume, and the port of `MarchingCubes`. `MarchingCubesTables.swift` is generated from the editor's `MarchingCubes.cpp`, so the tables cannot drift apart. Voxel `index` is centred on `(index + 0.5) * voxelSize`, so a cell's centre lies inside it.

- Allocation samples every second pixel each way, at points no more than half a block apart through the band one truncation either side of the depth. A block is integrated when any corner projects into the image within the depth range, or straddles the camera's plane.

- `takeChangedBlocks` returns the blocks a frame changed and the blocks behind them on each axis, whose cubes read their first voxel layer, ready for Part 2's extraction of changed blocks.

- `FusionVolumeTests` (12 cases) ports the editor's analytic sphere, flying pixel cull, fused plane, fused sphere, and back shell cases, and adds a speck that later frames erase, allocation only near surfaces, welding across blocks, an unseen side left open, eviction, the changed blocks, and the presets. The simulator suite is 162 tests, all passing.

- `tools/fusion_replay` holds the replay (`build.sh` builds it for macOS from the app's sources, and it writes the take's mesh as PLY) and `parity.py`, the parity measure, which needs numpy and scipy.

The parity gate ran on a tabletop recording, against `MikanCmd -occlusionMesh -voxel=0.008 -noRegistration -noTexture -bounds=<box>`, both fusing the same frames, inside the box shrunk by 3 cm. It passes both directions (phone to editor, and the area the editor's coverage calls seen to phone) well above the 95 percent target within 8 mm, on the can alone as well. The editor's sign fill adds closed backs in the box that the phone leaves open. The measurements behind this are in the experiments repo's notes, against recordings that are not checked in.

Measured on the Mac (an Apple silicon laptop, not the phone), the whole walk allocated several times the tabletop estimate below, because the tabletop preset fuses everything the LiDAR sees out to 5 m, the room around the desk included, at 8 mm. Part 2 has to measure this on the phone and decide whether the live volume limits its depth range or its cap.

## Part 2 as built

- `FusionWorker.swift` (compiled by the test target) integrates each observation of a take on its own queue, dropping one that arrives while the last is still being integrated. At most every 0.5 s, and only once the last pass has finished, it re-extracts each region (4 x 4 x 4 blocks) holding a block that changed or was evicted, whole, packed for SceneKit: the fusion queue takes the changed blocks and copies the regions' blocks and the neighbours their extraction reads (`FusionVolume.copy(of:)`, `FusionExtraction`), and a second queue extracts from the copy, the regions in parallel. Blocks that change during a pass wait for the next. A region whose blocks are all gone comes back empty, and the volume reports evicted blocks as changed so their regions are cleared. A take's end clears the display through the extraction queue, behind any pass already there.

- `FusionOverlay.swift` (`:app`) holds `FusionOverlay`, one SceneKit node per region, and `FusionSession`, which the streaming session owns. The material is constant-lit opaque grey, back faces culled, shaded by a surface shader modifier from the view-space normal's facing to the camera, so the mesh reads without any light in the scene. It was translucent at first, and the operator found opaque reads better.

- Observations arrive every 3 / fps seconds, the editor's every third frame at the take's own rate.

- Settings: Live Mesh (on), Show Mesh (on, also the status bar's cube button), and Detail (Tabletop or Room, the editor's presets, Tabletop by default). The open questions below were not answered before building, so these follow the proposal's stated defaults.

- `CoverageGrid`, its points, its HUD, its settings, and its tests are gone. `fusion [status|reset]` replaces `coverage`, and `fusion replay [<capture> [tabletop|room]]` fuses a stored capture on the phone at the live cadence (every third frame, the changed regions extracted after every fifth) on its own queue and reports the times, since that outlasts the debug channel's reply timeout.

- The simulator suite is 146 tests, all passing: the coverage grid's 19 went, and 3 for the regions came.

Measured on the iPhone 12 Pro, with `fusion replay` of a tabletop recording at the tabletop preset with the camera preview running, and with a live take with the phone at rest on the desk: integration fits the 100 ms a 10 Hz frame allows, and the recorder is untouched, but each extraction pass held the worker long enough that a large share of the frames were dropped, so the live mesh was fused from far fewer frames than the editor's build uses. The memory for the whole walk is well inside the 25,000 block cap. The live mesh sat on the desk, the chair, the mouse, and the can in the screenshot.

Extraction was then sped up twice, each measured on the replay of the same tabletop recording with the Mac's `fusion_replay`, which now times the live cadence's passes too:

- A block's samples are read straight from the voxel arrays, the block and its seven neighbours looked up once, rather than each of its 729 samples through the block hash. The marching cubes loop no longer allocates per cube. This nearly halved the Mac's median pass.

- The changed regions are extracted in parallel, since extraction only reads the volume. This cut the Mac's median pass to a small fraction of what remained.

The mesh is unchanged by both: the same vertex and triangle counts, and the same parity result. On the iPhone 12 Pro, whose two performance cores share the work with ARKit and the preview, the gain is smaller: a replayed pass took less than half as long as before, and a live take at rest fused more of its frames and dropped fewer, with the recorder again dropping none.

Extraction then moved off the fusion queue as described above, so the fusion queue's share of a pass is only the copy. On the iPhone 12 Pro the copy is a small part of a pass, extraction runs beside integration, and a live take at rest fused every frame it was meant to, with the recorder again dropping none, and the mesh read clearly on the desk, the laptop, the can, the keyboard, and the mouse. The suite is 148 tests, with a copy test and a test of the worker's two queues through a take's start and end.

A first tabletop walk around the can at the tabletop preset with the translucent mesh:

- The operator found holes easy to spot, the lag fine, and no stutter, and asked for an opaque mesh, which is now the default.

- The recorder missed a few more single frames than on a walk that ran the coverage grid. The live fusion costs the recorder a small share of its frames.

- The phone drew the walk's full mesh without visible stutter.

- Parity against `MikanCmd -noRegistration` boxed around the can passes both directions within 8 mm, on the can itself as well.

- The editor's registered build scores most of the can's sides good. The weak part is the side the walk covered least, where the camera spent far fewer frames than on the sides that came out good. Registration corrected more drift than on the earlier tabletop recording.

A room walk at the room preset with the opaque mesh:

- The recorder's writer dropped no frames, but ARKit did not deliver a small share of single frames, most of them in two bursts late in the walk. The fusion had little to do then: the trace of the replay shows few new blocks and a slow camera then, against a much larger burst of new blocks earlier in the walk, which passed without a skip. By then the volume and the mesh were large, so the phone's heat, drawing that mesh, and memory are each a candidate, and the recording cannot tell them apart.

- Replayed on the phone with the 25,000 block cap, the walk stayed under the cap, so eviction never ran.

- Parity against `MikanCmd -noRegistration -voxel=0.015 -pathRadius=0` boxed around the walk so the editor's voxel stays 1.5 cm (its automatic bounds grew it): both directions pass the 95 percent target within 15 mm, the phone's side with less margin. The phone keeps small islands the editor's cleanup removes, which is where most of the difference lies. `parity.py` samples a room at `PARITY_DENSITY=1e5` per square metre.

The measurements behind this section were made against recordings that are not checked in.

Extracting only the blocks that changed and joining each region from cached block meshes would make the mesh catch up sooner, since most of a changed region's blocks have not changed, but no frames are lost to extraction now.

## Design

### The volume

A sparse block volume in ARKit world: blocks of 8 x 8 x 8 voxels, keyed by integer block coordinates in a hash, each voxel holding the editor's tsdf and weight as 32-bit floats (4 KB a block). Dense like the editor is not an option, because the editor takes its box from the whole take after it ends, and the phone has to fuse before any box is known.

A block is allocated where a frame's usable depth lands within one truncation of a surface: the depth point and the points one truncation in front of and behind it along the ray mark their blocks. Every allocated block in the frame's frustum is then integrated with the editor's rule unchanged, free space included, so inside the allocated region the phone's voxels hold what the editor's would. Outside it nothing is stored, which only loses free space far from any surface, where the editor writes +1 that no extraction ever reads.

The preset is the editor's, chosen in the phone's settings (tabletop by default, since a small subject is where detail is judged). Estimates to be measured: a tabletop scene of about 6 m2 of surface in range is about 3,000 blocks (12 MB), and a room of about 100 m2 at the room preset is about 14,000 blocks (56 MB). A block cap (25,000, 100 MB) evicts the farthest blocks first, like the current grid.

### Integration

Every observation the take delivers at 10 Hz, which is the editor's every third frame at 30 fps, integrated on a worker queue that drops a frame while the last is still in flight, as today.

A pure Swift CPU implementation comes first and stays as the tested reference: the editor's rule over the allocated blocks in the frustum, split over blocks with `concurrentPerform`. A tabletop frame touches about 1.5 million voxels, which the CPU may hold at 10 Hz. A room frame touches several times that. A Metal compute kernel over the same blocks, checked against the CPU reference, is built only if the device measurement says the CPU cannot keep up.

### Extraction

Marching cubes over the blocks a frame changed, at most twice a second, each block's cubes reading the first voxel layer of its neighbours. The triangle table and the vertex interpolation are the editor's `MarchingCubes`, ported, with triangles wound counter-clockwise about the outward normal as the editor winds them.

One deliberate difference: a cube with an unobserved corner is skipped, rather than filling unobserved voxels with their nearest observed sign as the editor does. The editor's rule closes an object's unseen back, so its build has a surface there. Live, a hole where nothing was seen is the more useful picture, because it tells the operator where to walk. The parity check below measures around it.

### Display

The mesh is drawn in the `ARSCNView` scene in ARKit world, depth tested, with back faces culled (SceneKit's default), as a lit neutral grey surface over the camera image, translucent by default so the real object shows through for comparison. Geometry is grouped into one node per region of 4 x 4 x 4 blocks, and a region's geometry is rebuilt when one of its blocks is re-extracted, so the scene holds hundreds of nodes rather than thousands. A status bar button toggles the mesh, as it toggles the points today.

### Lifecycle

Like the coverage grid now: it runs only while a Track Camera take records on a LiDAR phone with the setting on, from the depth the take records anyway, starts empty with each take, and is discarded when the take ends. Nothing of it is written into the capture. The editor still builds its own mesh from the take, with registration, texturing, and coverage.

### What it replaces

`CoverageGrid`, its points, and the HUD's shares and arrow are removed when the mesh lands, since their measure was shown wrong on a real take. The `coverage` debug command becomes `fusion [status|reset]` (blocks, voxels integrated, triangles, last integration and extraction times, dropped frames). Coloured coverage returns only as a later part on the mesh itself, if the shaded surface turns out not to be enough.

## The pieces

### Part 1: the CPU fusion and the parity gate

- Pure Swift, compiled by the test target: the sparse block volume, allocation, the editor's integration rule, the cull, and marching cubes with the skipped-unobserved rule.

- Tests: the editor's `TsdfVolumeTests` cases ported, plus allocation, the block boundary in extraction, and the winding.

- The replay harness fuses a take offline with this code and writes the mesh. The parity check against `MikanCmd -occlusionMesh -noRegistration` at the same preset on a tabletop recording passes when 95% of the phone mesh's area lies within one voxel of the editor's mesh, and 95% of the editor mesh's area that its coverage pass calls seen lies within one voxel of the phone's. A failure is a bug in one of the two until shown otherwise.

### Part 2: live on the phone

- The worker integrates each observation of a take and extracts the changed blocks, the overlay draws the mesh, and the settings take the preset and the mesh toggle.

- `CoverageGrid`, the points, and the HUD are removed, and the `fusion` debug command replaces `coverage`.

- Measured on the iPhone 12 Pro: integration and extraction times on a tabletop and a room take, memory at the end of each, and the recorder's dropped frames, which must stay at zero.

### Part 3: Metal integration, only if Part 2's measurement needs it

- A compute kernel over the frustum's blocks, checked voxel for voxel against the CPU reference on the same frames.

### Part 4, later: coverage on the mesh

- Only if the shaded surface is not enough feedback. Colouring the live mesh by the editor's measure needs per-voxel accumulators (supporting frames, view directions, carving frames) gathered during integration and read at the vertices, a design of its own.

## Out of scope

- Registration on the phone. The editor's frame-to-model refinement corrects a take's drift, and the live mesh does without it. A take whose drift is large shows it as doubled surfaces, which is the right thing for the operator to see.

- Texturing, and any use of the phone's mesh by the editor.

## Open questions

- Should the preset be a setting, or follow the subject (tabletop when the depth in view is mostly under a metre)?

- Does the HUD keep a line at all once the shares and the arrow go (for example the triangle count), or does the mesh speak for itself?

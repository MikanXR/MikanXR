# Proposal: Capture archives and guided capture flows

Design decided and implemented on both sides: Parts 1 to 6 in MikanXR, Parts 7 and 8 in MikanARStreamer. This document is the record of the design: it carries the decisions, what exists on both sides today, the pieces in order, and the open questions. A reader starting from it should not need the conversation that produced it.

## Goal

A shot recorded on the phone is a loose set of files today: a movie or photo, a `<stem>.pose.json` pose track, for a LiDAR take a `<stem>.depth.bin` and `<stem>.conf.bin`, and a second `<stem>_marker` take with its own track. They are uploaded one file per request, paired only by file name, and split across the `movies` and `textures` folders. Deleting or renaming one file silently breaks the shot, and what has to be captured depends on the kind of shot, which the operator has to remember.

Two changes fix this:

- Each shot becomes one file, an uncompressed tar holding the media, the marker reference, and the sidecars, with a manifest saying what kind of shot it is.
- The phone walks the operator through each kind of shot with a step-by-step flow, so the archive always holds what its kind needs.

## Decisions

- **Three capture kinds, one flat tar each.** No tar nests inside another.
	- `.stillphoto`: a stationary camera's still. A clean plate image without the marker, a marker image with the marker in frame, and the marker image's one-frame pose track.
	- `.stillcamera`: a stationary camera's movie with a moving subject. The `.stillphoto` members plus the movie. No movie pose track: the editor treats the camera as fixed and places it from the marker image. The phone stops the recording itself if it detects that the camera moved.
	- `.trackcamera`: a moving camera's movie. The marker image and its pose track, the movie and its per-frame pose track, and on a LiDAR phone the depth and confidence tracks. A non-LiDAR phone produces a `.trackcamera` without depth: the camera motion is there, but no occlusion mesh can be built from it.

- **One `captures` asset folder** holds every capture. `movies` holds raw movies only and `textures` raw images only, usable in a file video source when tracking does not matter. The loose sidecar path is removed: neither folder accepts `*.pose.json`, `*.depth.bin`, or `*.conf.bin` any more.

- **Regenerable outputs live beside the capture, never inside it.** The occlusion mesh build writes `captures/<stem>.refined.pose.json`, and the `.glb` goes to `models/` as it does today. A capture is written once and never modified, which keeps it safe to hold open for playback.

- **Existing loose takes are packed once** into captures when a project loads, and the components that named them are repointed. The originals are moved aside, never deleted.

- **The editor's recorder writes captures too** (`VideoSourceRecorder`), so a shot recorded from any camera in the editor, including a file source playing an existing capture, comes out in the same format. Recording a playing capture is how a capture gets trimmed.

- **Stream mode gets an alignment prompt.** The phone asks the editor over HTTP whether its stream's camera can see the origin marker and whether the stream is aligned, and hides the prompt once the editor reports it aligned.

- **The phone repo is edited over SSH.** `C:\GitHub\BrendanWalker\MikanARStreamer` is a local clone for reading only. Changes to MikanARStreamer are made in its working copy on `MAC-BWALKER-4` over SSH, where it builds and its tests run.

## Validated: playing a movie in place inside a tar

The editor's OpenCV is 4.10.0 (`deps/opencv`), which predates `cv::IStreamReader` (added in 4.11), so a movie cannot be handed to `cv::VideoCapture` as a custom stream. The bundled `opencv_videoio_ffmpeg4100_64.dll` does include ffmpeg's `subfile` protocol, and a spike against that DLL confirmed it works:

```
cv::VideoCapture cap;
cap.open("subfile,,start,<dataOffset>,end,<dataOffset + size>,,:<absolute tar path>", cv::CAP_FFMPEG);
```

On a 60-frame mp4 stored as the second member of a ustar file (data at byte 1536), the subfile URL reported the same frame rate and frame count as the loose file, read all 60 frames, and seeked by `CAP_PROP_POS_MSEC` to the same frame. It also worked with a space and a comma in the directory name. So a capture's movie plays in place with no extraction and no cache, which is what makes an uncompressed tar the right container. Not tested: a path with non-ASCII characters, which `cv::VideoCapture`'s narrow-string path already does not handle for loose files either.

## The format

### Container

A POSIX ustar archive, uncompressed, written by our own code on both sides and read by our own reader in the editor. The reader accepts only what the writers produce and rejects everything else with a message:

- regular-file members only, no directories, links, or pax and GNU extension headers
- member names of at most 100 ASCII characters, with no path separators
- members under the ustar size limit of 8 GiB
- the manifest `capture.json` as the first member

Every member's data starts on a 512-byte boundary at a fixed offset, which is what lets the movie play through `subfile`, the depth reader seek to a record, and an image or JSON member be read into memory with one seek. No compression is applied: the media is already compressed, and compressing the depth would cost the fixed offsets.

The size limits that matter are the editor's upload path, not the format. A LiDAR take's depth and confidence tracks are 147,456 bytes per frame at 256x192, about 265 MB per minute at 30 fps, before the movie.

### Manifest

```
{
  "format": "mikan-capture", "version": 1,
  "kind": "trackcamera",
  "session_id": "<the ARKit world origin's UUID>",
  "pose_space": "arkit_world",
  "created_utc": "2026-10-14T12:00:00Z",
  "producer": "MikanARStreamer <version>",
  "interrupted": false,
  "members": {
    "marker_image": "marker.jpg",
    "marker_pose": "marker.pose.json",
    "movie": "movie.mp4",
    "movie_pose": "movie.pose.json",
    "depth": "movie.depth.bin",
    "confidence": "movie.conf.bin"
  }
}
```

- `kind` decides how the editor treats the camera, not the file extension: the extension only routes the file to the `captures` folder. A manifest kind that disagrees with the extension is rejected.

- `members` maps roles to member names, so an image may be `.jpg` or `.png`. The roles are `plate_image`, `marker_image`, `marker_pose`, `movie`, `movie_pose`, `depth`, `confidence`.

- `pose_space` is `arkit_world` for a phone capture, whose poses need the marker alignment's offset, or `stage` for an editor recording, whose poses are already stage-space. A `stage` capture plays as aligned with an identity offset.

- `session_id` is the one session every pose track in the archive was recorded in. The loader rejects an archive whose member tracks name a different session. This replaces today's check in `FileVideoSourceComponent::loadPoseTracks` that a take and its marker take share a session: the phone records both in one flow, so they cannot disagree.

- `interrupted` is true on a `.stillcamera` the phone stopped because the camera moved.

The member pose tracks and depth tracks keep today's formats unchanged (`mikan-pose-track` version 1 or 2, headerless depth and confidence records, [videosources.md](../reference/videosources.md)). Depth and confidence come together or not at all.

### Required members by kind

| kind | required | optional |
|---|---|---|
| `stillphoto` | `plate_image` | `marker_image`, `marker_pose` |
| `stillcamera` | `movie` | `plate_image`, `marker_image`, `marker_pose` |
| `trackcamera` | `movie`, `movie_pose` | `plate_image`, `marker_image`, `marker_pose`, `depth` with `confidence` |

The phone always writes the marker members, so a phone capture is always alignable. They are optional in the format because an editor recording in stage space needs no marker, and because a packed legacy take may have had no marker take. `marker_pose` matters for a still kind even though the camera is treated as fixed: its pinhole values are the camera's intrinsics. Without it the source falls back to default intrinsics from the frame size, as a trackless still does today.

## What exists today

On the MikanXR side:

- `FileVideoSourceDefinition` (`src/Editor/ECS/VideoSource/FileVideoSourceComponent.h`) holds `media_path`, `marker_media_path`, `pose_track_path`, `marker_pose_track_path`, `loop`, and the pose offset. Setting a media path re-derives its track slot from the stem (`onDefinitionMarkedDirty`, `sidecarPathIfPresent`). `loadPoseTracks` enforces the marker session rule. `IAlignmentReferenceSource` switches playback to the marker media for the align stages.

- **The four path properties are on the wire.** `MikanFileVideoSourceValues` (`src/Libraries/MikanClientAPI/Public/MikanVideoSourceTypes.h`) carries them, with the generated C# and TypeScript bindings, the schema guard (`EditorEntitySchemaTable.h`, `ClientApiPropertySchemaTests.cpp`), and localization keys for each property.

- `MovieDecoder` opens movies by path with `cv::CAP_FFMPEG` and stills with `cv::imread`, both by path only. `PoseTrack::loadFromJSON` already parses from a config, so a member read into memory needs no new parser. `DepthTrackReader` finds its files by stem but already reads one record at a time by offset, so it needs only a base offset.

- `ProjectAssetCatalog::getFolderDescs` lists `PoseTrackAssetReferenceFactory` in `textures` and `movies`, and `DepthTrackAssetReferenceFactory` in `movies`. `ProjectManager::ensureAssetFolders` creates every declared folder on load.

- `AssetUploadRequestHandler` receives the whole body in memory from `ix::HttpServer`, which parses `Content-Length` as an `int` (about a 2 GiB ceiling), holds the body twice while reading it, and drops a request that has not arrived in 300 s. No C++ GET route exists. The plan's "Asset upload memory" item records the buffering problem.

- `VideoSourceRecorder` writes `movies/take_<time>.mp4` or `textures/take_<time>.jpg` plus a version 1 pose track, with a marker toggle that names the next take `<take>_marker`.

- The occlusion mesh build (`AppStage_OcclusionMeshBuild`) and the headless `-occlusionMesh -take=<mp4>` find the depth tracks by the media's stem and write `<stem>.refined.pose.json` beside the take. With `replace_pose_track` on, the build repoints `pose_track_path` at the refined track.

- `AppStage_AlignCameraByOriginMarker` solves a frame-coupled source's offset (`setPoseOffset`) from averaged marker samples, and answers `stage get_marker_visible` over automation while it is active. Nothing answers whether a marker is visible outside that stage. `ARKitVideoSourceComponent` is identified by its `base_port`, the UDP port the phone streams to.

- The depth proxy and scene lighting captures read whatever frame the camera's source delivers. With a file source that is the main media, never the marker reference.

- There is no tar code anywhere in the repo.

On the MikanARStreamer side (SwiftUI, Swift 6, iOS 16, no third-party dependencies):

- `CaptureMode` is `photo | video | stream`. A marker toggle in `StatusBarView` arms the next capture as `<last take>_marker`.

- `StreamingSession` drives recording. `MovieRecorder` writes H.264 through `AVAssetWriter` with the pose and depth tracks in lockstep. `PhotoCapture` writes a JPEG and a one-frame track. Depth runs only for a non-marker video take.

- `CaptureStore` lists `.jpg` and `.mp4` files in Documents and attaches sidecars by stem. `CaptureUploader` sends the media and then each sidecar, streamed from the file with `URLSession.upload(for:fromFile:)`, movies to `movies` and photos to `textures`.

- The debug channel (`DebugChannel.swift`, commands in `StreamingSession.setupDebugCommands`) answers `capture [marker]`, `record start [marker]`, `record stop`, `captures list|upload|delete`, `mode`, and `stats`.

- No guided flow, no motion detection, no GET request to the editor, and no archive code exist. `ARCaptureController.trackingState` is exposed but unused.

- The Xcode project uses classic file references: a new `.swift` file is added to `project.pbxproj` by hand, in both the app and the test target, which compiles the app sources directly. Tests run on the simulator with `xcodebuild test`.

## The pieces

In dependency order. Each is a separate change and a separate review. Parts 1 to 6 are in this repo. Parts 7 and 8 are in MikanARStreamer and depend only on the format above, so they can proceed in parallel with Part 2 once Part 1's format is settled.

### Part 1: Capture archive library

- [x] A new `src/Editor/Capture/` subtree with a `Test/` folder, in the shape of `src/Editor/Reconstruction/`:
	- `CaptureArchiveReader`: parses the ustar headers into a member table (name, data offset, size), rejects anything outside the accepted subset, and reads a member into memory.
	- `CaptureArchiveWriter`: writes the manifest first, then members from files or memory, streaming a large member in chunks rather than loading it.
	- `CaptureManifest`: the manifest model, its JSON round trip, and validation of kind, required members, and session ids.
	- `Capture`: an opened archive with its manifest, the member table, and accessors that load a member's pose track, decode a member image, and build a member's `subfile` URL.
- [x] Unit tests: writer to reader round trip, rejection of each unsupported header type and of a kind whose required member is missing, a session mismatch between member tracks, and a `subfile` open of a movie member through `MovieDecoder`.
- [x] `layout.md` and `modules.md` gain the subtree.

### Part 2: Editor playback from captures

- [x] `CaptureAssetReference` and its factory (`*.stillphoto`, `*.stillcamera`, `*.trackcamera`) with a preview tile from the plate image, or the movie's first frame when there is no plate. A `captures` folder in `getFolderDescs` holding captures and pose tracks (for refined tracks). `movies` and `textures` lose the pose track factory. `DepthTrackAssetReference` is deleted.
- [x] `MovieDecoder` gains an open from a capture member: the `subfile` URL for a movie, and `cv::imdecode` of the member bytes for an image.
- [x] `DepthTrackReader` opens the two member ranges of the archive, with base offsets on its seeks and the size check made against the member sizes.
- [x] `FileVideoSourceDefinition`:
	- `media_path` accepts a raw movie, a raw image, or a capture.
	- `pose_track_path` stays as an override for a `trackcamera` (the refined track), empty by default and no longer re-derived from the media.
	- `marker_media_path` and `marker_pose_track_path` are removed.
- [x] `FileVideoSourceComponent` plays a capture by kind:
	- `stillphoto`: the plate image is the main media.
	- `stillcamera`: the movie is the main media, and the camera is fixed.
	- `trackcamera`: the movie with its pose track, or the override when set.
	- In all three, the marker image is the alignment reference when present, and intrinsics are seeded from the movie track's first record or else from `marker_pose`.
	- A `stage` capture reports itself aligned with an identity offset.
- [x] Wire change: `MikanFileVideoSourceValues` drops `marker_media_path` and `marker_pose_track_path`. Wire all three property legs, regenerate the bindings through the codegen targets, update the schema table, remove the two localization keys, and run the schema guard ([wire-protocol.md](../reference/wire-protocol.md)).
- [x] The occlusion mesh build takes its depth from the capture's members, refuses a capture without depth through the existing message box, and writes `captures/<stem>.refined.pose.json`. `-occlusionMesh -take=` takes a `.trackcamera` path.
- [x] Rewrite the fixtures in `FileVideoSourceTests`, `OcclusionMeshTests`, `FrameRegistrationTests`, and `ProjectAssetCatalogTests` that build loose takes, so they build captures through the Part 1 writer.
- [x] Docs: `videosources.md` (file video sources, pose tracks, depth tracks, uploading), `occlusion-mesh.md`, `objects.md` (the folder list), `automation.md` (the phone pull example that copies a take into `movies`).

Part 2 and Part 3 land together: removing the sidecar path without the migration breaks every existing project's takes.

### Part 3: One-time packing of loose takes

- [x] On project load, after `ensureAssetFolders`, any `*.pose.json`, `*.depth.bin`, or `*.conf.bin` in `movies` or `textures` triggers packing. Detection by the files' presence makes it rerun safely after an interrupted pack, with no project version flag.
- [x] Grouping by stem, with `<stem>_marker` as the marker take:
	- An image with a track or a marker take becomes a `stillphoto` (plate = the image, marker = the marker take's image). An image with neither is a raw texture and stays in `textures`.
	- A movie with a per-frame track becomes a `trackcamera`, with its depth tracks when present.
	- A movie without a track but with a marker take becomes a `stillcamera`.
	- A movie without a track and without a marker take is a raw movie and stays in `movies`.
	- A marker take that is a movie contributes its first frame as `marker.jpg` and that frame's pose as a one-frame `marker.pose.json`.
	- `pose_space` is `arkit_world` for a track the phone wrote, and `stage` for one the editor's recorder wrote. Neither writer records which it was, so the rule is the only judge available: a track whose session id is 32 hex characters without dashes came from `VideoSourceRecorder::makeSessionId`, and a UUID with dashes came from the phone.
	- `<stem>.refined.pose.json` moves to `captures` beside the new capture.
- [x] Every `FileVideoSource` definition naming a packed take is repointed: `media_path` to the capture, `pose_track_path` to the moved refined track if it named one and empty otherwise. The pose offset is kept, since the capture holds the same session it was solved in. The project is saved.
- [x] The originals, and any orphan sidecar, move to `<project>/legacy_takes/`, which is not an asset folder. A log line per capture and a summary line say what was packed and where the originals went.
- [x] Test: a fixture project with each grouping case packs to the expected kinds and members, repoints its components, and a second load does nothing.

### Part 4: Upload of large captures

- [x] Stream a request body to disk instead of memory, inside the IXWebSocket fork (`thirdparty/IXWebSocket`): parse `Content-Length` as 64-bit, and for a route registered as streaming, write the body in chunks to a temporary file whose path the route receives in place of `body`. The upload route resolves its destination folder from the query before the body arrives, so the temporary file lands on the destination's volume and the final move stays a rename.
- [x] Replace the 300 s whole-request deadline with an idle timeout for streaming routes, so a multi-gigabyte capture over Wi-Fi is not cut off while bytes are still arriving. Read how the fork applies `makeCancellationRequestWithTimeout` before choosing the mechanism.
- [x] The `captures` folder accepts uploads. Replacing a capture a file source holds open still fails with 409.
- [x] Remove the "Asset upload memory" item from `plan.md`, which this part resolves.
- [x] Test: an upload larger than the old in-memory path would hold arrives intact and the server's resident memory does not grow with it. `HttpServerTests` covers the streaming route.

### Part 5: The editor's recorder writes captures

- [x] `VideoSourceRecorder` writes captures to `captures/`, and the Video Source Settings stage gets the same step-by-step flows as the phone, with a kind selector replacing the marker toggle:
	- `stillphoto`: marker image, then plate image.
	- `stillcamera`: marker image, plate image, then record until stopped.
	- `trackcamera`: offered when the camera has a pose (a tracking mount or a frame-coupled source). It records the movie with its stage-space pose track, `pose_space` `stage`, and no marker steps, since the poses need no alignment.
- [x] The recorder writes media and tracks to a hidden working directory under `captures/`, packs the archive when the flow completes, and removes the working directory. A cancelled flow removes it too.
- [x] Recording from a file source playing a capture is trimming: play, record from the in point, stop at the out point. The result re-encodes the movie, carries no depth (the recorder records none), and has stage-space poses with the source's offset folded in.
- [x] Automation: `capture_start <kind>`, `capture_advance`, `capture_cancel`, and `get_capture_state` replace `record_start`, `record_stop`, `capture_image`, and `get_recording_state`. Update `automation.md`.
- [x] Test: each kind's flow produces an archive that the Part 1 reader validates.

### Part 6: Alignment status for the phone's stream prompt

- [x] `GET /arkit/alignment?port=<base_port>` answers for the `ARKitVideoSourceComponent` with that `base_port`: `{"marker_visible": true, "aligned": false}`, or 404 when no ARKit source uses the port. `aligned` is true when the source holds a pose offset solved since its stream session last restarted (the source already drops the offset when `frameSeq` runs backwards).
- [x] Marker detection for the ARKit source outside the align stage: while the source is unaligned and the phone has polled within the last few seconds, detect the origin marker on its frames at a reduced rate, reusing `ArucoMarkerPoseSampler`. It runs on the main thread, where the finder reads the view's buffers, four times a second rather than on a worker thread, since moving the detection off the GL thread would mean refactoring the finder to take a frame.
- [x] Once the marker has been seen steadily for a sample window, solve and apply the offset with the same frame-coupled averaging `AppStage_AlignCameraByOriginMarker` uses (`worldToStage` per sample, averaged, `setPoseOffset`). Lift that averaging out of the stage so both callers share it. The solve reads `getLatestSourceWorldPose`, never the offset-applied pose (videosources.md, "Marker reference for recorded takes" and the paragraphs before it).
- [x] First C++ GET route: a background route on the HTTP message server that checks `request.method`. Document it with the upload route in `videosources.md`.
- [x] Test: the route answers 404 for an unknown port, and `aligned` reads false before and true after a solve on a test source.

### Part 7: MikanARStreamer writes and uploads captures

In the MikanARStreamer repo at `/Users/bwalker/Documents/GitHub/MikanARStreamer` on `MAC-BWALKER-4`, edited over SSH.

- [x] A Swift ustar writer producing exactly the accepted subset, streaming file members in chunks, with a round-trip test that reads its own output back.
- [x] Each flow records into a working directory `Documents/<name>.capture/` (movie, tracks, images), and on completion packs the archive beside it with the manifest first, then deletes the working directory. Recording cannot write into the tar directly, since `AVAssetWriter` needs a file of its own and a member's size must be known before its header. A working directory left behind by a crash is cleaned up at launch.
- [x] `CaptureStore` lists archives by extension and reads kind, duration, depth presence, and the interrupted flag from the manifest, and draws thumbnails from the plate or marker image member. Delete removes the one file.
- [x] `CaptureUploader` sends one file per capture to the `captures` folder, keeping the streamed `upload(for:fromFile:)`. The per-sidecar loop and its progress shares go away.
- [x] Loose captures already on the device are packed once at launch by the Part 3 grouping rules, so takes recorded before the update can still be uploaded.
- [x] Debug channel: `flow start <stillphoto|stillcamera|trackcamera>`, `flow advance` (the current step's primary action), `flow cancel`, and `flow state` replace `capture [marker]` and `record start|stop [marker]`. `captures list|upload|delete` operate on archives. Update the MikanXR `automation.md` table that documents the phone's commands.
- [x] Update the README's capture modes, Captures panel, and sidecar sections.

### Part 8: MikanARStreamer guided flows

- [x] `CaptureMode` becomes `stillPhoto | stillCamera | trackCamera | stream`. The marker toggle goes away.
- [x] A capture flow state machine per kind, pure logic with no ARKit or UIKit dependency so it is unit tested like `CaptureStore`, driving a prompt overlay and the primary button:
	- Still Photo: place the marker in view, take the marker photo, remove the marker, take the plate photo, pack.
	- Still Camera: the Still Photo steps, then record until the operator stops. The recording stops itself if the camera moves.
	- Track Camera: place the marker in view, take the marker photo, remove the marker, record with depth on a LiDAR phone, stop, pack.
	- Stream: show "look at the origin marker", poll `GET /arkit/alignment?port=<base port>` about once a second, and hide the prompt once `aligned` is true. While the editor is unreachable the prompt says so and can be dismissed.
- [x] Motion detection for the stationary kinds: compare each frame's ARKit pose with the marker photo's pose and treat a translation or rotation beyond a threshold as moved. In Still Photo, a move between the two photos restarts the flow. In Still Camera, a move stops the recording and the capture is marked `interrupted`. Start from 1 cm and 0.5 degrees and tune on a device against a phone on a tripod, since ARKit's jitter at rest sets the floor. A limited tracking state counts as unknown, not as moved.
- [x] Depth stays off for the marker photo and for both still kinds, as it is for marker takes today.
- [x] Tests: each state machine's transitions, including cancel from every step, and the motion detector against synthetic pose sequences.

Two behaviours Part 8 left to the implementation: a flow interrupted by the app leaving the screen stops and packs its recording, so a take survives a backgrounding, and drops a flow between its shots, since the marker image then describes a camera that is about to be put down. The motion thresholds are the starting values and have not been tuned on a tripod yet.

## Constraints to respect

- The wire protocol is touched once, in Part 2. Read [wire-protocol.md](../reference/wire-protocol.md) before it, wire all three property legs, regenerate the bindings, and never hand-edit anything under `bindings/*/Generated`, `bindings/typescript/types`, or the Refureku output.
- Coordinate conventions have single conversion points ([conventions.md](../reference/conventions.md)). The archive changes where tracks are stored, not what they mean: no transform changes anywhere in this work.
- A capture is never rewritten after it is packed. Anything derived from it goes beside it.
- Code style per [standards.md](../reference/standards.md), no doc citations in code comments, and clang-format 19.1.x before commit.
- Verification is `MikanCmd.exe -runTests`, `unit_test_suite_cpp.exe`, and the schema guard, once in the main session before each commit. The phone parts verify with `xcodebuild test` on the simulator, on the Mac.
- Parts 1, 4, and 7's archive writer are tightly specified by this document and are good candidates for delegation. Part 2's definition and wire change, Part 3's grouping rules, and Part 6's solve stay in the main session.

## Open questions

- Whether trimming by re-recording (Part 5) is enough, or a trim that slices the archive directly is wanted: copying the movie's range without re-encoding, and the pose and depth records by `fileIndex`, which keeps the depth and the ARKit space a re-recording loses.

Resolved as built: a `trackcamera` takes no clean plate, the movie itself is the clean footage, and an interrupted `stillcamera` keeps the frames recorded before the move.

## References

- MikanXR docs: [videosources.md](../reference/videosources.md) (file video sources, pose tracks, depth tracks, uploads, the offset, the marker reference), [occlusion-mesh.md](../reference/occlusion-mesh.md), [objects.md](../reference/objects.md) (asset folders), [automation.md](../reference/automation.md), [calibration.md](../reference/calibration.md) (the marker alignment), [wire-protocol.md](../reference/wire-protocol.md).
- MikanARStreamer: README (capture modes, Captures panel, pose track and depth layouts, debug commands), `StreamingSession.swift`, `MovieRecorder.swift`, `PhotoCapture.swift`, `CaptureStore.swift`, `CaptureUploader.swift`, `ARCaptureController.swift`.
- POSIX ustar header layout: `https://pubs.opengroup.org/onlinepubs/9699919799/utilities/pax.html#tag_20_92_13_06`.
- ffmpeg subfile protocol: `https://ffmpeg.org/ffmpeg-protocols.html#subfile`.

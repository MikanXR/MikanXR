# Video Sources

How camera video gets into and out of the editor: the ECS video-source components, the device-plugin DLL model, frame delivery and texture upload, intrinsics and tracking-pose association, and the shared-texture/Spout paths that cross the process boundary. See [objects.md](./objects.md) for the ECS object model, [calibration.md](./calibration.md) for how intrinsics are computed, [compositor.md](./compositor.md) for how the frames are consumed, and [wire-protocol.md](./wire-protocol.md) for the client-facing request/event types.

---

## Core abstraction

There is no standalone `VideoSourceManager`/`VideoSourceView` layer; video sources are ECS components. `VideoSourceComponent` (`src/Editor/ECS/VideoSource/VideoSourceComponent.h`) is the abstract base, paired with a persisted `VideoSourceDefinition` (mirroring, frame queue size, and the `MikanVideoSourceIntrinsics` calibration blob). Four concrete subclasses exist, each owned by a matching object system:

- `USBVideoSourceComponent` / `USBVideoSourceSystem`: local USB cameras (device path, video mode, per-mode camera settings).
- `NetworkVideoSourceComponent` / `NetworkVideoSourceSystem`: RTMP/RTSP network streams (protocol, address, port, path).
- `ARKitVideoSourceComponent` / `ARKitVideoSourceSystem`: iPhone ARKit streaming from the MikanARStreamer app (a base UDP port; video RTP on `base_port+0`).
- `FileVideoSourceComponent` / `FileVideoSourceSystem`: a recorded movie or a still image played from disk, with an optional pose track beside it (see [File video sources](#file-video-sources)).

The first three talk to hardware through device interfaces defined in `src/Libraries/MikanCoreApp/Public`: `IVideoDevice` (settings, colorimetry) with per-transport extensions `IUsbVideoDevice`, `INetworkVideoDevice`, and `IARKitVideoDevice`, each with a listener interface (`IUsbVideoDeviceListener` etc.) and a manager (`IUsbVideoDeviceManager`, ...). The editor side implements the listener; the plugin side implements the device. The file source has no device: it decodes in-process.

`VideoSourceComponent` exposes the editor-facing surface: `openVideoSource()`/`closeVideoSource()`, `startVideoStream()`/`stopVideoStream()`, `getCameraIntrinsics()`/`setCameraIntrinsics()`, `getProjectionMatrix()`, `getVideoColorimetry()`, and `set/getVideoSetting()` (`eVideoSettingType`: exposure, gain, focus, ...). It is also an `IPropertyInterface`/`IFunctionInterface` participant, so clients can drive it remotely (reflected values structs in `src/Libraries/MikanClientAPI/Public/MikanVideoSourceTypes.h`; server routing in `src/Editor/Server/VideoSourceRequestHandler.cpp`).

---

## Plugin model

Video backends are separate DLLs under `src/Plugins`, loaded on demand by name. The contract lives in `MikanCoreApp`:

- `IMikanModule` (`startup()`/`shutdown()`) plus two exported C functions per plugin, `AllocatePluginModule` and `FreePluginModule` (`IMikanModule.h`).
- Per-domain module interfaces: `IUsbVideoDeviceModule::createUsbVideoDeviceManager()`, `INetworkVideoDeviceModule`, `IVRDeviceModule::createTrackingRuntime()`, and `IARKitVideoDeviceModule`.
- `MikanModuleManager` (`MikanModuleManager.cpp`) caches loaded modules; `MikanModule::load()` uses `dylib` to open the DLL from the executable directory (`PathUtils::getModulePath()`) and resolve the two exports.

A plugin's `startup()`/`shutdown()` may only manage state it exclusively owns. Process-global library state is shared with every other loaded plugin, so no plugin may tear it down. GStreamer is the live case: `MikanGStreamerVideo` and `MikanARKitVideo` both call `gst_init_check()`, whichever loads first does the real initialization, and the second gets a no-op success. Neither calls `gst_deinit()`, in `shutdown()` or on the init-failure path, because GStreamer aborts the process (`GStreamer should not be deinitialized a second time`) when the second plugin tears down. Its resources are reclaimed at process exit instead.

`MikanModuleManager` does not refcount module users. `disposeModule` unloads the DLL on the first call, so a second holder of the same cached module is left with objects allocated inside an unloaded DLL. In the app each module has exactly one owner, so this only constrains tests that stand up two loaders against one module.

There is no directory scan: each object system asks for a hardcoded module name. `USBVideoSourceSystem` loads `"MikanWMFVideo"`, `NetworkVideoSourceSystem` loads `"MikanGStreamerVideo"`, `VRObjectSystem` builds `"Mikan" + runtime name` (currently yielding `"MikanSteamVR"`), and `ARKitVideoDeviceManagerLoader` loads `"MikanARKitVideo"`. Module load and manager startup run on a detached worker thread (see `USBVideoSourceSystem::initUsbVideoDeviceManagerOnThread`, which also enters COM MTA for WMF enumeration); the system polls a future each tick and retries any components left in `isPendingOpen()` once the manager is ready. The manager and its module then live for the process: a project unload disposes the components, which close their devices, but the system keeps the manager and tears it down in its own destructor at app shutdown. Disposing it with the project unloaded the DLL under a receive thread and left the loader state reporting ready with no manager behind it, so the next project could never open a camera. In CMake, `MikanGStreamerVideo` is gated behind `MIKAN_WITH_GSTREAMER` (off in CI), and `MikanARKitVideo` behind `MIKAN_WITH_ARKIT_VIDEO`, which additionally needs a CUDA Toolkit. `MikanSteamVR` and `MikanWMFVideo` always build (`src/Plugins/CMakeLists.txt`).

---

## Backends

- `MikanWMFVideo`: Windows Media Foundation USB capture. `MikanWMFVideoDeviceManager` enumerates devices (`WMFDeviceList`, `WMFDeviceInfo`) and watches hotplug via `DeviceHotplugNotifier`. `WMFVideoFrameProcessor` is an `IMFSourceReaderCallback`; `OnReadSample` decodes/forwards samples to `MikanWMFVideoDevice`, which raises `notifyVideoFrameReceived` with a `UsbVideoFrameBuffer` (formats `RGB24`, `NV12`, `YUY2`, with per-section stride info). Video modes carry `UsbVideoModeProperties` including `VideoColorimetry` (color matrix + transfer function enums in `IVideoDevice.h`). Samples arrive on a Media Foundation work queue thread, so the device's listener set is walked under a mutex that `removeListener` also takes, and stopping the stream is synchronous: `stopVideoFrameStream` issues the reader's asynchronous `Flush` and waits for `OnFlush`, after which the reader delivers nothing more, before `close` releases the reader and the processor. The processor is a COM callback the reader also references, so it is released rather than deleted, and the media source is `Shutdown` so the same camera can be reopened in the process. `open` finds the camera's `IMFActivate` by symbolic link, not by the index it had when the list was enumerated, so a camera plugged or pulled since then does not shift the open onto its neighbour, and a failed open logs the failing stage with the `HRESULT` text. Hotplug runs through a message-only window that `DeviceHotplugNotifier` creates on the first manager `update()` and pumps on every one after, which `USBVideoSourceSystem` calls each tick, so the window lives on the main thread rather than the loader thread that started the manager. Arrival and removal re-enumerate, a removed camera closes its device and drops out of the list, and a source whose camera returns reopens on the next list change.

- `MikanGStreamerVideo`: network streams. `MikanGStreamerVideoDevice` implements `INetworkVideoDevice` for `eNetworkVideoProtocol::RTMP`/`RTSP` URLs built from `NetworkVideoConnectionSettings`; open/close are async (`WorkerThread`, futures) and `update()` polls the pipeline. Camera settings are not applicable and stubbed.

- `MikanARKitVideo`: iPhone ARKit over the network. `MikanARKitVideoDevice` builds a GStreamer RTP receive pipeline (`udpsrc`/`rtpjitterbuffer`/`rtph264depay`/`h264parse`) on `base_port+0`. Camera pose and frame sequence ride inside the video RTP stream's own header extension: `ARKitRTPHeaderExtension` parses them and attaches an `ARKitFrameSeqMeta` to each decoded buffer, so `update()` can emit an `ARKitVideoFrameBundle` (frameSeq, timestamp, optional pose, optional decoded pixels) with pose exactly coupled to its frame. Decode is two-tier: a hardware pipeline (`nvh264dec`, decoded frames stay in CUDA device memory and reach GL via CUDA-GL interop) is tried first, falling back to a software pipeline (`openh264dec`, packed BGR in system memory) when the NVIDIA path fails to build. The wire format is defined in `ARKitWireProtocol.h`.
GStreamer/CUDA caveats baked into the ARKit backend, learned empirically:

- `MikanARKitVideoDevice` owns its own `CUcontext` (lazily created) and re-asserts it with `cuCtxSetCurrent` at the start of every CUDA-touching `update()` tick. A successful `gst_buffer_map(..., GST_MAP_CUDA)` does not leave nvcodec's internal context current on the calling thread, so relying on "whatever context is current" fails with `CUDA_ERROR_INVALID_CONTEXT`. Unified virtual addressing makes the mapped `CUdeviceptr` usable from the device's own context.

- After a fatal CUDA fault (e.g. illegal address reported at synchronize), the context is unrecoverable: make no further Driver API calls against it, including cleanup, and tear the whole session down. A fault can also poison later fresh `cuCtxCreate` calls in the same process, so full process restart is the only fully reliable recovery.

- GStreamer bus messages are polled explicitly per tick (`gst_bus_pop_filtered` in `update()`); `gst_bus_add_watch` is inert in this codebase because nothing pumps a GLib main loop.

- `MikanSteamVR`: not a video backend. `MikanSteamVRModule` implements `IVRDeviceModule` and creates a `MikanSteamVRManager` tracking runtime. It provides `IVRDevice` objects (HMD/tracker/controller poses via `getDevicePose` with frame-delay support, serial/role properties, render meshes via `SteamVRRenderModelResource`, and named attachment sockets). `VRObjectSystem` consumes it to drive `VRDeviceComponent` poses, which video sources use for camera tracking (below).

---

## Open and streaming lifecycle

`openVideoSource()` binds the component to a plugin device (by device path for USB, by URL or port for the network transports); if the async manager is not up yet the component stays `isPendingOpen()` and the system retries. Status is reported via `eVideoOpeningStatus` (the network transports open asynchronously) and `eVideoStreamingStatus` (`stopped`/`pendingStart`/`started`/`failed`).

Streaming is refcounted by consumers: `VideoSourceComponent::startVideoStream(VideoFrameDistortionView*)` adds the view to a mutex-guarded `m_activeViews` set (max 32) and calls the subclass `startVideoStreamInternal()` on the first subscriber; `stopVideoStream()` stops the device when the last view unsubscribes.

`closeVideoSource()` drops every subscriber, since the device is going away. A device swap is not that: `reopenVideoSource()` snapshots the subscriber set, closes, restores the set, and opens, so a stage or compositor watching the source keeps its view and `update()` starts the stream again on the new device. A USB source takes that path when its desired device path changes. Without it the settings stage went black on a swap until it was exited and re-entered, since it subscribes once in `enter()`. The view resizes itself for the new resolution when the first frame arrives (`ensureFrameBufferSize`). The receive thread holds the same mutex for the whole fan-out into the views, so an unsubscribe returns only after any write in progress has finished and the caller may free its view at once; `CompositorComponent::dispose` relies on that to unsubscribe before its view goes away on a project unload. `OnOpened`/`OnClosed`/`OnStarted`/`OnStopped`/`OnFrameSizeChanged`/`OnIntrinsicsChanged` multicast delegates notify the rest of the editor.

---

## Frame delivery into the editor

`VideoFrameDistortionView` (`src/Editor/Calibration/VideoFrameDistortionView.h`) is the per-consumer frame sink and texture pipeline. Device receive threads call `VideoSourceComponent::writeVideoFrame()` (or `writeStereoVideoFrameSection()`), which fans out to every active view's BGR source buffer under a mutex and bumps an atomic write index. On the main thread, `readAndProcessVideoFrame()` detects a new index, converts/undistorts, and uploads into a circular queue of GL textures (`VideoFrameQueueEntry`, queue size from `VideoSourceDefinition::getVideoFrameQueueSize()`); `getVideoTexture(frameIndex)` serves delayed frames so video can be latency-matched against tracking (`CameraDefinition::getTrackingFrameDelay()`). Processing runs in one of two modes (`eVideoFrameProcessorMode`): `CALIBRATION` uses `CVVideoFrameProcessor` (CPU `cv::remap` undistortion plus grayscale buffers for pattern detection) and `COMPOSITOR` uses `GLVideoFrameProcessor` (GPU shader undistortion driven by a distortion-map texture).

GPU-direct sources bypass the CPU buffer entirely: `VideoSourceComponent::getDirectColorTexture()` / `processDirectVideoFrame()` / `getDirectFrameIndex()` default to null/no-op and only `ARKitVideoSourceComponent` overrides them: its NV12 luma/chroma GL textures (exposed by the plugin as raw GL ids) are wrapped in `IMkExternalTexture`s and converted to RGBA by a fullscreen shader pass once per tick.

Calibration needs those pixels on the CPU, so a `CALIBRATION`-mode view reads the converted texture back (`readbackDirectColorTexture`, using `IMkTexture::readTextureIntoBuffer`) and pushes it through `writeVideoFrame`, after which the ordinary undistort and grayscale path runs exactly as it would for a USB camera. A `COMPOSITOR`-mode view never does this: it reads the texture directly, and a readback there would stall every frame for nothing. Note that `getVideoTexture()` returns the direct texture ahead of the queue, so live video on screen is not evidence that the readback is working.

Whatever a source does internally, every video texture reaches `getVideoTexture()` in one orientation, image row 0 at `v=0`. That is what `copyOpenCVMatIntoGLTexture` produces for CPU sources, and the NV12 planes already arrive that way because the plugin copies them with a plain `cuMemcpy2D`. A conversion pass of this kind therefore builds its quad unflipped: it writes a video texture rather than displaying one, and the single V flip belongs to the consumer's own display quad ([conventions.md](./conventions.md)). Flipping in both places cost a period of upside-down video on the hardware decode tier only, which was invisible while a PATH problem was forcing every local run onto the software tier.

---

## Intrinsics, settings, and persistence

Camera calibration data is a `MikanVideoSourceIntrinsics` (reflected wire type, `MikanVideoSourceTypes.h`): a polymorphic pointer to `MikanMonoIntrinsics` or `MikanStereoIntrinsics`, both deriving `MikanBaseIntrinsics` (pixel dimensions, hfov/vfov, znear/zfar). `MikanMonoIntrinsics` carries `MikanDistortionCoefficients` (k1-k6 rational radial + p1/p2 tangential, per the OpenCV model) plus distorted and undistorted camera matrices. It lives on `VideoSourceDefinition` and persists with the rest of the component definition into the project file (Configuru JSON, `*.mikanproj`, saved by `ProjectManager`/`ProjectConfig`). `setCameraIntrinsics()` triggers `recomputeCameraProjectionMatrix()`, so `VideoSourceComponent::getProjectionMatrix()` always reflects current calibration. USB sources additionally persist per-video-mode `eVideoSettingType` values in `USBVideoSourceDefinition`'s `m_videoSettingsMap`. That map is keyed by video mode name alone and two cameras can name the same mode, so `setDevicePath` empties it: a source pointed at a different camera has no settings for the mode it lands on, which is what makes `handleVideoModeUpdated` pull that camera's own driver defaults rather than replay the last camera's values.

Nothing rescales intrinsics, so intrinsics calibrated at one resolution silently break the projection when the source runs another. `getCameraIntrinsicsStatus()` reports that as `uncalibrated` or `resolutionMismatch`, and the video source panels draw it as a warning line (`VideoSourceStatusGui::drawIntrinsicsWarning`). Stereo intrinsics report the per-eye section width rather than the frame width, so they are not compared and always read `ok`.

The depth proxy mesh capture is the one consumer that depends on these intrinsics quantitatively rather than for rendering: it feeds `MikanMonoIntrinsics::hfov` to the model, and the recovered metric scale rides directly on that number. An uncalibrated or wrong FOV silently produces a proxy of the wrong size rather than an error. See [depth-proxy-mesh.md](./depth-proxy-mesh.md).

---

## File video sources

`FileVideoSourceComponent` plays a movie file or shows a still image in place of a live camera, so a shot can be composited and calibrated without the camera or phone running. There is no plugin and no device manager: `MovieDecoder` (`src/Editor/ECS/VideoSource/MovieDecoder.h`) wraps `cv::VideoCapture` over OpenCV's ffmpeg videoio backend for movies and `cv::imread` for stills, and treats a still as a one-frame movie so the component has one path. The backend DLL, `opencv_videoio_ffmpeg4100_64.dll`, ships in the OpenCV package and is copied beside the executables by the Editor's CMake, which is what lets the decoder tests run in CI.

The definition holds the media path, an optional marker reference media path (the same shot with the origin marker in frame), an optional pose track path for each, and a loop flag. Both media rows accept a movie or an image (`MediaAssetReferenceFactory`, the union of the movie and texture patterns), and setting a media path re-derives its track slot: the `<stem>.pose.json` beside the file when there is one, otherwise empty, so a track never lingers under media it does not describe. The slot stays visible and editable for a track that was moved. The `movies` asset folder holds recorded takes and their tracks, and the `textures` folder lists tracks too so a photo's sidecar can live next to it.

Decoding runs on a worker thread that owns the `MovieDecoder` outright. The main thread paces playback in `update()` and talks to the worker through two mailboxes: a request struct (open, seek, decode the next frame, quit) and a single decoded-frame slot. Frames are decoded one ahead; while playing they are read sequentially and a seek happens only on a scrub, a loop wrap, a stop, or when the clock has run more than a second past the decoded frame. A seek carries a generation number so a frame decoded before the seek landed is discarded rather than shown. Delivered pixels go through `writeVideoFrame`, so the undistort, queue, and texture path is exactly the USB camera's.

Playback is a runtime state (`playback_state`, `playback_time`, `duration_seconds` properties and `play`/`pause`/`stop` functions), so the project file never records where a take was scrubbed to. A movie starts playing when it opens, and the clock only advances while a view is subscribed. A paused movie or a still re-emits its current frame at the nominal rate (the movie's frame rate, 30 for a still): `VideoFrameDistortionView` counts every write as a new frame and the compositor and the client frame events are paced by frames arriving, so without re-emission a paused source would freeze the whole loop and a view that subscribed late would never see a frame.

Intrinsics are seeded when the media opens: from the pose track's first record when there is one, otherwise a default set from the frame size when the definition holds none, so a trackless still can enter the align stages, which require valid intrinsics. A calibration the operator ran is never overwritten.

### Pose tracks

A pose track is the JSON sidecar the MikanARStreamer app writes beside every movie and photo it records, `<stem>.pose.json`, read by `PoseTrack` (`src/Editor/ECS/VideoSource/PoseTrack.h`):

```
{
  "format": "mikan-pose-track", "version": 1,
  "session_id": "<one UUID per ARKit world origin>",
  "image_width": 1920, "image_height": 1440,
  "first_capture_timestamp_us": 123456789,
  "frames": [
    {"time_us": 0, "capture_timestamp_us": 123456789,
     "transform": [16 floats, row-major camera-to-world in ARKit world space],
     "fx": 1450.1, "fy": 1450.1, "cx": 960.0, "cy": 720.0}
  ]
}
```

`time_us` is relative to the first appended frame and equals the container's presentation time, because the phone stamps each sample's presentation time with its capture clock and the mp4 rebases to zero. A photo's track has one frame at zero. The phone records a pose only for frames it actually wrote, so the track never drifts from the file. The transform is stored row-major like `ARKitPoseInRTPPayload` and transposed at load. Frames are sorted by time on load.

The component matches each decoded frame to its pose by nearest `time_us` within half a frame period. Frame index is never used: ffmpeg's frame counter is unreliable after a seek. On a hit it publishes the pose through `IFrameCoupledPoseProvider` with intrinsics built from the record's pinhole values (`createMonoIntrinsicsFromPinhole`), and on a miss it keeps the previous pose. The frame sequence it reports is a delivery counter that never resets on seek, loop, or stop, and unlike the ARKit source it never clears the alignment offset on a backwards sequence: a recording cannot restart its tracking session.

Without a track the source offers no pose at all, and `CameraComponent` and the align stages treat it as a fixed camera.

### Uploading takes from the phone

The MikanARStreamer app's Captures panel sends a take and its sidecar to the editor over the HTTP message server, one file per request:

```
POST /assets/upload?folder=<folderId>&name=<fileName>    body: the raw file bytes
```

`AssetUploadRequestHandler` (`src/Editor/Server/AssetUploadRequestHandler.h`) owns the route. A movie lands in `movies`, a photo in `textures`, and a `.pose.json` goes to its media's folder, since both folders accept pose tracks. The file type is judged by the name's extension against the folder's asset factories, the same gate the Assets panel's Add button applies. A file of the same name is replaced in place rather than given the numeric suffix `ProjectAssetCatalog::importAsset` applies to a colliding import: the same name is the same take sent again, and a suffix would part a take from its sidecar. Replacing a take a `FileVideoSourceComponent` is playing fails with 409, since the reader holds the file open. The reply is `{"storedPath": "movies/take.mp4", "replaced": false}` on 200 and `{"error": "..."}` otherwise.

The body is read on the connection thread and written to a hidden temporary file beside its destination, and only the validation and the final move into place cross the main thread, so an upload never stalls a frame. The server buffers a body in memory before the route sees it, so one upload transiently costs a few times its size in RAM, and a request has 300 s to arrive in full before the connection is dropped without a reply.

The server binds loopback only until "Allow connections from other machines" is on in the HTTP Triggers panel (`httpServerAllowRemote` in the app settings), which is what a phone on the LAN needs. The first bind on all interfaces prompts Windows Firewall for `Mikan.exe`. A same-machine test needs no setting:

```
curl -X POST --data-binary @take.mp4 "http://127.0.0.1:8090/assets/upload?folder=movies&name=take.mp4"
```

### Recording from the editor

The video source settings stage records the live feed of any source into the same files: Record and Stop write `movies/take_YYYYMMDD_HHMMSS.mp4`, Capture Image writes `textures/take_YYYYMMDD_HHMMSS.jpg`, and each gets a `<stem>.pose.json` sidecar when a camera names the source through `video_source_id`. The Marker reference toggle arms the next capture as `<take>_marker` of the most recent main take, as on the phone, and every take in one stage visit shares a session id. `VideoSourceRecorder` (`src/Editor/AppStages/VideoSourceSettings/VideoSourceRecorder.h`) owns the work, and the stage answers `record_start [marker]`, `record_stop`, `capture_image [marker]`, and `get_recording_state` over the automation channel ([automation.md](./automation.md)).

Frames are recorded undistorted. The recorder owns a second `VideoFrameDistortionView` in calibration mode for the duration of a capture, which undistorts on the CPU into a BGR buffer every tick, and the sidecar carries the source's undistorted camera matrix, so a played take is a pinhole camera exactly as the format assumes. The pose written per frame is the camera's stage-space aperture pose at the moment the frame was taken, whether it comes from a tracking mount, a frame-coupled source, or an authored transform. A track recorded this way therefore plays back with no alignment step and an identity `pose_offset`, unlike a phone take, whose poses live in ARKit world space until the marker alignment solves the offset. Without a camera, with an uncalibrated source, or with stereo intrinsics the media is still written and the status line says why the sidecar was not.

Frames are paired with their pose on the main thread and handed to an encoder thread over a bounded queue. A frame the queue cannot take is dropped together with its pose, so the movie and the sidecar stay in lockstep, and `time_us` is the constant-rate presentation time of each written frame. The movie is written by `MovieWriter` (`src/Editor/ECS/VideoSource/MovieWriter.h`) through `cv::VideoWriter`, trying the Media Foundation plugin's H.264 first (the ffmpeg DLL's own H.264 route needs an OpenH264 DLL that does not ship), then ffmpeg MPEG-4 part 2, then MJPG in an avi. The Media Foundation writer lives in `opencv_videoio_msmf4100_64.dll`, which the Editor's CMake copies beside the executables with the ffmpeg DLL. `PoseTrackWriter` in `PoseTrack.h` writes the sidecar with the phone's keys.

---

## Tracking pose association

A `CameraComponent` (`src/Editor/ECS/Camera/CameraComponent.h`) ties everything together via IDs on `CameraDefinition`: a `MikanVideoSourceID` (which video source feeds it), a `MikanTrackingMountID` (which physical tracker rig it sits on), and an aperture pose offset (tracker-to-lens transform produced by alignment calibration, see [calibration.md](./calibration.md)). `TrackingMountComponent` binds a VR device path plus an attachment socket name and produces a `VRDevicePoseView`; `CameraComponent::updateAperturePoseFromTrackingMount()` polls it each tick and composes the aperture offset to get the stage-space camera pose (`getStageSpaceAperturePose`, `getApertureViewMatrix`, `getApertureProjectionMatrix`).

The ARKit source instead implements `IFrameCoupledPoseProvider` (`src/Editor/ECS/Camera/IFrameCoupledPoseProvider.h`): pose and intrinsics arrive coupled to each video frame in the RTP header extension, and `getLatestFrameCoupledPose()` hands `CameraComponent` a camera-to-world transform plus a frame sequence number for correlation, with intrinsics applied only when they change meaningfully. That change test compares focal length and principal point both, so a correction to either one propagates.

### ARKit world-to-stage offset

ARKit picks a fresh world origin every session, so a streamed pose only means something once it is anchored to the stage. The operator anchors it with the ordinary Align Camera action (`AppStage_AlignCameraByOriginMarker`, see [calibration.md](./calibration.md)), and the phone stays unaware that stage space exists.

The stage detects that its video source is an `IFrameCoupledPoseProvider` and takes a different path than it does for a tripod camera. A static camera's aperture-relative marker poses can be averaged directly. A handheld one moves between samples, so each sample is first converted into the quantity that is actually constant, `worldToStage = inverse(markerInCamera) * inverse(cameraInWorld)`, using the ARKit pose from that same frame. Those are averaged (translation componentwise, rotation through quaternion slerp) and handed to `setPoseOffset` instead of `setRelativeTransform`, which a frame-coupled camera would overwrite on the next bundle.

The solve reads the pose through `getLatestSourceWorldPose`, which reports it in ARKit's own world space, rather than `getLatestFrameCoupledPose`, which reports it in stage space with the current offset already applied. Solving from the latter folds the stored offset into the new one, so the first alignment on a fresh project looks right and every one after it compounds. Reproducibility is the test that catches this: two alignments in a row must agree on the offset even when the phone has moved between them.

The offset is applied at exactly one point per source: `ARKitVideoSourceComponent::notifyFrameBundleReceived`, right after the row-major to column-major transpose of the incoming transform, and `FileVideoSourceComponent::publishPoseForFrame` for a recorded take. It persists on the definition as plain JSON with no property descriptor, since no client application has any use for it and a descriptor would mean a wire change. An absent key means never aligned, and the offset is identity until then.

A stored offset describes the ARKit session it was solved in. A `frameSeq` that runs backwards means the phone restarted that session, so the ARKit source drops the offset and logs the reason rather than leaving the camera confidently in the wrong place.

### Marker reference for recorded takes

A recorded take usually keeps the origin marker out of shot, so the file source can carry a second recording of the same shot with the marker in frame. `IAlignmentReferenceSource` (`src/Editor/ECS/Camera/IAlignmentReferenceSource.h`) is the runtime mixin the two marker alignment stages discover by `dynamic_cast`, beside `IFrameCoupledPoseProvider`. On entry a stage calls `beginAlignmentReference()`, which switches the source's frames and pose track to the marker media, looping from the start, without dropping the stage's own view subscription (a close and reopen would). On exit `endAlignmentReference()` restores the main media and the playback position it had. Which media is active is stage state rather than a property, so a cancelled stage cannot leave the project pointing at the reference file.

The reference is only usable when the solved offset applies to the main take: the source refuses it when the main take carries a pose track but the reference does not, or when the two tracks name different `session_id`s, since an offset solved in one ARKit world means nothing in another. Both cases are logged at open. A handheld take therefore records its marker reference in the same app run, which the phone's marker toggle is for. A tripod take without tracks needs no session match at all: the stage takes the fixed camera path and places the camera from the reference frames directly.

---

## ARKit debug side channel

Separate from the video path and independent of it: `ARKitDebugChannel` (`src\Editor\Interprocess\ARKitDebugChannel.h`) is a TCP line channel to the MikanARStreamer app, exposed as the automation server's `arkit` namespace ([automation.md](./automation.md)). The phone dials out, because the editor cannot learn the phone's address from a GStreamer `udpsrc`. Diagnostics the phone pushes are re-emitted through the editor's logger, which is what puts phone-side encode timing and editor-side receive timing on one ordered timeline. Commands travel the other way, with the phone owning the vocabulary.

It has no dependency on `ARKitVideoSourceComponent` or the plugin, and works whether or not video is streaming. It also binds every interface rather than loopback, so it is off unless enabled.

---

## ARKit receive tuning

The RTP receive pipeline sets `udpsrc buffer-size` explicitly. A keyframe is an order of magnitude larger than a P-frame and leaves the phone as one back-to-back burst of hundreds of RTP packets, while the socket is drained once per render tick. On the OS default receive buffer that burst overruns, the keyframe arrives incomplete, and the decoder has no reference until the next IDR, so a single lost keyframe costs a whole keyframe interval of video. P-frames are small enough never to trigger it, which makes the symptom look periodic rather than load related.

Measured at 1920x1440 and 30fps: with the default buffer, every stall was a lost keyframe and lasted exactly one 4s keyframe period. With the buffer sized to hold several bursts, 6621 consecutive frames across roughly 55 keyframe periods arrived with zero loss and no stall over 100ms.

The appsink's `max-buffers` is the other tuned value, and it matters more than it looks. The sink is polled once per render tick with `drop=TRUE`, so a single slot loses every frame but the newest whenever more than one arrives between polls. That is not a starved consumer: the losses are overwhelmingly isolated single frames, the render loop has ample headroom, and the identical pipeline into a standalone `fakesink` sustains the full source rate. The hardware decoder delivers burstier than the software one, so the cap bites hardest where throughput matters. Going from one slot to three, measured on `nvh264dec`:

- 30fps: 27.65 to 29.97 fps received, 7.66 percent to 0.13 percent lost
- 60fps: 57.42 to 59.50 fps received, 3.43 percent to 0.11 percent lost

Latency stays bounded because the poll runs faster than frames arrive, so the queue drains rather than accumulating, and `drop=TRUE` caps the worst case at three frames.

The dominant latency term was never on this side of the wire. VideoToolbox selects the H.264 profile level automatically, and the level implies a decoded picture buffer of `min(MaxDpbMbs / PicSizeInMbs, 16)` frames: at 1920x1440 that is 10 frames under Level 5.0 and 16 under Level 5.1. The phone encodes with `AllowFrameReordering` off, so nothing in the stream needs that buffer, but the SPS carries no VUI `bitstream_restriction` saying so and hardware decoders honour the level default. Each IDR then flushes the buffer, which is where the once-per-keyframe stall came from. `openh264dec` ignores the declaration and never showed either symptom, which is what identified the bitstream rather than either decoder as the cause.

The phone now rewrites the SPS before sending it (`H264SPSRewriter` in MikanARStreamer), declaring `max_num_reorder_frames = 0` and `max_dec_frame_buffering = max_num_ref_frames`. The statement is true rather than merely convenient, so encoded picture data is unchanged and only the SPS grows, by two bytes per keyframe. Measured through `nvh264dec` on the same stream:

- 30fps: p50 latency 333.5ms to 33.8ms, standard deviation 101.3ms to 4.2ms, stalls over 100ms from 1.00/s to none in steady state
- 60fps: p50 latency 268.2ms to 17.1ms, full 60.0fps delivered

The consistency matters as much as the magnitude. Before the rewrite only 48.6 percent of frames landed within a frame period of the median latency, because each IDR dumped the buffer and then refilled it; a sawtooth like that cannot be compensated by a fixed offset the way a stable delay can. Afterwards 99.4 percent do.

---

## Crossing the process boundary

Pixel data never travels over the websocket; only events and requests do (see [wire-protocol.md](./wire-protocol.md)).

- Editor to client: when the compositor processes a new video frame, `CompositorComponent` builds a `MikanCameraNewFrameEvent` via `CameraComponent::makeNewCameraFrameEvent()` (camera pose, frame index, dimensions) and `CameraRequestHandler::publishCameraNewFrameEvent()` broadcasts it as JSON. Clients use it to render their scene for that frame. A camera whose frame sync mode resolves to free running gets a `MikanCameraNewPropertiesEvent` on change instead, and its clients render on their own clock and publish with frame index -1 ([wire-protocol.md](./wire-protocol.md)).

- Client to editor: clients allocate a shared render target through `MikanClientCore` using `createSharedTextureWriteAccessor` (`src/Libraries/MikanSharedTexture/Public/SharedTextureWriter.h`; color, optional depth and shadow buffers per `SharedTextureDescriptor`, OpenGL, D3D11, D3D12, and Vulkan writers, D3D9 and Metal being enum values with no writer behind them). On a `PublishCameraRenderTargetTextures` request, `CameraRequestHandler::frameRenderedHandler` reads the textures through a per-client, per-camera `SharedTextureReadAccessor` (`src/Editor/Interprocess/SharedTextureReader.h`) and hands them to the compositor.

- Composited output: `CompositorComponent` can publish the final composited frame as a named Spout sender (`k_spoutEnableOutputNamePropertyId` / spout output name on `CompositorDefinition`), using the same `ISharedTextureWriteAccessor` API. The `MikanSharedTexture` library is itself implemented on Spout (`Private/SpoutGL`, `Private/SpoutDX`), so both the client render-target path and the Spout output path are Spout shared textures underneath.

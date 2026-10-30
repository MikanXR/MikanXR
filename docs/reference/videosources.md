# Video Sources

How camera video gets into and out of the editor: the ECS video-source components, the device-plugin DLL model, frame delivery and texture upload, intrinsics and tracking-pose association, and the shared-texture/Spout paths that cross the process boundary. See [objects.md](./objects.md) for the ECS object model, [calibration.md](./calibration.md) for how intrinsics are computed, [compositor.md](./compositor.md) for how the frames are consumed, and [wire-protocol.md](./wire-protocol.md) for the client-facing request/event types.

---

## Core abstraction

There is no standalone `VideoSourceManager`/`VideoSourceView` layer; video sources are ECS components. `VideoSourceComponent` (`src/Editor/ECS/VideoSource/VideoSourceComponent.h`) is the abstract base, paired with a persisted `VideoSourceDefinition` (mirroring, frame queue size, and the `MikanVideoSourceIntrinsics` calibration blob). Four concrete subclasses exist, each owned by a matching object system:

- `USBVideoSourceComponent` / `USBVideoSourceSystem`: local USB cameras (device path, video mode, per-mode camera settings).
- `NetworkVideoSourceComponent` / `NetworkVideoSourceSystem`: RTMP/RTSP network streams (protocol, address, port, path).
- `ARKitVideoSourceComponent` / `ARKitVideoSourceSystem`: iPhone ARKit streaming from the MikanARStreamer app (a base UDP port; video RTP on `base_port+0`).
- `FileVideoSourceComponent` / `FileVideoSourceSystem`: a recorded shot played from disk, as a capture archive carrying its media, marker reference, and pose tracks, or as a raw movie or image (see [File video sources](#file-video-sources)).

The first three talk to hardware through device interfaces defined in `src/Libraries/MikanCoreApp/Public`: `IVideoDevice` (settings, colorimetry) with per-transport extensions `IUsbVideoDevice`, `INetworkVideoDevice`, and `IARKitVideoDevice`, each with a listener interface (`IUsbVideoDeviceListener` etc.) and a manager (`IUsbVideoDeviceManager`, ...). The editor side implements the listener; the plugin side implements the device. The file source has no device: it decodes in-process.

`VideoSourceComponent` exposes the editor-facing surface: `openVideoSource()`/`closeVideoSource()`, `startVideoStream()`/`stopVideoStream()`, `getCameraIntrinsics()`/`setCameraIntrinsics()`, `getProjectionMatrix()`, `getVideoColorimetry()`, and `set/getVideoSetting()` (`eVideoSettingType`: exposure, gain, focus, ...). It is also an `IPropertyInterface`/`IFunctionInterface` participant, so clients can drive it remotely (reflected values structs in `src/Libraries/MikanClientAPI/Public/MikanVideoSourceTypes.h`; server routing in `src/Editor/Server/VideoSourceRequestHandler.cpp`).

---

## Plugin model

Video backends are separate DLLs under `src/Plugins`, loaded on demand by name. The contract lives in `MikanCoreApp`:

- `IMikanModule` (`startup()`/`shutdown()`) plus two exported C functions per plugin, `AllocatePluginModule` and `FreePluginModule` (`IMikanModule.h`).
- Per-domain module interfaces: `IUsbVideoDeviceModule::createUsbVideoDeviceManager()`, `INetworkVideoDeviceModule::createNetworkVideoDeviceManager()` plus `createRtpVideoSender()` (an `IRtpVideoSender` that encodes editor RGBA frames to H.264 RTP over UDP, so the editor reaches GStreamer encoding without linking it), `IVRDeviceModule::createTrackingRuntime()`, and `IARKitVideoDeviceModule`.
- `MikanModuleManager` (`MikanModuleManager.cpp`) caches loaded modules; `MikanModule::load()` uses `dylib` to open the DLL from the executable directory (`PathUtils::getModulePath()`) and resolve the two exports.

`MikanGStreamerVideo` and `MikanARKitVideo` link the GStreamer runtime implicitly, and the build does not copy it next to the executable, so its DLLs have to be reachable through the standard search order before either plugin loads. `NetworkVideoSourceSystem` and `ARKitVideoDeviceManagerLoader` both gate on `GStreamerUtils::ensureRuntimeLoadable()` (`MikanUtility`) before launching their async load. On Windows it takes the first root whose `bin/libgstreamer-1.0-0.dll` exists, trying `GSTREAMER_1_0_ROOT_MINGW_X86_64` and then the MSI's `HKLM\SOFTWARE\GStreamer1.0\x86_64` `InstallDir` with `1.0\mingw_x86_64` appended. It then prepends that `bin` to the process `PATH` once. `PATH` rather than `AddDllDirectory`, because the plugins load through plain `LoadLibrary` and GStreamer's `gst-plugin-scanner.exe` child process inherits `PATH`. No root means the loader logs "GStreamer not installed" and its manager stays failed. On other platforms the system loader finds GStreamer and the call succeeds without doing anything.

A plugin's `startup()`/`shutdown()` may only manage state it exclusively owns. Process-global library state is shared with every other loaded plugin, so no plugin may tear it down. GStreamer is the live case: `MikanGStreamerVideo` and `MikanARKitVideo` both call `gst_init_check()`, whichever loads first does the real initialization, and the second gets a no-op success. Neither calls `gst_deinit()`, in `shutdown()` or on the init-failure path, because GStreamer aborts the process (`GStreamer should not be deinitialized a second time`) when the second plugin tears down. Its resources are reclaimed at process exit instead.

`MikanModuleManager` does not refcount module users. `disposeModule` unloads the DLL on the first call, so a second holder of the same cached module is left with objects allocated inside an unloaded DLL. In the app each module has exactly one owner, so this only constrains tests that stand up two loaders against one module.

There is no directory scan: each object system asks for a hardcoded module name. `USBVideoSourceSystem` loads `"MikanWMFVideo"`, `NetworkVideoSourceSystem` loads `"MikanGStreamerVideo"`, `VRObjectSystem` builds `"Mikan" + runtime name` (currently yielding `"MikanSteamVR"`), and `ARKitVideoDeviceManagerLoader` loads `"MikanARKitVideo"`. Module load and manager startup run on a detached worker thread (see `USBVideoSourceSystem::initUsbVideoDeviceManagerOnThread`, which also enters COM MTA for WMF enumeration); the system polls a future each tick and retries any components left in `isPendingOpen()` once the manager is ready. The manager and its module then live for the process: a project unload disposes the components, which close their devices, but the system keeps the manager and tears it down in its own destructor at app shutdown. Disposing it with the project unloaded the DLL under a receive thread and left the loader state reporting ready with no manager behind it, so the next project could never open a camera. In CMake, `MikanGStreamerVideo` is gated behind `MIKAN_WITH_GSTREAMER` (`AUTO` by default, building it when GStreamer is installed, and `OFF` in CI), and `MikanARKitVideo` behind `MIKAN_WITH_ARKIT_VIDEO`, which additionally needs a CUDA Toolkit. `MikanSteamVR` and `MikanWMFVideo` always build (`src/Plugins/CMakeLists.txt`).

---

## Backends

- `MikanWMFVideo`: Windows Media Foundation USB capture. `MikanWMFVideoDeviceManager` enumerates devices (`WMFDeviceList`, `WMFDeviceInfo`) and watches hotplug via `DeviceHotplugNotifier`. `WMFVideoFrameProcessor` is an `IMFSourceReaderCallback`; `OnReadSample` decodes/forwards samples to `MikanWMFVideoDevice`, which raises `notifyVideoFrameReceived` with a `UsbVideoFrameBuffer` (formats `RGB24`, `NV12`, `YUY2`, with per-section stride info). Video modes carry `UsbVideoModeProperties` including `VideoColorimetry` (color matrix + transfer function enums in `IVideoDevice.h`). Samples arrive on a Media Foundation work queue thread, so the device's listener set is walked under a mutex that `removeListener` also takes, and stopping the stream is synchronous: `stopVideoFrameStream` issues the reader's asynchronous `Flush` and waits for `OnFlush`, after which the reader delivers nothing more, before `close` releases the reader and the processor. The processor is a COM callback the reader also references, so it is released rather than deleted, and the media source is `Shutdown` so the same camera can be reopened in the process. `open` finds the camera's `IMFActivate` by symbolic link, not by the index it had when the list was enumerated, so a camera plugged or pulled since then does not shift the open onto its neighbour, and a failed open logs the failing stage with the `HRESULT` text. Hotplug runs through a message-only window that `DeviceHotplugNotifier` creates on the first manager `update()` and pumps on every one after, which `USBVideoSourceSystem` calls each tick, so the window lives on the main thread rather than the loader thread that started the manager. Arrival and removal re-enumerate, a removed camera closes its device and drops out of the list, and a source whose camera returns reopens on the next list change.

- `MikanGStreamerVideo`: network streams. `MikanGStreamerVideoDevice` implements `INetworkVideoDevice` for `eNetworkVideoProtocol::RTMP`/`RTSP` URLs built from `NetworkVideoConnectionSettings`; open/close are async (`WorkerThread`, futures) and `update()` polls the pipeline. Camera settings are not applicable and stubbed.

- `MikanARKitVideo`: iPhone ARKit over the network. `MikanARKitVideoDevice` builds a GStreamer RTP receive pipeline (`udpsrc`/`rtpjitterbuffer`/`rtph264depay`/`h264parse`) on `base_port+0`. Camera pose and frame sequence ride inside the video RTP stream's own header extension: `ARKitRTPHeaderExtension` parses them and attaches an `ARKitFrameSeqMeta` to each decoded buffer, so `update()` can emit an `ARKitVideoFrameBundle` (frameSeq, timestamp, optional pose, optional decoded pixels) with pose exactly coupled to its frame. Decode is two-tier: a hardware pipeline (`nvh264dec`, decoded frames stay in CUDA device memory and reach GL via CUDA-GL interop) is tried first, falling back to a software pipeline (`openh264dec`, packed BGR in system memory) when the NVIDIA path fails to build. The wire format is defined in `ARKitWireProtocol.h`.
GStreamer/CUDA caveats baked into the ARKit backend, learned empirically:

- From CUDA 13, `cuda.lib` is a static loader that resolves Driver API entry points from `nvcuda.dll` at runtime instead of an import library, and a driver from an older CUDA major release faults inside `nvcuda.dll` rather than returning an error. `MikanARKitVideoModule::startup()` therefore reads `cuDriverGetVersion` straight from `nvcuda.dll` (`isCudaDriverCompatible`, `Cuda/CudaDriverVersion.h`) before any other CUDA call and fails with a log line naming both versions when the driver's major version is below the Toolkit's. The ARKit manager then reads as failed and the rest of the editor runs on. The driver read itself is `CudaDriverUtils::getInstalledCudaDriverVersion()` (`MikanUtility`), so the editor makes it too: once the manager has failed and the project holds an ARKit source, `ARKitVideoSourceSystem` compares the driver with the plugin's Toolkit version (`MIKAN_ARKIT_CUDA_VERSION`, defined by CMake) and shows a modal the user dismisses, at most once per process, worded for an old driver or for none. The `arkit_cuda_gl_interop` unit test skips on the same check.

- `MikanARKitVideoDevice` owns its own `CUcontext` (lazily created) and re-asserts it with `cuCtxSetCurrent` at the start of every CUDA-touching `update()` tick. A successful `gst_buffer_map(..., GST_MAP_CUDA)` does not leave nvcodec's internal context current on the calling thread, so relying on "whatever context is current" fails with `CUDA_ERROR_INVALID_CONTEXT`. Unified virtual addressing makes the mapped `CUdeviceptr` usable from the device's own context. The context is created on the CUDA device the editor's GL context runs on (`findCudaDeviceForCurrentGLContext`, `cuGLGetDevices`), not on device 0, since CUDA can only share textures with a GL context on its own device. When GL runs on a GPU with no CUDA device, such as the integrated GPU of a hybrid laptop, the plugin logs that once and shows no ARKit video (see [debugging.md](./debugging.md) for the discrete GPU request that prevents it).

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

GPU-direct sources bypass the CPU buffer entirely: `VideoSourceComponent::getDirectColorTexture()` / `processDirectVideoFrame()` / `getDirectFrameIndex()` default to null/no-op and only `ARKitVideoSourceComponent` overrides them: its NV12 luma/chroma GL textures (exposed by the plugin as raw GL ids) are wrapped in `IMkExternalTexture`s and converted to RGBA by a fullscreen shader pass once per tick. A hardware-decoded bundle carries no CPU pixels, so that pass is also where the source learns its frame size: without it `getVideoPixelDimensions` answers false on the hardware tier, and every consumer of the size (the compositor's buffers, `DepthMaskNode`, the client and phone size reports, click to focus) falls back or fails.

Calibration needs those pixels on the CPU, so a `CALIBRATION`-mode view reads the converted texture back (`readbackDirectColorTexture`, using `IMkTexture::readTextureIntoBuffer`) and pushes it through `writeVideoFrame`, after which the ordinary undistort and grayscale path runs exactly as it would for a USB camera. Its `getVideoTexture()` returns the direct texture ahead of the queue, so live video on screen there is not evidence that the readback is working. A `COMPOSITOR`-mode view never reads back, since that would stall every frame. It runs the converted texture through the same GPU undistortion pass an uploaded CPU frame takes, into its frame queue under the source's frame index.

Every source texture holds image row 0 at `v=0`: `copyOpenCVMatIntoGLTexture` produces it for CPU sources, and the NV12 planes already arrive that way because the plugin copies them with a plain `cuMemcpy2D`, so the NV12 conversion builds its quad unflipped. The compositor's undistortion pass then writes through a v-flipped quad, which every video layer undoes with `vertical_flip` ([conventions.md](./conventions.md)). Before the GPU-direct path took that pass too, the two ARKit decode tiers composited upside down relative to each other, and a graph authored on one tier was inverted on the other. Launching `Mikan.exe` from Git Bash selects the software tier ([debugging.md](./debugging.md)), which is how that showed.

---

## Intrinsics, settings, and persistence

Camera calibration data is a `MikanVideoSourceIntrinsics` (reflected wire type, `MikanVideoSourceTypes.h`): a polymorphic pointer to `MikanMonoIntrinsics` or `MikanStereoIntrinsics`, both deriving `MikanBaseIntrinsics` (pixel dimensions, hfov/vfov, znear/zfar). `MikanMonoIntrinsics` carries `MikanDistortionCoefficients` (k1-k6 rational radial + p1/p2 tangential, per the OpenCV model) plus distorted and undistorted camera matrices. It lives on `VideoSourceDefinition` and persists with the rest of the component definition into the project file (Configuru JSON, `*.mikanproj`, saved by `ProjectManager`/`ProjectConfig`). `setCameraIntrinsics()` triggers `recomputeCameraProjectionMatrix()`, so `VideoSourceComponent::getProjectionMatrix()` always reflects current calibration. USB sources additionally persist per-video-mode `eVideoSettingType` values in `USBVideoSourceDefinition`'s `m_videoSettingsMap`. That map is keyed by video mode name alone and two cameras can name the same mode, so `setDevicePath` empties it: a source pointed at a different camera has no settings for the mode it lands on, which is what makes `handleVideoModeUpdated` pull that camera's own driver defaults rather than replay the last camera's values.

Nothing rescales intrinsics, so intrinsics calibrated at one resolution silently break the projection when the source runs another. `getCameraIntrinsicsStatus()` reports that as `uncalibrated` or `resolutionMismatch`, and the video source panels draw it as a warning line (`VideoSourceStatusGui::drawIntrinsicsWarning`). Stereo intrinsics report the per-eye section width rather than the frame width, so they are not compared and always read `ok`.

The depth proxy mesh capture is the one consumer that depends on these intrinsics quantitatively rather than for rendering: it feeds `MikanMonoIntrinsics::hfov` to the model, and the recovered metric scale rides directly on that number. An uncalibrated or wrong FOV silently produces a proxy of the wrong size rather than an error. See [depth-proxy-mesh.md](./depth-proxy-mesh.md).

---

## File video sources

`FileVideoSourceComponent` plays a recorded shot in place of a live camera, so it can be composited and calibrated without the camera or phone running. There is no plugin and no device manager: `MovieDecoder` (`src/Editor/ECS/VideoSource/MovieDecoder.h`) wraps `cv::VideoCapture` over OpenCV's ffmpeg videoio backend for movies and `cv::imdecode` or `cv::imread` for stills, and treats a still as a one-frame movie so the component has one path. The backend DLL, `opencv_videoio_ffmpeg4100_64.dll`, ships in the OpenCV package and is copied beside the executables by the Editor's CMake, which is what lets the decoder tests run in CI.

The definition holds the media path, a pose track path, and a loop flag. The media is a capture archive (`CaptureAssetReference`, the `captures` folder, below), or a raw movie or image from `movies` or `textures` for a shot where tracking does not matter. The media row accepts all three (`MediaAssetReferenceFactory`, the union of the capture, movie, and texture patterns). The pose track path is an override for a `trackcamera` capture or the track of a raw movie, set by hand, and is empty otherwise: a capture carries its own tracks, so naming new media never fills it in. Its row's right-click menu clears it. An override whose session differs from the capture's is logged and ignored.

A capture plays by its kind. A `stillphoto` shows its plate image. A `stillcamera` plays its movie with the camera fixed. A `trackcamera` plays its movie with the per-frame poses of its track, or of the override when one is set. An override belongs to the take it was recorded with: when the media loads as a capture of another session, or as a capture that is not a `trackcamera`, the override is dropped from the definition rather than carried along, so a source repointed at a new take starts from that take's own track. In all three the marker image is the alignment reference (see [Marker reference for recorded takes](#marker-reference-for-recorded-takes)). The capture's members are read in place: a movie member plays through ffmpeg's `subfile` protocol with the member's byte range in the URL (`Capture::makeSubfileUrl`, `MovieDecoder::openMovieUrl`), and an image member decodes from its bytes (`MovieDecoder::openEncodedStill`), so nothing is extracted and no cache exists. A `stage` capture, one the editor recorded, reports itself aligned with an identity offset and refuses another.

Decoding runs on a worker thread that owns the `MovieDecoder` outright. The main thread opens the capture, paces playback in `update()`, and talks to the worker through two mailboxes: a request struct (open a path, a `subfile` URL, or encoded still bytes; seek; decode the next frame; quit) and a single decoded-frame slot. Frames are decoded one ahead; while playing they are read sequentially and a seek happens only on a scrub, a loop wrap, a stop, or when the clock has run more than a second past the decoded frame. A seek carries a generation number so a frame decoded before the seek landed is discarded rather than shown. Delivered pixels go through `writeVideoFrame`, so the undistort, queue, and texture path is exactly the USB camera's.

Playback is a runtime state (`playback_state`, `playback_time`, `duration_seconds` properties and `play`/`pause`/`stop`/`step_back`/`step_forward` functions), so the project file never records where a take was scrubbed to. A movie starts playing when it opens, and the clock only advances while a view is subscribed. A paused movie or a still re-emits its current frame at the nominal rate (the movie's frame rate, 30 for a still): `VideoFrameDistortionView` counts every write as a new frame and the compositor and the client frame events are paced by frames arriving, so without re-emission a paused source would freeze the whole loop and a view that subscribed late would never see a frame. A step seeks one frame period from the frame on screen and holds the source paused there, so a moving take can be checked frame by frame: the frame arrives with its own pose, and a paused source's re-emission cycles the compositor's queue until the composite shows it.

Intrinsics are seeded when the media opens: from the movie track's first record when there is one, else from the marker track's, whose pinhole values are the camera's even when the camera is treated as fixed, otherwise a default set from the frame size when the definition holds none, so a trackless still can enter the align stages, which require valid intrinsics. A calibration the operator ran is never overwritten.

### Capture archives

A capture is one shot as one file: a POSIX ustar archive, uncompressed, holding the media, the marker reference, and the pose and depth tracks, with a manifest saying what kind of shot it is. The library is `src/Editor/Capture`: `CaptureArchiveReader` and `CaptureArchiveWriter` over the accepted ustar subset, `CaptureManifest`, and `Capture`, the opened archive. The editor writes captures through the writer (the one-time packing below and the recorder) and the phone writes the same subset in Swift, so the reader accepts exactly what those writers produce and rejects everything else with a message:

- regular-file members only, no directories, links, or pax and GNU extension headers
- member names of at most 100 ASCII characters, with no path separators
- members under the ustar size limit of 8 GiB
- the manifest `capture.json` as the first member

Every member's data starts on a 512-byte boundary at a fixed offset, which is what lets the movie play through `subfile`, `DepthTrackReader` seek to a record, and an image or JSON member be read into memory with one seek. No compression is applied: the media is already compressed, and compressing the depth would cost the fixed offsets. A capture is written once and never modified, which keeps it safe to hold open for playback. Anything derived from it goes beside it in `captures`.

The file extension names the kind and only routes the file to the `captures` folder. The manifest's `kind` is what the editor acts on, and a manifest that disagrees with the extension is rejected:

```
{
  "format": "mikan-capture", "version": 1,
  "kind": "trackcamera",
  "session_id": "<the ARKit world origin's UUID, or the editor recorder's session id>",
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

- `members` maps roles to member names, so an image may be `.jpg` or `.png`. The roles are `plate_image`, `marker_image`, `marker_pose`, `movie`, `movie_pose`, `depth`, and `confidence`. Depth and confidence come together or not at all, and need a `movie_pose` to declare their record size.
- `pose_space` is `arkit_world` for a phone capture, whose poses need the marker alignment's offset, or `stage` for an editor recording, whose poses are stage space already.
- `session_id` is the one session every pose track in the archive was recorded in. `Capture::open` rejects an archive whose member tracks name a different session, which is what lets an offset solved from the marker track apply to the movie track.
- `interrupted` is true on a `stillcamera` the phone stopped because the camera moved.

| kind | camera | required | optional |
|---|---|---|---|
| `stillphoto` | fixed | `plate_image` | `marker_image`, `marker_pose` |
| `stillcamera` | fixed | `movie` | `plate_image`, `marker_image`, `marker_pose` |
| `trackcamera` | per-frame poses | `movie`, `movie_pose` | `plate_image`, `marker_image`, `marker_pose`, `depth` with `confidence` |

The phone always writes the marker members, so a phone capture is always alignable. They are optional in the format because an editor recording in stage space needs no marker. `marker_pose` matters for a still kind even though the camera is treated as fixed: its pinhole values are the camera's intrinsics.

### Loose takes and their packing

Before captures, a take was a loose set of files: `movies/<stem>.mp4` or `textures/<stem>.jpg`, a `<stem>.pose.json` beside it, for a LiDAR take `<stem>.depth.bin` and `<stem>.conf.bin`, and a second `<stem>_marker` take with its own track. `LegacyTakeMigration` (`src/Editor/Project/LegacyTakeMigration.h`) packs those once, on project load, after the asset folders exist and before the project file is read. Any `*.pose.json`, `*.depth.bin`, or `*.conf.bin` in `movies` or `textures` triggers it, so an interrupted pack reruns on the next load and a current project is left alone. Grouping is by stem, with `<stem>_marker` as the marker take:

- an image with a track or a marker take becomes a `stillphoto` (plate = the image, marker = the marker take's image)
- a movie with a per-frame track becomes a `trackcamera`, with its depth tracks when the track declares them and both are present
- a movie without a track but with a marker take becomes a `stillcamera`
- a movie or image with neither a track nor a marker take is raw media and stays where it is
- a marker take that is a movie contributes its first frame as `marker.jpg` and that frame's pose as a one-frame `marker.pose.json`
- a marker take recorded in another session than its take is refused and packed as a take of its own
- `pose_space` is `arkit_world` for a track the phone wrote and `stage` for one the editor's recorder wrote. Neither writer recorded which it was, so the session id is the judge: 32 hex characters without dashes is `VideoSourceRecorder::makeSessionId`, a UUID with dashes is the phone
- `<stem>.refined.pose.json` moves to `captures` beside the new capture

Every file video source in the project file naming a packed take, or its marker take, is repointed at the capture, with its pose track path set to the moved refined track when it named one and cleared otherwise, since the capture carries the recorded track. The pose offset is kept, but it carries no session tag, so it reads as unaligned until the capture's marker shot solves it again on open (see [ARKit world-to-stage offset](#arkit-world-to-stage-offset)). The originals, and any orphan sidecar, move to `<project>/legacy_takes/`, which is not an asset folder. A log line per capture and a summary line say what was packed and where the originals went.

### Pose tracks

A pose track is the JSON the MikanARStreamer app records for every movie and photo, the `movie_pose` and `marker_pose` members of a capture, read by `PoseTrack` (`src/Editor/ECS/VideoSource/PoseTrack.h`):

```
{
  "format": "mikan-pose-track", "version": 2,
  "session_id": "<one UUID per ARKit world origin>",
  "image_width": 1920, "image_height": 1440,
  "first_capture_timestamp_us": 123456789,
  "depth_width": 256, "depth_height": 192, "depth_unit_mm": 1,
  "frames": [
    {"time_us": 0, "capture_timestamp_us": 123456789,
     "transform": [16 floats, row-major camera-to-world in ARKit world space],
     "fx": 1450.1, "fy": 1450.1, "cx": 960.0, "cy": 720.0}
  ]
}
```

`time_us` is relative to the first appended frame and equals the container's presentation time, because the phone stamps each sample's presentation time with its capture clock and the mp4 rebases to zero. A photo's track has one frame at zero. The phone records a pose only for frames it actually wrote, so the track never drifts from the file. The transform is stored row-major like `ARKitPoseInRTPPayload` and transposed at load. Frames are sorted by time on load.

The loader accepts versions 1 and 2. Version 2 adds the three optional `depth_*` keys and nothing else, so a version 1 track and a version 2 track without them load identically. The editor's own recorder writes version 1, since nothing it records carries depth. The depth keys declare the two depth members below, and `PoseTrack::hasDepthTrack` reports whether they are present. `depth_width` and `depth_height` come together or the track is rejected. `depth_unit_mm` defaults to 1 when absent.

### Depth tracks

A `trackcamera` recorded on a LiDAR phone also carries the `depth` and `confidence` members, the ARKit `sceneDepth` and its confidence map for every frame of the movie. Both are headerless arrays of fixed-size records, one record per movie frame, in the same order as the pose track's `frames`: record `i` describes frame `i` and pose `i`. A depth record is `depth_width * depth_height` little-endian uint16 values, each `depth_unit_mm` millimetres, 0 where the sensor reported nothing. A confidence record is the same count of uint8 values, ARKit's 0 low, 1 medium, 2 high. The depth camera's intrinsics are the frame's colour intrinsics scaled by `depth_width / image_width` and `depth_height / image_height`. A frame that reached the movie before the sensor delivered is a zero record, so the members never fall out of step with the pose track. The phone records the raw per-frame depth rather than ARKit's temporally smoothed variant, and holds autofocus off for the span of the take so the recorded intrinsics stay stable. `DepthTrackReader` (`src/Editor/Reconstruction/DepthTrackReader.h`) opens the two member ranges of the archive, checks their sizes against the track's record count, and reads one record at a time at its offset. The occlusion mesh build ([occlusion-mesh.md](./occlusion-mesh.md)) is their consumer. Record `i` belongs to the frame at file position `i`, which `PoseTrackFrame::fileIndex` keeps through the loader's sort by time. `PoseTrackWriter::setDepthTrack` is what makes a written track version 2. Without it the writer stays at version 1.

### Matte tracks

A take's occlusion matte is a sidecar beside the capture rather than a member of it, since a capture is written once and the matte is regenerable: `<stem>.matte.json` and `<stem>.matte.bin` in the same folder. The binary is headerless fixed-size records indexed by the pose track's `fileIndex` like the depth members, two bits per pixel at the matte's own size (the video's, or half of it), one label per pixel: keep, added or removed. The json carries the session id, the sizes, the band, the frame step, the record count, the model and the stencils it was built against. `MatteTrackReader` and `MatteTrackWriter` (`src/Editor/Reconstruction/MatteTrack.h`) read and write them, and `FileVideoSourceComponent` opens the reader by the capture's stem on load and hands each delivered frame's record to the compositor through `IOcclusionMatteSource`. The build and the consumer are in [occlusion-matte.md](./occlusion-matte.md).

The component matches each decoded frame to its pose by nearest `time_us` within half a frame period. Frame index is never used: ffmpeg's frame counter is unreliable after a seek. On a hit it publishes the pose through `IFrameCoupledPoseProvider` with intrinsics built from the record's pinhole values (`createMonoIntrinsicsFromPinhole`), and on a miss it keeps the previous pose. The frame sequence it reports is a delivery counter that never resets on seek, loop, or stop, and unlike the ARKit source it never clears the alignment offset on a backwards sequence: a recording cannot restart its tracking session.

A still kind, and a raw file without an override track, offers no pose at all, and `CameraComponent` and the align stages treat it as a fixed camera.

### Uploading takes from the phone

The MikanARStreamer app's Captures panel sends a take to the editor over the HTTP message server, one file per request:

```
POST /assets/upload?folder=<folderId>&name=<fileName>    body: the raw file bytes
```

`AssetUploadRequestHandler` (`src/Editor/Server/AssetUploadRequestHandler.h`) owns the route. A capture lands in `captures`, a raw movie in `movies`, and a raw photo in `textures`. The loose `.pose.json`, `.depth.bin`, and `.conf.bin` sidecars are refused by every folder: a take travels as one capture. The file type is judged by the name's extension against the folder's asset factories, the same gate the Assets panel's Add button applies. A file of the same name is replaced in place rather than given the numeric suffix `ProjectAssetCatalog::importAsset` applies to a colliding import: the same name is the same take sent again. Replacing a capture a `FileVideoSourceComponent` is playing fails with 409, since the reader holds the file open. The reply is `{"storedPath": "captures/take.trackcamera", "replaced": false}` on 200 and `{"error": "..."}` otherwise.

The route is a streaming route of `HttpInterprocessMessageServer`: once the headers are in, the connection thread hops to the main thread to validate the folder and file type, names a hidden temporary file beside the destination, and the IXWebSocket fork streams the body into that file in chunks as it arrives, so the body never sits in memory and the final move into place is a rename on one volume. A request the validation refuses has its body read and dropped before the refusal is sent. `Content-Length` is parsed as a 64-bit value, so a capture past 2 GiB is accepted, and a streamed body is paced by an idle timeout (60 s without a byte, `HttpInterprocessMessageServer::k_defaultStreamingIdleTimeoutSecs`) rather than the 30 s whole-request deadline the small trigger requests keep, so a multi-gigabyte capture over Wi-Fi is not cut off while bytes still arrive and a stalled one is. Only the validation and the final move cross the main thread, so an upload never stalls a frame. The `http_server` test module streams a 64 MiB body through the route from a raw socket client and checks the file and the process working set.

The server binds loopback only until "Allow connections from other machines" is on in the HTTP Triggers panel (`httpServerAllowRemote` in the app settings), which is what a phone on the LAN needs. The first bind on all interfaces prompts Windows Firewall for `Mikan.exe`. A same-machine test needs no setting:

```
curl -X POST --data-binary @take.trackcamera "http://127.0.0.1:8090/assets/upload?folder=captures&name=take.trackcamera"
```

### Finding the editor from the phone

The editor advertises itself on the LAN over DNS-SD so the phone needs no typed address. `ServiceAdvertiser` (`src/Editor/Interprocess/ServiceAdvertiser.h`) registers a `_mikanxr._tcp` service named `MikanXR (<computer name>)` on the HTTP server's port, through the DNS-SD API built into Windows (`windns.h`, Windows 10 1903 and later), so no Bonjour install is needed. The TXT record carries three keys:

- `txtvers`: the record's version, `1`
- `arkit`: the `base_port` of every ARKit video source, sorted ascending and comma separated, empty when the project has none
- `ws`: the client API websocket port, for a phone that connects as a client ([wire-protocol.md](./wire-protocol.md))

The phone takes the first editor it finds and the lowest port in `arkit`, and fills its host, base port, and HTTP port settings from them. The service is advertised only while "Allow connections from other machines" is on, since a loopback-only server is unreachable from the phone. That one setting opens both the HTTP server and the client API websocket to the LAN. Once a second the advertiser rebuilds what it should advertise from the remote access setting, the HTTP port, and the project's ARKit sources, and registers again when that differs from what is registered. That one comparison follows port edits, source adds and removes, and project loads with no event subscriptions. A withdrawn registration sends the mDNS goodbye, so browsers drop it at once. The replacement is registered only after that withdrawal completes: two registrations of one name collide, Windows renames the second `MikanXR (<computer name>)(1)`, and a renamed registration the editor fails to withdraw stays advertised by the Windows DNS Client service after the editor exits. `MikanCmd` does not advertise. From a Mac, `dns-sd -B _mikanxr._tcp` lists the editor and `dns-sd -L "MikanXR (<computer name>)" _mikanxr._tcp` shows its port and TXT record.

### Alignment status for the phone

The phone's stream mode shows "look at the origin marker" until its stream is aligned, and reads that state over the client API websocket ([wire-protocol.md](./wire-protocol.md)). The `GetVideoSourceAlignment` request takes a `video_source_id` and answers `MikanVideoSourceAlignmentResponse`. `MikanVideoSourceAlignmentEvent` carries the same fields whenever they change, so the phone asks once when it connects and follows the events from then on. The request is declared in `src/Libraries/MikanClientAPI/Public/MikanVideoSourceRequests.h` and the event in `MikanVideoSourceEvents.h` beside it. The fields:

- `video_source_id`: the source the state belongs to.

- `is_aligned`: the source holds a stage alignment. For an ARKit source this is `hasPoseOffset`, which the source drops itself when the phone restarts its ARKit session. Every other source answers true, since its poses are stage space by construction.

- `marker_visible`: the marker watch saw the origin marker on its last detection. False while no watch runs.

- `is_watching`: a marker watch runs on the source. A client uses it to tell "point the camera at the marker" from "the editor is not looking yet".

`SetVideoSourceAlignmentWatch` with a `video_source_id` and `watch` asks the editor to watch that source for the origin marker on the phone's behalf, answering a plain `MikanResponse`. The phone sends it once it streams, so its stream is aligned in any stage, even when nothing else in the editor consumes the source. A watch is scoped to the connection: it lives in the connection's `MikanClientConnectionState`, and ends with `watch` false or when the client disconnects. `MikanServer::isVideoSourceAlignmentWatched` reports whether any connected client watches a source. While a source is watched, the aligner's own view subscription keeps the source's pipeline playing, so the device starts receiving without another consumer.

The phone asks for the composited picture back the same way: `SetCompositorReturnFeed` adds its address to the compositor's RTP output for the life of the connection (see [Outputs](./compositor.md#outputs) in compositor.md).

`VideoSourceRequestHandler` (`src/Editor/Server/VideoSourceRequestHandler.h`) answers both requests. For an ARKit source the alignment request reads `ARKitVideoSourceSystem::getAlignmentState`. An unknown id answers `NoVideoSource` to either request, and a watch from a connection the server does not know answers `UnknownClient`. `ARKitVideoSourceSystem` publishes the event from its tick: it computes every ARKit source's state and publishes when that differs from the state it last published for the source, or when the source is new. That one comparison covers a solve, the marker coming into or out of view, a watch starting or stopping, and the offset dropping on a session restart. A deleted source's entry goes with it. The watch is the `ARKitMarkerAutoAligner` described under [ARKit world-to-stage offset](#arkit-world-to-stage-offset).

### Session description for the compositor's RTP output

A player receives the compositor's RTP output ([compositor.md](./compositor.md)) from a session description rather than a hand-written file, so the document follows the output's size and destination:

```
GET /compositor/stream.sdp?compositor=<id>
```

`CompositorStreamRequestHandler` (`src/Editor/Server/CompositorStreamRequestHandler.h`) owns it, a background route that hops to the main thread once. The id may be left off when the project has one compositor. The reply is `application/sdp` with the destination as its connection address, the port, `H264/90000` on payload 96 with `packetization-mode=1`, the picture size and the frame rate. It answers 405 for another verb, 400 for a malformed id or for no id when the project has several compositors, 404 when the compositor is missing or its RTP output is off, and 504 when the editor did not answer. Saving the body as a `.sdp` file and opening it in VLC or `ffplay` on the destination machine plays the stream. Another machine reaches the route only while "Allow connections from other machines" is on, like the phone's routes. The `compositor_stream_route` test module drives the route through a stub editor and checks the size math and the SDP text.

### Recording from the editor

The video source settings stage records the live feed of any source as a capture, through the same step-by-step flow of a kind the phone walks, with a kind selector in place of the old marker toggle. `CaptureFlow` (`src/Editor/Capture/CaptureFlow.h`) is the step machine, pure logic the tests drive directly: a `stillphoto` takes the marker image then the clean plate, a `stillcamera` takes both then records until stopped, and a `trackcamera` records straight away with no marker steps, since its poses are stage space and need no alignment. Track Camera is offered only when a camera on the source has a tracked pose, from a tracking mount or a frame-coupled source, with mono intrinsics. `VideoSourceRecorder` (`src/Editor/AppStages/VideoSourceSettings/VideoSourceRecorder.h`) owns the work: each shot is written as a file into a hidden working directory `captures/.capture_<take>/` (`marker.jpg` with its one-frame `marker.pose.json`, `plate.jpg`, `movie.<ext>` with `movie.pose.json` for a trackcamera), and when the flow completes `CapturePacker` (`src/Editor/Capture/CapturePacker.h`) packs the members into `captures/take_YYYYMMDD_HHMMSS.<kind>` on a worker thread, with `pose_space` `stage` and the editor as producer, and the working directory is removed. A cancelled flow removes it too, and leaving the stage mid-flow cancels. A shot that fails leaves the flow on its step for a retry. The stage answers `capture_start <kind>`, `capture_advance`, `capture_cancel`, and `get_capture_state` over the automation channel ([automation.md](./automation.md)).

Frames are recorded undistorted. The recorder owns a second `VideoFrameDistortionView` in calibration mode for the duration of a shot, which undistorts on the CPU into a BGR buffer every tick, and the tracks carry the source's undistorted camera matrix, so a played take is a pinhole camera exactly as the format assumes. The pose written per frame is the camera's stage-space aperture pose at the moment the frame was taken, whether it comes from a tracking mount, a frame-coupled source, or an authored transform, with any alignment offset already folded in. A capture recorded this way therefore plays back with no alignment step. The marker image of a still kind carries a one-frame track too, which is where a still kind reads its intrinsics from. Without a camera, with an uncalibrated source, or with stereo intrinsics a still kind's marker track is left out and the status line says why, while a trackcamera refuses to start.

Recording from a file source playing a capture is how a capture is trimmed: play, record from the in point, stop at the out point. The result re-encodes the movie, carries no depth, and has stage-space poses with the source's offset folded in, since the camera's pose is what the recorder reads.

Frames are paired with their pose on the main thread and handed to an encoder thread over a bounded queue. A frame the queue cannot take is dropped together with its pose, so the movie and the track stay in lockstep, and `time_us` is the constant-rate presentation time of each written frame. The movie is written by `MovieWriter` (`src/Editor/ECS/VideoSource/MovieWriter.h`) through `cv::VideoWriter`, trying the Media Foundation plugin's H.264 first (the ffmpeg DLL's own H.264 route needs an OpenH264 DLL that does not ship), then ffmpeg MPEG-4 part 2, then MJPG in an avi. The Media Foundation writer lives in `opencv_videoio_msmf4100_64.dll`, which the Editor's CMake copies beside the executables with the ffmpeg DLL. `PoseTrackWriter` in `PoseTrack.h` writes the track with the phone's keys.

---

## Tracking pose association

A `CameraComponent` (`src/Editor/ECS/Camera/CameraComponent.h`) ties everything together via IDs on `CameraDefinition`: a `MikanVideoSourceID` (which video source feeds it), a `MikanTrackingMountID` (which physical tracker rig it sits on), and an aperture pose offset (tracker-to-lens transform produced by alignment calibration, see [calibration.md](./calibration.md)). `TrackingMountComponent` binds a VR device path plus an attachment socket name and produces a `VRDevicePoseView`; `CameraComponent::updateAperturePoseFromTrackingMount()` polls it each tick and composes the aperture offset to get the stage-space camera pose (`getStageSpaceAperturePose`, `getApertureViewMatrix`, `getApertureProjectionMatrix`).

The ARKit source instead implements `IFrameCoupledPoseProvider` (`src/Editor/ECS/Camera/IFrameCoupledPoseProvider.h`): pose and intrinsics arrive coupled to each video frame in the RTP header extension, and `getLatestFrameCoupledPose()` hands `CameraComponent` a camera-to-world transform plus a frame sequence number for correlation, with intrinsics applied only when they change meaningfully. That change test compares focal length and principal point both, so a correction to either one propagates. Since live intrinsics that match the stored ones are never applied, the projection cannot depend on that path: `VideoSourceComponent::setDefinition` derives it from the stored intrinsics for every source, and each subclass calls through it. Before that, an ARKit source restarted against a phone whose intrinsics had not moved kept an identity projection, and every consumer of it broke silently (the depth mask, shapes, depth of field, click to focus).

### ARKit world-to-stage offset

ARKit picks a fresh world origin every session, so a streamed pose only means something once it is anchored to the stage. The operator anchors it with the ordinary Align Camera action (`AppStage_AlignCameraByOriginMarker`, see [calibration.md](./calibration.md)), and the phone stays unaware that stage space exists.

The stage detects that its video source is an `IFrameCoupledPoseProvider` and takes a different path than it does for a tripod camera. A static camera's aperture-relative marker poses can be averaged directly. A handheld one moves between samples, so each sample is first converted into the quantity that is actually constant, `worldToStage = inverse(markerInCamera) * inverse(cameraInWorld)`, using the ARKit pose from that same frame. Those are averaged (translation componentwise, rotation through quaternion slerp) and handed to `setPoseOffset` instead of `setRelativeTransform`, which a frame-coupled camera would overwrite on the next bundle.

The solve reads the pose through `getLatestSourceWorldPose`, which reports it in ARKit's own world space, rather than `getLatestFrameCoupledPose`, which reports it in stage space with the current offset already applied. Solving from the latter folds the stored offset into the new one, so the first alignment on a fresh project looks right and every one after it compounds. Reproducibility is the test that catches this: two alignments in a row must agree on the offset even when the phone has moved between them.

The offset is applied at exactly one point per source: `getLatestFrameCoupledPose` on `ARKitVideoSourceComponent` and on `FileVideoSourceComponent`. Each source stores only the pose in its own world space and applies the offset as the pose is read, so a new alignment takes effect on the next read. A paused take or a still marker shot publishes no new pose, so an offset baked in at publish time would leave Align Camera's verify view drawing from the old alignment. The offset persists on the definition as plain JSON with no property descriptor, since no client application has any use for it and a descriptor would mean a wire change. An absent key means never aligned, and the offset is identity until then.

A stored offset describes the ARKit session it was solved in. A `frameSeq` that runs backwards means the phone restarted that session, so the ARKit source drops the offset and logs the reason rather than leaving the camera confidently in the wrong place.

A file source records that session with the offset: `pose_offset_session_id` is the session id of the capture, or of the override track for a raw movie, at the moment the offset was set (`FileVideoSourceComponent::getPoseSessionId`). `hasPoseOffset` and `getPoseOffset` report the offset only while that id matches the media now loaded. An offset from another session, or one stored before the tag existed, reads as no alignment and stays stored until replaced. Repointing a source at another take therefore never carries one take's alignment into another's world.

A trackcamera take aligns itself. `FileVideoSourceSystem` checks each source once per media open, and when its capture has a marker shot and no offset for its session, it solves the offset from that shot. The origin marker is the one on the stage of the camera that plays the source, and a source no camera on a marker stage plays waits until one does. `CaptureMarkerAlignment` (`src/Editor/Calibration/CaptureMarkerAlignment.h`) decodes the marker image from the capture, finds the marker with the detector, corner geometry, and solve the Align Camera stage uses (`CalibrationPatternFinder_Aruco::estimateMarkerPoseInImage`), and converts the result through `FrameCoupledAlignment::computeWorldToStageSample` with the marker track's pose. It reads the capture directly and needs no window or GL context. A shot whose marker cannot be found logs once and leaves the source unaligned. Align Camera on the take's camera still works and overrides: its result is tagged the same way, and the automatic solve runs only when no offset for the session exists. The `capture_marker_alignment` test module covers the solve from synthetic marker shots and a packed take that ignores another session's offset and aligns itself.

The marker shot catches ARKit's world at one instant, and that world moves a few millimetres over a take, while the take's mesh is fused in the world averaged over the take. So once a take with depth members has an offset, by its marker shot or by Align Camera, `FileVideoSourceSystem` starts a height snap on a worker thread (`FileVideoSourceComponent::startHeightSnap`). `CaptureMarkerAlignment::snapToTableHeight` reads every depth record with its own reader, fits the table in an annulus around the marker (1.2 to 3 marker half-widths, confidence 2, RANSAC then least squares) in each frame's own world, and takes the median height along the marker's normal over the take. The offset's origin moves onto that height, and the shot keeps its tilt, in-plane position, and heading, which the shot measures better than a table patch does. The unsnapped offset applies until the job lands, under a second on the measured takes, and `pose_offset_height_snapped` records the result so a reopened project does not redo it. Stencils that follow the take move with it. A take without depth, or whose depth never sees the table around the marker, keeps the shot's height and logs why. While a snap is wanted or running, `isAlignmentSettling` is true and the occlusion build refuses to start. On three tabletop takes the snap moved the origin by 0.3, 1.7, and 6.2 mm and cut the median cross-session depth error before a rigid fit from 4.8 to 3.1 mm, against a 2.6 mm floor ([the proposal](../proposals/marker-alignment-accuracy.md)). The snap only translates, so the shot solve's tilt passes through, which is why the marker solve is the square-marker one ([calibration.md](./calibration.md)). `MikanCmd -alignTake` prints a take's shot and snapped offsets for that comparison.

The same solve runs without the stage while an unaligned phone streams. `ARKitVideoSourceSystem` runs an `ARKitMarkerAutoAligner` (`src/Editor/Calibration/ARKitMarkerAutoAligner.h`) for each ARKit source without a pose offset that either streams or is watched by a client through `SetVideoSourceAlignmentWatch` (see [Alignment status for the phone](#alignment-status-for-the-phone)). Streaming means the device reports `eVideoStreamingStatus::started`. The device idles until a view subscribes and reaches `started` only once frames arrive, so without a client watch the aligner starts only on a source another consumer already streams, such as the compositor's video source. The system drops the aligner when the source is deleted, when it aligns, or when it neither streams nor is watched. The aligner subscribes its own calibration-mode `VideoFrameDistortionView`, waits for frames, a camera on the source with mono intrinsics, and a stage with an origin marker, then runs `ArucoMarkerPoseSampler` on the main thread four times a second rather than every frame, since the finder reads the view's buffers on the GL thread. Once the marker has been seen for twelve consecutive detections, with a gap of over a second starting the window over, it averages the samples through `FrameCoupledAlignment` (`src/Editor/Calibration/FrameCoupledAlignment.h`), the helper the stage uses for the same step, stores the offset with `setPoseOffset`, and releases the stream. The watch reads frames through the editor window's GL context, so `MikanCmd` runs none. The `frame_coupled_alignment` test module checks the shared averaging.

### Marker reference for recorded takes

A recorded take usually keeps the origin marker out of shot, so a capture carries a marker image of the same shot with the marker in frame, and its one-frame pose track. `IAlignmentReferenceSource` (`src/Editor/ECS/Camera/IAlignmentReferenceSource.h`) is the runtime mixin the two marker alignment stages discover by `dynamic_cast`, beside `IFrameCoupledPoseProvider`. On entry a stage calls `beginAlignmentReference()`, which switches the source's frames to the marker image, and for a `trackcamera` its poses to the marker track, without dropping the stage's own view subscription (a close and reopen would). On exit `endAlignmentReference()` restores the main media and the playback position it had. Which media is active is stage state rather than a property, so a cancelled stage cannot leave the project pointing at the reference.

The reference is offered when the capture has a marker image and its poses are not stage space already. `Capture::open` has already refused an archive whose marker track and movie track name different sessions, so an offset solved from the marker applies to the movie. A still kind publishes no pose, so the stage takes the fixed camera path and places the camera from the marker image directly, with the intrinsics seeded from the marker track.

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

- Composited output: `CompositorComponent` can publish the final composited frame as a named Spout sender (`k_spoutEnableOutputNamePropertyId` / spout output name on `CompositorDefinition`), using the same `ISharedTextureWriteAccessor` API. The `MikanSharedTexture` library is itself implemented on Spout (`Private/SpoutGL`, `Private/SpoutDX`), so both the client render-target path and the Spout output path are Spout shared textures underneath. The same frame can also leave the machine as H.264 RTP over UDP through the `MikanGStreamerVideo` plugin's `IRtpVideoSender` ([compositor.md](./compositor.md)), the path a phone or a player on another machine receives.

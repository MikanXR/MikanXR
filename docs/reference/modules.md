# Modules

Architectural map of the MikanXR source tree: every CMake target under `src/`, what it wraps, and which direction the dependencies point. Directory layout is covered in [layout.md](./layout.md), build invocations in [build.md](./build.md), the request/response protocol in [wire-protocol.md](./wire-protocol.md), and code style in [standards.md](./standards.md).

---

## Dependency shape

`src/CMakeLists.txt` steps into four subtrees in order: `Libraries`, `Plugins`, `Programs`, `Editor`.

- `src/Libraries` sits at the bottom. Each library is its own DLL except `MikanDMX` and `MikanOnnx`, which are static libs. `MikanUtility` and `MikanSerialization` have no in-repo dependencies; everything else stacks on top of them.

- `src/Plugins` are DLLs that link only low-level libraries (`MikanCoreApp`, `MikanUtility`, sometimes `MikanRenderer`/`MikanWindow`). The editor does not link them; it loads them at runtime by name through `IMikanModuleManager` (see Plugins below).

- `src/Programs` are executables (code generator, tests) that link the client-facing libraries.

- `src/Editor` is the top: all editor code compiles once into an OBJECT library `MikanEditor`, which both executables (`Mikan`, `MikanCmd`) link. It links every library plus the heavyweight third parties (OpenCV, CEF, Lua, OpenVR).

Dependency direction below is derived from each target's `target_link_libraries`.

---

## src/Libraries

All are `SHARED` DLLs with a `Public/` (installed headers) and `Private/` (implementation) split, unless noted. Each exports through a per-DLL macro pair (`MIKAN_<NAME>_EXPORTS` define, `MIKAN_<NAME>_FUNC(...)` decorated functions).

### MikanUtility
Leaf utility DLL (string/path helpers, a polled directory watcher, process launching, etc.). No third-party or in-repo dependencies, though on Windows it links two system libraries: `winhttp` behind `HttpDownloader` (streaming HTTPS GET to a file, with progress and cancel, used for the ML model downloads) and `bcrypt` behind `HashUtils::sha256File`. Linked by nearly everything: `MikanCoreApp`, `MikanClientCore`, `MikanClientAPI`, `MikanRenderer`, `MikanWindow`, `MikanGUI`, every plugin, `MikanEditor`, `MikanClientCodeGen`, both test executables.

### MikanSerialization
Wire and config (de)serialization layer. Wraps Refureku (runtime reflection, `${RFK_LIBRARIES}`) and uses nlohmann json headers. Runs `RefurekuGenerator` as a pre-build step (`MikanSerializationReflection` target). Built with `CXX_VISIBILITY_PRESET hidden`. `Serialization::String` is deliberately a `const char*`-only type because this DLL crosses the DLL/EXE boundary into client applications built with a different CRT; see [wire-protocol.md](./wire-protocol.md). Linked by `MikanClientCore`, `MikanClientAPI`, `MikanMath`, `MikanEditor`, `MikanClientCodeGen`, tests.

### MikanSharedTexture
Out-of-band video-frame transport via GPU shared textures. Compiles vendored Spout2 `SpoutDX`/`SpoutGL` sources in `Private/` plus `volk.c` from `thirdparty/volk`, and links `${SPOUT2_LIBRARIES}`, GLEW, and `opengl32`. Four writers, one file per graphics API behind the `ISharedTextureWriterBackend` seam in `Private/SharedTextureWriterBackend.h`: OpenGL, D3D11, D3D12, and Vulkan, the last importing Spout's D3D11 textures into the client's device ([wire-protocol.md](./wire-protocol.md)). `SharedTextureWriter.cpp` keeps only the API-agnostic accessor, which derives the sender names and forwards frames, so adding an API adds a file rather than another arm of its dispatch. Each backend file is excluded from the unity build, since merging the three APIs' headers into one translation unit is what the split undoes. volk loads `vulkan-1.dll` at runtime, so the DLL has no Vulkan import. Public API is `SharedTextureWriter`/`SharedTextureUtility`/`SharedTextureLogger` (graphics-API enums such as `SharedClientGraphicsApi`, and the `SharedVulkan*` mirrors of the Vulkan client contract). Linked by `MikanClientCore` and `MikanEditor` (the editor-side reader is `src/Editor/Interprocess/SharedTextureReader`).

### MikanCoreApp
Process/plugin plumbing shared by editor and clients: `Logger`, `WorkerThread`, `ThreadUtils`, `CrashHandler` (the minidump crash reporter the two executables install; wraps `dbghelp`), and the plugin module system. Defines `IMikanModule` (the plugin interface: `startup()`/`shutdown()` plus the exported C entry points `AllocatePluginModule`/`FreePluginModule`) and `IMikanModuleManager` (`getMikanModuleManager()->getModule<T>(name)`). Also declares the per-device-category module interfaces plugins implement: `IUsbVideoDeviceModule`, `INetworkVideoDeviceModule`, `IVRDeviceModule`, and the device interfaces (`IVideoDevice`, `IUsbVideoDevice`, `INetworkVideoDevice`, `IVRDevice`) with their managers. `IARKitVideoDeviceModule` / `IARKitVideoDevice` sit alongside them. Wraps `dylib` for runtime DLL loading. Links `MikanUtility`. Linked by `MikanClientAPI`, `MikanRenderer`, `MikanWindow`, `MikanGUI`, all plugins, `MikanEditor`, programs.

### MikanClientCore
Client-side transport core: the websocket RPC client (`Private/Interprocess/WebsocketInterprocessMessageClient`, wrapping `ixwebsocket`) and render-target/shared-texture handoff (Spout DX enabled via `ENABLE_SPOUT_DX`; lock-free queue headers). `Public/MikanCoreVulkanTypes.h` is the Vulkan client contract (the device interface and the per-frame texture struct), the one SDK header that includes a Vulkan header and the only one Refureku does not parse. Refureku-reflected (`MikanClientCoreReflection` pre-build target). Hidden symbol visibility; ships to client apps. Links `MikanSerialization`, `MikanSharedTexture`, `MikanUtility`, `ixwebsocket`, `${RFK_LIBRARIES}`. Linked by `MikanClientAPI`, `MikanEditor`, `MikanClientCodeGen`, tests.

### MikanClientAPI
The public client API DLL (`IMikanAPI` in `Public/MikanAPI.h`) plus the reflected request/response/event structs that define the wire contract (`MikanAnchorTypes.h`, `MikanCameraRequests.h`, ...). Changing these structs changes the protocol and the generated C#/TypeScript bindings under `bindings/`; regenerate rather than hand-edit (see [wire-protocol.md](./wire-protocol.md)). Refureku-reflected (`MikanClientAPIReflection` pre-build target), hidden visibility, `/bigobj`. Links `MikanClientCore`, `MikanCoreApp`, `MikanSerialization`, `MikanUtility`, `${RFK_LIBRARIES}`. Linked by `MikanMath`, `MikanEditor`, `MikanClientCodeGen`, tests.

### MikanMath
Math helper DLL over GLM (header-only, `thirdparty/glm`), with conversions to the `MikanClientAPI` math types. Also holds `SphericalHarmonics.h`: the order-2 SH environment type, its basis evaluation, and the least-squares solver the lighting estimate is built on. That lives here rather than in the editor because it is pure math with no ECS or OpenCV dependency, which is what lets `unit_test_suite_cpp` cover it without linking the editor. Links `MikanClientAPI` and `MikanSerialization`. Linked by `MikanEditor` and `unit_test_suite_cpp`.

### MikanRenderer
OpenGL rendering abstraction (`IMkTexture`, `MkMaterial`, `MkStateModifiers`, shader/vertex constants; `Mk` prefix). Wraps OpenGL/GLEW and uses stb headers. Links `MikanCoreApp`, `MikanUtility`. Linked by `MikanWindow`, `MikanGUI`, `MikanSteamVR`, `MikanEditor`, `MikanClientTestCPP`, `MikanARKitVideo`.

### MikanWindow
SDL window/context management (`IMkWindowContext`, `IMkWindowContextManager`, `IMkFontManager`, window events). Wraps SDL2, SDL2_image, SDL2_ttf; uses easy_profiler headers. Links `MikanCoreApp`, `MikanRenderer`, `MikanUtility`. Linked by `MikanGUI`, `MikanSteamVR`, `MikanEditor`.

### MikanGUI
Immediate-mode UI DLL. Compiles Dear ImGui (docking branch) and imgui-node-editor sources directly into the DLL (re-exported via `IMGUI_API`/`IMGUI_NODE_EDITOR_API` dllexport defines) rather than linking them as libraries. Owns the whole ImGui surface the editor sees: `MkGuiContext` (one ImGui context per window, each with its own ini file), the `MkGuiScoped*` RAII wrappers, the `MkCanvas*` facade over imgui-node-editor (scoped editor/node/pin brackets, pin icon drawing, the graph-id to canvas-id offset, and the zoom-tied font density that keeps canvas text crisp), `MkGuiDrawUtils` (the `MkGui::drawXProperty` widget vocabulary, including the node-editor widgets and colors), `MkGuiStyleManager` (named scoped styles loaded from `resources/gui_styles/*.json`), `MkGuiTheme` (the base palette, metrics, and the UI font with the icon font merged in), and `MkGuiDockspace` (the dockspace host and the DockBuilder calls, which keeps ImGui's internal API out of the editor). Every `drawXProperty` widget lays out a fixed-width label column followed by a value column that fills the remaining panel width, so a style's `valueWidth` is that column's floor rather than a fixed size. The file path widget is the exception: its field starts just after the label text instead of at the value column, because a path needs the whole row to show a readable tail. Docking is opt-in per context and enabled only for the main window. Links SDL2 family, OpenGL/GLEW, `MikanCoreApp`, `MikanRenderer`, `MikanUtility`, `MikanWindow`. Linked by `MikanEditor` only.

### MikanDMX
`STATIC` library: standalone E1.31 (sACN) DMX lighting sender. No editor/ECS dependencies; links only `Ws2_32` on Windows. Linked by `MikanEditor`.

### MikanOnnx
`STATIC` library: a thin ONNX Runtime wrapper (`Public/OnnxSession.h`) with DirectML-first execution provider selection and CPU fallback. Deliberately has no editor/ECS dependencies, only the logger from `MikanCoreApp`; links `${ONNXRUNTIME_LIBRARIES}`. Linked by `MikanEditor` and `unit_test_suite_cpp`. The two consumers are the scene lighting estimator ([scene-lighting.md](./scene-lighting.md)) and the depth proxy mesh capture ([depth-proxy-mesh.md](./depth-proxy-mesh.md)), both of which live in `src/Editor/Calibration`. A session must be created, run, and destroyed on one thread; `requestTerminate()` is the only method safe to call cross-thread, and both capture stages rely on it for cancellation.

### ARKitReceiver, MikanARKitReceiver
Empty directories, not referenced by `src/Libraries/CMakeLists.txt` (leftover scaffolding, no targets).

---

## src/Plugins

Each plugin is a `SHARED` DLL with hidden symbol visibility that links only low-level libraries and exports two C functions, `AllocatePluginModule` and `FreePluginModule` (names fixed by `IMikanModule.h`). The editor never links a plugin; at runtime the ECS device systems ask `IMikanModuleManager` (in `MikanCoreApp`, backed by `dylib`) to load the DLL by module name and cast the result to the expected module interface, e.g. `getMikanModuleManager()->getModule<IUsbVideoDeviceModule>(moduleName)` in `src/Editor/ECS/VideoSource/USBVideoSourceSystem.cpp`. Video-source behavior is covered in [videosources.md](./videosources.md).

- `MikanWMFVideo`: USB webcams via Windows Media Foundation (`mfplat`, `mf`, `mfuuid`, `mfreadwrite`, `wmcodecdspuuid`). Module class implements `IUsbVideoDeviceModule`. Links `MikanCoreApp`, `MikanUtility`. Loaded by `USBVideoSourceSystem`.

- `MikanGStreamerVideo`: network video sources via GStreamer (core/app/base/video plus GLib/GObject). Implements `INetworkVideoDeviceModule`. Links `MikanCoreApp`, `MikanUtility`. Loaded by `NetworkVideoSourceSystem`. Only built when `MIKAN_WITH_GSTREAMER=ON` (off in CI).

- `MikanARKitVideo`: iOS ARKit camera streaming (RTP over GStreamer, with a pose payload in the RTP stream). Uses GStreamer core/app/base/video/rtp/cuda, the CUDA Driver API (`Private/Cuda/CudaGLInterop` for CUDA-GL texture registration and plane copies), and GLEW. Implements `IARKitVideoDeviceModule`. Links `MikanCoreApp`, `MikanRenderer`, `MikanUtility`. Loaded by `ARKitVideoDeviceManagerLoader`. Only built when `MIKAN_WITH_ARKIT_VIDEO` is on, which is `MIKAN_WITH_GSTREAMER=ON` plus an installed CUDA Toolkit ([build.md](./build.md)).

- `MikanSteamVR`: VR tracker/HMD poses via OpenVR (`${OPENVR_LIBRARY}`). Implements `IVRDeviceModule`. Links `MikanCoreApp`, `MikanRenderer`, `MikanWindow`, `MikanUtility`. Loaded by `VRObjectSystem`.

---

## src/Programs

- `ClientCodeGen` (target `MikanClientCodeGen`): console app that walks the Refureku reflection metadata of `MikanClientAPI`/`MikanClientCore`/`MikanSerialization` and emits the C# and TypeScript client bindings under `bindings/`. Links those three DLLs plus `MikanCoreApp`, `MikanUtility`, `${RFK_LIBRARIES}`.

- `Tests/UnitTests` (target `unit_test_suite_cpp`): C++ unit test executable. Links the client libraries plus `MikanMath` and `MikanOnnx` (the latter for the spherical harmonic fit and ONNX session modules). Has its own Refureku pre-build target. It additionally compiles `ARKitVideoDeviceManagerLoader` from the editor tree and, when `MIKAN_WITH_GSTREAMER=ON`, the plugin's `CudaGLInterop` sources directly; its CMakeLists documents why `ARKitRTPHeaderExtension` must not be dual-compiled (GObject type registration is process-global).

- `Tests/MikanClientTestCPP`: interactive client test app exercising the client API end to end, with DirectX 11, OpenGL, and Vulkan render paths (`-dx`, `-gl`, `-vk`; SDL2 window, `d3d11`/`d3dcompiler`, and volk over a Vulkan 1.3 device whose cube shader dxc compiles to SPIR-V at build time).

- `Tests/MikanClientTestCSharp`: C# client test; only added under Visual Studio generators (C# is unsupported under Ninja, so CI skips it).

The editor unit tests are not a Program: they live in `src/Editor/Server/Test` and `src/Editor/Calibration/Test` and run via `MikanCmd.exe -runTests` (see [commands.md](./commands.md)). They need the editor's own types, which is why they cannot move into `unit_test_suite_cpp`.

---

## src/Editor

All editor sources compile once into `add_library(MikanEditor OBJECT ...)`; `Mikan` (WIN32 GUI app, `AppCore/EntryPoint.cpp`, embeds `Mikan.exe.manifest` for CEF) and `MikanCmd` (console app, `AppCore/CmdEntryPoint.cpp`) each add only their entry-point file and link `MikanEditor`. `MikanEditor` links every `src/Libraries` target plus: OpenCV (`${OpenCV_LIBS}`), OpenVR, CEF (`libcef_dll_wrapper` + `libcef`), Lua (+ LuaBridge3 and `lrdb` debugger headers), libharu (PDF), easy_profiler, fast_obj, ixwebsocket, dylib, GLEW, Spout2; header-only deps include Configuru, nlohmann json, glm, stb, FastCSV, tinyfiledialogs.

Subdirectories (each is a source group, not a separate target):

- `AppCore`: `App` (GUI app object, state machine over `AppStages`), `CmdApp` (headless commands and `-runTests`), entry points, settings config, frame timer.

- `AppStages` holds the modal UI screens, one folder per stage: calibration flows (`MonoLensCalibration`, `AlignmentCalibration`, `AnchorTriangulation`, `LightFixtureCalibration`, `PointCloudAlignment`, `StencilAlignment`, `AlignCameraByOriginMarker`, `AlignCameraByUtilityMarker`, `VRTrackingRecenter`), the ML capture tools (`SceneLightingCapture`, `DepthMeshCapture`), `MainMenu`, `Project`, settings and modal dialogs.

- `Asset`: asset references (models, materials, textures, node graphs) with `Import/` and `Export/` subfolders.

- `Calibration` holds the OpenCV-based calibration algorithms: `CalibrationPatternFinder` (chessboard/charuco/aruco variants), `AnchorTriangulator`, `ArucoMarkerPoseSampler`, `CVVideoFrameProcessor`, plus `Test/`. See [calibration.md](./calibration.md). The ONNX-backed estimators live here too (`SceneLightingEstimator`, `MarigoldInference`, `MoGeInference`, `DepthMeshGenerator`), since they all start from an undistorted frame plus its calibrated intrinsics; see [scene-lighting.md](./scene-lighting.md) and [depth-proxy-mesh.md](./depth-proxy-mesh.md).

- `Config`: `CommonConfig`, the Configuru-backed config base used by project/device configs.

- `Delegates`: header-only `SinglecastDelegate`/`MulticastDelegate`.

- `ECS` holds the entity/component scene representation, one folder per object system: `Anchor`, `Camera`, `Collision`, `Compositor`, `Editor`, `Light`, `Marker`, `Scene`, `Script`, `Shape`, `Stage`, `Stencil`, `TextureSource`, `TrackingMount`, `TrackingVolume`, `VideoSource`, `VRObject`. Device-backed systems here load the plugins. See [objects.md](./objects.md) and [compositor.md](./compositor.md).

- `Events`: `EventBus`.

- `Interprocess` is the server-side transport: `WebsocketInterprocessMessageServer` (ixwebsocket), `HttpInterprocessMessageServer`, `SharedTextureReader`, `ClientTextureFrameQueue`.

- `Localization`: `LocalizationManager` (JSON string tables under `resources/localization`, one file per language, generated from the gettext catalogs in `localization/`), the `LocText.h` call-site helpers, the remote fetcher that overlays community translations from the CDN, and `LocDebugUI` (the show-keys mode and the ID Stack Tool item every View menu carries). See [localization.md](./localization.md).

- `Math`: editor-side math helpers (`CameraMath`, `MathTypeConversion` between glm/OpenCV/Mikan types).

- `Models`: where the ML models come from. `ModelCatalog` is the table of what exists (required files, size, license, download source), `ModelRepository` answers where one is installed and whether it is complete, and `ModelDownloadTask` installs one on a worker thread. Nothing here knows about inference; the capture stages and `MikanCmd -fetchModels` are the callers. See [commands.md](./commands.md).

- `NodeEditors` holds the node graph visual scripting: `Graphs`, `Nodes`, `Pins`, `Properties`, `DataSources`, `Windows`. See [scripting.md](./scripting.md).

- `OpenCV`: `OpenCVManager`, `DeepNeuralNetwork` (OpenCV DNN), OpenCV math glue.

- `Project`: `ProjectManager`, `ProjectConfig` (persisted project state).

- `Renderer` builds editor viewport rendering on top of `MikanRenderer`: `MikanCamera`, line renderer, model resource manager, shader cache.

- `Scripting`: the project script context (LuaBridge3 bindings, `LuaDebugServer` via lrdb).

- `Server` is the RPC surface: `MikanServer` plus per-domain `IServerRequestHandler` implementations (`CameraRequestHandler`, `StageRequestHandler`, `StencilRequestHandler`, `VideoSourceRequestHandler`, `MarkerRequestHandler`, `LightRequestHandler`, `PropertyRequestHandler`, `ScriptRequestHandler`, `ShapeRequestHandler`, `TextureSourceRequestHandler`, `FunctionRequestHandler`, and `AssetUploadRequestHandler`, which serves the HTTP upload route rather than a websocket request), `RemoteControlManager`/`IRemoteControllable`, and `Test/` (unit tests run by `MikanCmd -runTests`).

---

## DLL boundary notes

- Every library is a DLL, so `Mikan.exe`/`MikanCmd.exe` and every test executable need the DLLs beside them. `copy_mikan_runtime_deps` in `src/Editor/CMakeLists.txt` copies all Mikan DLLs plus third-party runtime DLLs post-build; `Mikan` and `MikanCmd` share an output directory and only `Mikan` runs the copier (with `MikanCmd` depending on `Mikan` to avoid a copy race). A DLL missing from these copy lists can pass locally but fail in CI with `0xC0000135`.

- `MikanClientCore`, `MikanClientAPI`, `MikanSerialization`, and `MikanSharedTexture` are shipped to external client applications, which may be built with a different compiler/CRT. That is why they use hidden symbol visibility and why `Serialization::String` stays `const char*`-only across that interface.

- The reflected libraries (`MikanSerialization`, `MikanClientCore`, `MikanClientAPI`, `UnitTests`) each have a `*Reflection` custom target that runs `RefurekuGenerator` over a local `RefurekuSettings.toml` before every build; generated headers land under `${RFK_GENERATED_ROOT_DIR}/<module>`.

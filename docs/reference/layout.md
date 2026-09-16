# Repository Layout

Annotated map of the MikanXR directory hierarchy: where source, generated code, dependencies, and build output live. See [build.md](./build.md) for how these pieces are compiled and [commands.md](./commands.md) for the commands that drive them. The architectural view of the same code is in [modules.md](./modules.md).

---

## Root

```
MikanXR/
├── CMakeLists.txt                      # top-level CMake: includes cmake/*.cmake, adds src/, bindings/, thirdparty/
├── InitialSetup_x64.bat                # first-time setup: wipes build/ and deps/, downloads prebuilt deps into deps/
├── GenerateProjectFiles_X64_VS2022.bat # configures build/ with the "Visual Studio 17 2022" generator
├── CLAUDE.md / README.md / CONTRIBUTING.md / LICENSE
├── .github/workflows/build-and-test.yml # CI: Linux format-check and localization-check jobs + Windows Ninja build/test job
├── .github/workflows/release.yml       # tag-triggered release: Release build with GStreamer, tests, app zip + symbols zip + installer as a draft release
├── .gitmodules                         # submodule list (all under thirdparty/)
├── cmake/                              # CMake modules: Environment, Version, ThirdParty, Installer, Symbols, ClangFormat, Localization, Find*.cmake
├── src/                                # all first-party C++ source (the only tree clang-format touches)
├── bindings/                           # generated client bindings for C# and TypeScript
├── thirdparty/                         # git submodules + a few vendored libs, built from source or used header-only
├── deps/                               # prebuilt binary dependencies downloaded by InitialSetup_x64.bat (not in git)
├── build/                              # CMake binary dir (build/Mikan.sln, object files, built exes; not in git)
├── dist/                               # install prefix (dist/Win64, PDBs under dist/symbols) written by the INSTALL target; version.txt; the packaging zips and the installer
├── resources/                          # runtime assets: calibration patterns, config, dnn models, fonts, icons, gui_styles, localization, lua-definitions
├── localization/                       # gettext catalogs translators own; resources/localization is generated from them
├── models/                             # ONNX checkpoints for the ML capture tools (gitignored, produced by tools/*.py)
├── templates/                          # installer_win64.iss.in (Inno Setup script template, filled in by cmake/Installer.cmake)
├── tools/                              # checked-in helper tools: 7zip/7za.exe (used by InitialSetup), Spout2, and the Python model tools
└── docs/                               # documentation, including this reference set
```

- `deps/` holds prebuilt downloads (SDL2, OpenCV, GLEW, Spout2, easy_profiler, Refureku, libharu, CEF, nuget.exe, the DirectX Shader Compiler under `dxc/`). It is recreated from scratch by `InitialSetup_x64.bat`; never edit it by hand. GStreamer is the exception: its MSIs install system-wide rather than into `deps/`.

- `thirdparty/` holds git submodules (see `.gitmodules`: `openvr`, `glm`, `Configuru`, `stb`, `fast-cpp-csv-parser`, `LuaBridge3`, `imgui`, `imgui-node-editor`, `fast_obj`, `IXWebSocket`, `readerwriterqueue`, `nlohmann_json`, `Vulkan-Headers`, `volk`) plus vendored non-submodule dirs (`lua` prebuilt binaries, `lrdb`, `tinyfiledialogs`). `thirdparty/CMakeLists.txt` builds `fast_obj_lib`, `ixwebsocket`, and CEF's `libcef_dll_wrapper`; the rest are consumed header-only or as source lists from `cmake/ThirdParty.cmake`.

- `build/` layout depends on the generator: the VS generator puts executables in per-target per-config folders (e.g. `build\src\Editor\Release\Mikan.exe`), while CI flattens everything to `build\bin` via `CMAKE_RUNTIME_OUTPUT_DIRECTORY`. Refureku reflection codegen output lands in `build/RfkGenerated/<Library>`.

- `tools/` mixes two unrelated things. `7zip/` and `Spout2/` are checked-in binaries the build depends on. The `*.py` scripts are developer-only: they export or fetch the ONNX checkpoints into `models/`, and they are the reference implementations the C++ ports are validated against. Neither ships with Mikan nor runs at runtime. See [scene-lighting.md](./scene-lighting.md) and [depth-proxy-mesh.md](./depth-proxy-mesh.md).

- `models/` holds the ONNX checkpoints (`marigold/`, `moge2/`, roughly 8GB together). It is gitignored and `InitialSetup_x64.bat` does not create it; the tools above do.

- `localization/` is a source tree, not a resource tree. It holds the language registry and one gettext catalog per translation, and nothing in it is installed. `resources/localization/` is the generated counterpart that ships and that the CDN serves: only `en.json` there is hand-authored. See [localization.md](./localization.md).

---

## src/

`src/CMakeLists.txt` steps into `Libraries`, `Plugins`, `Programs`, `Editor` in that order.

### src/Editor

The editor/server application itself. All editor code compiles once into the `MikanEditor` OBJECT library; the `Mikan` (GUI, `AppCore/EntryPoint.cpp`) and `MikanCmd` (console, `AppCore/CmdEntryPoint.cpp`) executables both link it.

- `AppCore`: entry points, top-level app object, `Version.h` (source of the project version string)
- `AppStages`: top-level app state machine (menus, calibration flows, compositor stage)
- `Asset`, `Project`, `Config`: persisted project/scene/asset state (Configuru-backed config files); `Asset` also holds the `ProjectAssetCatalog` behind the Assets panels, the shared tile and property-row widgets, and `Asset/Test` for the catalog test module (see [objects.md](./objects.md))
- `Calibration`: camera/mixed-reality calibration algorithms (OpenCV-based), see [calibration.md](./calibration.md); also the ONNX-backed frame estimators behind [scene-lighting.md](./scene-lighting.md) and [depth-proxy-mesh.md](./depth-proxy-mesh.md)
- `ECS`: entity/component scene representation (see [objects.md](./objects.md))
- `Delegates`, `Events`: callback/event plumbing
- `Interprocess`: websocket/http message servers, shared-texture reader
- `Server`: `MikanServer` and per-domain RPC request handlers (see [wire-protocol.md](./wire-protocol.md))
- `Localization`, `Math`, `OpenCV`: support code
- `NodeEditors`, `Scripting`: node graph editors and Lua scripting (see [scripting.md](./scripting.md))
- `Renderer`: editor viewport and compositor rendering (see [compositor.md](./compositor.md))

### src/Libraries

Each is a target of the same name, shared unless noted:

- `MikanClientAPI`: public request/response/event structs of the client API; Refureku-annotated wire contract
- `MikanClientCore`: the low-level client core (C API, connection/transport types)
- `MikanCoreApp`: module/device interfaces shared between the editor and plugins (`IMikanModule`, video/VR device interfaces)
- `MikanDMX`: DMX lighting control (`IDMXManager`, E1.31); static
- `MikanGUI`: ImGui wrapper layer (`MkGui*` scoped helpers, styles)
- `MikanMath`: math utilities and transforms (GLM-based)
- `MikanOnnx`: ONNX Runtime wrapper with DirectML-first provider selection (`OnnxSession`); static
- `MikanRenderer`: OpenGL rendering abstraction, the `IMk*` interfaces over shaders, meshes, framebuffers, and scenes
- `MikanSerialization`: reflection-driven JSON/binary (de)serialization used by the wire protocol and configs
- `MikanSharedTexture`: shared texture memory for out-of-band video frame delivery to clients
- `MikanUtility`: general utilities (strings, paths, JSON, logging helpers)
- `MikanWindow`: SDL window / GL context / font management layer

`ARKitReceiver` and `MikanARKitReceiver` directories exist but are empty and not referenced by any `CMakeLists.txt`.

### src/Plugins

Optional backends, each built as a separate `SHARED` DLL and copied next to the executables post-build:

- `MikanWMFVideo`: USB webcam video source via Windows Media Foundation
- `MikanGStreamerVideo`: GStreamer-based network video source (built only when `MIKAN_WITH_GSTREAMER=ON`)
- `MikanSteamVR`: SteamVR/OpenVR tracking device integration

- `MikanARKitVideo`: iOS ARKit network video source (RTP video + pose, CUDA-GL interop), gated behind `MIKAN_WITH_GSTREAMER` alongside `MikanGStreamerVideo`

File video sources are not a plugin: they decode in-process through OpenCV's ffmpeg videoio backend, and takes are recorded through OpenCV's Media Foundation videoio backend, two plugin DLLs the Editor's CMake copies beside the executables.

### src/Programs

- `ClientCodeGen`: the `MikanClientCodeGen` executable; walks Refureku reflection metadata and emits C#/TypeScript bindings
- `Tests/UnitTests`: `unit_test_suite_cpp` C++ unit test executable
- `Tests/MikanClientTestCPP`: C++ client API test app (`DirectX/`, `OpenGL/`, and `Vulkan/` render backends, the last with its HLSL under `Vulkan/Shaders/`)
- `Tests/MikanClientTestCSharp`: C# client test app (Visual Studio generator only)

---

## bindings/

Client-side binding packages consumed by external applications. Generated portions are emitted by `MikanClientCodeGen`; do not hand-edit them, regenerate instead (see [wire-protocol.md](./wire-protocol.md)).

- `bindings/csharp`: C# client library. Hand-written core (`MikanAPI.cs`, `MikanRequestManager.cs`, ...) plus `Generated/` (generated, driven by `CSharpCodeGenConfig.json`). Only builds under a Visual Studio generator.

- `bindings/typescript`: TypeScript client package (`package.json`, `tsconfig.json`). Hand-written core at the root and `Serialization/`; `types/` is generated (driven by `TypeScriptCodeGenConfig.json`); `dist/` is compiled JS output.

---

## Build outputs and generated trees (never hand-edit)

- `build/`: everything under it, including `build/RfkGenerated` (Refureku reflection output)
- `deps/`: recreated by `InitialSetup_x64.bat`
- `dist/`: populated by the `INSTALL` target and the installer
- `bindings/csharp/Generated`, `bindings/typescript/types`, `bindings/typescript/dist`

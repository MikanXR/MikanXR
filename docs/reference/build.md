# Build System

How MikanXR is configured and built: toolchain, dependency setup, CMake targets, the CI configuration, and build gotchas. Directory context is in [layout.md](./layout.md); a copy-paste command cheat sheet is in [commands.md](./commands.md).

---

## Toolchain

- Windows only in practice (Win10/11). The CMake files carry Linux/Darwin branches but the Windows-specific install/copy steps and prebuilt deps make MSVC the supported path.

- MSVC via Visual Studio 2022. CMake minimum 3.15 (`cmake_minimum_required` in the root `CMakeLists.txt`).

- C++20 (`CMAKE_CXX_STANDARD 20` in `cmake/Environment.cmake`), `/W4` with a suppression list, `/MP`, `NOMINMAX` and `_CRT_SECURE_NO_WARNINGS` defined globally.

- Project version is parsed out of `src/Editor/AppCore/Version.h` by `cmake/Version.cmake` (`MIKAN_RELEASE_VERSION_*` defines) into `MIKAN_VERSION_STRING`.

---

## First-time setup

`InitialSetup_x64.bat` (run from the repo root) deletes any existing `build/` and `deps/` folders, then downloads and unpacks prebuilt dependencies into `deps/` using `tools/7zip/7za.exe`: SDL2 2.30.10, SDL2_image 2.8.8, SDL2_ttf 2.24.0 (the devel zips, which carry the runtime DLLs; SDL2_ttf 2.24 statically links freetype so no separate `libfreetype-6.dll`/`zlib1.dll` ship anymore), OpenCV 4.10.0, GLEW 2.2.0, Spout2 2.007h, easy_profiler 2.1.0, libharu 2.4.5, CEF (Chromium 145 binary distribution), ONNX Runtime 1.20.1 (DirectML flavor) and DirectML 1.15.4 (both from NuGet packages), `nuget.exe`, and the DirectX Shader Compiler v1.9.2607 release (`dxc_2026_07_29.zip`, unpacked to `deps/dxc`).

Refureku is not among them. It builds from source as part of the tree, out of the `thirdparty/Refureku` submodule, so it arrives with `git submodule update --init --recursive` rather than through the setup script. See the Refureku section below.

`deps/dxc` exists for one consumer: the Vulkan path of `MikanClientTestCPP` compiles its HLSL to SPIR-V at build time with `dxc -spirv`. The Windows SDK also ships a `dxc.exe`, and its help even lists the `-spirv` options, but that build has no SPIR-V code generator and fails with "SPIR-V CodeGen not available". The harness's CMake therefore looks only under `deps/dxc` and at an explicit `-DDXC_EXECUTABLE=<path>`, and warns and skips the shader step when neither is present, so a tree without `deps/dxc` still configures and builds everything else. The Vulkan headers and volk arrive as `thirdparty/` submodules rather than through the setup script, and nothing in the build links a Vulkan library: volk loads `vulkan-1.dll` at runtime.

The DirectML package ships every architecture at roughly 350MB. The script keeps only `bin/x64-win` and deletes the rest, which matters because CI caches `deps/` keyed on the hash of `InitialSetup_x64.bat`. ONNX Runtime backs the two ML capture tools through the `MikanOnnx` library: the scene lighting estimator ([scene-lighting.md](./scene-lighting.md)) and the depth proxy mesh capture ([depth-proxy-mesh.md](./depth-proxy-mesh.md)).

The model checkpoints those tools consume are not dependencies and `InitialSetup_x64.bat` does not fetch them. They live under a gitignored `models/` at the repo root and are produced by the Python tools in `tools/` (see [commands.md](./commands.md)).

GStreamer is different: the script downloads runtime and devel MSIs (1.26.10 mingw x86_64) and installs them system-wide via `msiexec`, silently (`/qn`) and under `start /wait`, because a plain `msiexec` call from a batch file in an unattended session returns at once without installing. Each install writes a `gstreamer-*-install.log` next to the MSI in `deps/`. An MSI whose product is already installed at that version is skipped, download included: run over an identical install, msiexec switches to maintenance mode, and the secure repair check there rejects the devel package's elevated custom action under `/qn` (error 1730, msiexec exit code 1603). Setting the environment variable `SKIP_GSTREAMER=1` skips both MSIs (CI does this), and `GSTREAMER_ONLY=1` runs only the two MSI installs (the release workflow does this after restoring the cached `deps/`).

The CUDA Toolkit follows GStreamer in the script and is handled much the same way. The 10MB network installer downloads and runs as `-s cudart_<version>`, which fetches only the `include/cuda.h` and `lib/x64/cuda.lib` that `MikanARKitVideo` needs, no nvcc and no driver. `-s` is also how NVIDIA's license is accepted, and the install needs administrator rights. A Toolkit the build can already use, meaning 13.0 or newer at `CUDA_PATH`, is left alone: installing over a newer one would point `CUDA_PATH` back at the older release for every project on the machine. Because the installer writes `CUDA_PATH` machine-wide rather than into the shell that ran setup, project generation has to happen in a new shell or the plugin drops out of the build. `SKIP_CUDA=1` declines the Toolkit, and `SKIP_GSTREAMER=1` implies it, since `MikanARKitVideo` needs both.

Since the script wipes `build/` and `deps/`, rerun project generation afterwards.

The repo's batch files must keep CRLF line endings. `cmd` seeks by byte offset when it resolves `call :label`, and in an LF-only file the second call to a given label fails with "The system cannot find the batch label specified". `.gitattributes` normalizes them on checkout, so the trap is an editor or script that rewrites one with bare newlines.

---

## Configuring

`GenerateProjectFiles_X64_VS2022.bat` configures `build/` with `-G "Visual Studio 17 2022" -A x64` and produces `build/Mikan.sln`. It passes the dependency locations as cache variables: `CEF_ROOT`, `OpenCV_DIR`, `OPENVR_ROOT_DIR`/`OPENVR_HEADERS_ROOT_DIR` (from `thirdparty/openvr`), the `SDL2*_LIBRARY`/`SDL2*_INCLUDE_DIR` pairs, `CMAKE_PREFIX_PATH` for easy_profiler, `NUGET_PATH`, `CMAKE_INSTALL_PREFIX=dist/Win64`, and `-DCMAKE_UNITY_BUILD=ON`.

Notable CMake options (defined in `cmake/ThirdParty.cmake` unless noted):

- `MIKAN_WITH_GSTREAMER` (default `ON`): gates the GStreamer `find_package` calls, the `MikanGStreamerVideo` plugin (`src/Plugins/CMakeLists.txt`), and the GStreamer-dependent unit tests.

- `MIKAN_WITH_ARKIT_VIDEO`: derived, not set by hand. On when `MIKAN_WITH_GSTREAMER` is on and a CUDA Toolkit of 13.0 or newer was found through `CUDA_PATH`. It gates the `MikanARKitVideo` plugin, the copies of its DLL next to `Mikan.exe` and the test executable, and the `CudaGLInterop` sources the unit test suite dual-compiles (`MIKAN_ARKIT_CUDA_GL_INTEROP_AVAILABLE`, which switches `arkit_cuda_gl_interop_unit_tests.cpp` to a skip stub). The remaining ARKit test modules build either way and report a skip when the plugin DLL cannot load. Configure prints which way it resolved.

- `CMAKE_UNITY_BUILD`: on in both local and CI configurations; see the gotcha below.

- `CEF_ROOT`: defaults to the versioned folder under `deps/cef` if not given.

- `CUDA_PATH` (environment): when set with GStreamer enabled, locates the CUDA Toolkit headers and `cuda.lib` for the ARKit plugin's CUDA-GL interop. Toolkit 13 or newer: the plugin uses the four-argument `cuCtxCreate` that 13.0 introduced, so an older Toolkit is read as absent and `MIKAN_WITH_ARKIT_VIDEO` stays off rather than failing the compile. The version comes from `CUDA_VERSION` in the Toolkit's own `cuda.h`. The release workflow installs 13.1.

- `CLANG_FORMAT_EXE`: overrides clang-format discovery for the format targets.

Third-party source builds: `thirdparty/CMakeLists.txt` builds `fast_obj_lib`, `ixwebsocket`, and CEF's `libcef_dll_wrapper` (forced to `/MD` to match Mikan's dynamic CRT). `dylib` is fetched via `FetchContent` at configure time.

---

## Refureku

The reflection library and its generator build from the `thirdparty/Refureku` submodule, which carries its own nested `Kodgen` submodule, so the checkout needs `git submodule update --init --recursive`. `cmake/ThirdParty.cmake` owns the whole integration and exports the same four variables the rest of the tree consumes:

- `RFK_INCLUDE_DIR`: the submodule's `Library/Include/Public`
- `RFK_LIBRARIES`: the `Refureku` target
- `RFK_GENERATOR_EXE`: `$<TARGET_FILE:RefurekuGenerator>`
- `RFK_SHARED_LIBRARIES`: `$<TARGET_FILE:Refureku>`

Four things about the integration are load-bearing, and each exists because the submodule's CMake assumes it is the top-level project:

- **The submodule's own `Refureku/CMakeLists.txt` is skipped.** `ThirdParty.cmake` adds `Generator/` and `Library/` directly. The file in between hardcodes the output directories to `${CMAKE_BINARY_DIR}/Bin` and `/Lib`, and it also carries an `include(CTest)` and the `Dist` target that assembled the prebuilt package this replaced.

- **`CMAKE_CXX_STANDARD` drops to 17 for the subtree.** Kodgen declares `cxx_std_17` as a floor rather than a pin, so Mikan's C++20 would otherwise reach its vendored toml11, which uses the `std::result_of` that C++20 removed. 17 is also what the prebuilt package was compiled as.

- **`CMAKE_UNITY_BUILD` drops to `OFF` for the subtree.** Both local generation and CI configure with it on as a cache entry, and toml11 does not compile with its translation units concatenated.

- **`RefurekuGeneratorRuntime` stages `libclang.dll` and `vswhere.exe` beside the generator.** Kodgen copies both next to its own output directory, which is not where `RefurekuGenerator.exe` lands. The generator loads libclang at startup and shells out to vswhere to locate the MSVC toolchain; without vswhere it fails with `ParsingSettings::compilerExeName must be set to parse files` even though the toml sets it. Each `*Reflection` target depends on this staging target rather than on `RefurekuGenerator` directly.

`ThirdParty.cmake` is `include()`d rather than added as a subdirectory, so it shares the root's variable scope and every override above is saved and restored around the two `add_subdirectory` calls. Everything except `Refureku` and `RefurekuGenerator` is `EXCLUDE_FROM_ALL`, so the submodule's `LibraryGenerator`, examples, and tests cost nothing.

A reflected header's `projectIncludeDirectories` in each `RefurekuSettings.toml` names the submodule's include tree, so a submodule move needs those four files updated alongside `ThirdParty.cmake`.

Kodgen's own post-build step still copies `libclang.dll` into `build/Bin/<Config>/`. That is a duplicate of the staged copy and nothing reads it, but it is 64MB and it is why the subtree's output directories are kept apart from the flattened `build/bin` CI configures.

---

## Key targets

- `MikanEditor`: OBJECT library containing all `src/Editor` code, compiled once.

- `Mikan`: GUI executable (`WIN32` subsystem, `AppCore/EntryPoint.cpp`, embedded manifest so CEF subprocesses see the real OS version). Links `MikanEditor`.

- `MikanCmd`: console executable (`AppCore/CmdEntryPoint.cpp`), same editor object code; runs the editor unit test suite via `-runTests` and the headless ML capture commands (see [commands.md](./commands.md)). `add_dependencies(MikanCmd Mikan)` so the runtime DLL copies (attached only to `Mikan`) are present and never race.

- Library targets: one per `src/Libraries` subdirectory, `SHARED` except `MikanDMX` and `MikanOnnx`, which are `STATIC` (see [layout.md](./layout.md)).

- Plugin DLLs: `MikanWMFVideo`, `MikanSteamVR`, `MikanGStreamerVideo` (GStreamer builds only), and `MikanARKitVideo` (GStreamer plus CUDA builds only). Each is a `SHARED` library compiled with `CXX_VISIBILITY_PRESET hidden` and its own `*_EXPORTS` define, then copied next to `Mikan.exe` by the `copy_mikan_runtime_deps` post-build step in `src/Editor/CMakeLists.txt` (along with SDL2, OpenCV, GLEW, Spout2, CEF, Lua, Refureku, easy_profiler, ONNX Runtime, and DirectML runtime DLLs).

- `MikanClientCodeGen`: bindings generator executable (`src/Programs/ClientCodeGen`).

- `MikanTypeScriptCodeGen` / `MikanCSharpCodeGen`: custom targets that run `MikanClientCodeGen` with `TypeScriptCodeGenConfig.json` / `CSharpCodeGenConfig.json`; the `MikanClientTypeScript` target then runs `npm install` and `npm run build`. The generators walk the reflection database in an order that varies between runs, so a regeneration can reorder unchanged classes in a generated file. Discard those reorders with `git checkout` rather than committing them, since the content is identical.

- `unit_test_suite_cpp`: C++ unit tests; its `unit_test_suite_reflection` dependency runs `RefurekuGenerator.exe` first. `MikanClientAPIReflection`, `MikanClientCoreReflection`, `MikanSerializationReflection` do the same for the reflected libraries.

- `FormatFix` / `FormatCheck`: clang-format wrappers (`cmake/ClangFormat.cmake`), both delegating to `cmake/RunClangFormat.cmake`.

- `LocalizationSync` / `LocalizationCheck`: wrappers around `tools/localization.py` (`cmake/Localization.cmake`), which regenerates the JSON string tables from the gettext catalogs. See [localization.md](./localization.md).

- `CREATE_INSTALLER`: Inno Setup installer build (`cmake/Installer.cmake`); only created when `ISCC.exe` (Inno Setup 6) is found. Fills in `templates/installer_win64.iss.in` and writes `dist/Mikan_<version>_Win64_Setup.exe` from the `dist/Win64` payload.

- `INSTALL`: installs exes, DLLs, bindings, and the bundled `resources/` tree into `dist/Win64`, and every executable and DLL PDB into `dist/symbols/Win64`. The client API headers it installs are gathered by a configure-time glob, so a header added to `MikanClientAPI/Public` reaches `dist/Win64/include` only after a reconfigure (`cmake -B build`). The resources filter ships the graph, material, shader, and model sources as well as images, fonts, scripts, and ONNX models, since a project reads the bundled assets in place rather than owning copies.

- `PACKAGE_APP` / `PACKAGE_SYMBOLS` (`cmake/Symbols.cmake`): zip `dist/Win64` and `dist/symbols/Win64` into `dist/Mikan_<version>_Win64.zip` and `dist/Mikan_<version>_Win64_symbols.zip`. Both assume `INSTALL` has run.

Output locations: under the VS generator, executables land in per-target config folders (`build\src\Editor\<Config>\Mikan.exe`, `build\src\Programs\Tests\UnitTests\<Config>\unit_test_suite_cpp.exe`). CI overrides this with `CMAKE_RUNTIME_OUTPUT_DIRECTORY=build\bin`.

---

## Symbols

Release builds compile with `/Z7` and link with `/DEBUG:FULL /OPT:REF /OPT:ICF` (`cmake/Environment.cmake`), so every executable and DLL has a PDB while the optimized code stays what a plain Release link produces. The options are directory-scoped on purpose: `cmake/cef_variables.cmake` clears `CMAKE_CXX_FLAGS_RELEASE` under Ninja in the scope where `find_package(CEF)` runs, which would silently drop a flag set that way. `/Z7` rather than `/Zi` because sccache and unity builds need the debug info inside the object file; `thirdparty/CMakeLists.txt` forces `CEF_DEBUG_INFO_FLAG` to the same for `libcef_dll_wrapper`.

`cmake/Symbols.cmake` walks the `src` target tree and installs the PDB of every executable, shared, and module library under `dist/symbols/Win64` (the type filter matters: `$<TARGET_PDB_FILE>` is a generate-time error for OBJECT, static, and utility targets). A crash minidump from a shipped build is only readable against that build's PDBs, which is why every release carries the symbols zip (see [debugging.md](./debugging.md)).

---

## CI configuration

`.github/workflows/build-and-test.yml` has three jobs:

- `format-check` (Linux, no build tree): `pipx install clang-format==19.1.5`, then `cmake -P cmake/RunClangFormat.cmake -- --check`. The version is pinned to match the clang-format 19.1.x bundled with VS2022; other major versions format differently.

- `localization-check` (Linux, no build tree): `pip install polib`, then `python tools/localization.py check`. Fails when a committed string table is not what the generator produces from the catalogs, or when a translation breaks a loader rule. See [localization.md](./localization.md).

- `build` (windows-2022): initializes only the needed submodules, caches `deps/` keyed on the hash of `InitialSetup_x64.bat`, runs setup with `SKIP_GSTREAMER=1`, and configures with the Ninja generator instead of Visual Studio. Ninja is used specifically so `CMAKE_C/CXX_COMPILER_LAUNCHER=sccache` takes effect (the VS/MSBuild generator ignores compiler launchers). Key configure differences from local:

```
-G "Ninja" -DCMAKE_BUILD_TYPE=Release
-DMIKAN_WITH_GSTREAMER=OFF
-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=%GITHUB_WORKSPACE%\build\bin
-DCMAKE_C_COMPILER_LAUNCHER=sccache -DCMAKE_CXX_COMPILER_LAUNCHER=sccache
-DCMAKE_UNITY_BUILD=ON
```

CI then builds `MikanCmd` and `unit_test_suite_cpp`, runs `build\bin\MikanCmd.exe -runTests` (dumping `MikanCmd.log` afterwards) and `build\bin\unit_test_suite_cpp.exe`, crashes `MikanCmd` on purpose with `-crash=access` and requires the report files to appear, and uploads `build\bin` (PDBs included) as an artifact on `main` pushes.

`.github/workflows/release.yml` runs on a `v*` tag push. It restores the same `deps/` cache, installs the GStreamer MSIs (`GSTREAMER_ONLY=1`) and a minimal CUDA toolkit (the `MikanARKitVideo` plugin builds under the GStreamer gate and includes `cuda.h`), checks the tag against `src/Editor/AppCore/Version.h`, configures like CI with `MIKAN_WITH_GSTREAMER=ON`, builds the `install` target, runs both suites and the crash check, then `PACKAGE_SYMBOLS`, `PACKAGE_APP`, and `CREATE_INSTALLER`, and drafts a GitHub release with the three assets. See [commands.md](./commands.md) for the tagging procedure.

---

## Gotchas

- Unity build: `CMAKE_UNITY_BUILD=ON` concatenates unrelated translation units, so a transitively included `windows.h` can rewrite a same-named method via macro (e.g. `GetObject` becomes `GetObjectA`) and produce an `LNK2019` far from the actual conflict. Avoid method names that collide with Win32 macros (`GetObject`, `SendMessage`, `CreateWindow`, ...). Files with inclusion-order problems are opted out via `SKIP_UNITY_BUILD_INCLUSION` in `src/Editor/CMakeLists.txt`.

- C# is only supported by Visual Studio generators. `bindings/csharp` and `src/Programs/Tests/MikanClientTestCSharp` are skipped under Ninja (`if(CMAKE_GENERATOR MATCHES "Visual Studio")` in `bindings/CMakeLists.txt` and `src/Programs/Tests/CMakeLists.txt`), so CI never builds them.

- The TypeScript binding targets require `npm` on PATH; if it is missing, configuration emits a warning and skips them.

- Refureku generated headers land in `build/RfkGenerated/<Library>`. The `*Reflection` custom targets regenerate them before each dependent build.

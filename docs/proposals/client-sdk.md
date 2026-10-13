# Proposal: A separate client SDK tree, and a client SDK release asset

Design agreed. Parts 1 and 2 are implemented, and part 3 is implemented pending its dry run. Three parts, each building on the last:

- Part 1: install the client SDK to its own tree, `dist/ClientSDK/Win64`, holding only what a client compiles, links, and loads.
- Part 2: stop shipping headers and import libraries in the app tree, `dist/Win64`.
- Part 3: package the SDK tree as a fourth release asset, so a client can take its SDK from a GitHub release instead of a local build.

Parts 1 and 2 together subsume the earlier option of moving `DiscreteGpuPreference.h` out of `MikanCoreApp/Public`: the SDK tree never ships `MikanCoreApp` headers, and the app tree stops shipping headers at all.

The SDK has two consumers today, the Unreal plugin and the link-engine game `jasper-mozu-studio`, and a Unity plugin is planned. Breaking changes to how they consume it are acceptable.

---

## The problem

`dist/Win64/include` is the client SDK today. Client integrations copy it (the Unreal plugin's `FetchMikan.bat` copies `%MIKAN_DIST_PATH%\include\*.h` wholesale, plus DLLs and import libraries from `dist/Win64` and `dist/Win64/lib`). It is also part of the `dist/Win64` payload that `PACKAGE_APP` zips and `CREATE_INSTALLER` packs, so end users receive it.

Nothing selects which files belong in it. Each library's `CMakeLists.txt` globs `Public/*.h` into its `PUBLIC_HEADER` property, and its `install(TARGETS ...)` rule sends that property to `${MIKAN_ARCH_INSTALL_PATH}/include` and its import library to `${MIKAN_ARCH_INSTALL_PATH}/lib`. Every library and plugin that installs this way ships every public header and every import library:

- `MikanClientAPI` (50)
- `MikanCoreApp` (23)
- `MikanSerialization` (14)
- `MikanUtility` (13)
- `MikanSharedTexture` (5)
- `MikanClientCore` (4)
- `MikanARKitVideo`, `MikanGStreamerVideo`, `MikanSteamVR`, `MikanWMFVideo` (2 each)

That is 117 headers and 10 import libraries. The client-facing files are also mixed into a directory that holds the editor, CEF, OpenCV, and the test programs, so a consumer script that copies DLLs has to name each one, and it has no way to tell a new client dependency from a new editor dependency.

`DiscreteGpuPreference.h` (`MikanCoreApp/Public`) is the shipped header with a failure mode. It defines `MIKAN_REQUEST_DISCRETE_GPU()`, which exports `NvOptimusEnablement` and `AmdPowerXpressRequestHighPerformance`. Expanding it in a client is wrong in both directions: Unreal's Windows launch code already exports both symbols, so a monolithic game build fails to link on duplicates, and in a modular build the export lands in a DLL, where the drivers never read it.

---

## What a client needs (measured)

**Headers.** The transitive include closure of `MikanClientAPI/Public`, restricted to repo headers, is 58 headers from three libraries:

- `MikanClientAPI` (all 50)
- `MikanSerialization` (6 of 14): `SerializableString.h`, `SerializableList.h`, `SerializableMap.h`, `SerializableObjectPtr.h`, `SerializationProperty.h`, `SerializationExport.h`
- `MikanClientCore` (2 of 4): `MikanCoreTypes.h`, `MikanCoreExport.h`

The Refureku `.rfkh.h` includes and `Refureku/*.h` includes in those headers sit behind the reflection defines (`SERIALIZATION_REFLECTION_ENABLED` and its siblings), which a client never sets, so no Refureku header is part of the SDK. No other third-party header is reached. The closure measured against the Unreal plugin alone was 46, so the plugin is a subset of this set.

The SDK also ships the other two `MikanClientCore` headers, which are outside that closure, bringing the set to 60:

- `MikanCoreCAPI.h`: the core C API that the C# bindings P/Invoke into. It includes only `MikanCoreExport.h` and `MikanCoreTypes.h`.
- `MikanCoreVulkanTypes.h`: the Vulkan render target contract. `jasper-mozu-studio` includes it, and so does `MikanClientTestCPP`. It includes `<vulkan/vulkan_core.h>`, so a client using it supplies its own Vulkan headers. The SDK does not ship them.

**Import libraries.** A client needs one only for a DLL whose exported symbols its code calls. The SDK headers export from three DLLs:

- `MikanClientAPI.lib` (`MIKAN_API` classes and functions)
- `MikanSerialization.lib` (`SERIALIZATION_API` on `String`, `PolymorphicObjectPtr`, the map enumerators, `findEnumValueByString`, `getEnumStringValue`)
- `MikanClientCore.lib` (the `MIKAN_CORE_CAPI` functions in `MikanCoreCAPI.h`, and `MIKAN_CORE_API` on two structs in `MikanCoreTypes.h`)

Both consumers also link `MikanCoreApp.lib`, `MikanUtility.lib`, and `MikanSharedTexture.lib`, which nothing in the SDK headers calls.

**Runtime DLLs.** The import-table closure of `MikanClientAPI.dll`, restricted to files MikanXR ships:

- `MikanClientAPI.dll`
- `MikanClientCore.dll`
- `MikanCoreApp.dll`
- `MikanSerialization.dll`
- `MikanUtility.dll`
- `MikanSharedTexture.dll`
- `Refureku.dll`
- `glew32.dll`
- `SpoutLibrary.dll`

`MikanSharedTexture.dll` also imports `D3DCOMPILER_47.dll`, which Windows 10 and later carry in `System32`, so the SDK does not ship it.

**Bindings.** `MikanClientCSharp.dll` and its NuGet dependencies (`Newtonsoft.Json.dll`, `System.Buffers.dll`, `System.Memory.dll`, `System.Numerics.Vectors.dll`, `System.Runtime.CompilerServices.Unsafe.dll`) currently install to `dist/Win64/lib`. The compiled TypeScript bindings install to `dist/Win64/bindings/typescript`.

---

## Part 1: the client SDK tree

A new install tree beside the app tree, populated by the same `INSTALL` target:

- `dist/ClientSDK/Win64/`
	- `include/`: the 60 SDK headers
	- `lib/`: the three import libraries
	- `bin/`: the nine runtime DLLs
	- `bindings/csharp/`: `MikanClientCSharp.dll` and its dependencies
	- `bindings/typescript/`: the compiled TypeScript bindings
	- `symbols/`: PDBs for the Mikan DLLs in `bin/`, so an integrator debugging a crash inside the client library has symbols
	- `version.txt`: the MikanXR version the SDK was built from

A consumer script then copies directory by directory (`bin/*.dll`, `lib/*.lib`, `include/*.h`) and never has to name a file. A new runtime dependency of `MikanClientAPI.dll` reaches clients by being added to the SDK's DLL list, in one place.

### Where the rules live

A single `cmake/ClientSdk.cmake`, included from the root `CMakeLists.txt` after the libraries and bindings are defined, holds every SDK install rule. The SDK is then reviewable in one file, and the per-library `CMakeLists.txt` files keep only the app tree's rules. CMake allows a target to be installed by more than one `install(TARGETS ...)` rule, so the SDK rules install `MikanClientAPI`, `MikanClientCore`, and the others a second time to the SDK destinations without touching the app rules.

Every SDK rule carries `COMPONENT ClientSDK`. `INSTALL` still installs everything, and iterating on the client API alone becomes a fast partial install:

```
cmake --install build --config Release --component ClientSDK
```

An `INSTALL_CLIENT_SDK` custom target wrapping that command gives the Visual Studio solution the same one-click entry point `INSTALL` has.

### Defining the header set

An explicit list in `ClientSdk.cmake` names every SDK header, all 60 of them, including the 50 from `MikanClientAPI/Public` rather than a glob of that folder. The list is the definition of the client API's native surface. A header reaches clients only by being added to it, so every addition is a reviewed line in one file.

### Keeping it correct

The explicit list rules out a leak, since nothing reaches the SDK without being named. The remaining failure is a missing header or import library, which breaks every client. A consumer build guards it:

- A small CMake project, configured against `dist/ClientSDK/Win64` plus the repo's `thirdparty/Vulkan-Headers` (for `MikanCoreVulkanTypes.h`) and nothing else, compiles one translation unit per SDK header and links a test executable that calls into all three import libraries. A header that includes an unlisted repo header, or a symbol whose import library is absent, fails the build.

CI runs it after the install step. The Ninja CI configuration does not build the C# bindings, so the CI check covers the native SDK only.

### Steps

- [x] Add `cmake/ClientSdk.cmake` with the header list, import library, DLL, bindings, and PDB rules under `COMPONENT ClientSDK`, and the `INSTALL_CLIENT_SDK` target.
- [x] Add the SDK consumer project and wire it into `build-and-test.yml` after install.
- [x] Reconfigure, run `INSTALL`, and confirm the SDK tree holds exactly the expected files.
- [x] Update `docs/reference/build.md` (the install targets and the new component), `docs/reference/layout.md` (the `dist/ClientSDK` tree), `docs/reference/commands.md` (the component install), and `docs/reference/wire-protocol.md` (the SDK header list as the native client surface, and that adding a client-facing header means adding it to the list).

---

## Part 2: the app tree stops shipping SDK files

Once clients read from the SDK tree, `dist/Win64/include` and `dist/Win64/lib` have no consumer. The app never compiles or links against its own install. Dropping them:

- [x] Remove the `PUBLIC_HEADER DESTINATION` and `ARCHIVE DESTINATION` clauses from every library and plugin install rule, keeping `RUNTIME`.
- [x] Move the C# bindings' install out of `dist/Win64/lib` into the SDK tree only, since the app does not load them. `MikanClientTestCSharp` is the exception to check: it runs from `dist/Win64` and needs the assembly beside it, which is its own `RUNTIME` rule's concern.
- [x] Move `dist/Win64/bindings/typescript` to the SDK tree only. Nothing in `src/` or `resources/` references it, which is confirmed again at implementation time.

The end-user app zip and installer then carry no headers or import libraries, and `DiscreteGpuPreference.h` stops shipping anywhere. Breaking the old `dist/Win64` layout is acceptable, and the consumers move to the SDK tree as part 2 lands.

---

## Part 3: the SDK as a release asset

A `PACKAGE_CLIENT_SDK` target beside `PACKAGE_APP` and `PACKAGE_SYMBOLS` zips `dist/ClientSDK/Win64` to `dist/Mikan_<version>_Win64_ClientSDK.zip`. `release.yml` runs it after install and attaches the zip to the draft release and the workflow artifact, alongside the app zip, the symbols zip, and the installer.

The SDK's version is the release's version, and `version.txt` inside the zip records it, so a client can state which MikanXR release its SDK came from and check it against the editor it connects to.

On the consumer side, each fetch script gains a second source: given a release tag instead of a local path, it downloads the release's `_ClientSDK.zip` and unpacks it into the same layout it builds from a local `dist/ClientSDK/Win64`. Local iteration and release pinning then share one copy step.

### C# in the release SDK

The release build configures with Ninja, which skips the C# bindings. `release.yml` gains a second configure with a Visual Studio generator that builds `MikanClientCSharp` and installs only the C# rules into the same `dist/ClientSDK/Win64`, so the zip is complete. The upcoming Unity plugin is the consumer that needs it.

- The C# install rules carry their own component, `ClientSDKCSharp`, so the Visual Studio build installs the assembly and its dependencies without overwriting the Ninja build's native DLLs, whose PDBs are the ones in the symbols zip. `INSTALL_CLIENT_SDK` installs both components.

- `MikanClientCSharp` depends on `MikanCSharpCodeGen`, which builds the C++ `MikanClientCodeGen` and the libraries it links. The second configure therefore compiles part of the C++ tree a second time, outside sccache's reach. That is the cost of this step. The generated C# is checked in, so cutting that dependency for the release build is possible later if the step is slow.

### Steps

- [x] Add `PACKAGE_CLIENT_SDK` to the packaging targets.
- [x] Give the C# install rules the `ClientSDKCSharp` component.
- [x] Add the Visual Studio configure, the C# build and component install, the packaging step, and the zip to the artifact and release file lists in `release.yml`.
- [ ] Run a manual release dry run and unpack the SDK zip into clean checkouts of both consumers to confirm they build.
- [x] Update `docs/reference/build.md` for the new packaging target, the C# release step, and the release asset.

---

## Outside this repo

Both consumers change in lockstep with parts 1 and 2, and gain the release download mode with part 3:

- The Unreal plugin (`MikanXR_UE`, and its vendored copy in client projects): `FetchMikan.bat` reads `dist/ClientSDK/Win64` and copies `include/`, `lib/`, and `bin/` whole. `PublicAdditionalLibraries` drops to `MikanClientAPI.lib`, `MikanClientCore.lib`, and `MikanSerialization.lib`. Both of its targets are built against the new tree.

- `jasper-mozu-studio` (link-engine): `tools/fetch-mikan.mjs` defaults to `../MikanXR/dist/ClientSDK/Win64` and copies `include/`, `lib/`, and `bin/` whole instead of naming six libraries and three runtime DLLs. `source/CMakeLists.txt` declares imported targets for the three SDK import libraries only. Its runtime copy step already globs `bin/*.dll`, so it needs no change. It already supplies Vulkan headers for `MikanCoreVulkanTypes.h`.

---

## Open questions

- Is the C# assembly loadable by Unity as built (it targets the .NET Framework through the Visual Studio generator)? This is settled when the Unity plugin starts, and it may move the C# build to an SDK-style project.

# Proposal: Ship only client-facing headers in the SDK

Proposed, not decided. Two changes: option 1 removes the one shipped header a client could misuse, and option 2 stops the install from shipping internal headers at all. Option 1 stands on its own and can land first. Option 2 subsumes it but is larger.

---

## The problem

`dist/Win64/include` is the client SDK. It is what client integrations copy (the Unreal plugin's `FetchMikan.bat` copies `%MIKAN_DIST_PATH%\include\*.h` wholesale), and it is part of the `dist/Win64` payload that `PACKAGE_APP` zips and `CREATE_INSTALLER` packs, so end users receive it too.

Nothing selects which headers belong in it. Each library's `CMakeLists.txt` globs `Public/*.h` into its `PUBLIC_HEADER` property, and its `install(TARGETS ...)` rule sends that property to `${MIKAN_ARCH_INSTALL_PATH}/include`. Every library and plugin that installs this way ships every public header:

- `MikanClientAPI` (50)
- `MikanCoreApp` (23)
- `MikanSerialization` (14)
- `MikanUtility` (13)
- `MikanSharedTexture` (5)
- `MikanClientCore` (4)
- `MikanARKitVideo`, `MikanGStreamerVideo`, `MikanSteamVR`, `MikanWMFVideo` (2 each)

That is 117 headers. Measured against the Unreal plugin, whose sources and their transitive includes reach 46 of them, a real client needs headers from three libraries only: `MikanClientAPI` (38), `MikanSerialization` (6), and `MikanClientCore` (2). Nothing from `MikanCoreApp`, `MikanUtility`, `MikanSharedTexture`, or the video and VR plugins is reached.

A refresh of the Unreal plugin's SDK copy surfaced three new arrivals, none of them client-facing:

- `DiscreteGpuPreference.h` (`MikanCoreApp/Public`): defines `MIKAN_REQUEST_DISCRETE_GPU()`, which exports `NvOptimusEnablement` and `AmdPowerXpressRequestHighPerformance`. Only the MikanXR executables' entry points use it.

- `FatalStartupError.h` (`MikanCoreApp/Public`): the editor's startup failure record, used by `MikanRenderer` (`GlShaderCache.cpp`) and the editor app.

- `ReflectionHandles.h` (`MikanSerialization/Public`): the opaque `rfk::Struct` handle alias, included by `TypeRegistry.h`, `JsonSerializer.h`, `JsonDeserializer.h`, `BinarySerializer.h`, `BinaryDeserializer.h`, and `SerializationVisitor.h`. The Unreal plugin reaches none of those.

`DiscreteGpuPreference.h` is the one with a failure mode. Including it is inert, since it only defines a macro. Expanding the macro in a client is wrong in both directions: Unreal's Windows launch code already exports both symbols, so a monolithic game build fails to link on duplicates, and in a modular build the export lands in a DLL, where the drivers never read it, so it silently does nothing. The GPU preference is a property of the executable, and a client SDK has no business offering it.

---

## Option 1: move `DiscreteGpuPreference.h` out of the installed set

The header's consumers are all MikanXR executables:

- `src/Editor/AppCore/EntryPoint.cpp` (`Mikan`)
- `src/Editor/AppCore/CmdEntryPoint.cpp` (`MikanCmd`)
- `src/Programs/Tests/MikanClientTestCPP/EntryPoint.cpp`
- `src/Programs/Tests/UnitTests/unit_test_suite.cpp`

Each of those targets already names `MikanCoreApp/Public` in its include directories, which is the only reason the header lives there. It is header-only and exports nothing from `MikanCoreApp.dll`, so it has no reason to belong to that library.

- [ ] Move the header to a directory no install rule reads, shared by the executables. Candidate: a new `src/Shared/` (or similar) holding build-internal headers for programs.
- [ ] Add that directory to the include paths of `Mikan`/`MikanCmd` (`src/Editor/CMakeLists.txt`), `MikanClientTestCPP`, and `unit_test_suite_cpp`.
- [ ] Reconfigure (`cmake -B build`) so the `MikanCoreApp/Public` glob drops it, build, run both suites, and run `INSTALL` to confirm it is absent from `dist/Win64/include`.
- [ ] Update `docs/reference/layout.md` for the new directory, and `docs/reference/modules.md` if it lists the header under `MikanCoreApp`.

Cost is a handful of lines. It does not touch the other 70 unneeded headers.

---

## Option 2: install an explicit client header set

Replace "every library's `Public/*.h`" with a defined SDK: the headers a client compiles against, and nothing else.

### Defining the set

The set is the transitive include closure of `MikanClientAPI/Public`, restricted to headers inside the repo. It is not the closure of any one client's includes. The Unreal plugin's 46 is a lower bound that measures what one integration touches today, and the SDK has to serve every client of the API. Computing the real closure is the first step, and its result decides which of the design choices below applies.

### Design choices

- **Where the selection lives.** Either a single install rule (say `cmake/ClientSdk.cmake`) that installs a named header list to `${MIKAN_ARCH_INSTALL_PATH}/include`, with the per-library `PUBLIC_HEADER DESTINATION` clauses removed, or per-library `PUBLIC_HEADER` lists narrowed to their client-facing subset. The single rule makes the SDK reviewable in one place. Per-library lists keep ownership with each library but spread the definition across files.

- **Libraries with no client headers.** `MikanCoreApp`, `MikanUtility`, `MikanSharedTexture`, and the four video and VR plugins drop their header install entirely if the closure confirms they contribute nothing. Their DLLs keep installing, since clients load `MikanClientAPI.dll`, which depends on them at runtime.

- **Libraries split between client and internal.** `MikanSerialization` ships 14 headers where the plugin reaches 6. If the closure confirms a split, the choice is between listing the client-facing ones explicitly and moving the internal ones out of `Public/` (for example a `Public/` versus `Internal/` split, where only `Public/` installs). Moving files is the stronger invariant, because a new header then lands on the right side by folder rather than by remembering to update a list.

- **Import libraries.** The Unreal plugin also links `MikanCoreApp.lib`, `MikanUtility.lib`, `MikanSerialization.lib`, and `MikanSharedTexture.lib`. A client only needs an import library for a DLL whose exported symbols it calls. Once the header set is settled, check which import libraries a client still needs, and stop installing the rest to `lib/` if nothing else consumes them.

### Keeping it correct

A list rots in both directions: a new client-facing header that is not added breaks clients, and an internal header added to the list leaks again. A guard makes both mechanical:

- **Self-contained:** every installed header compiles on its own against only `dist/Win64/include`. This catches a missing header.
- **Minimal:** every installed header is reachable from the `MikanClientAPI` entry headers. This catches a leak.

Both can run as a post-install check, in CI after the `install` target, or as a small script beside the existing tooling. The self-contained check is the more valuable of the two, since a missing header breaks every client and a leak breaks none.

### Steps

- [ ] Compute the transitive include closure of `MikanClientAPI/Public` and record which libraries it reaches.
- [ ] Decide where the selection lives and how split libraries are handled, based on that closure.
- [ ] Implement the install change, reconfigure, run `INSTALL`, and diff `dist/Win64/include` before and after.
- [ ] Add the self-contained and minimal checks, and wire them into CI after `install`.
- [ ] Build the Unreal plugin against the new `dist` (`FetchMikan.bat` already clears old headers before copying, so stale ones do not survive) and confirm both of its targets still compile.
- [ ] Update `docs/reference/build.md` (its `INSTALL` entry describes the header glob), `docs/reference/wire-protocol.md` if it describes the shipped SDK, and `docs/reference/modules.md` for any library that stops shipping headers.

### Outside this repo

The Unreal plugin (`MikanXR_UE`, and its vendored copy in client projects) is a separate repo. Nothing there has to change for option 2, since it copies whatever `dist/include` holds. Its `PublicAdditionalLibraries` list is worth trimming afterwards to match the import library decision above.

---

## Open questions

- Does any client besides the Unreal plugin compile against `dist/Win64/include` (MikanARStreamer, a Unity native plugin, third-party integrations)? The closure of `MikanClientAPI/Public` should cover them, but a consumer that reaches into `MikanCoreApp` or `MikanUtility` directly would break when those stop shipping.
- Should `dist/Win64/include` stay in the end-user app package at all, or move to a separate SDK artifact alongside `PACKAGE_APP` and `PACKAGE_SYMBOLS`? Out of scope for both options, but option 2 makes the SDK boundary explicit enough to split out later.

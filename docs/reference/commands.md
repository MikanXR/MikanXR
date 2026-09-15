# Command Cheat Sheet

Handy commands for working in the MikanXR repo, all run from the repo root unless noted. Background on the build system is in [build.md](./build.md); directory context is in [layout.md](./layout.md).

---

## One-time setup

```
InitialSetup_x64.bat
```

Downloads prebuilt dependencies into `deps/` (large download) and installs GStreamer system-wide via MSI. Warning: it deletes any existing `build/` and `deps/` first. Set `SKIP_GSTREAMER=1` in the environment to skip the GStreamer MSIs (then configure with `-DMIKAN_WITH_GSTREAMER=OFF`).

```
git submodule update --init --recursive
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

The second command (once per clone) makes `git blame` skip the repo-wide clang-format commit listed in `.git-blame-ignore-revs`.

---

## Generate project files

```
GenerateProjectFiles_X64_VS2022.bat
```

Configures `build/` with the `Visual Studio 17 2022` generator and produces `build/Mikan.sln`. Rerun after `InitialSetup_x64.bat` (which wipes `build/`).

---

## Build

Via CMake (works for any generator; add `--config Release` under the VS generator):

```
cmake --build build --target Mikan --config Release --parallel
cmake --build build --target MikanCmd --config Release --parallel
cmake --build build --target unit_test_suite_cpp --config Release --parallel
```

Or open `build\Mikan.sln` in Visual Studio 2022 and build there.

Building `MikanCmd` also builds `Mikan` (dependency) and copies all runtime DLLs and plugin DLLs into the shared output folder.

---

## Run tests

Under the local VS generator, executables land in per-config folders:

```
build\src\Editor\Release\MikanCmd.exe -runTests
build\src\Programs\Tests\UnitTests\Release\unit_test_suite_cpp.exe
```

In CI (and any configure with `-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=...\build\bin`) both live in `build\bin\`:

```
build\bin\MikanCmd.exe -runTests
build\bin\unit_test_suite_cpp.exe
```

- `MikanCmd.exe -runTests` runs the editor unit test modules (tracker pose calibrator, client-API property schema guard, depth mesh generator, DMX universe RLE, file video source, light environment persistence, script variable persistence, project asset catalog) and writes details to `MikanCmd.log` in the working directory in addition to stdout. Check that file if the run fails with no console output (e.g. a crash exit code like `0xC0000005`).

- Both suites must pass in CI (`.github/workflows/build-and-test.yml`).

The crash reporter is exercised by crashing on purpose (see [debugging.md](./debugging.md)):

```
build\bin\MikanCmd.exe -crash=access -crashReportDir=build\crash   # kinds: access abort terminate purecall invalidparam stackoverflow
python tools/automate.py "app crash access"                          # same through a running editor
```

CI runs the first form and requires a `.dmp` and a `.txt` in the report folder.

One check needs a running editor instead, so it sits outside the suites:

```
python tools/large_response_check.py
```

It fetches a model stencil's render geometry over a raw websocket and requires the connection to keep answering afterwards, guarding the large-response stall described in [debugging.md](./debugging.md). The loaded project needs a model stencil with geometry.

---

## Client test harness

`MikanClientTestCPP.exe` (built beside its DLLs under `build/src/Programs/Tests/MikanClientTestCPP/Release/`) connects to a running `Mikan.exe` and renders a spinning cube into every camera the editor reports, over one of three render paths:

```
MikanClientTestCPP.exe -dx
MikanClientTestCPP.exe -gl
MikanClientTestCPP.exe -vk
```

`-dump <png>` writes the connected camera's color target five seconds in, and beside it `<png>.spout.png` and `<png>.depth.spout.png`, the color and packed-depth frames read back from the client's own Spout senders through the editor's receiver path, so a headless drive compares what the client rendered against what the editor receives without depending on a compositor graph. `-cube <x> <y> <z>` places the cube in meters from the camera (x right, y up, z forward) instead of the default 10 m ahead, which is how a drive puts it at a stencil's distance for a depth mask check. `MIKAN_VULKAN_VALIDATION=1` enables the Khronos validation layer on the `-vk` path when one is installed.

---

## ML models and headless capture

The scene lighting estimator and the depth proxy mesh capture run ONNX models out of a gitignored `models/` at the repo root. `InitialSetup_x64.bat` does not fetch them; they are produced once per developer machine by the Python tools in `tools/`, which need `diffusers` pinned to 0.34.0 under torch 2.4.1:

```
python tools/fetch_moge2_onnx.py
python tools/export_marigold_onnx.py
```

`fetch_moge2_onnx.py` downloads the authors' official MoGe-2 export into `models/moge2` (1.3GB, no local export step). `export_marigold_onnx.py` converts the Marigold checkpoints into `models/marigold` (~6.8GB). Details in [scene-lighting.md](./scene-lighting.md) and [depth-proxy-mesh.md](./depth-proxy-mesh.md).

Both capture pipelines have a headless entry point that runs the same C++ code the editor stages do, which is how they are validated against the Python references:

```
MikanCmd.exe -estimateLighting -image=<path> [-fov=<degrees>] [-seed=<n>] [-dump=<dir>] [-cpu]
MikanCmd.exe -depthMesh -image=<path> -fov=<degrees> [-obj=<path>] [-stride=<n>] [-maxDepth=<metres>] [-cpu]
```

- `-estimateLighting` prints the recovered spherical harmonic environment plus its confidence numbers; `-dump=<dir>` also writes the raw model outputs as float TIFFs and the three reconstruction views the capture panel shows.

- `-depthMesh` prints the mesh statistics and writes the OBJ (plus the source frame as a sibling `.png` for the projected texture).

- `-cpu` forces the CPU execution provider instead of DirectML. Both commands take `-models=<dir>` / `-mogeModels=<dir>` to override the default `models/marigold` and `models/moge2` paths.

---

## Material graphs

Compile a material graph headlessly, writing its `.vert`, `.frag`, and `.compmat` or `.shapemat` beside the `.matgraph` (the same code the material editor runs on save; see [materials.md](./materials.md)):

```
MikanCmd.exe -compileMaterial=resources/compositor_materials/rgbaFrame/rgbaFrame.matgraph
```

Run it after editing a bundled material graph outside the editor, since `MikanCmd.exe -runTests` fails when a checked-in graph and its generated files disagree.

---

## Formatting

Only `src/` is formatted; `thirdparty/` is never touched. Use clang-format 19.1.x to match CI (VS2022 bundles a compatible copy under `VC\Tools\Llvm\bin\clang-format.exe`, which the CMake scripts find automatically; otherwise `pip install clang-format==19.1.5`).

With a configured build tree:

```
cmake --build build --target FormatFix      # reformat in place
cmake --build build --target FormatCheck    # check only (what CI runs)
```

Standalone, no build tree needed (same script CI runs):

```
cmake -P cmake/RunClangFormat.cmake -- --check
cmake -P cmake/RunClangFormat.cmake -- --fix
```

Point at a specific binary with `cmake -DCLANG_FORMAT_EXE=path\to\clang-format -P cmake/RunClangFormat.cmake -- --check`.

---

## Localization

Needs `polib` (`pip install polib`). Background in [localization.md](./localization.md).

```
python tools/localization.py sync                        # regenerate the tables from the catalogs
python tools/localization.py check                       # verify they are current (what CI runs)
python tools/localization.py stats                       # per-language translated and reviewed counts
python tools/localization.py new fr Francais             # bootstrap a new language
```

Run `sync` after adding a key to `resources/localization/en.json`; CI fails on a stale generated table. `new` takes `--from-json <path>` to seed a catalog from an existing table, flagging every seeded string fuzzy.

The same first two are also CMake targets, next to `FormatFix`/`FormatCheck` in the IDE:

```
cmake --build build --target LocalizationSync
cmake --build build --target LocalizationCheck
```

---

## Client bindings codegen

Never hand-edit `bindings/csharp/Generated` or `bindings/typescript/types`; regenerate them instead (see [wire-protocol.md](./wire-protocol.md)):

```
cmake --build build --target MikanTypeScriptCodeGen --config Release   # regenerate bindings/typescript/types
cmake --build build --target MikanClientTypeScript --config Release    # codegen + npm install + npm run build
cmake --build build --target MikanCSharpCodeGen --config Release       # regenerate bindings/csharp/Generated (VS generator only)
```

All of these build and invoke `MikanClientCodeGen` with the matching `*CodeGenConfig.json`. The TypeScript targets require `npm` on PATH.

---

## AI usage badges

The badges in `README.md` and the tables in `TOKEN_STATS.md` are rendered from `docs/ai-usage.json`, a committed ledger of per-session token counts:

```
python tools/token_stats.py            # scan local Claude Code transcripts, update the ledger, re-render
python tools/token_stats.py --render   # rewrite the ledger and the docs without scanning
python tools/token_stats.py --check    # verify the rendered files match the ledger (what CI runs)
```

The scan needs the transcripts under `~/.claude/projects/`, so it only works on the machine the sessions ran on. `--check` reads committed files only and writes nothing.

To drop a session the attribution rule wrongly claimed, add its id to the ledger's `excluded` map with `"reason": "manual"` and an optional `note`, then run `--render`. That list is authoritative and permanent: the run deletes the session's counters, and later scans never re-add it. Use `--render` rather than a full scan when the only change is by hand, since a scan of a live session also folds in tokens spent since the last run.

---

## Install, packaging, and the installer

```
cmake --build build --target INSTALL --config Release           # exes/DLLs/bindings/resources into dist\Win64, PDBs into dist\symbols\Win64
cmake --build build --target PACKAGE_APP --config Release       # zip dist\Win64 into dist\Mikan_<version>_Win64.zip
cmake --build build --target PACKAGE_SYMBOLS --config Release   # zip dist\symbols\Win64 into dist\Mikan_<version>_Win64_symbols.zip
cmake --build build --target CREATE_INSTALLER                   # build dist\Mikan_<version>_Win64_Setup.exe (requires Inno Setup 6)
```

The packaging targets assume `INSTALL` has run. `CREATE_INSTALLER` only exists if `ISCC.exe` was found at configure time (`cmake/Installer.cmake`); it compiles `templates/installer_win64.iss.in` with the version and paths filled in.

---

## Releases

Versions are calendar dates. Set `MIKAN_RELEASE_VERSION_YEAR`, `MONTH`, and `DAY` in `src/Editor/AppCore/Version.h` to the release date, zero padded (they are string literals because `09` is not a valid integer literal), leave `REVISION` empty or set it to `.1`, `.2`, ... for a same-day re-cut, commit, then push a tag `v<YYYY.MM.DD>` (plus the revision when there is one):

```
git tag v2026.09.14 && git push origin v2026.09.14
```

`.github/workflows/release.yml` fails unless the tag matches the header. It builds Release with GStreamer on, runs both suites and the crash check, and drafts a GitHub release carrying the app zip, the setup exe, and the symbols zip. Publish the draft after checking the assets. Every published build keeps its symbols zip: a crash dump from that build cannot be read without it.

---

## Reproducing the CI build locally

CI uses Ninja (for sccache) with GStreamer off and a flattened output dir. From a VS2022 developer command prompt:

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DMIKAN_WITH_GSTREAMER=OFF -DCMAKE_UNITY_BUILD=ON -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=%CD%\build\bin ...
```

plus the same dependency path variables as `GenerateProjectFiles_X64_VS2022.bat`; see `.github/workflows/build-and-test.yml` for the exact full invocation. Note the C# bindings and C# client test are skipped under Ninja.

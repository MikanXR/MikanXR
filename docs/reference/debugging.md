# Debugging

Logging, test suites, and diagnostic surfaces for the editor. Build invocations and CI details are in [build.md](./build.md), the executable inventory in [commands.md](./commands.md), and Lua-specific debugging in [scripting.md](./scripting.md).

---

## Logging

The logger lives in `MikanCoreApp` (`src/Libraries/MikanCoreApp/Public/Logger.h`). Levels are `LogSeverityLevel` trace/debug/info/warning/error/fatal. Log with the `MIKAN_LOG_<LEVEL>("Scope::function") << ...` stream macros; use the `MIKAN_MT_LOG_*` variants only off the main thread (they take a mutex). Each process calls `log_init(LoggerSettings)` once; settings choose the minimum level, an optional log file, an optional callback, and whether a Win32 GUI process allocates a console (`enable_console`).

Every line goes to stdout (stderr for error and above) and, if configured, to the log file:

- `Mikan.exe`: `Documents\MikanXR\MikanXR.log` (the projects root from `PathUtils::getProjectsRootDirectory`, a folder an installed build can write to, since `Program Files` is not), minimum level debug, no console (`App::startup`); the same lines feed the editor's log panel through `AutomationLogBuffer::logCallback`.
- `MikanCmd.exe`: `MikanCmd.log` in the working directory, minimum level debug (`CmdApp::exec`). It is a developer tool run from the repo root, and CI reads the log from there.

Both executables parse `-flag` and `-key=value` command-line arguments (`hasCommandLineFlag` / `getCommandLineStringArg`).

The logger flushes every line, so the file is complete up to a crash, which is what lets the crash handler copy it into the report (see [Crash reports](#crash-reports)).

---

## Test suites

Two suites, both required by CI (see [build.md](./build.md)):

**`build\bin\MikanCmd.exe -runTests`** runs the headless editor tests. `CmdApp::exec` (`src/Editor/AppCore/CmdApp.cpp`) builds the Refureku type registry, then `runTests()` runs `run_all_editor_unit_tests()`:

- `run_tracker_pose_calibrator_unit_tests`: `src/Editor/Calibration/Test/TrackerPoseCalibratorTests.cpp`
- `run_client_api_property_schema_tests`: `src/Editor/Server/Test/ClientApiPropertySchemaTests.cpp` (the schema guard that keeps client-API values structs, property descriptors, and `getPropertyValue` consistent; see [wire-protocol.md](./wire-protocol.md))
- `run_depth_mesh_generator_unit_tests`: `src/Editor/Calibration/Test/DepthMeshGeneratorTests.cpp` (the depth shift solver including its negative-z pole case, discontinuity culling, and OBJ winding; see [depth-proxy-mesh.md](./depth-proxy-mesh.md))
- `run_dmx_universe_rle_tests`: `src/Editor/Server/Test/DMXUniverseRLETests.cpp`
- `run_light_environment_persistence_tests`: `src/Editor/Server/Test/LightEnvironmentPersistenceTests.cpp`
- `run_localization_unit_tests`: `src/Editor/Localization/Test/LocalizationTests.cpp` (key parity against English, printf specifier parity, window-title uniqueness, and glyph coverage against the baked font ranges; see [localization.md](./localization.md) and [standards.md](./standards.md))
- `run_material_compiler_tests`: `src/Editor/NodeEditors/Test/MaterialCompilerTests.cpp` (the material graph compiler: stage split, broadcast and type errors, cycles, parameter conflicts, snapshot round trip, and the golden guard that recompiles every shipped `.matgraph` against its checked-in shaders; see [materials.md](./materials.md))

Exit code is nonzero on any failure. Results also go to `MikanCmd.log` next to the working directory.

The localization module reads `resources/localization` relative to the working directory, so run `MikanCmd.exe` from the repo root (as the commands in [commands.md](./commands.md) do) or the tables will not be found.

`Mikan.exe` no longer allocates a console window: its log is the editor's Log panel (View menu), backed by the same `AutomationLogBuffer` ring the `log tail` automation command reads. `MikanXR.log` still receives every line. A callback passed to `log_init` is an additional sink rather than a replacement, so the standard streams and the log file stay connected alongside it.

**`build\bin\unit_test_suite_cpp.exe`** runs the C++ unit tests, entry point `src/Programs/Tests/UnitTests/unit_test_suite.cpp`. Modules: math utility, crash handler (the report directory and pending-marker plumbing; the crash paths themselves are exercised by `MikanCmd.exe -crash`, below), GLM math, serialization, mikan API, spherical harmonic lighting, and ONNX session. The `unit_test.h` macros print a `PASSED`/`FAILED` line per test and per module. The spherical harmonic module is the round-trip guard on the lighting solve described in [scene-lighting.md](./scene-lighting.md): it validates the fit against synthetic data with known ground truth and asserts the l=2 ridge actually shrinks that band. Neither ML module touches a checkpoint or requires a GPU, which is what lets both suites run unchanged on a CI runner: the ONNX module probes for DirectML and reports the result without failing when it is absent, and otherwise only checks that a missing model file fails cleanly instead of throwing. On the `iphone` branch this suite also carries the ARKit wire-protocol, CUDA-GL-interop, video-device, and video-source-system modules; the CUDA-GL interop module deliberately runs early there because it creates its own CUDA context.

---

## When a run crashes with no output

`CmdApp::exec` sets stdout unbuffered (`setvbuf(stdout, nullptr, _IONBF, 0)`) precisely because a piped stdout (CI) is fully buffered and a hard crash would otherwise discard everything: the last printed line is the crash site. Also check `MikanCmd.log`, since test results are written there in addition to stdout. A hard crash also leaves a minidump in the crash report folder (see [Crash reports](#crash-reports)).

If a process dies instantly with exit status `0xC0000135` (`STATUS_DLL_NOT_FOUND`), a runtime DLL is missing next to the executable. Known project gotcha: a locally installed GStreamer on `PATH` can satisfy DLL loads that CI cannot, so a missing post-build DLL copy passes locally and only fails in CI. Verify the DLL copy steps rather than the code.

Second known gotcha: with the default `CMAKE_UNITY_BUILD=ON`, a transitively included `windows.h` in a jumbo TU can rewrite a method name that collides with a Win32 macro (`GetObject` → `GetObjectA`, likewise `SendMessage`, `CreateWindow`, ...), producing an `LNK2019` far from the real conflict. Avoid such method names; see [build.md](./build.md).

Third known gotcha, specific to the node editor: ImGui's `IM_ASSERT` is plain `assert()`, so imgui-node-editor's API-contract checks fire only in a Debug build and vanish under `NDEBUG`. Misusing that API therefore looks harmless in Release and stops the app dead in Debug. Its `Begin`/`End` pairs are not uniformly forgiving either, so read the one you are calling: `DeleteItemsAction::End` returns early when inactive and is safe to call unconditionally, while `CreateItemAction::End` asserts, so `ed::EndCreate()` may only run when `ed::BeginCreate()` returned true.

---

## Crash reports

`CrashHandler` (`src/Libraries/MikanCoreApp/Public/CrashHandler.h`) turns an unhandled crash in `Mikan.exe` or `MikanCmd.exe` into a report on disk instead of the Windows error dialog. Nothing is uploaded anywhere. Each report is a set of files sharing one base name, `<app>_<version>_<yyyy-mm-dd_hh-mm-ss>`:

- `.dmp`: a minidump carrying the crashing thread's context, thread info, handle data, indirectly referenced memory, and the unloaded module list. Data segments are left out to keep the file within an issue attachment's size limit.
- `.txt`: the exception code and its name, the faulting address, the module it falls in with the offset, and the crashing thread id
- `.log`: a copy of the process log at the moment of the crash
- `pending_report.txt`: a marker naming the newest report. The editor consumes it on the next launch: the main menu shows a notice with a button that opens the folder, then clears the marker.

`Mikan.exe` writes to `Documents\MikanXR\CrashReports`. `MikanCmd.exe` writes to the same folder unless `-crashReportDir=<dir>` says otherwise.

What is caught: SEH exceptions (access violations, stack overflows, uncaught C++ exceptions), `abort()` and `std::terminate`, pure virtual calls, and CRT invalid-parameter faults. The last three arrive as CRT callbacks rather than exceptions, so the handler synthesizes an exception record for them with codes `0xE0000001` (abort), `0xE0000002` (purecall), and `0xE0000003` (invalid parameter). Not caught: `__fastfail` paths such as `/GS` stack cookie failures, which no user-mode handler can see, and anything that happens while a debugger is attached, since the debugger takes the exception first.

The dump is written by a thread created at install time, which is what lets a stack overflow on the crashing thread still produce one. The other half of that story is `SetThreadStackGuarantee` at install: after an overflow the OS needs stack of its own to dispatch the exception to the filter, and without a guaranteed slice that dispatch fails on some stack layouts and the process dies with no report (the CI runner hit exactly this while the dev machine did not). The guarantee is per thread and only the installing thread gets it, so an overflow on a worker thread is still best effort. Chromium replaces the process crash hooks during `CefInitialize`, so `App::startup` re-asserts them right after and logs a warning when they had been swapped. A CRT fault that shows up in a report as a breakpoint exception means that re-assert was lost.

Symbolicating a dump needs the PDBs of the build that crashed. Release builds emit them (see [build.md](./build.md)) and each GitHub release carries `Mikan_<version>_Win64_symbols.zip`. Unpack it beside the dump and open the dump in WinDbg (`.sympath+ <folder>` then `!analyze -v`), in Visual Studio, or in a minidump viewer such as CrashCatch Analyze.

To exercise a report path on purpose:

```
build\bin\MikanCmd.exe -crash=access -crashReportDir=build\crash
python tools/automate.py "app crash access"
```

Kinds: `access`, `abort`, `terminate`, `purecall`, `invalidparam`, `stackoverflow`. The process exits with the exception code, and the automation client reports the connection closed, since the reply never comes. CI runs the `MikanCmd` form and requires the `.dmp` and `.txt` to appear.

---

## Stall watchdogs and the delta clamp

`App::tick` clamps the per-frame delta to 0.1 s (`deltaSeconds = fminf(now - m_lastFrameTimestamp, 0.1f)` in `src/Editor/AppCore/App.cpp`), and that clamped value propagates down the entire update chain (`MainWindow` to `ProjectManager` to systems to device managers). A watchdog or timeout that accumulates `deltaSeconds` therefore badly under-counts across a freeze: after a debugger breakpoint resumes, the delta is still at most 0.1 s, so an N-second timeout takes another N real seconds to trip instead of firing immediately. Stall detection must store a `std::chrono::steady_clock::time_point` and compare against `steady_clock::now()` (which keeps advancing while threads are frozen), never `+= deltaSeconds`. `MikanGStreamerVideoDevice`'s frame-stall watchdog is the reference implementation of the correct pattern.

---

## In-process GStreamer decode failures

`Failed to load plugin ... The specified procedure could not be found` when loading `nvh264dec`/`d3d11h264dec` is a PATH problem, not a D3D11/CUDA conflict. That message is `ERROR_PROC_NOT_FOUND`: a dependency did load, but was missing an export the plugin needed.

`libgstnvcodec.dll` links the MinGW runtime (`libstdc++-6.dll`, `libgcc_s_seh-1.dll`, `libwinpthread-1.dll`) alongside the GStreamer libraries. Git for Windows ships its own copies of all three in `C:\Program Files\Git\mingw64\bin`, built from a different GCC. That directory is not on the machine PATH, but Git Bash prepends it to the PATH of anything launched from a Git Bash shell. The loader then resolves the plugin's imports against Git's copies and the load fails. The software decoder `openh264dec` survives it, which is what makes the failure look decoder specific.

So launching `Mikan.exe` from Git Bash silently forces the software decode tier, while the same binary launched from Explorer, Visual Studio, or `cmd` gets hardware decode. Verified both ways: with the machine PATH the ARKit device logs `Decoded CUDA video frame` and never emits the fallback message.

The same cause also presents as `Hardware decode pipeline unavailable ... link has no sink`, logged by `MikanARKitVideoDevice::openOnThread` before it falls back to software. That message reads like a pipeline construction bug rather than a loader problem, because by the time linking is attempted the unloadable element simply does not exist. Do not take the wording as evidence against the PATH explanation.

Filtering the offending directory out of `PATH` needs care from a shell: Git Bash lists it as `/mingw64/bin`, not as anything containing `Program Files/Git`, so a filter written against the Windows-style spelling silently keeps it. Retaining only the `/c/`-prefixed entries is a reliable way to get a Windows-equivalent `PATH`. Note that this also drops `/usr/bin`, so anything invoked through it (`nohup`, `timeout`) must be called by absolute path or avoided.

Before suspecting pipeline code, check how the process was launched, then run the identical pipeline string through a standalone `gst-launch-1.0.exe`. A failed hardware open also produces harmless `GStreamer-CRITICAL`/`GLib-GObject-CRITICAL` assertion spam (GStreamer's own cleanup on this failure mode); it does not crash the process.

---

## Spout logging

Spout keeps its own diagnostics, and Spout 2.007 exposes no log callback: `EnableSpoutLog()` allocates a console window titled "Spout Log" over the host process, and `EnableSpoutLogFile()` appends to a file. Both flags live in module globals, so one call configures every sender and receiver in that module. Spout logging is off by default everywhere.

The editor turns it on through `SpoutLogRelay` (`src/Editor/Interprocess/SpoutLogRelay.cpp`), owned by `App` and gated on the `spoutLogEnabled` setting in `AppSettingsConfig` (the "Relay Spout Logs" checkbox in the project Settings panel, off by default). Enabling it points Spout's file logging at `MikanSpout.log` next to `MikanXR.log` at verbose level, and `App::tick` tails the new bytes each frame into the logger under the `Spout` scope, so Spout's output lands in the log panel and `MikanXR.log` with no extra window. Level tags map `[warning]`/`[error]`/`[fatal]` onto the matching severity; `[notice]` and untagged verbose lines log as info.

Client processes (and the editor's own sender path inside `MikanSharedTexture`) have no settings file to read, so they gate on the `MIKAN_SPOUT_LOG` environment variable instead, applied once per process in `SharedTextureWriter.cpp`:

- unset, `0`, `off`: no Spout logging (default)
- `console`, `1`, `on`, `true`: Spout's own console window
- `file`: `MikanSpoutSender.log` in the Spout log folder (`%APPDATA%\Spout`)

The variable wins over the editor setting. When it is set, `SpoutLogRelay` logs one line saying so and stands down, so the two never fight over Spout's log destination.

---

## DMX output not reaching a controller

`MikanDMX` sends E1.31 multicast (`239.255.<hi>.<lo>` per universe, port 5568) from the interface named by the DMX system's `network_interface_ip`. At the default `0.0.0.0` Windows picks the egress by the lowest-metric route for `224.0.0.0/4`, which on a multi-homed machine (a second Ethernet port, a VPN adapter) is often not the controller's network, and nothing errors: the packets simply leave elsewhere. `route print -4` shows the candidate interfaces and metrics. Set the interface to the address on the controller's subnet; `UdpMulticastSocket::open` sets `IP_MULTICAST_IF` from it, since a bind alone does not choose the multicast egress on Windows. A quick way to separate network from editor faults is a scratch Python sender that builds one E1.31 packet and sends it with `IP_MULTICAST_IF` forced to each candidate interface while watching the fixture. Note that an ESPixelStick in multicast mode binds its socket to the group address and ignores unicast to its own IP, and in unicast mode the reverse, so a unicast test only means something when the controller is configured for it.

## Profiling

The editor is instrumented with easy_profiler (`EASY_FUNCTION()` / `EASY_BLOCK()` throughout the tick, compositor, and node evaluation paths). `App::startup` calls `profiler::startListen()`, so a running `Mikan.exe` accepts connections from the easy_profiler GUI at any time. Launch with `-waitForProfiler` to block startup until a profiler client connects and starts capturing, which is useful for profiling initialization.

---

## Other diagnostic surfaces

- **ML capture runs.** `OnnxSession` logs each loaded model with the execution provider it actually got (`Loaded <path> (EP: DirectML|CPU)`) plus every input and output shape, so a session that fell back from DirectML to CPU is visible rather than just slow. `MoGeInference::run` additionally logs the inputs the metric recovery depended on (`fov_x=... shift=... metric_scale=...`); a wrong FOV reaching the model is the first suspect when a depth proxy comes out the wrong size, and that line settles it. See [scene-lighting.md](./scene-lighting.md) and [depth-proxy-mesh.md](./depth-proxy-mesh.md).

- **Bad UI strings.** `View > Show String Keys` (F9) replaces every localized string with its own key, so a string that reads wrong names itself where it is drawn, and `View > ID Stack Tool` reports the key of whatever interactive widget is hovered. A key visible with the mode off is one no table defines. See [localization.md](./localization.md).

- **Node graph errors.** Failed compositor/shape graph evaluations accumulate `NodeEvaluationError` values on the owning component (`getLastNodeEvalErrors()`); `CompositorNodeEditorWindow` and `ShapeNodeEditorWindow` read and display them each frame.

- **Automation server.** A loopback TCP text command channel on port 21120 for driving and inspecting a running `Mikan.exe`: stage control, property read/write, function invokes, screenshots, Lua eval, and log tail. See [automation.md](./automation.md).

- **Lua debugger.** `LuaDebugServer` listens on TCP 21110 (LRDB protocol, VSCode `vscode-lrdb` extension). Open the project folder in VSCode, set breakpoints in any project script file, and run the generated "Attach to MikanXR Lua" launch configuration; the project's script context is already hooked, since it attaches itself when its Lua state is created. `lrdb_break()` forces a programmatic break. Details in [scripting.md](./scripting.md).

- **Lua errors.** Script failures are logged with full Lua tracebacks (`CommonScriptContext::checkLuaResult`), and the failing script's state is disposed rather than left half-broken.

- **Websocket server.** `WebsocketInterprocessMessageServer` logs listen errors and malformed/unroutable requests at warning level. There is no full per-message wire log. A response big enough to fragment, meaning over 32 KiB and in practice the depth proxy mesh behind `GetModelStencilRenderGeometry`, used to wedge the connection it was sent on. That client stopped being read from while events kept streaming out to it, so its requests hung forever with both ends still reporting the connection as up. The cause was IXWebSocket's server-side blocking send, which flushes on the caller's thread through the same select interrupt the connection's own poll thread waits on. The `MikanXR/IXWebSocket` fork leaves that send non-blocking. `python tools/large_response_check.py` is the regression check: it fetches the geometry over a raw websocket and requires the same connection to answer a small request afterwards. It needs a running editor with a model stencil loaded, so it is a manual check rather than part of the suites. Separately, `processRequests` no longer drains its queues to empty each tick: it stops once the responses it has produced pass a per-tick byte budget, and starts on a different connection each tick. Several stencils fetching geometry on connect therefore spread across frames instead of stalling one. A request that seems slow to answer under load is expected to be a frame or two late, not lost.

- **GL state stack debug.** `App::tickWindows` has a static `bDebugPrintStack` flag that, when flipped in a debugger, makes `MkStateStack` print state push/pop activity for one frame.

- **Client-side logging.** Client apps get their own logger (`MikanClientLogger` inside `MikanClientCore`); the `LoggerSettings::log_callback` hook lets an embedding app route editor-side log lines into its own sink.

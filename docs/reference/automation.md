# Automation server

How to drive and inspect a running `Mikan.exe` over the automation server: a loopback TCP text channel for test scripts and AI tooling. It is deliberately separate from the websocket client API, which serves shipped client applications ([wire-protocol.md](./wire-protocol.md)), and it complements the headless `MikanCmd.exe` commands ([commands.md](./commands.md)), which cover code paths that need no running editor. The server lives in `src/Editor/Server/AutomationServer` over the line socket in `src/Editor/Interprocess/AutomationSocket`, pumped once per frame on the main thread from `MainWindow::update`, so commands run synchronously against editor state.

---

## Enabling and port

The server is off by default. The channel drives, inspects and scripts the editor, so any local process that reaches it owns the session, and a shipped editor has no reason to be listening. It binds 127.0.0.1 only. The default port is 21120 (clear of the websocket 8080, HTTP 8090, and Lua debugger 21110 ports). Of those, only the HTTP server can be opened to the LAN, by a setting in the HTTP Triggers panel, for the phone's capture upload.

- `automationServerEnabled` in `AppSettingsConfig` turns it on, and the Automation section of the project settings panel is the switch for it. Toggling there opens or closes the listener immediately, with no restart.

- `-automationServer` on the command line turns it on for that launch without touching the setting, which is how a drive script gets a channel on a machine that leaves it off.

- `-automationPort=<n>` overrides the port and implies `-automationServer`, since a port with nothing listening is a trap.

- `-noAutomationServer` holds the listener closed for the whole session, overriding both the setting and the flags. The settings checkbox shows as disabled in that session rather than as a control that does nothing.

- The port persists in `AppSettingsConfig` under `automationServerPort`. Editing it in the settings panel rebinds an open listener, and the edit commits on Enter rather than on each keystroke.

- A failed bind logs an error and the editor runs on without the channel. The settings panel reports the listener state rather than the setting, so a failed bind and a `-automationPort` override are both visible there.

The command namespaces register whether or not the listener opens, so the registry survives the listener being toggled off and on.

## Protocol

One client at a time. Commands are newline-terminated text lines (an optional trailing `\r` is stripped). Tokens split on spaces and tabs; a double-quoted span groups spaces into one token, with `\"` and `\\` escapes inside quotes.

Every command answers with one framed reply: a line holding the reply's content line count, then exactly that many content lines. A single value is `1` then the value. A success with nothing to say is just `0`. An error is `1` then one line `<namespace> <verb>: <message>`. An unknown namespace replies naming the valid namespaces. Nothing falls through silently, so a client awaits each reply and stays in sync.

Stage transitions land on the frame after the command that requested them, so a drive that pushes a stage waits a frame (or polls `app info`) before acting on the new stage.

## Commands

`help` lists every registered command.

### App control (app)

- `app info` replies the current stage name, the parent stage name (`none` at the root), and the loaded project path (`none` when no project is loaded)
- `app push <stageName>` / `app pop` push a registered app stage by name, or pop back to the parent stage
- `app open <projectPath>` / `app new <projectPath>` / `app resume` open, create, or resume a project (routed to the main menu stage's remote control commands, so they answer an error outside the main menu)
- `app quit` requests a clean shutdown, the same path as the window close button
- `app crash <kind>` crashes the editor on purpose to exercise the crash reporter (kinds: `access`, `abort`, `terminate`, `purecall`, `invalidparam`, `stackoverflow`; see [debugging.md](./debugging.md)). No reply ever comes: the client sees the connection close, and `tools/automate.py` exits with its closed-connection error.

### Stage commands (stage)

`stage <command> [parameters...]` passes a command to the current app stage's `IRemoteControllable::handleRemoteControlCommand`, the same channel the websocket `MikanRemoteControlCommand` request uses. The stage's result strings become the reply lines. The calibration stages answer commands such as `get_state` and `begin`; a stage that does not recognize the command answers an error naming the stage.

`AlignCameraByOriginMarker` answers four, which is enough to drive a whole alignment headlessly:

- `get_state` replies the menu state (`verifySetup`, `capturing`, `testCalibration`, ...)
- `get_marker_visible` replies whether the origin marker is currently detected
- `begin` starts sampling, failing unless the stage is in `verifySetup` with the marker visible
- `restart` discards the samples and returns to `verifySetup`

Reach the stage with `function invoke CameraObjectSystem <cameraId> align_camera`, which is what the editor's own Align Camera button calls.

### Scene introspection (system, component, property, function)

These reach every object system and component through the property and function databases ([objects.md](./objects.md)).

- `system list` replies one object system class name per line
- `component list <system> [componentClass]` replies `<componentId> <componentClass>` per live component
- `component create <system> <componentClass>` creates an object of the system's primary component type with a default definition, replying the new component id
- `component destroy <system> <componentId>` destroys the object owning the component
- `property list [system] [componentClass]` replies `<system> <componentClass> <name> <type> <ro|rw>` per property
- `property get <system> <componentId> <name>` replies the value as text
- `property set <system> <componentId> <name> <value...>` writes a property, coercing the value tokens to the descriptor's type
- `function list [system] [componentClass]` replies `<system> <componentClass> <functionName> <displayName>` per function
- `function invoke <system> <componentId> <name>` invokes a function (functions take no arguments; the pattern is set properties, then invoke)

A component id of `-1` targets the system itself in `property get`/`set` and `function invoke`.

A `property set` mutates the component's definition, and the project autosave persists it to the project file a few seconds later. A drive that mutates project state restores the original value and waits out the autosave cooldown before quitting, or its last write may not be the one on disk.

Value syntax for `property get`/`set`:

- booleans are `true`/`false`
- vectors take one numeric token per component (`property set ... relative_position 0 1.5 0`)
- quaternions order their components w x y z
- matrices take 16 floats in `MikanMatrix4f` field order (x0 x1 x2 x3 y0 ... w3)
- strings with spaces are quoted (`"My Anchor"`); a bare string may span tokens, which rejoin with single spaces
- int arrays take one integer token per element and accept a set (`property set ... fixture_ids 1052 1118`)
- other array, map, and object typed properties read back as formatted text but refuse a set

### Screenshots (screenshot)

- `screenshot compositor [componentId] [path]` writes the composited frame to a PNG and replies the absolute path. The component id may be omitted when the project has exactly one compositor. Without a path it writes `mikan_compositor.png` in the working directory, overwritten each capture.
- `screenshot window [windowIndex] [path]` writes a window's back buffer (the editor UI included) at the end of that window's frame render, replying the absolute path once the capture lands. The index comes from `window list`; omitting it means the main window. Default `mikan_window.png`.

### Windows (window)

- `window list` replies `<index> <width>x<height> <title>` per open window, main window first. The index is the handle the `screenshot` and `input` commands take, and it shifts as windows open and close, so list before driving rather than hard-coding one.
- `window focus <windowIndex>` raises the window and gives it OS focus

### Synthetic input (input)

Injected input is posted to the real event queue, so it travels the path a user's input does: the same window routing, the same ImGui capture arbitration, the same listener. A driven session is therefore evidence about the shipping input path rather than about a test-only shortcut. Every verb raises the target window first, since input only lands where the OS is delivering it.

Mouse position is warped rather than fabricated, which moves the real cursor. ImGui's backend re-reads the OS cursor every frame whenever the pointer is not over one of our windows, and would overwrite a fabricated position before the widget under it saw the click.

- `input move <windowIndex> <x> <y>` moves the cursor to a window client position
- `input click <windowIndex> <x> <y> [left|middle|right] [clickCount]` moves, then presses and releases. Pass `2` as the click count for a double click.
- `input press|release <windowIndex> <x> <y> [left|middle|right]` are the halves of a click, for drags and for widgets whose popup only survives while the button is held
- `input wheel <windowIndex> <x> <y> <scrollY> [scrollX]` scrolls in notches
- `input key <windowIndex> <keyName> [modifiers...]` presses and releases a key, and `input keydown` / `input keyup` are its halves, so a drive can hold a modifier across a mouse press, move, and release (`input keydown 1 ctrl ctrl`, then `input keyup 1 ctrl`; the modifier key names are `ctrl`, `shift`, `alt`). Names are `return`/`enter`, `tab`, `escape`, `backspace`, `delete`, `space`, the arrows, `home`, `end`, `pageup`, `pagedown`, `insert`, `f1` to `f12`, or a single printable character. Modifiers are `shift`, `ctrl`, `alt`, `gui`.
- `input text <windowIndex> <text...>` commits text the way typing does, taking the raw untokenized rest of the line

Two things a warp cannot fake. A warped move often reports a zero relative delta, so anything driven by relative motion rather than by cursor position is unreliable under synthetic input: the viewport camera's right-drag rotate and pan usually see delta 0 and leave the camera where it was. A `input key` press and release land in the same frame, so held-key behavior such as the WASD fly never accumulates. Verify camera manipulation by hand, or seed the state and check that the editor applies it.

One caveat worth knowing when a click seems to vanish: an ImGui window's capture flags are computed at `NewFrame` and so describe the previous frame's cursor position. A click injected in the same command that moved the cursor is judged against where the cursor used to be. Code that owns a non-ImGui region of a window should route the mouse by geometry rather than by asking ImGui, which is what `CEFBrowserEditorWindow` does for its page area.

### Lua scripting (script)

- `script list` replies `<scriptId> <path> <loaded|not_loaded> [trigger,trigger]` per project script, in pool order (`-` for a script with no path set)
- `script eval <lua-code>` runs a statement in the project's script state and replies what it returns (the code is the raw untokenized rest of the line, so Lua quotes pass through verbatim): `script eval return SceneSystem:getSceneByName("MyScene").name`
- `script trigger <triggerName> [key=value ...]` invokes a script trigger, the same call the HTTP trigger routes make. The trailing tokens become the trigger's argument table, standing in for a route's query string, and a token with no `=` is an error. Quote a value holding spaces: `script trigger new_sub user=bob "message=thanks for the stream"`
- `script reload` rebuilds the project's script state, re-running every script in pool order

### Log access (log)

`log tail <lineCount> [minLevel]` replies the most recent log lines at or above the level, oldest first, from an in-process ring of the last 2000 lines. Levels are the logger's names:

- trace
- debug
- info
- warning
- error
- fatal

### Transaction history (history)

The editor transaction system's automation face ([transactions.md](./transactions.md)). Every persistent edit records as an undoable transaction, and a JSONL session log persists beside the project for post-session diagnosis.

- `history list [n]` replies the last n transactions as `<seq> <applied|undone> <description>`
- `history info` replies depth, cursor, can_undo/can_redo, the open gesture, and the session log path
- `history undo [n]` / `history redo [n]` step the stack, replying the resulting cursor
- `history clear` drops the in-memory history (the log keeps its record)

A drive that mutates state can restore it exactly with `history undo` instead of hand-reverting property values, and a session that went wrong reads back from the log file even after a crash.

### Node graph editing (nodegraph)

Drives the node editor window and its snapshot undo history ([transactions.md](./transactions.md)). One node editor window is targeted at a time; the commands answer an error when none is open.

- `nodegraph open [compositorComponentId]` opens the compositor graph editor window (the id may be omitted when the project has exactly one compositor)
- `nodegraph open material <graphPath>` opens the material graph editor window on a `.matgraph` file, reusing and raising the window when one is already open. A relative path resolves against the project (then the app resources) and otherwise against the working directory. The command answers an error when the file does not load or is not a material graph.
- `nodegraph open material new [compositor|shape]` opens the material editor with a fresh graph of that domain (default `compositor`)
- `nodegraph close` asks the app to tear the window down at the end of the frame
- `nodegraph info` replies the graph class and path, node/pin/link/property counts, a `page` line with the current page id, a `pages` line with the page count (the implicit root is not counted), `can_undo`/`can_redo`, the history depth and cursor, the session log path ([transactions.md](./transactions.md)), for the compositor editor a `running` line, and for the material editor `domain`, `vertex_preset`, and `compile_errors` (the error count of the last compile) lines
- `nodegraph list nodes|pins|links|properties|pages` replies one object per line: nodes as `<id> <class> <editorTitle> <pageId>` (the title may hold spaces, so the page id is the last token), pins adding owner node id, direction, and name, links as `<id> <startPinId> <endPinId>`, pages as the root first (`0 root Main`) then `<id> <className> <name>` in id order
- `nodegraph page` replies the current page id. `nodegraph page <pageId>` switches the canvas to that page, replying the resulting page id, or an error when the page does not exist.
- `nodegraph createpage <pageClassName>` creates a page of one of the graph's page classes (the Pages panel's add buttons), replying the new page id, or an error naming an unknown class
- `nodegraph deletepage <pageId>` deletes a page with every node on it. The root page cannot be deleted, and a missing page answers an error.
- `nodegraph createnode <nodeClassName> [x y]` creates a node at the grid position on the current page, replying the new node id
- `nodegraph deletenode <nodeId>` deletes a node with its pins and links
- `nodegraph createlink <startPinId> <endPinId>` connects two compatible pins, replying the new link id
- `nodegraph deletelink <linkId>` deletes a link
- `nodegraph undo [n]` / `nodegraph redo [n]` step the window's snapshot history, replying the resulting cursor
- `nodegraph run on|off` pauses or resumes compositor evaluation of the editor graph (the Compositor menu's Run item), replying the resulting state
- `nodegraph compile` compiles the material editor's graph and writes its shaders and material file beside the graph file (the Material menu's Compile item), replying `compiled`, or one `error <nodeId> <message>` line per compile error. An unsaved graph has nowhere to write and answers an error. On any other editor window the command answers an error.
- `nodegraph renamevar <propertyId> <name...>` renames a graph variable (the name is the rest of the line, so spaces survive). A name another variable holds gets a numeric suffix, the same rule every rename surface applies
- `nodegraph reordervar <movedPropertyId> <targetPropertyId>` moves a variable to the target's slot in the list, the headless equivalent of dragging one variable row onto another

`nodegraph list properties` replies in variable-list order, so a reorder is observable there.

### Materials (material)

Loads and compiles material files headlessly, with no editor window involved, so a drive can check a material file or rebuild a graph's outputs without opening the editor. Paths resolve the way `nodegraph open material` resolves them.

- `material info <matPath>` loads a `.compmat` or `.shapemat` config and replies `name`, `domain`, `vertex_preset`, and `source_graph` lines (`none` where a hand-authored material leaves them unset), then one `uniform <name> <semantic>` line per uniform semantic map entry
- `material compile <graphPath>` loads the graph without an owner window, compiles it with the GLSL writer, and writes the `.vert`, `.frag`, and the domain's material file beside the graph file, replying those three paths. Compile errors reply as `error <nodeId> <message>` lines and write nothing.

### Project assets (assets)

Drives the `ProjectAssetCatalog` ([objects.md](./objects.md) covers the property database the component scan reads). All verbs run inline on the main thread, so no reply is deferred.

- `assets folders` replies one line per folder descriptor: `<id> project:<projectSubfolder or -> bundled:<bundledSubfolder or -> <readonly|writable>`
- `assets list [folderId]` replies one line per entry as `<folderId> <className> <storedPath>[ readonly]`, all folders when the id is omitted
- `assets refresh` rescans every folder from disk, replying `refreshed`
- `assets import <folderId> <sourcePath>` copies a file (or a material's folder) into the project and replies the stored path. The source path may contain spaces. Fails with the catalog's own error text, for example a read-only folder or an unsupported file type.
- `assets refs <storedPath>` replies one `<graph|material|component> <name> <detail>` line per referrer, empty when nothing references the path. The path may contain spaces.
- `assets delete <folderId> <storedPath>` deletes an asset after checking `findReferences` itself, replying `deleted`. A referenced asset is refused with `referenced by: <kind> <name>, ...` rather than deleting out from under a live reference.
- `assets select <folderId> <storedPath>` selects an entry in the project Assets panel through the stage's `select_asset` command, replying `selected`, or an error `asset not found`
- `assets open <folderId> <storedPath>` opens the entry in its editor, the same open a double click on it performs (a compositor or shape graph in that editor, bound to the component driving it when one does; a material's source graph in the material editor), replying `opened`, or an error when nothing opens that kind
- `assets selected` reads the panel's selection back through `selected_asset`, replying `<folderId> <storedPath>` or `none`
- `assets folder` replies the panel's current folder id through `current_asset_folder`

The `select`, `selected`, and `folder` verbs answer an error when the current app stage is not the project stage, since that is where the Assets panel lives.

A short drive that imports a texture, wires it into a DMX sequence's content path, and confirms the delete guard:

```
python tools/automate.py "assets import textures ./stripe.png" "assets list textures"
python tools/automate.py "property set DMXSequenceSystem 1 content_path textures/stripe.png"
python tools/automate.py "assets refs textures/stripe.png"
python tools/automate.py "assets delete textures textures/stripe.png"
python tools/automate.py "property set DMXSequenceSystem 1 content_path \"\"" "assets delete textures textures/stripe.png"
```

The first delete answers `referenced by: component <sequenceName>` since the sequence still names the file. Clearing `content_path` first lets the second delete succeed with `deleted`.

### ARKit debug channel (arkit)

Relays debug traffic to and from the MikanARStreamer iPhone app ([videosources.md](./videosources.md)). The phone pushes diagnostics, which are re-emitted through the editor's own logger and so read back through `log tail` interleaved with editor lines on one timeline. Commands travel the other way.

- `arkit status` replies whether the listener is enabled, its port, whether a phone is connected, and that phone's address, device name, and protocol version
- `arkit send <text...>` sends one command line to the phone and replies with the phone's own reply

The channel is off by default, like this server. It binds every interface rather than loopback only, because its peer is a phone on the LAN: `arkitDebugChannelEnabled` in `AppSettingsConfig` (port `arkitDebugChannelPort`, default 21121), or `-arkitDebugChannel` on the command line with `-arkitDebugPort=<n>` to override the port. The `arkit` namespace registers either way, so `arkit status` still answers when the listener is off.

`arkit send` takes the raw rest of the line, so quoting reaches the phone verbatim. Its reply is parked until the phone answers rather than being answered immediately, and a phone that goes quiet fails the command after five seconds instead of leaving the client waiting. Only one command is in flight at a time. The command text is opaque to the editor: the phone owns its own vocabulary, so it can grow without an editor rebuild.

Only one peer is accepted at a time, so a peer that connects and then never sends its hello is dropped after ten seconds rather than holding the slot. Without that, one wedged client locks the channel until the editor restarts.

The commands the MikanARStreamer app answers today:

- `ping` replies `pong`
- `stats` replies the capture, encode, drop, and send counters as `name value` lines
- `verbose on|off` gates the per-frame encode-latency relay, which is off by default because one line per frame fills the 2000-line log ring in about a minute and evicts the editor's own diagnostics
- `screenshot [name]` captures the app's own UI to a PNG in its container and replies with the path, pixel size, and byte count
- `mode [photo|video|stream]` reads or sets the capture mode the phone's screen is in
- `capture [marker]` writes the next frame as a JPEG with its one-frame pose track beside it, `marker` naming it as the marker reference for the last take, and replies with both container paths, the pixel size, and the byte count
- `record start [marker]` begins recording an H.264 movie with its pose track, replying with the take name; `record stop` finishes it and replies with both paths, the frames written and dropped, and the duration; `record` alone reports whether one is in progress

A take and its marker reference pulled off the phone drop straight into a project's `movies` folder as a file video source ([videosources.md](./videosources.md)):

```
python tools/automate.py "arkit send record start" "sleep:5" "arkit send record stop"
python tools/automate.py "arkit send capture marker"
xcrun devicectl device copy from --device <deviceId> \
  --domain-type appDataContainer --domain-identifier com.mikan.ARStreamer \
  --source Documents/take_20260915_120000.mp4 --destination ./movies/take_20260915_120000.mp4
```

The screenshot command exists because nothing else can see that screen. `devicectl` can copy files off a device but cannot capture one, and `simctl io screenshot` is simulator only, so the app takes the picture itself and leaves it where a copy can reach:

```
python tools/automate.py "arkit send screenshot land"
xcrun devicectl device copy from --device <deviceId> \
  --domain-type appDataContainer --domain-identifier com.mikan.ARStreamer \
  --source Documents/land.png --destination ./land.png
```

The image travels as a file rather than as base64 through the channel, which keeps a line-oriented text protocol from carrying megabytes. Note that `devicectl device orientation set` is not available on every device (an iPhone 12 Pro reports the capability as unsupported), so a layout has to be checked in whichever orientation the phone is physically in.

A real phone session can be driven without touching the device. The app's settings live in `UserDefaults`, so launch arguments override them for that launch only, and `-autostart 1` starts streaming without a tap:

```
xcrun devicectl device process launch --device <deviceId> com.mikan.ARStreamer \
  -- -settings.host <editorHost> -settings.basePort 27015 -autostart 1
```

The phone must be unlocked for install, and ARKit still needs textured surroundings for the pose to mean anything.

`tools/arkit_debug_stub.py` stands in for the phone, which is how the channel is tested without a device on the bench. It answers `ping`, `stats`, and `empty`, and deliberately ignores `silent` so the timeout path can be exercised.

```
build/src/Editor/Release/Mikan.exe -automationServer -arkitDebugChannel
python tools/arkit_debug_stub.py
python tools/automate.py "arkit status" "arkit send ping" "log tail 20 info"
```

Mutations and undo/redo run inside the node editor window's next update (its GL and gui contexts are only current there), so those replies land a frame late, and a mutation's undo snapshot commits on the window's next quiescent frame. A drive polls `nodegraph info` (or rides automate.py's inter-command delay) before asserting `can_undo`.

## Client helper

`tools/automate.py` is the checked-in client: it connects (retrying a refused port until `--wait` seconds, default 20), sends each argument as one command, prints each framed reply, and exits nonzero on a connection failure or timeout. `--port` overrides the default 21120, `--delay` sets the pause between commands (default 0.1 s, enough for a stage transition to land), and `--timeout` bounds each reply and `until` poll (default 60 s).

```
python tools/automate.py "app info"
python tools/automate.py "app resume" sleep:1 "screenshot compositor"
python tools/automate.py "property get CompositorObjectSystem 1051 spout_output_name"
```

Three arguments run client-side instead of being sent:

- `ready` waits until the server answers `app info` (the connect retry already covers a fresh launch)
- `sleep:<seconds>` pauses that long instead of the default delay
- `until:<command>:<expected>` polls the command until a reply line starts with the expected text, so a drive waits for a state edge instead of sleeping out a worst-case budget

## The drive and verify loop

The standard way to verify an editor feature objectively: launch `Mikan.exe` with the channel on, drive it with commands, then read state back rather than assuming behavior.

```
./build/src/Editor/Release/Mikan.exe -automationServer &
python tools/automate.py "app resume" "until:app info:stage Compositor" "screenshot compositor"
python tools/automate.py "property get <system> <id> <name>" "log tail 20 error" "app quit"
```

`app push <stageName>` enters a stage but gives it no target, so a stage that normally acts on a selected object comes up empty. Reach those through the owning component instead: `function invoke ARKitVideoObjectSystem <componentId> show_video_source_settings` is what the editor's own button calls, and it both enters `VideoSourceSettings` and binds the source to it. A bare `app push VideoSourceSettings` reports the expected stage from `app info` while showing no video at all, which reads like a broken device rather than a missing selection. Video sources open on demand from that path too, so nothing binds the receive socket until it runs.

A handler that cannot answer straight away (the ARKit channel's `arkit send` is the one case today) calls `AutomationServer::deferReply`, then answers later through `sendDeferredReply`. Nothing bounds that wait to the current frame, so a handler that defers owns arming its own timeout.

Growth convention: when a feature adds automation commands, register the namespace in `AutomationServer::registerCoreNamespaces` (or from the owning subsystem) and add its section here.

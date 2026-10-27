# Proposal: Client API protocol split, GLB over HTTP, JSON-RPC 2.0 with OpenRPC discovery, and an OSCQuery facade

Design agreed. Part 1 (model geometry as GLB over HTTP) and Part 2 (the JSON-RPC 2.0 envelope with OpenRPC discovery) have landed and are documented in [wire-protocol.md](../reference/wire-protocol.md), which is the authority where this text and the code differ. Part 3 is not started.

The client API is a custom websocket protocol today: JSON text frames with `requestTypeName`, `requestId`, `responseTypeName`, `resultCode` and `eventTypeName` fields inside the struct JSON, plus binary frames for the two render geometry responses. Nothing standard speaks it without the generated bindings. The question behind this proposal was whether an open WebSocket protocol exists for object state replication, request and event messaging, and mixed text and binary payloads. None covers all three in a shape that fits a C++ editor talking to game engines: JSON-RPC 2.0 covers request, response and notification with no binary and no object model; WAMP assumes a router; MQTT and rosbridge assume a broker or a robotics message set; OPC UA is the one standard with a node address space and subscriptions but carries an industrial session and security model; OSCQuery is a parameter tree for creative tools, not objects with methods. The decision is therefore three surfaces, each on the transport its audience already speaks, all facades over the same request handlers and property database:

- Part 1 (landed): bulk geometry left the websocket. Stencil and shape model geometry is served as GLB over the existing HTTP server with each mesh's diffuse texture embedded, the component values carry the URL, and the binary websocket path is deleted.
- Part 2: the websocket text envelope becomes JSON-RPC 2.0, with an `rpc.discover` method returning an OpenRPC document generated from reflection, so tools such as the OpenRPC inspector list every RPC with its schemas.
- Part 3: an OSCQuery facade exposes the component property tree to OSC clients, lighting consoles first, with LISTEN pushes, discovery over DNS-SD, and OSC writes that land on `setPropertyValue` so the transaction recorder captures them and undo reaches listeners.

Decisions made:

- GLB is the geometry container.
- OSCQuery writes are transaction captured. Undo re-applies through `setPropertyValue`, which fires `OnPropertyChanged`, which the LISTEN push subscribes to, so an undo in the editor reaches the OSC client that made the write.
- OSCQuery visibility reuses `PropertyDescriptor::isClientAPIHidden()`. A second flag or a bitmask can come later if the two surfaces ever need to differ.
- The spot light `red`, `green` and `blue` stay runtime-only and hidden. OSC lighting control goes through the DMX sequence and preset properties.
- OSC addresses use system and component names.

Part 1 removed the binary frames before Part 2 redefines the text envelope, and added the HTTP server features (response headers, prefix routes, CORS) that Part 3 needs. Every part is a wire protocol change: bindings are regenerated through `MikanCSharpCodeGen` and `MikanTypeScriptCodeGen`, nothing under `bindings/*/Generated` or `bindings/typescript/types` is hand-edited, and `MIKAN_SERVER_API_VERSION`, `MikanConstants_ClientAPIVersion` and `MIKAN_MIN_ALLOWED_CLIENT_API_VERSION` bump from 0 to 1 once, in Part 2.

---

## Part 2: JSON-RPC 2.0 envelope and OpenRPC discovery

### Wire contract

- Request: `{"jsonrpc":"2.0","method":"<RequestStructName>","params":{<struct fields>},"id":<int>}`. A request without `id` is a notification: the handler runs, nothing is sent back, including for an unknown method. Params are always by-name (an object).
- Response: `{"jsonrpc":"2.0","id":<int>,"result":{<response struct fields>}}`. A request whose response is the plain `MikanResponse` returns `"result":{}`.
- Error: `{"jsonrpc":"2.0","id":<int>,"error":{"code":<int>,"message":"<MikanAPIResult name>","data":{"resultCode":"<MikanAPIResult name>"}}}`. `code` is `-32601` for `UnknownFunction`, `-32602` for `MalformedParameters`, `-32700` for an unparsable frame (with `"id":null`), `-32600` for a frame that is not a JSON-RPC object (batch arrays included: batches are not supported and are answered with one `-32600`), and otherwise the `MikanAPIResult` numeric value, which lies outside the reserved range. `data.resultCode` is always present so runtimes fill the response struct from it.
- Event: `{"jsonrpc":"2.0","method":"<EventStructName>","params":{<event fields>}}`, a server-to-client notification.
- `MikanRequest` loses `requestId` and `MikanResponse` loses `requestId` and `responseTypeName` as wire fields. `requestTypeName`, `responseTypeName`, `eventTypeName` and `resultCode` remain as plain members without `FIELD()`: the C++ macros in `MikanAPITypes.h` still stamp the type name in both the reflection and non-reflection branches (the request manager reads it as `method`), and runtimes set `eventTypeName` from `method` on receipt and `resultCode` from the envelope. The deserializers are strict about reflected fields in all three languages, which is why these must stop being reflected rather than merely omitted.
- Each request struct declares its response type with a new reflection property `Serialization::ResponseType("<ResponseStructName>")` beside `CodeGenModule` (defined in `MikanSerialization/Public/SerializationProperty.h` like `ClientComponentNames`). Absent means `MikanResponse`. This is what lets a `result` carry no type tag and what `rpc.discover` needs for the result schema. Today no runtime records which response a request expects: all three type a response from the type-name string inside the payload.
- The sub-protocol becomes `Mikan-1`. A connection with no sub-protocol is still accepted with a warning, which is how the OpenRPC inspector connects.

### Envelope codec

- `src/Libraries/MikanUtility/Public/JsonRpc.h` and `Private/JsonRpc.cpp` (new): `JsonRpcRequest {method, params (nlohmann::json), id (optional int)}`, `JsonRpcResponse`, `JsonRpcError`, with `parseIncoming(string, outKind, ...)` classifying a frame as request, response or notification, the `format*` builders, and `jsonRpcErrorCode(MikanAPIResult)`. `MikanClientCore` and the editor both link `MikanUtility` already (the SAX searchers in `JsonUtils.h` live there). The `JsonSax*ValueSearcher` uses for `requestTypeName` and `requestId` go away; frames are small once geometry is gone, so a full nlohmann parse is fine.
- Tests in `src/Programs/Tests/UnitTests/jsonrpc_unit_tests.cpp`: round trips, notification without id, null id on parse error, batch rejection, the error code mapping for every `MikanAPIResult`.

### Server

- `WebsocketInterprocessMessageServer::processRequest`: parse with the codec; `ClientRequest` becomes `{connectionId, method, id (optional), params string}`; dispatch by `method` through the existing `m_requestHandlers` map; the `UnknownFunction` fallback goes through the codec's error builder and is suppressed for notifications; `processRequests` keeps the connection rotation.
- `ServerResponseHelpers.{h,cpp}`: `readTypedRequest` deserializes `params` with `Serialization::deserializeFromJson(json, T)`; `writeTypedJsonResponse` serializes the response struct and wraps it as `result`; `writeSimpleJsonResponse` emits `result:{}` on `Success` and an error object otherwise. The handlers themselves (34 `readTypedRequest` and 125 `write*` call sites) are untouched.
- `MikanServer::publishMikanJsonEvent` and the eight direct publish sites (`CameraRequestHandler`, `LightRequestHandler`, `MikanClientConnectionState`, `RemoteControlManager`, `ScriptRequestHandler`, `TextureSourceRequestHandler`, `VideoSourceRequestHandler`, `MikanServer::onClientConnectedHandler`) go through one `mikanEventToJsonRpc(event)` helper in `ServerResponseHelpers` that reads the struct's archetype name as `method`.
- `rpc.discover`: registered in `MikanServer::startup` as a method name on the dispatcher (handlers are keyed by string, and this one ignores `params`). Answers with the document from `src/Editor/Server/OpenRpcDocument.{h,cpp}` (new):
	- `openrpc: "1.3.2"`, `info {title: "MikanXR", version: <MIKAN_VERSION string>}`, `servers: [{name: "editor", url: "ws://127.0.0.1:<port>"}]`.
	- One method per registered dispatcher name except `rpc.discover` itself: `paramStructure: "by-name"`, one content descriptor per reflected field of the request struct (name, `required: true`, schema), `result: {name: <ResponseStructName>, schema: {$ref}}`, `errors` listing the `MikanAPIResult` codes and names.
	- `components.schemas`: every reflected struct and enum reachable from a method, keyed by archetype name. The mapping follows the serializer's JSON shapes: bool to `boolean`; integer fundamentals to `integer`; float and double to `number`; `Serialization::String` to `string`; enums to `string` with `enum` listing the `ENUMVALUE_STRING` names, since the serializer writes names; `List<T>` to `array` of T; `Map<String,String>` to an array of `{key,value}` objects, which is what the serializer writes; nested structs to `$ref`; `PolymorphicObjectPtr` to the discriminated object shape the serializer writes, with `oneOf` over the reflected subclasses.
	- Events under a top-level `x-mikan-events` array with the same content-descriptor shape, since OpenRPC methods are client-callable and an event is not.
	- Walks the same `Serialization::StructTypeHandle` and `ValueAccessor` vocabulary the schema guard test uses, never `rfk::` directly.
	- Also served at `GET /openrpc.json` on the HTTP server (a background route) so `curl` verifies the document without a websocket client.
- Version constants: `MIKAN_SERVER_API_VERSION` and `MIKAN_MIN_ALLOWED_CLIENT_API_VERSION` in `src/Editor/AppCore/Version.h` to 1; `MikanConstants_ClientAPIVersion` in `MikanCoreTypes.h` to 1 (it flows into the generated constants).

### Codegen

- `MikanClientLanguageGen`: `getTypeNameFieldForStruct` stays for constructor stamping, plus a new `getResponseTypeForRequest` reading the `ResponseType` property. The three base structs `MikanRequest`, `MikanResponse` and `MikanEvent` are no longer emitted from the `MikanAPITypes` module; each runtime hand-writes them (`bindings/csharp/MikanAPITypes.cs`, `bindings/typescript/MikanAPITypes.ts`) with the non-wire members. The generator keeps stamping `<typeNameField> = "<ClassName>"` in derived constructors and emits the expected response type per request class (`public override System.Type ResponseType` in C#, `static responseType = X` in TypeScript).
- `client_names_unit_tests.cpp` gains the `ResponseType` guard: every reflected `MikanRequest` subclass's property names a reflected `MikanResponse` subclass or is absent, and every `MikanResponse` subclass other than the base is named by at least one request.

### Runtimes

- C++: `WebsocketInterprocessMessageClient` classifies with the codec (`method` without `id` is an event, `id` with `result` or `error` is a response). `MikanRequestManager::sendRequest` builds the envelope with `method = requestTypeName`, keeps `PendingRequest {id, promise, expectedResponseType}` resolved from the `ResponseType` property through `TypeRegistry`, and `parseResponseString` deserializes `result` into that type or builds a base `MikanResponse` with `resultCode` from `error.data`. `MikanEventManager` types events from `method` and stamps `eventTypeName`. The internal `disconnect:<code>:<reason>` queue string stays. `MikanRenderTargetAPI::tryProcessRequest` (local answers by `typeid`) is untouched.
- C#: `MikanRequestManager.SendRequest`, `InternalTextResponseCallback` and `ParseResponseString`, and `MikanEventManager` as above; `MikanWebSocketTransport.HandleTextMessage` classifies by parsing the envelope rather than the substring test; the response type cache keyed by name stays for the error path. `MikanAPIUnitTests.cs` updated.
- TypeScript: `MikanClient.handleMessage`, `MikanRequestManager.sendRequest` and `parseResponse` (which also switches to `serializeToJsonString` so enums go out as names like the other two runtimes), `MikanEventManager.parseEvent` (listener names stay the struct names). `TypeRegistration.ts` keeps registering responses. Tests updated, `dist/` rebuilt, since the CEF GUI consumes it.
- `mikan_api_unit_tests.cpp` in `src/Programs/Tests/UnitTests`: the four envelope tests rewritten against the JSON-RPC shape.

### Docs

- wire-protocol.md: "The contract libraries" (the base types and macro rule), "Message flow over the websocket", "Client side" and "Managed clients" rewritten for the envelope, the `ResponseType` property, the notification rule and the error mapping, plus a new "OpenRPC discovery" section naming `rpc.discover`, `GET /openrpc.json`, the schema mapping, and that the inspector at inspector.open-rpc.org connects to `ws://127.0.0.1:8080` with no sub-protocol.
- README.md: one sentence under the client API description that the websocket speaks JSON-RPC 2.0 with OpenRPC discovery.
- plan.md: Next items for the Unreal plugin (the timeout response it synthesizes in `UMikanEngineSubsystem` and the vendored SDK headers), the CEF GUI (`npm install` against the rebuilt package) and the Unity package.

---

## Part 3: OSCQuery facade

### Surface

- HTTP on the existing server (port 8090, `AppSettingsConfig::getHttpServerPort()`). A prefix route at `/` would shadow the trigger routes, so OSCQuery mounts under `/osc/`: `GET /osc/?HOST_INFO`, `GET /osc/<address>` for a subtree, `GET /osc/<address>?VALUE` (and `?TYPE`, `?RANGE`, `?ACCESS`, `?DESCRIPTION`) for one attribute, 404 for an unknown address, 204 for an unsupported attribute. The root node's `FULL_PATH` is `/` and addresses below it are the OSC addresses without the `/osc` mount, so traffic on `OSC_PORT` uses bare addresses. HOST_INFO carries `NAME` from `ServiceAdvertiser::buildInstanceLabel`, `EXTENSIONS {ACCESS, VALUE, RANGE, DESCRIPTION, TYPE, LISTEN, PATH_ADDED, PATH_REMOVED, PATH_RENAMED}` all true, `OSC_IP`, `OSC_PORT`, `OSC_TRANSPORT: "UDP"`, `WS_IP` and `WS_PORT`.
- LISTEN websocket: an upgrade on the same HTTP port. `HttpInterprocessMessageServer` wires `WebSocketServer::setOnClientMessageCallback` on its `ix::HttpServer` (which derives from `WebSocketServer` and already hands an `Upgrade: websocket` request to `handleUpgrade`), exposing `setWebSocketHandler(path, handler)` keyed by the upgrade request's path (`/osc/`), so the JSON-RPC websocket on 8080 is untouched. Clients send `{"COMMAND":"LISTEN","DATA":"/addr"}` and `IGNORE`; the server pushes OSC messages as binary frames on value change and JSON `PATH_ADDED`, `PATH_REMOVED` and `PATH_RENAMED {OLD,NEW}` commands on tree change. OSC binary frames from a client are writes, on the same path as UDP.
- OSC over UDP: a receive socket bound to `oscServerPort` (a new `AppSettingsConfig` property, default 8091, beside `httpServerPort`), on 127.0.0.1 or 0.0.0.0 following `httpServerAllowRemote`, polled non-blocking from the main thread like `AutomationSocket::poll`. `ix::UdpSocket` is send-only, so this is a small Winsock and POSIX `SOCK_DGRAM` class in the OSC library mirroring `MikanDMX/Private/UdpMulticastSocket` with `bind` and `recvFrom`. `oscQueryEnabled` (new, default true) gates all three.
- Discovery: `ServiceAdvertiser` generalizes from one fixed registration to a list of `{serviceType, port, txtRecord}` registrations (the `DnsSdRegistration` object is already self-contained), and advertises `_oscjson._tcp` on the HTTP port with `txtvers=1` beside `_mikanxr._tcp` under the same remote-access condition. A loopback-only server is unreachable at the LAN address DNS-SD would resolve to, so advertising it would mislead.

### Tree model

- Address scheme: `/<SystemClassName>/<component_name>/<property>` for component properties and `/<SystemClassName>/<property>` for object system properties. `component_name` is sanitized to OSC-safe characters (space and `#*,/?[]{}` replaced with `_`); on a collision inside a system the later component gets `_<componentId>` appended. `DESCRIPTION` on a property node is the localized label through `locResolveDescriptorKey`, and on a component node the component name.
- Visibility: descriptors with `isClientAPIHidden()` are skipped; read-only descriptors get `ACCESS: 1`, writable `ACCESS: 3`. `PropertyRequestHandler` does not consult the flag today (only the schema test does), so the facade is its first runtime consumer.
- Type mapping (`MikanVariantType` to OSC type tags), in `OscPropertyBridge`: BOOL to `T` with a boolean VALUE; UBYTE, USHORT and INT to `i` (UBYTE and USHORT get `RANGE [{MIN:0,MAX:255}]` and `65535`); LONG to `h`; FLOAT to `f`; DOUBLE to `d`; STRING to `s`; VECTOR2F, VECTOR3F and VECTOR4F to `ff`, `fff` and `ffff`; QUATERNIONF to `ffff` in `w x y z` order, matching `AutomationVariantText`; MATRIX4F to sixteen `f`, read-only; the double vector variants to their `d` equivalents. An INT descriptor with `EnumPropertyMetaData` is `s` with `RANGE [{VALS:[<keys>]}]`, converting name to index on write. `*_ARRAY`, `STRING_MAP` and `POLYMORPHIC_OBJECT` descriptors are omitted from the tree, with a plan.md Later item for variable-length arrays once a client needs `sh_coefficients`.
- `RangePropertyMetaData {min, max}` (new, beside `EnumPropertyMetaData` in `src/Editor/ECS`) feeds `RANGE` for the floats and ints that carry it. It is attached only to descriptors whose setters already clamp (`cone_angle_degrees`, `cone_range_meters`, `brightness`, `transmit_rate_hz`, `exposure_scale` and the sequence timing floats), read from the existing clamp constants. No panel change.
- Tree rebuild: the tree is rebuilt lazily from `ProjectManager::getSystems()` and each system's objects and components when a request arrives after a dirty mark. The marks come from `ProjectManager::OnProjectLoaded` and `OnProjectPreUnload`, each system's `OnNewObjectFinalized` and `OnObjectWillBeDestroyed`, and a `component_name` change. Those same signals emit `PATH_ADDED`, `PATH_REMOVED` and `PATH_RENAMED` to LISTEN clients.

### Value flow

- Reads: `getPropertyValue` on the main thread. An HTTP GET of a subtree hops with `runOnMainThread(..., 2000)` (the `ARKitAlignmentRequestHandler` pattern) and serializes the JSON there.
- Writes: an OSC message (UDP or websocket binary frame) is converted by `OscPropertyBridge` against the descriptor's type, rejected when the address is unknown, read-only, or the argument tags do not match, and applied with `setPropertyValue` on the main thread. Nothing else is needed for capture: `TransactionHistory` records every definition change passively, and its 0.75 s coalescing window (`k_coalesceWindowSeconds`) already merges a fader stream on one component into one transaction. Interleaved streams on two components seal alternately, which is acceptable and noted in transactions.md.
- Pushes: `OscQueryServer` subscribes to `ProjectConfig::OnPropertyChanged` exactly as `PropertyRequestHandler::onProjectConfigChanged` does, including the rebind on project load, resolves the leaf definition to its system or component, and for each listened address whose property changed reads the value and sends one OSC message per listening connection. Undo and redo arrive here like any write. A writer that also listens hears its own write echoed, which is standard OSCQuery behavior and what makes undo reach it.

### Code layout

- `src/Libraries/MikanOsc` (new static library, no `MIKAN_API` export): `OscMessage` and `OscBundle` encode and decode for the tags `i f s b T F N h d`, `OscAddress` validation and sanitizing, and `OscUdpSocket`. Tests in `src/Programs/Tests/UnitTests/osc_unit_tests.cpp` (round trips, padding, bundle nesting, malformed packets, sanitizing).
- `src/Editor/Server/OscQuery/`: `OscQueryServer.{h,cpp}` (an `IServerRequestHandler`-shaped object owned by `MikanServer`, started in `startup`, ticked from `update`, re-registering its routes after `restartMessageServers`), `OscQueryTree.{h,cpp}` (path model, JSON emission, dirty tracking), `OscPropertyBridge.{h,cpp}` (variant to and from OSC, the RANGE and TYPE strings). Everything that touches live objects runs on the main thread.
- `GuiPanel_HttpTriggers.cpp` (the panel that already owns the HTTP port and remote-access toggle) gains the OSCQuery enable toggle and OSC port, with localization keys added through `tools/localization.py` so `run_localization_unit_tests` passes.
- `tools/oscquery_check.py` (new, standard library only): fetches `HOST_INFO`, walks the tree, optionally LISTENs on an address and prints pushes, and sends one OSC message built with `struct`. The drive-and-verify tool for this surface, listed in commands.md.

### Tests

- `osc_unit_tests.cpp` in `unit_test_suite_cpp` for the codec.
- `src/Editor/Server/Test/OscQueryTests.{h,cpp}` in `MikanCmd -runTests`: tree JSON from headless components (the `PropertyNotificationGuardTests` construction pattern), hidden descriptors absent, read-only ACCESS, enum RANGE VALS, name sanitizing and collision suffixing, every supported `MikanVariantType` through the bridge in both directions, rejection of wrong tags and read-only writes, the HOST_INFO shape, and the HTTP route answers through `bindTestServer` (`?HOST_INFO`, a subtree, a single attribute, 404, 204).

### Docs

- New `docs/reference/oscquery.md`: the three transports and ports, the address scheme, the type mapping table, visibility, RANGE sources, the write and push flow and its transaction behavior, discovery, the check tool, and what is omitted (arrays, maps, objects, spot light RGB). CLAUDE.md gets its entry in the reference list; README.md one sentence.
- modules.md and layout.md: `MikanOsc` and `src/Editor/Server/OscQuery`. transactions.md: OSC writes coalesce through the window. commands.md: `tools/oscquery_check.py`. plan.md: Later items for variable-length array nodes and for serving the OSC websocket on a port of its own if a client cannot upgrade on the HTTP port.

---

## Verification

Each part is verified before the next starts, and each ends with FormatCheck (clang-format 19.1.x), `MikanCmd.exe -runTests` and `unit_test_suite_cpp.exe` green, bindings regenerated, and `MikanClientTestCSharp --managed` plus the C++ test client run against a live editor through the automation drive loop (automation.md).

- Part 2: the inspector at inspector.open-rpc.org pointed at `ws://127.0.0.1:8080`: `rpc.discover` lists every registered method with its params and result schemas; calling `GetVideoSourceMode` from the inspector returns a typed result and an invalid id returns a JSON-RPC error. `curl http://127.0.0.1:8090/openrpc.json` returns the same document. The CEF GUI rebuilt against the new package connects and lists components.
- Part 3: `tools/oscquery_check.py --host-info`, then a tree walk shows the lights' properties with TYPE, ACCESS and RANGE; `--listen /DMXSequenceSystem/<name>/brightness` prints a push when the panel slider moves and when Ctrl+Z reverts it; `--send /DMXSequenceSystem/<name>/brightness 0.5` moves the panel and shows up in `history list` as one transaction after a burst. A DNS-SD browser sees `_oscjson._tcp` when remote access is on.

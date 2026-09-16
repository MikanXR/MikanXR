# Wire Protocol

The foundational contract in MikanXR: every struct and enum in `src/Libraries/MikanClientAPI` and `src/Libraries/MikanClientCore` is the wire protocol. Changing one changes what crosses the websocket between `Mikan.exe` and every client, and changes the generated C#/TypeScript bindings under `bindings/`. This doc covers the contract libraries, the Refureku reflection and codegen pipeline, the serialization layer and its traps, the server-side request routing, the property schema contract and its guard test, and the out-of-band shared texture path. See [modules.md](./modules.md) for the surrounding architecture, [build.md](./build.md) for how the codegen targets fit into the build, and [standards.md](./standards.md) for general coding rules.

---

## The contract libraries

Two DLLs define everything that crosses the process boundary:

- `src/Libraries/MikanClientCore` is the low-level client. A C API (`Public/MikanCoreCAPI.h`: `Mikan_Initialize`, `Mikan_Connect`, `Mikan_SendRequestJSON`, `Mikan_FetchNextEvent`, render target texture functions) plus the core types and enums in `Public/MikanCoreTypes.h` (`MikanCoreResult`, `MikanClientGraphicsApi`, `MikanRenderTargetDescriptor`, ...).

- `src/Libraries/MikanClientAPI`: the typed C++ API. `Public/MikanAPI.h` declares `IMikanAPI` (`connect`, `sendRequest`, `fetchNextEvent`). The rest of `Public/` is one header per domain holding the request/response/event structs and shared value types: `Mikan*Requests.h`, `Mikan*Events.h`, `Mikan*Types.h` (camera, stencil, stage, marker, light, property, script, shape, texture source, video source, remote control, ...).

Base types live in `Public/MikanAPITypes.h`: `MikanRequest` (`requestTypeName`, `requestId`), `MikanResponse` (`responseTypeName`, `requestId`, `resultCode`), `MikanEvent` (`eventTypeName`). Every concrete request/response/event derives from one of these.

Rules that follow from this being the wire contract:

- The wire type name of a message is the C++ struct name. Each struct's constructor stamps its type-name field via `MIKAN_REQUEST_TYPE_INFO_INIT` / `MIKAN_RESPONSE_TYPE_INFO_INIT` / `MIKAN_EVENT_TYPE_INFO_INIT`, which read `staticGetArchetype().getName()` from Refureku. Renaming a struct renames the message on the wire.

- Enum values must stay numerically stable across versions. `MikanAPIResult` in `MikanAPITypes.h` says so explicitly, and its low range must stay in sync with `MikanCoreResult` in `MikanCoreTypes.h`.

- Strings in these structs are `Serialization::String`, never `std::string` (see the serialization section).

---

## Refureku annotations

Both libraries (and `MikanSerialization`) are annotated for Refureku, a C++ static reflection generator that runs at build time. The pattern, from `Public/MikanStencilRequests.h`:

```cpp
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanStencilRequest")) GetModelStencilRenderGeometry
    : public MikanRequest
{
public:
    GetModelStencilRenderGeometry() { MIKAN_REQUEST_TYPE_INFO_INIT(GetModelStencilRenderGeometry) }

    FIELD() MikanStencilID stencilId= INVALID_MIKAN_ID;

#ifdef MIKANAPI_REFLECTION_ENABLED
    GetModelStencilRenderGeometry_GENERATED
#endif
};
```

Enums follow the same shape (`MikanAPITypes.h`):

```cpp
enum class ENUM(Serialization::CodeGenModule("MikanAPITypes")) MikanAPIResult
{
    Success ENUMVALUE_STRING("Success")= 0,
    ...
};
```

The moving parts:

- `STRUCT(...)` / `ENUM(...)` / `FIELD()` / `CLASS()` are the Refureku parse markers. Only marked entities are reflected (the `shouldParseAll*` flags in `RefurekuSettings.toml` are false, except enum values).

- `Serialization::CodeGenModule("Name")` (defined in `MikanSerialization/Public/SerializationProperty.h`) is an `rfk::Property` that tags the entity with an output-module name. The codegen tool groups types by this tag; one tag becomes one generated `.cs`/`.ts` file.

- Each header includes its generated `<Name>.rfkh.h` and ends with `File_<Name>_GENERATED`; each struct body carries `<Name>_GENERATED`. These expand to the reflection metadata.

- Each library has a `RefurekuSettings.toml` listing exactly which headers are parsed (`toProcessFiles`) and where generated files go (`build/RfkGenerated/<LibraryName>`). A new header with wire types must be added to that list or it is invisible to reflection and codegen.

- CMake runs the generator before compiling: `src/Libraries/MikanClientAPI/CMakeLists.txt` defines a `MikanClientAPIReflection` custom target that runs `RefurekuGenerator.exe` on the toml, and `MikanClientAPI` depends on it. `MikanClientCore` and `MikanSerialization` follow the same pattern.

---

## Codegen pipeline: C++ to C# and TypeScript

`src/Programs/ClientCodeGen/ClientCodeGen.cpp` builds `MikanClientCodeGen.exe`. It links against the reflected `MikanClientCore`/`MikanClientAPI`/`MikanSerialization` DLLs, walks the runtime Refureku database (`rfk::Struct`, `rfk::Enum`), buckets every entity by its `Serialization::CodeGenModule` property, and emits equivalent types per module.

How it runs (all wired in CMake, not manual):

- `bindings/csharp/CMakeLists.txt` defines the custom target `MikanCSharpCodeGen`, which runs `MikanClientCodeGen` with `bindings/csharp/CSharpCodeGenConfig.json` (`"output_path": "Generated"`). The `MikanClientCSharp` project depends on it, so building the C# bindings regenerates first. C# only builds under the Visual Studio generator (the project is `LANGUAGES CSharp`), so it is skipped under Ninja and in CI.

- `bindings/typescript/CMakeLists.txt` defines `MikanTypeScriptCodeGen`, which runs the tool with `TypescriptCodeGenConfig.json` (`"output_path": "types"`), then `MikanTypeScriptNpmInstall` (`npm install`), then `MikanClientTypeScript` (`npm run build`) compile to `dist/`.

The generated outputs are checked into git (`bindings/csharp/CMakeLists.txt` carries a TODO about removing that need). This creates the hard rule:

**Never hand-edit `bindings/csharp/Generated/` or `bindings/typescript/types/`.** They are overwritten by the next codegen run. To change a client-facing type, edit the C++ header in `MikanClientAPI`/`MikanClientCore`, rebuild, and let `MikanCSharpCodeGen` / `MikanTypeScriptCodeGen` regenerate. The hand-written runtime around the generated types (`bindings/csharp/MikanAPI.cs`, `bindings/typescript/MikanClient.ts`, the `Serialization/` folders) is editable.

---

## Message flow over the websocket

`src/Editor/Interprocess/WebsocketInterprocessMessageServer` hosts the socket (IXWebSocket, `ws://127.0.0.1`, port `8080`, protocol prefix `Mikan-`, all `#define`d in `InterprocessMessageServerInterface.h`).

- Requests are JSON text frames. `WebsocketInterprocessMessageServer::processRequests` SAX-scans the incoming string for `requestTypeName` and dispatches to the handler registered under that exact name; `requestId` is optional (fire-and-forget when absent). An unregistered type name gets a plain `MikanResponse` with `MikanAPIResult::UnknownFunction`.

- Responses fill a `ClientResponse` with either `utf8String` (sent as a text frame) or `binaryData` (sent as a binary frame). Binary responses use the `BinarySerializer` path; JSON responses use `Serialization::serializeToJsonString`.

- `processRequests` stops dequeuing once a tick's responses exceed a byte budget, leaving the rest for the next tick. Requests stay in arrival order per connection, and the starting connection rotates each tick. See [debugging.md](./debugging.md) for why.

- Events are server-to-client JSON pushes. `MikanServer::publishMikanJsonEvent` fans a serialized `MikanEvent` subclass out to every connection. Clients poll them off a queue via `IMikanAPI::fetchNextEvent` / `Mikan_FetchNextEvent`; there is no per-event acknowledgement.

---

## Serialization layer and its traps

`src/Libraries/MikanSerialization` walks reflected structs generically: `JsonSerializer`/`JsonDeserializer` (nlohmann-backed) for the websocket and config files, `BinarySerializer`/`BinaryDeserializer` for binary response payloads, with `SerializationVisitor` as the shared field-visiting core and `Serialization::List`/`Map`/`PolymorphicObjectPtr`/`String` as the reflected container types. `TypeRegistry::buildFromRfkDatabase` must run at startup before deserializing polymorphic objects by type name (both `MikanServer` clients and `CmdApp::exec` do this).

A struct's binary encoding is the concatenation of its fields in memory offset order, parents first, with no framing between them. `src/Editor/Server/ServerModelGeometryPayload` leans on that: model render geometry is serialized once, cached on the `MikanRenderModelResource` it came from, and each response is built by serializing only the `MikanResponse` header and appending those cached bytes, rather than walking every vertex through reflection again. `ModelGeometryPayloadTests` (in `MikanCmd.exe -runTests`) compares the spliced bytes against a whole serialization for both the stencil and shape responses, so a field added to `MikanResponse` or inserted ahead of `render_geometry` fails there.

Two traps, both real and both verified in code:

**1. `Serialization::String` is `const char*`-only. Do not widen it.** `Public/SerializableString.h` exposes only `const char*` (`setUtf8Value`/`getUtf8Value`, `const char*` constructor and assignment) and hides `std::string` in a pimpl. The header comment states why: `std::string` memory layout differs between Debug and Release CRTs, so passing it across a DLL boundary built against a different CRT (the Unreal plugin uses the Release CRT) corrupts the data. Bytes are always UTF-8; convert at the ingestion boundary. Keep the interface `const char*` even though `std::string` would be more convenient.

**2. The `const char*` to `bool` overload trap in `to_binary`.** `Public/BinaryUtility.h` declares `to_binary(BinaryWriter&, bool)` and `to_binary(BinaryWriter&, const Serialization::String&)` but no `const char*` overload. Passing a raw `const char*` therefore binds the `bool` overload: pointer-to-bool is a standard conversion, which beats the user-defined `const char*` to `Serialization::String` conversion in overload resolution. The string silently serializes as one byte of `true`. When writing `to_binary`/`from_binary` overloads for a new type, wrap C strings in `Serialization::String` (or `std::string`) explicitly before serializing.

---

## Server side: MikanServer and request handlers

`src/Editor/Server/MikanServer` owns the `WebsocketInterprocessMessageServer` (plus an `HttpInterprocessMessageServer` for HTTP triggers) and a fixed set of per-domain handlers constructed in its constructor: `CameraRequestHandler`, `FunctionRequestHandler`, `LightRequestHandler`, `PropertyRequestHandler`, `MarkerRequestHandler`, `ScriptRequestHandler`, `ShapeRequestHandler`, `StencilRequestHandler`, `TextureSourceRequestHandler`, `VideoSourceRequestHandler`, and the `RemoteControlManager`. `MikanServer` itself only handles connection lifecycle (`InitClientRequest`/`DisposeClientRequest`). `ScriptRequestHandler` also installs the project's HTTP trigger routes on the HTTP server from the script system's route table, and `InvokeScriptTrigger` addresses a script component by name or broadcasts (see [scripting.md](./scripting.md)).

Each handler derives from `IServerRequestHandler` (`src/Editor/Server/IServerRequestHandler.h`) and, in its `startup`, registers each request type by reflected name:

```cpp
messageServer->setRequestHandler(GetVideoSourceMode::staticGetArchetype().getName(), ...);
```

Recipe for a new remote-controllable capability (do not modify request routing in `MikanServer` beyond construction/startup wiring):

1. Define the request/response structs in a `MikanClientAPI/Public` header, annotated with `STRUCT(Serialization::CodeGenModule(...))` and `FIELD()`, with the `MIKAN_*_TYPE_INFO_INIT` constructor macro. Add the header to `RefurekuSettings.toml` if new.
2. Add (or extend) an `IServerRequestHandler` in `src/Editor/Server` that registers the type names and implements the behavior, and wire it into `MikanServer`'s constructor/`startup` if it is a new handler class.
3. Rebuild and regenerate the C#/TypeScript bindings.

`RemoteControlManager` plus `IRemoteControllable` (`handleRemoteControlCommand`, `sendRemoteControlEvent`) is the string-command channel objects implement to be remotely driven; the structured "properties" system on top is served by `PropertyRequestHandler` (`PropertyGetValueRequest`, `PropertySetValueRequest`, `GetPropertyDescriptors`, `ComponentGetValuesRequest`, `GetComponentListRequest`, `SystemGetValuesRequest`, ...).

---

## The property contract and its guard test

A client-facing property must be wired consistently in three places:

1. **Values struct**: the `Mikan*Values` struct in `MikanClientAPI` (e.g. `MikanAnchorComponentValues` in `MikanAnchorTypes.h`) with a `FIELD()` per property. This is what `ComponentGetValuesRequest`/`SystemGetValuesRequest` return, filled generically by `Serialization::serializeFromEntity` (`src/Editor/Server/ServerEntitySerializer.cpp`), whose `EntityAccessorReadVisitor` maps each field type to a required `MikanVariantType`.
2. **Descriptor**: the class's static `getPropertyDescriptors` must advertise a `PropertyDescriptor` (`src/Editor/ECS/PropertyInterface.h`) with the same name and the exact `MikanVariantType` the serializer demands. Descriptors default to `isClientAPIHidden() == true`; a descriptor left hidden is invisible to clients and exempt from the field check.
3. **`getPropertyValue`**: the component/system's `IPropertyInterface::getPropertyValue` must answer for that name with a `MikanVariant` of that type, since `EntityAccessorReadVisitor` fills every values-struct field by calling it.

The guard test is `src/Editor/Server/Test/ClientApiPropertySchemaTests.cpp`, run from `CmdApp::runTests` via `MikanCmd.exe -runTests` (see [commands.md](./commands.md)). For every entry in `k_schemaTestEntries` (each `SCHEMA_ENTRY(EditorClass, ValuesStruct)` pair, covering all components and object systems) it:

- instantiates the values struct via reflection and walks it with `PropertySchemaVisitor`, a deliberate mirror of `EntityAccessorReadVisitor`, computing the `MikanVariantType` the serializer will demand per field (failing on unsupported field types);
- fails if any values-struct field has no non-hidden descriptor of the exact same name and type;
- fails if any non-hidden descriptor has no matching values-struct field.

What it cannot check is leg 3: a `getPropertyValue` that ignores a name fails only at runtime (the field stays default-valued). So when adding a property: add the `FIELD()`, add the descriptor with matching name/type, implement `getPropertyValue` (and `setPropertyValue` if writable), and add a `SCHEMA_ENTRY` if the class is new. Then regenerate bindings, since the values struct changed the wire protocol.

---

## Out-of-band: video frames via shared textures

Rendered frames never travel over the websocket. Clients allocate shared render target textures through the core C API (`Mikan_AllocateCameraRenderTargetTextures` with a `MikanRenderTargetDescriptor`, then `Mikan_WriteCameraColorRenderTargetTexture` / `...Depth...` / `...Shadow...` per frame), backed by the `src/Libraries/MikanSharedTexture` library (`SharedTextureWriter.h`). On the editor side, `src/Editor/Interprocess/SharedTextureReader.h` (`SharedTextureReadAccessor`) opens the same shared textures by sender name and pulls color/depth/shadow into `IMkTexture`s when `readRenderTargetTextures` sees a new frame index. A float depth buffer (`FLOAT_DEVICE_DEPTH` or `FLOAT_SCENE_DEPTH`) is packed on the client side into an RGBA8 sender as linear eye depth normalized between the camera's near and far planes, `(eyeDepth - zNear) / (zFar - zNear)` clamped below 1, the same normalization the editor's `DepthMaskNode` writes for stencils, so a client surface and a stencil at one distance compare equal in the depth mask. The websocket carries only the control traffic around this seam (the render-target requests in `MikanRenderTargetRequests.h` and frame events); pixels move through GPU shared texture memory. See [compositor.md](./compositor.md) for how the editor consumes these frames.

A Vulkan client takes the same path with one more step, since a Spout sender is a D3D11 texture. `SpoutVulkanTextureWriter` (in `SharedTextureWriter.cpp`, beside the OpenGL, D3D11, and D3D12 writers) creates each buffer as a `D3D11_RESOURCE_MISC_SHARED` texture on a D3D11 device of its own, on the adapter whose LUID the `VkPhysicalDevice` reports, and imports that texture's KMT share handle into the client's `VkDevice` through `VK_KHR_external_memory_win32` as a dedicated allocation. A write blits the client's image into the imported image on the client's queue, waits the submission's fence, then sends the D3D11 side through the ordinary `SendTexture` path. The fence wait is the synchronization between the two APIs, and the editor's reader sees a plain Spout sender. The client hands its device as a `MikanVulkanDeviceInterface` (instance, physical device, device, a graphics queue and its family, the device created with `VK_KHR_external_memory_win32` enabled) and each frame as a `MikanVulkanTexture` (image, layout, format, extent). Both are declared in `MikanClientCore/Public/MikanCoreVulkanTypes.h`, the one SDK header that names a Vulkan type, kept outside the Refureku-parsed set so nothing Vulkan reaches the wire or the bindings. `MikanClient.cpp` converts them at the boundary into the header-free `SharedVulkanDeviceInterface` and `SharedVulkanTexture` mirrors in `SharedTextureWriter.h`, the way it converts the render target descriptor. A depth buffer is expected as an `R32_SFLOAT` color image already holding depth, because a depth-format image cannot be copied to a color format, and the same D3D11 packer the DX11 writer uses packs it to RGBA8. volk loads `vulkan-1.dll` at runtime, so `MikanSharedTexture.dll` carries no Vulkan import and every other client is unaffected.

---

## Client side

A C++ client calls `IMikanAPI::createMikanAPI()`, `init(...)`, then `connect()` (defaults) or `connect(host, port)`, and drives everything through `sendRequest(MikanRequest&)`, which returns a `MikanResponseFuture` keyed by `requestId`. Events arrive by polling `fetchNextEvent(MikanEventPtr&)` each frame. The C# (`bindings/csharp/MikanAPI.cs`, `MikanRequestManager.cs`, `MikanEventManager.cs`) and TypeScript (`bindings/typescript/MikanClient.ts`, `MikanRequestManager.ts`, `MikanEventManager.ts`) runtimes mirror this shape over the same JSON protocol, using the generated types in `Generated/` and `types/` respectively. Under the hood all three serialize the request struct to JSON (with `requestTypeName` and `requestId`), send it as a websocket text frame, and match responses back to futures by `requestId`.

A C++ consumer stamps `requestTypeName` in the request struct's inline constructor. With `ENABLE_MIKANAPI_REFLECTION` the name comes from the Refureku archetype, and without it (the Unreal plugin and any other consumer that ships only the public headers) `MikanAPITypes.h` stringifies the class name instead, which is the same string. The inline constructor is what runs in an optimized consumer build, so the header must produce the name in both configurations or the server answers `UnknownFunction` and the future times out.

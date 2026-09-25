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

- CMake runs the generator before compiling: `src/Libraries/MikanClientAPI/CMakeLists.txt` defines a `MikanClientAPIReflection` custom target that runs `RefurekuGenerator.exe` on the toml, and `MikanClientAPI` depends on it. `MikanClientCore` and `MikanSerialization` follow the same pattern. Refureku and its generator build from the `thirdparty/Refureku` submodule as part of the tree. A change to either is a submodule bump rather than a new binary package, and [build.md](./build.md) covers that integration.

---

## Codegen pipeline: C++ to C# and TypeScript

`src/Programs/ClientCodeGen` builds `MikanClientCodeGen.exe`. It links against the reflected `MikanClientCore`/`MikanClientAPI`/`MikanSerialization` DLLs, walks the runtime Refureku database (`rfk::Struct`, `rfk::Enum`), buckets every entity by its `Serialization::CodeGenModule` property, and emits equivalent types per module.

A target language is one file behind the `MikanClientLanguageGen` base class, which declares a `create*ClientGen` factory per language the way `ISharedTextureWriterBackend` does per graphics API. The base carries what every language needs from the reflection data: the wire type-name field of a request, response or event, a default-constructed instance to read real field defaults from, the inheritance ordering a language needs when its file cannot reference a type declared later, and which module owns a type. A generator emits one file per module through `generateModuleFile`, plus whatever files describe the whole output through `generateWholeOutputFiles` (TypeScript writes four: the shared field descriptor, the two registries, and the barrel; C# writes none). Adding a language is a generator file, a case in `createLanguageGen`, a `target_language` string, and a config plus CMake target under `bindings/`.

Entity order is decided once, in `CodeGenDatabase::sortEntities`, before anything emits. The reflection database enumerates in an order that varies between runs, so a generator that walks those vectors as filled rewrites unchanged files on every regeneration. Sorting centrally is also what makes the TypeScript inheritance ordering deterministic, since it preserves input order among structs that have no relationship to each other.

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

`src/Libraries/MikanSerialization` walks reflected structs generically: `JsonSerializer`/`JsonDeserializer` (nlohmann-backed) for the websocket and config files, `BinarySerializer`/`BinaryDeserializer` for binary response payloads, with `SerializationVisitor` as the shared field-visiting core and `Serialization::List`/`Map`/`PolymorphicObjectPtr`/`String` as the reflected container types. `TypeRegistry::build` must run at startup before deserializing polymorphic objects by type name (both `MikanServer` clients and `CmdApp::exec` do this).

Refureku is MikanSerialization's implementation detail, not a vocabulary the rest of the tree shares. Code outside the library names two things instead:

- `Serialization::StructTypeHandle` (`Public/ReflectionHandles.h`), the opaque reflected-struct handle that `getClientAPIValuesStructType`, `TypeRegistry::getStructByName`, and every serializer entry point traffic in
- `Serialization::ValueAccessor`'s own type questions: `isType<T>()`, `isTemplateInstantiation()`, `getTemplateName()`, `getTemplateArgumentCount()`, `isTemplateArgumentType<T>(index)`, and `setEnumValueFromInt()`

`isType<T>()` stays compile-time checked without the public header including Refureku: a reflected struct carries a `staticGetArchetype()` whose address the accessor compares, and a fundamental resolves through the `Serialization::FundamentalType` enum. A visitor subclass outside the library (`EntityAccessorReadVisitor` in `src/Editor/Server/ServerEntitySerializer.cpp`, `PropertySchemaVisitor` in the schema test) is written entirely in that vocabulary. `rfk::` itself appears only in `MikanSerialization`, `ClientCodeGen.cpp`, and `MikanClientAPI/Private/MikanVariantTypes.cpp`, which reads its own enum's `ENUMVALUE_STRING`.

A struct's binary encoding is the concatenation of its fields in memory offset order, parents first, with no framing between them. `Serialization::getStructFieldsInWireOrder` is the definition of that order for one struct: its own public, non-static fields sorted by memory offset. The serializer walks the parent chain around it, and the bindings generator emits the fields in the same order. Those two used to compute the order separately and disagreed on whether to skip non-public and static fields, which nothing would have caught until a reflected field was one of those.

Each client runtime recovers that order from the generated type, and neither infers it from reflection order:

- C#: every generated field carries `[MikanFieldOrder(n)]`, which `Utils.memoryOffsetSortStructFields` sorts on. `Type.GetFields()` is documented as returning fields in no particular order, so declaration order is not something the runtime may assume.
- TypeScript: each generated class carries a `__serializationMetadata` array, and the runtime walks the prototype chain base-first to concatenate them.

`src/Editor/Server/ServerModelGeometryPayload` leans on the same layout: model render geometry is serialized once, cached on the `MikanRenderModelResource` it came from, and each response is built by serializing only the `MikanResponse` header and appending those cached bytes, rather than walking every vertex through reflection again. `ModelGeometryPayloadTests` (in `MikanCmd.exe -runTests`) compares the spliced bytes against a whole serialization for both the stencil and shape responses, so a field added to `MikanResponse` or inserted ahead of `render_geometry` fails there.

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

- instantiates the values struct via reflection and walks it with `PropertySchemaVisitor`, a deliberate mirror of `EntityAccessorReadVisitor`, computing the `MikanVariantType` the serializer will demand per field (failing on unsupported field types). The mirror is only as good as what it checks: for a `Serialization::Map` field it used to record `STRING_MAP` on the template name alone, while the serializer tested the key against `std::string`. Every map on the wire is `Map<Serialization::String, Serialization::String>`, so that test never passed and `MikanUSBVideoSourceSystemValues::usb_device_map` came back empty with a serialization error. Both sides now check the key and value types, and a map of any other shape fails the guard test rather than at runtime;
- fails if any values-struct field has no non-hidden descriptor of the exact same name and type;
- fails if any non-hidden descriptor has no matching values-struct field.

What it cannot check is leg 3: a `getPropertyValue` that ignores a name fails only at runtime (the field stays default-valued). So when adding a property: add the `FIELD()`, add the descriptor with matching name/type, implement `getPropertyValue` (and `setPropertyValue` if writable), and add a `SCHEMA_ENTRY` if the class is new. Then regenerate bindings, since the values struct changed the wire protocol.

There is a fourth wire, easy to miss on an object system. `SystemGetValuesRequest` allocates its response object from `MikanObjectSystem::getClientAPIValuesStructType`, whose base returns the empty `MikanSystemValues`. A system that declares a `Mikan*SystemValues` struct but never overrides that accessor answers the base struct instead, so a client reading the response as the derived type reads whatever follows the base in its own memory. Nothing fails loudly. A `SCHEMA_ENTRY` for the system is what catches it, which is why every system with a values struct has one.

Note also that a property the editor panels show needs a localization key, or `run_localization_unit_tests` fails the build ([localization.md](./localization.md)). Only a `setUIHidden` descriptor is exempt.

---

## Camera frame pacing

A client renders for a camera in one of two ways, chosen by the camera's `frame_sync_mode` property (`MikanCameraFrameSyncMode` in `MikanCameraTypes.h`).

- `VideoFrame`: the editor publishes a `MikanCameraNewFrameEvent` per video frame, carrying the camera state and the frame index, and the client renders each one from that state and publishes with that index. This is the pairing a moving camera needs, since a render from a stale pose composites against the wrong video frame.
- `FreeRunning`: the editor publishes a `MikanCameraNewPropertiesEvent` (the same fields without `frame`) only when the camera state changes, and the client renders on its own clock, capped to the video source frame rate it reads through `GetVideoSourceMode`, publishing with `frame_index` of -1. The compositor serves its newest texture for that client. A static camera loses nothing to this, and the wire carries no per-frame traffic for it.
- `Auto`, the default, resolves at runtime from `pose_driven_per_frame`, a read-only camera property that is true when a tracking mount or a frame-coupled video source (ARKit, or a take with a pose track) drives the pose. A marker-aligned camera is static and resolves to free running.

The properties event is published on change while the compositor runs, and is republished regardless of change when the compositor starts and when the camera's effective mode becomes free running. A client that starts or binds a camera between changes hears nothing, so it pulls the state with `GetCameraProperties` (`MikanCameraRequests.h`), whose `MikanCameraPropertiesResponse` carries the same fields plus `compositor_running`. `MikanCompositorStartedEvent` and `MikanCompositorStoppedEvent` (`MikanCompositorEvents.h`, `compositor_id` plus `camera_id`) bracket the run, which is what a free-running client has in place of the frame events as its signal that something consumes what it publishes. The Unreal plugin pulls when its camera data binds, which covers a play-in-editor session starting against an editor whose compositor has been running all along.

Only the Unreal plugin implements the free-running side today. A client that ignores the mode and renders per frame event keeps working for a video-frame synced camera, and simply never renders for a free-running one.

---

## The resolution a client renders at

`pixel_size` on the camera events is not the video resolution. It is the size the camera asks a client to render its color buffer at, and the resolution `focal_length` and `principal_point` are expressed in. Three camera properties decide it:

- `client_color_render_scale` for the color buffer, default 1, held to [0.5, 2]
- `client_aux_render_scale` for the depth and shadow buffers, same range, whose size travels separately as `aux_pixel_size`
- `client_max_buffer_dimension` (`MikanClientMaxBufferDimension`: 1024, 2048, 4096 or 8192, default 4096), a ceiling on the longer edge of every buffer, applied after the scales with the aspect ratio kept

A scale above 1 is a supersample. The composite is always built at video resolution ([compositor.md](./compositor.md)), so extra client pixels buy edge coverage that survives the downsample rather than a larger output. The depth and shadow buffers get their own scale because each is a full render pass on the client and neither usually needs the color buffer's resolution. The ceiling exists because the scales alone cannot bound the result: they multiply a video resolution the camera does not choose, so without it a setting can outrun a client's texture allocator.

`CameraDefinition::computeClientRenderSize` is the only place that arithmetic lives, and both the published size and the size the editor's camera panel reports come through it, so the two cannot disagree.

`CameraComponent::applyClientRenderScales` is the single place it is applied. It multiplies `pixel_size`, `focal_length` and `principal_point` by the same factor, taken from the size that was actually published rather than from the requested scale. A client builds its projection from those values only as ratios against the pixel size, so the projection matrix comes out unchanged and neither a scale nor the ceiling can move where anything lands in frame. The depth and shadow passes share that projection and differ only in target size. `CameraComponent::getAperturePixelDimensions` remains the video resolution and is what the calibration tools and `DepthMaskNode` read.

`MikanRenderTargetDescriptor` carries `aux_width` and `aux_height` beside `width` and `height` so the editor allocates its texture ring to match. Zero in either means the depth and shadow buffers are the color size. A client that ignores `aux_pixel_size` renders all three buffers at `pixel_size` and keeps working, since `SharedTextureReadAccessor` sizes each texture from its own Spout sender.

Adding a field to this struct has a fourth leg the other wire types do not have. The struct crosses the core C API by value, so the C# bindings mirror it by hand as `MikanRenderTargetDescriptor_Native` in `bindings/csharp/MikanCoreNative.cs`, with `LayoutKind.Sequential` and no field names on the wire to catch a mismatch. A field missing there shifts every field after it, and since `Mikan_GetCameraRenderTargetDescriptor` takes the struct as an `out` parameter, a short struct is also written past its end. `MikanRenderTargetAPI.cs` then has to copy the new field in both directions. The generated bindings and the property schema guard test cover none of this, because the struct reaches C# through P/Invoke rather than through JSON.

`csharp_interop_unit_tests.cpp` (in `unit_test_suite_cpp`) is what holds that leg. It reads the mirror's field names out of the C# source and compares them, in order, against `getStructFieldsInWireOrder` on the reflected struct, so a field added on one side and not the other fails the build's test run rather than corrupting a client's stack. A `static_assert` on `sizeof` sits beside it to catch a field whose type changed without its name changing, which the name comparison cannot see. The test needs the repo it was configured from, which arrives as the `MIKAN_REPO_ROOT_DIR` compile definition.

---

## Which space a client receives

The component values a client mirrors are relative, never absolute. `MikanTransformComponentValues` (`MikanTransformTypes.h`) carries `parent_transform_id` plus a relative scale, quaternion and position, and nothing else. A client rebuilds a world transform by walking the parent chain itself, which is what lets it anchor a whole stage wherever it likes in its own scene. The Unreal plugin does exactly this, and deliberately lets the artist place the stage actor freely and pin it there, so the editor's own stage transform is not the client's.

Two things on the wire are absolute, and both are therefore stage-relative rather than world-relative:

- `MikanCameraNewFrameEvent` and `MikanCameraNewPropertiesEvent` (`MikanCameraEvents.h`) carry `camera_position` / `camera_forward` / `camera_up` from `CameraComponent::getStageSpaceAperturePose`. A world-space pose here would be applied a second time by the client's own stage anchor, putting the composited CG off by the stage transform.
- `MikanLightEnvironmentComponentValues`' `sh_coefficients` and `key_light_direction` (`MikanLightTypes.h`) are in the stage space of the capturing camera's stage, so a stage's lighting travels with the stage. Spherical harmonics do not transform like vectors. See [conventions.md](./conventions.md) and [scene-lighting.md](./scene-lighting.md) for why a client evaluates them in Mikan space rather than converting them.

---

## Out-of-band: video frames via shared textures

Rendered frames never travel over the websocket. Clients allocate shared render target textures through the core C API (`Mikan_AllocateCameraRenderTargetTextures` with a `MikanRenderTargetDescriptor`, then `Mikan_WriteCameraColorRenderTargetTexture` / `...Depth...` / `...Shadow...` per frame), backed by the `src/Libraries/MikanSharedTexture` library (`SharedTextureWriter.h`). A client may hand a different texture each write, as the Unreal plugin does with its staging ring, since the D3D12 writer keeps its D3D11On12 wrap of each pointer it has seen rather than rewrapping per frame, and drops them all when the client frees the render target. On the editor side, `src/Editor/Interprocess/SharedTextureReader.h` (`SharedTextureReadAccessor`) opens the same shared textures by sender name and pulls color/depth/shadow into `IMkTexture`s when `readRenderTargetTextures` sees a new frame index. A float depth buffer (`FLOAT_DEVICE_DEPTH` or `FLOAT_SCENE_DEPTH`) is packed on the client side into an RGBA8 sender as linear eye depth normalized between the camera's near and far planes, `(eyeDepth - zNear) / (zFar - zNear)` clamped below 1, the same normalization the editor's `DepthMaskNode` writes for stencils, so a client surface and a stencil at one distance compare equal in the depth mask. The websocket carries only the control traffic around this seam (the render-target requests in `MikanRenderTargetRequests.h` and frame events); pixels move through GPU shared texture memory. See [compositor.md](./compositor.md) for how the editor consumes these frames.

A Vulkan client takes the same path with one more step, since a Spout sender is a D3D11 texture. `SpoutVulkanTextureWriter` (in `SharedTextureWriter.cpp`, beside the OpenGL, D3D11, and D3D12 writers) creates each buffer as a `D3D11_RESOURCE_MISC_SHARED` texture on a D3D11 device of its own, on the adapter whose LUID the `VkPhysicalDevice` reports, and imports that texture's KMT share handle into the client's `VkDevice` through `VK_KHR_external_memory_win32` as a dedicated allocation. A write blits the client's image into the imported image on the client's queue, waits the submission's fence, then sends the D3D11 side through the ordinary `SendTexture` path. The fence wait is the synchronization between the two APIs, and the editor's reader sees a plain Spout sender. The client hands its device as a `MikanVulkanDeviceInterface` (instance, physical device, device, a graphics queue and its family, the device created with `VK_KHR_external_memory_win32` enabled) and each frame as a `MikanVulkanTexture` (image, layout, format, extent). Both are declared in `MikanClientCore/Public/MikanCoreVulkanTypes.h`, the one SDK header that names a Vulkan type, kept outside the Refureku-parsed set so nothing Vulkan reaches the wire or the bindings. `MikanClient.cpp` converts them at the boundary into the header-free `SharedVulkanDeviceInterface` and `SharedVulkanTexture` mirrors in `SharedTextureWriter.h`, the way it converts the render target descriptor. A depth buffer is expected as an `R32_SFLOAT` color image already holding depth, because a depth-format image cannot be copied to a color format, and the same D3D11 packer the DX11 writer uses packs it to RGBA8. volk loads `vulkan-1.dll` at runtime, so `MikanSharedTexture.dll` carries no Vulkan import and every other client is unaffected.

---

## Client side

A C++ client calls `IMikanAPI::createMikanAPI()`, `init(...)`, then `connect()` (defaults) or `connect(host, port)`, and drives everything through `sendRequest(MikanRequest&)`, which returns a `MikanResponseFuture` keyed by `requestId`. Events arrive by polling `fetchNextEvent(MikanEventPtr&)` each frame. The C# (`bindings/csharp/MikanAPI.cs`, `MikanRequestManager.cs`, `MikanEventManager.cs`) and TypeScript (`bindings/typescript/MikanClient.ts`, `MikanRequestManager.ts`, `MikanEventManager.ts`) runtimes mirror this shape over the same JSON protocol, using the generated types in `Generated/` and `types/` respectively. Under the hood all three serialize the request struct to JSON (with `requestTypeName` and `requestId`), send it as a websocket text frame, and match responses back to futures by `requestId`.

A C++ consumer stamps `requestTypeName` in the request struct's inline constructor. With `ENABLE_MIKANAPI_REFLECTION` the name comes from the Refureku archetype, and without it (the Unreal plugin and any other consumer that ships only the public headers) `MikanAPITypes.h` stringifies the class name instead, which is the same string. The inline constructor is what runs in an optimized consumer build, so the header must produce the name in both configurations or the server answers `UnknownFunction` and the future times out.

# TypeScript Bindings Code Generation

The TypeScript type definitions in the `types/` folder are auto-generated from C++ reflection data using the ClientCodeGen tool.

## How to Regenerate Types

1. Build the MikanXR project (this builds the ClientCodeGen tool)

2. Run the code generator for TypeScript:
```bash
cd build/Debug/bin  # or build/Release/bin
ClientCodeGen.exe ../../../src/Programs/ClientCodeGen/config_typescript.json
```

3. The generated TypeScript files will be written to `bindings/typescript/types/`

## Generated Files

The following files are auto-generated and should NOT be edited manually:

- `MikanAPITypes.ts` - The `MikanAPIResult` enum (the base request/response/event classes are hand-written in `../MikanAPITypes.ts`, since the JSON-RPC envelope carries their members)
- `MikanCoreTypes.ts` - Core enums and constants
- `MikanMathTypes.ts` - Math types (vectors, quaternions, transforms, matrices)
- `MikanClientTypes.ts` - Client information structures
- `MikanClientRequests.ts` - Client request types
- `MikanClientEvents.ts` - Client event types
- `MikanSpatialAnchorTypes.ts` - Spatial anchor data structures
- `MikanSpatialAnchorRequests.ts` - Spatial anchor request types
- `MikanSpatialAnchorEvents.ts` - Spatial anchor event types
- Additional module type files as they are added

## Code Generation Features

The generator:
- Converts C++ types to TypeScript types
- Generates TypeScript enums from C++ enums
- Creates TypeScript interfaces from C++ structs
- Exports class ID constants as bigint (for protocol compatibility)
- Handles inheritance (extends for interfaces)
- Maps C++ types to appropriate TypeScript types:
  - `int`, `float`, `double` → `number`
  - `int64_t`, `uint64_t` → `bigint`
  - `bool` → `boolean`
  - `String` → `string`
  - `List<T>` → `T[]`
  - `Map<K,V>` → `Record<K,V>`

## Manual Files

The following files are maintained manually and are NOT generated:

- `index.ts` - Main export file (update this when new modules are added)
- `PolymorphicObject.ts` - Base classes for polymorphic types
- `MikanClient.ts` - Main WebSocket client implementation
- `MikanAPITypes.ts` - The `MikanRequest`, `MikanResponse` and `MikanEvent` bases
- `MikanJsonRpc.ts` - The JSON-RPC 2.0 envelope: format a request, parse and classify a frame
- `MikanRequestManager.ts` - Request/response handling
- `MikanEventManager.ts` - Event handling
- `MikanResponseFuture.ts` - Promise-based response futures

## Updating After C++ Changes

Whenever you add or modify:
- Mikan API types
- Event types
- Request/Response types

You must regenerate the TypeScript bindings to keep them in sync with the C++ API.

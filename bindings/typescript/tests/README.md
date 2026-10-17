# MikanXR TypeScript Serialization Tests

This directory contains unit tests for the TypeScript serialization code, mirroring the C# tests in `bindings/csharp/Tests/SerializationUnitTests.cs`.

## Setup

Before running the tests, install the dependencies:

```bash
cd bindings/typescript
npm install
```

## Running Tests

### Run all tests
```bash
npm test
```

### Run tests in watch mode (auto-rerun on file changes)
```bash
npm run test:watch
```

### Run tests with coverage report
```bash
npm run test:coverage
```

## Test Structure

### SerializationTestClasses.ts
Contains test data structures used for serialization testing:
- `SerializationPoint`, `SerializationPoint2d`, `SerializationPoint3d` - Polymorphic object hierarchies
- `SerializationTestObject` - Complex object with various field types
- `buildSerializationTestObject()` - Factory function for creating test data
- `verifySerializationTestObject()` - Verification function for comparing objects

### SerializationUnitTests.test.ts
Contains the actual test cases:
- **JSON Serialization Tests** - Test JSON serialization/deserialization
  - Basic round-trip serialization
  - Empty arrays and maps
  - BigInt preservation
  - Negative numbers
  - Special characters in strings

- **Edge Cases** - Test edge conditions
  - Null polymorphic objects
  - Boolean edge cases
  - Zero values
  - Empty strings

- **Type Registry** - Test type registration system
  - Type registration and retrieval
  - Unregistered type handling

## Test Coverage

The tests cover the following serialization features:

### Primitive Types
- `boolean`
- `number` (byte, ubyte, short, ushort, int, uint, float, double)
- `bigint` (long, ulong)
- `string`

### Complex Types
- Enums
- Classes/Structs
- Arrays/Lists
- Maps/Dictionaries
- Polymorphic Objects (via `PolymorphicObject`)

### Serialization Formats
- JSON (text-based websocket messages)

## Comparison with C# Tests

The TypeScript tests mirror the C# tests in structure and coverage:

| C# Test | TypeScript Test | Status |
|---------|----------------|--------|
| `TestReflectionFromJson()` | JSON Serialization suite | ✅ Implemented |
| Additional edge cases | Edge Cases suite | ✅ Enhanced |

## Notes

- The TypeScript implementation uses metadata decorators instead of C# reflection
- BigInt is used for 64-bit integers (TypeScript limitation compared to C#)
- The visitor pattern is used for JSON serialization
- TypeRegistry provides runtime type lookup similar to C# reflection

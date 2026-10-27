#pragma once

#include "SerializationExport.h"

#include <cstdint>
#include <functional>

namespace rfk
{
class Enum;
class Struct;
}; // namespace rfk

namespace Serialization
{
// Opaque handles to a reflected struct and enum type.
//
// Consumers outside this library name these aliases rather than the underlying reflection
// types, so the choice of reflection library stays a MikanSerialization implementation
// detail. Only MikanSerialization's Private sources and the bindings generator work with
// the concrete types.
using StructTypeHandle= rfk::Struct const*;
using EnumTypeHandle= rfk::Enum const*;

// The type's archetype name, which is its name on the wire
SERIALIZATION_API const char* getStructTypeName(StructTypeHandle structType);
SERIALIZATION_API const char* getEnumTypeName(EnumTypeHandle enumType);

// Every value of the enum in declaration order, named by the string the serializer writes
// for it. Two names may share a value.
SERIALIZATION_API void forEachEnumValue(EnumTypeHandle enumType,
										const std::function<void(const char* valueName, int64_t value)>& visitor);

// The name the serializer writes for a value, or null when the value names nothing
SERIALIZATION_API const char* getEnumValueName(EnumTypeHandle enumType, int64_t value);

// The value a name the serializer writes stands for. False when the name names nothing.
SERIALIZATION_API bool findEnumValueByName(EnumTypeHandle enumType, const char* valueName, int64_t& outValue);

// The response struct name a request struct's ResponseType property names, or null when it
// carries none, which means the request is answered with the plain MikanResponse
SERIALIZATION_API const char* getResponseTypeName(StructTypeHandle requestType);

// Every reflected struct that derives from the struct, directly or through another subclass.
// Each one is visited once and the struct itself is not visited.
SERIALIZATION_API void forEachSubclass(StructTypeHandle structType,
									   const std::function<void(StructTypeHandle subclass)>& visitor);
} // namespace Serialization

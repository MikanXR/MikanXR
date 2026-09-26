#pragma once

namespace rfk
{
class Struct;
};

namespace Serialization
{
// Opaque handle to a reflected struct type.
//
// Consumers outside this library name these aliases rather than the underlying reflection
// types, so the choice of reflection library stays a MikanSerialization implementation
// detail. Only MikanSerialization's Private sources and the bindings generator work with
// the concrete types.
using StructTypeHandle= rfk::Struct const*;
} // namespace Serialization

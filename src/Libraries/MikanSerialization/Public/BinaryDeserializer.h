#pragma once

#include "SerializationExport.h"
#include "ReflectionHandles.h"
#include "SerializationVisitor.h"

#include <vector>

namespace Serialization
{
SERIALIZATION_API bool deserializeFromBytes(const std::vector<uint8_t>& inBytes, void* instance,
											StructTypeHandle structType, std::string& outErrorMesg);
SERIALIZATION_API bool deserializeFromBytes(const uint8_t* inBytes, const size_t inSize, void* instance,
											StructTypeHandle structType, std::string& outErrorMesg);

#ifdef SERIALIZATION_REFLECTION_ENABLED
template <typename t_object_type>
bool deserializeFromBytes(const std::vector<uint8_t>& inBytes, t_object_type& instance, std::string& outErrorMesg)
{
	return deserializeFromBytes(inBytes, &instance, &t_object_type::staticGetArchetype(), outErrorMesg);
}
#endif // SERIALIZATION_REFLECTION_ENABLED
}; // namespace Serialization

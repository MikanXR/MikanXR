#pragma once

#include "SerializationExport.h"
#include "ReflectionHandles.h"
#include "SerializationVisitor.h"

#include <vector>

namespace Serialization
{
SERIALIZATION_API bool serializeToBytes(const void* instance, StructTypeHandle structType,
										std::vector<uint8_t>& outBytes, std::string& outErrorMsg);

#ifdef SERIALIZATION_REFLECTION_ENABLED
template <typename t_object_type>
bool serializeToBytes(const t_object_type& instance, std::vector<uint8_t>& outBytes, std::string& outErrorMsg)
{
	return serializeToBytes(&instance, &t_object_type::staticGetArchetype(), outBytes, outErrorMsg);
}
#endif // SERIALIZATION_REFLECTION_ENABLED
}; // namespace Serialization

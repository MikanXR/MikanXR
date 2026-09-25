#pragma once

#include "SerializationExport.h"
#include "ReflectionHandles.h"
#include "SerializationVisitor.h"

#include <nlohmann/json_fwd.hpp>
#include <string>

namespace Serialization
{
SERIALIZATION_API bool deserializeFromJsonString(const std::string& jsonString, void* instance,
												 StructTypeHandle structType, std::string& outErrorMsg);

SERIALIZATION_API bool deserializeFromJson(const nlohmann::json& jsonObject, void* instance,
										   StructTypeHandle structType, std::string& outErrorMsg);

#ifdef SERIALIZATION_REFLECTION_ENABLED
template <typename t_object_type>
bool deserializeFromJsonString(const std::string& jsonString, t_object_type& instance, std::string& outErrorMsg)
{
	return deserializeFromJsonString(jsonString, &instance, &t_object_type::staticGetArchetype(), outErrorMsg);
}

template <typename t_object_type>
bool deserializeFromJson(const nlohmann::json& jsonObject, t_object_type& instance, std::string& outErrorMsg)
{
	return deserializeFromJson(jsonObject, &instance, &t_object_type::staticGetArchetype(), outErrorMsg);
}
#endif // SERIALIZATION_REFLECTION_ENABLED
}; // namespace Serialization

#pragma once

#include "SerializationExport.h"
#include "ReflectionHandles.h"
#include "SerializationVisitor.h"

#include <nlohmann/json_fwd.hpp>
#include <string>

namespace Serialization
{
SERIALIZATION_API bool serializeToJsonString(const void* instance, StructTypeHandle structType, std::string& jsonString,
											 std::string& outErrorMsg);

SERIALIZATION_API bool serializeToJson(const void* instance, StructTypeHandle structType, nlohmann::json& jsonObject,
									   std::string& outErrorMsg);

#ifdef SERIALIZATION_REFLECTION_ENABLED
template <typename t_object_type>
bool serializeToJsonString(const t_object_type& instance, std::string& jsonString, std::string& outErrorMsg)
{
	return serializeToJsonString(&instance, &t_object_type::staticGetArchetype(), jsonString, outErrorMsg);
}

template <typename t_object_type>
bool serializeToJson(const t_object_type& instance, nlohmann::json& jsonObject, std::string& outErrorMsg)
{
	return serializeToJson(&instance, &t_object_type::staticGetArchetype(), jsonObject, outErrorMsg);
}
#endif // SERIALIZATION_REFLECTION_ENABLED
}; // namespace Serialization

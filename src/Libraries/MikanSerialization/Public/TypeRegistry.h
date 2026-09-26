#pragma once

#include "SerializationExport.h"
#include "ReflectionHandles.h"
#include <string>

#ifdef SERIALIZATION_REFLECTION_ENABLED

namespace Serialization
{
class SERIALIZATION_API TypeRegistry
{
public:
	static void build();
	static StructTypeHandle getStructByName(const std::string& typeName);
};
} // namespace Serialization
#endif // SERIALIZATION_REFLECTION_ENABLED

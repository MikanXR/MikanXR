#include "TypeRegistry.h"

#ifdef SERIALIZATION_REFLECTION_ENABLED
#include "SerializationProperty.h"
#include <Refureku/Refureku.h>
#include <unordered_map>

namespace Serialization
{
static std::unordered_map<std::string, rfk::Struct const*> s_structs;

void TypeRegistry::build()
{
	s_structs.clear();

	rfk::getDatabase().foreachFileLevelStruct(
		[](rfk::Struct const& entity, void* userData) -> bool
		{
			if (entity.getProperty<Serialization::CodeGenModule>() != nullptr)
			{
				s_structs[entity.getName()]= &entity;
			}
			return true;
		},
		nullptr);
}

StructTypeHandle TypeRegistry::getStructByName(const std::string& typeName)
{
	auto it= s_structs.find(typeName);
	return (it != s_structs.end()) ? it->second : nullptr;
}
} // namespace Serialization
#endif // SERIALIZATION_REFLECTION_ENABLED

#include "SpoutOutputSystem.h"
#include "MikanObject.h"
#include "MikanPropertyDatabase.h"
#include "ProjectConfig.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

// -- SpoutOutputSystemDefinition -----
SpoutOutputSystemDefinition::SpoutOutputSystemDefinition(const std::string& configName,
														 IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

// -- SpoutOutputSystem -----
SpoutOutputSystem::SpoutOutputSystem(ProjectManagerPtr ownerObjectSystem)
	: Super::MikanTypedObjectSystem(ownerObjectSystem)
{
}

// -- Lua Binding ----
void SpoutOutputSystem::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.beginClass<SpoutOutputSystem>("SpoutOutputSystem")
		.addFunction("getOutputById", [](SpoutOutputSystem* s, int id) -> SpoutOutputComponent*
					 { return s->getOutputById(static_cast<MikanOutputID>(id)).get(); })
		.addFunction("getOutputByName", [](SpoutOutputSystem* s, const std::string& name) -> SpoutOutputComponent*
					 { return s->getOutputByName(name).get(); })
		.addFunction("getOutputCount",
					 [](SpoutOutputSystem* s) -> int { return static_cast<int>(s->getComponentMap().size()); })
		.addFunction("getOutputAtIndex",
					 [](SpoutOutputSystem* s, int i) -> SpoutOutputComponent*
					 {
						 int n= 0;
						 for (auto& [id, wp] : s->getComponentMap())
							 if (n++ == i)
								 return wp.lock().get();
						 return nullptr;
					 })
		.endClass();
}

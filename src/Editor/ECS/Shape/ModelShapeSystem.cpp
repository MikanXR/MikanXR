#include "ModelShapeSystem.h"
#include "MikanObject.h"
#include "MikanShapeTypes.h"
#include "SelectionComponent.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

#include <assert.h>

// -- ModelShapeSystemDefinition -----
ModelShapeSystemDefinition::ModelShapeSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

configuru::Config ModelShapeSystemDefinition::writeToJSON() { return Super::writeToJSON(); }

void ModelShapeSystemDefinition::readFromJSON(const configuru::Config& pt) { Super::readFromJSON(pt); }

// -- ModelShapeSystem ----
ModelShapeSystem::ModelShapeSystem(ProjectManagerPtr ownerObjectSystem)
	: Super::MikanTypedObjectSystem(ownerObjectSystem)
{
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle ModelShapeSystem::getClientAPIValuesStructType() const
{
	return &MikanModelShapeSystemValues::staticGetArchetype();
}

void ModelShapeSystem::getModelShapeComponentList(std::vector<ModelShapeComponentPtr>& outList) const
{
	outList.clear();
	for (const auto& pair : Super::getComponentMap())
	{
		ModelShapeComponentPtr comp= pair.second.lock();
		if (comp)
			outList.push_back(comp);
	}
}

void ModelShapeSystem::additionalComponentFactory(MikanObjectPtr ownerComponentObject,
												  ComponentDefinitionPtr componentDefinition)
{
	// SelectionComponent for picking in the editor
	ownerComponentObject->addComponent<SelectionComponent>();
	// Note: mesh colliders are created by ModelShapeComponent::rebuildMeshComponents()
}

// -- Lua Binding ----
void ModelShapeSystem::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.beginClass<ModelShapeSystem>("ModelShapeSystem")
		.addFunction("getModelShapeById", [](ModelShapeSystem* s, int id) -> ModelShapeComponent*
					 { return s->getModelShapeById(static_cast<MikanShapeID>(id)).get(); })
		.addFunction("getModelShapeByName", [](ModelShapeSystem* s, const std::string& name) -> ModelShapeComponent*
					 { return s->getModelShapeByName(name).get(); })
		.addFunction("getModelShapeCount",
					 [](ModelShapeSystem* s) -> int { return static_cast<int>(s->getComponentMap().size()); })
		.addFunction("getModelShapeAtIndex",
					 [](ModelShapeSystem* s, int i) -> ModelShapeComponent*
					 {
						 int n= 0;
						 for (auto& [id, wp] : s->getComponentMap())
							 if (n++ == i)
								 return wp.lock().get();
						 return nullptr;
					 })
		.endClass();
}

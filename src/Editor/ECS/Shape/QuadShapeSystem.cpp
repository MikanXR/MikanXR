#include "QuadShapeSystem.h"
#include "BoxColliderComponent.h"
#include "MikanObject.h"
#include "MikanShapeTypes.h"
#include "SelectionComponent.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

#include <assert.h>

// -- QuadShapeSystemDefinition -----
QuadShapeSystemDefinition::QuadShapeSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

configuru::Config QuadShapeSystemDefinition::writeToJSON() { return Super::writeToJSON(); }

void QuadShapeSystemDefinition::readFromJSON(const configuru::Config& pt) { Super::readFromJSON(pt); }

// -- QuadShapeSystem ----
QuadShapeSystem::QuadShapeSystem(ProjectManagerPtr ownerObjectSystem)
	: Super::MikanTypedObjectSystem(ownerObjectSystem)
{
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle QuadShapeSystem::getClientAPIValuesStructType() const
{
	return &MikanQuadShapeSystemValues::staticGetArchetype();
}

void QuadShapeSystem::getQuadShapeComponentList(std::vector<QuadShapeComponentPtr>& outList) const
{
	outList.clear();
	for (const auto& pair : Super::getComponentMap())
	{
		QuadShapeComponentPtr comp= pair.second.lock();
		if (comp)
			outList.push_back(comp);
	}
}

void QuadShapeSystem::additionalComponentFactory(MikanObjectPtr ownerComponentObject,
												 ComponentDefinitionPtr componentDefinition)
{
	TransformComponentPtr rootComponent= ownerComponentObject->getRootComponent();
	assert(rootComponent);

	QuadShapeDefinitionPtr quadDef= std::static_pointer_cast<QuadShapeDefinition>(componentDefinition);

	// Attach box collider sized to the quad
	BoxColliderComponentPtr boxCollider= ownerComponentObject->addComponent<BoxColliderComponent>();
	boxCollider->setHalfExtents(glm::vec3(quadDef->getQuadWidth() * 0.5f, quadDef->getQuadHeight() * 0.5f, 0.01f));
	boxCollider->attachToComponent(rootComponent);

	// Attach selection component
	ownerComponentObject->addComponent<SelectionComponent>();
}

// -- Lua Binding ----
void QuadShapeSystem::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.beginClass<QuadShapeSystem>("QuadShapeSystem")
		.addFunction("getQuadShapeById", [](QuadShapeSystem* s, int id) -> QuadShapeComponent*
					 { return s->getQuadShapeById(static_cast<MikanShapeID>(id)).get(); })
		.addFunction("getQuadShapeByName", [](QuadShapeSystem* s, const std::string& name) -> QuadShapeComponent*
					 { return s->getQuadShapeByName(name).get(); })
		.addFunction("getQuadShapeCount",
					 [](QuadShapeSystem* s) -> int { return static_cast<int>(s->getComponentMap().size()); })
		.addFunction("getQuadShapeAtIndex",
					 [](QuadShapeSystem* s, int i) -> QuadShapeComponent*
					 {
						 int n= 0;
						 for (auto& [id, wp] : s->getComponentMap())
							 if (n++ == i)
								 return wp.lock().get();
						 return nullptr;
					 })
		.endClass();
}

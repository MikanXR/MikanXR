#include "BoxShapeSystem.h"
#include "BoxColliderComponent.h"
#include "MikanObject.h"
#include "MikanShapeTypes.h"
#include "SelectionComponent.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

#include <assert.h>

// -- BoxShapeSystemDefinition -----
BoxShapeSystemDefinition::BoxShapeSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

configuru::Config BoxShapeSystemDefinition::writeToJSON() { return Super::writeToJSON(); }

void BoxShapeSystemDefinition::readFromJSON(const configuru::Config& pt) { Super::readFromJSON(pt); }

// -- BoxShapeSystem ----
BoxShapeSystem::BoxShapeSystem(ProjectManagerPtr ownerObjectSystem)
	: Super::MikanTypedObjectSystem(ownerObjectSystem)
{
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle BoxShapeSystem::getClientAPIValuesStructType() const
{
	return &MikanBoxShapeSystemValues::staticGetArchetype();
}

void BoxShapeSystem::getBoxShapeComponentList(std::vector<BoxShapeComponentPtr>& outList) const
{
	outList.clear();
	for (const auto& pair : Super::getComponentMap())
	{
		BoxShapeComponentPtr comp= pair.second.lock();
		if (comp)
			outList.push_back(comp);
	}
}

void BoxShapeSystem::additionalComponentFactory(MikanObjectPtr ownerComponentObject,
												ComponentDefinitionPtr componentDefinition)
{
	TransformComponentPtr rootComponent= ownerComponentObject->getRootComponent();
	assert(rootComponent);

	BoxShapeDefinitionPtr boxDef= std::static_pointer_cast<BoxShapeDefinition>(componentDefinition);

	// Attach box collider sized to the box shape
	BoxColliderComponentPtr boxCollider= ownerComponentObject->addComponent<BoxColliderComponent>();
	boxCollider->setHalfExtents(
		glm::vec3(boxDef->getBoxXSize() * 0.5f, boxDef->getBoxYSize() * 0.5f, boxDef->getBoxZSize() * 0.5f));
	boxCollider->attachToComponent(rootComponent);

	// Attach selection component
	ownerComponentObject->addComponent<SelectionComponent>();
}

// -- Lua Binding ----
void BoxShapeSystem::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.beginClass<BoxShapeSystem>("BoxShapeSystem")
		.addFunction("getBoxShapeById", [](BoxShapeSystem* s, int id) -> BoxShapeComponent*
					 { return s->getBoxShapeById(static_cast<MikanShapeID>(id)).get(); })
		.addFunction("getBoxShapeByName", [](BoxShapeSystem* s, const std::string& name) -> BoxShapeComponent*
					 { return s->getBoxShapeByName(name).get(); })
		.addFunction("getBoxShapeCount",
					 [](BoxShapeSystem* s) -> int { return static_cast<int>(s->getComponentMap().size()); })
		.addFunction("getBoxShapeAtIndex",
					 [](BoxShapeSystem* s, int i) -> BoxShapeComponent*
					 {
						 int n= 0;
						 for (auto& [id, wp] : s->getComponentMap())
							 if (n++ == i)
								 return wp.lock().get();
						 return nullptr;
					 })
		.endClass();
}

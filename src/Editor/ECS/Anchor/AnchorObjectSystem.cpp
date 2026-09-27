#include "App.h"
#include "AnchorObjectSystem.h"
#include "AnchorComponent.h"
#include "BoxColliderComponent.h"
#include "TransformComponent.h"
#include "MathTypeConversion.h"
#include "MikanObject.h"
#include "MikanAPITypes.h"
#include "MikanMathTypes.h"
#include "ProjectConfig.h"
#include "SelectionComponent.h"
#include "StringUtils.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

// -- AnchorObjectSystemDefinition -----
AnchorObjectSystemDefinition::AnchorObjectSystemDefinition(const std::string& configName,
														   IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

configuru::Config AnchorObjectSystemDefinition::writeToJSON()
{
	configuru::Config pt= Super::writeToJSON();

	return pt;
}

void AnchorObjectSystemDefinition::readFromJSON(const configuru::Config& pt) { Super::readFromJSON(pt); }

// -- AnchorObjectSystem -----
AnchorObjectSystem::AnchorObjectSystem(ProjectManagerPtr ownerObjectSystem)
	: Super::MikanTypedObjectSystem(ownerObjectSystem)
{
}

bool AnchorObjectSystem::getSpatialAnchorWorldTransform(MikanSpatialAnchorID anchorId, glm::mat4& outXform) const
{
	AnchorComponentPtr anchorPtr= getSpatialAnchorById(anchorId);
	if (anchorPtr)
	{
		outXform= anchorPtr->getWorldTransform();
		return true;
	}

	return false;
}

void AnchorObjectSystem::additionalComponentFactory(MikanObjectPtr ownerComponentObject,
													ComponentDefinitionPtr componentDefinition)
{
	TransformComponentPtr rootComponent= ownerComponentObject->getRootComponent();
	assert(rootComponent);

	// Add a selection component
	ownerComponentObject->addComponent<SelectionComponent>();

	// Attach a box collider to anchor component
	const float size= 0.1f;
	BoxColliderComponentPtr boxColliderPtr= ownerComponentObject->addComponent<BoxColliderComponent>();
	boxColliderPtr->setHalfExtents(glm::vec3(size * 0.5f, size * 0.5f, size * 0.5f));
	boxColliderPtr->setRelativeTransform(GlmTransform(glm::vec3(size * 0.5f, size * 0.5f, size * 0.5f)));
	boxColliderPtr->attachToComponent(rootComponent);
}

// -- IPropertyInterface ----
void AnchorObjectSystem::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	Super::getPropertyDescriptors(outDescriptors);
}

bool AnchorObjectSystem::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	return Super::getPropertyValue(propertyName, outValue);
}

bool AnchorObjectSystem::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	return MikanObjectSystem::setPropertyValue(propertyName, inValue);
}
// -- Lua Binding ----
void AnchorObjectSystem::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.beginClass<AnchorObjectSystem>("AnchorObjectSystem")
		.addFunction("getAnchorById", [](AnchorObjectSystem* s, int id) -> AnchorComponent*
					 { return s->getSpatialAnchorById(static_cast<MikanSpatialAnchorID>(id)).get(); })
		.addFunction("getAnchorByName", [](AnchorObjectSystem* s, const std::string& name) -> AnchorComponent*
					 { return s->getSpatialAnchorByName(name).get(); })
		.addFunction("getAnchorCount",
					 [](AnchorObjectSystem* s) -> int { return static_cast<int>(s->getComponentMap().size()); })
		.addFunction("getAnchorAtIndex",
					 [](AnchorObjectSystem* s, int i) -> AnchorComponent*
					 {
						 int n= 0;
						 for (auto& [id, wp] : s->getComponentMap())
							 if (n++ == i)
								 return wp.lock().get();
						 return nullptr;
					 })
		.endClass();
}

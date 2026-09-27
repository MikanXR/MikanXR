#include "QuadStencilSystem.h"
#include "QuadStencilComponent.h"
#include "BoxColliderComponent.h"
#include "MathTypeConversion.h"
#include "MathGLM.h"
#include "MikanObject.h"
#include "MikanPropertyDatabase.h"
#include "ProjectConfig.h"
#include "SelectionComponent.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

#include <assert.h>

// -- QuadStencilSystemDefinition -----
QuadStencilSystemDefinition::QuadStencilSystemDefinition(const std::string& configName,
														 IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

configuru::Config QuadStencilSystemDefinition::writeToJSON()
{
	configuru::Config pt= Super::writeToJSON();

	return pt;
}

void QuadStencilSystemDefinition::readFromJSON(const configuru::Config& pt) { Super::readFromJSON(pt); }

// -- QuadStencilSystem ----
QuadStencilSystem::QuadStencilSystem(ProjectManagerPtr ownerObjectSystem)
	: Super::MikanTypedObjectSystem(ownerObjectSystem)
{
}

void QuadStencilSystem::getRelevantQuadStencilList(const std::vector<MikanStencilID>* allowedStencilIds,
												   const glm::vec3& cameraPosition, const glm::vec3& cameraForward,
												   std::vector<QuadStencilComponentPtr>& outStencilList) const
{
	outStencilList.clear();
	for (const auto& stencilPair : Super::getComponentMap())
	{
		MikanStencilID stencilId= stencilPair.first;
		QuadStencilComponentPtr componentPtr= stencilPair.second.lock();

		if (componentPtr->getStencilComponentDefinition()->getIsDisabled())
			continue;

		// If there is an active allow list, make sure stencil is on it
		if (allowedStencilIds != nullptr)
		{
			if (std::find(allowedStencilIds->begin(), allowedStencilIds->end(), stencilId) == allowedStencilIds->end())
			{
				continue;
			}
		}

		if (!isStencilFacingCamera(componentPtr, cameraPosition, cameraForward))
			continue;

		{
			const glm::mat4 worldXform= componentPtr->getWorldTransform();
			const glm::vec3 stencilCenter= glm::vec3(worldXform[3]);  // position is 3rd column
			const glm::vec3 stencilForward= glm::vec3(worldXform[2]); // forward is 2nd column
			const glm::vec3 cameraToStencil= stencilCenter - cameraPosition;

			// Stencil is in front of the camera
			// Stencil is facing the camera (or double sided)
			if (glm::dot(cameraToStencil, cameraForward) > 0.f
				&& (componentPtr->getQuadStencilDefinition()->getIsDoubleSided()
					|| glm::dot(stencilForward, cameraForward) < 0.f))
			{
				outStencilList.push_back(componentPtr);
			}
		}
	}
}

void QuadStencilSystem::additionalComponentFactory(MikanObjectPtr ownerComponentObject,
												   ComponentDefinitionPtr componentDefinition)
{
	TransformComponentPtr rootComponent= ownerComponentObject->getRootComponent();
	assert(rootComponent);

	QuadStencilDefinitionPtr quadDefinition= std::static_pointer_cast<QuadStencilDefinition>(componentDefinition);

	// Attach a box collider to quad stencil component
	BoxColliderComponentPtr boxColliderPtr= ownerComponentObject->addComponent<BoxColliderComponent>();
	boxColliderPtr->setHalfExtents(
		glm::vec3(quadDefinition->getQuadWidth() * 0.5f, quadDefinition->getQuadHeight() * 0.5f, 0.01f));
	boxColliderPtr->attachToComponent(rootComponent);

	// Add a selection component
	ownerComponentObject->addComponent<SelectionComponent>();
}

bool QuadStencilSystem::isStencilFacingCamera(StencilComponentConstPtr stencil, const glm::vec3& cameraPosition,
											  const glm::vec3& cameraForward)
{
	StencilComponentConfigConstPtr configPtr= stencil->getStencilComponentDefinition();
	eStencilCullMode cullMode= configPtr->getCullMode();

	if (cullMode == eStencilCullMode::none)
		return true;

	glm::mat4 stencilXform= stencil->getWorldTransform();
	glm::vec3 stencilCenter= glm_mat4_get_position(stencilXform);
	glm::vec3 stencilForward;
	switch (cullMode)
	{
	case eStencilCullMode::zAxis:
		stencilForward= glm_mat4_get_z_axis(stencilXform);
		break;
	case eStencilCullMode::yAxis:
		stencilForward= glm_mat4_get_y_axis(stencilXform);
		break;
	case eStencilCullMode::xAxis:
		stencilForward= glm_mat4_get_x_axis(stencilXform);
		break;
	}

	const glm::vec3 cameraToStencil= stencilCenter - cameraPosition;
	const glm::vec3 stencilToCamera= -cameraToStencil;

	return glm::dot(cameraToStencil, cameraForward) > 0.f && glm::dot(stencilToCamera, stencilForward) > 0.f;
}

// -- IPropertyInterface ----
void QuadStencilSystem::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	Super::getPropertyDescriptors(outDescriptors);
}

bool QuadStencilSystem::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	return Super::getPropertyValue(propertyName, outValue);
}

bool QuadStencilSystem::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	return MikanObjectSystem::setPropertyValue(propertyName, inValue);
}

// -- Lua Binding ----
void QuadStencilSystem::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.beginClass<QuadStencilSystem>("QuadStencilSystem")
		.addFunction("getQuadStencilById", [](QuadStencilSystem* s, int id) -> QuadStencilComponent*
					 { return s->getQuadStencilById(static_cast<MikanStencilID>(id)).get(); })
		.addFunction("getQuadStencilByName", [](QuadStencilSystem* s, const std::string& name) -> QuadStencilComponent*
					 { return s->getQuadStencilByName(name).get(); })
		.addFunction("getQuadStencilCount",
					 [](QuadStencilSystem* s) -> int { return static_cast<int>(s->getComponentMap().size()); })
		.addFunction("getQuadStencilAtIndex",
					 [](QuadStencilSystem* s, int i) -> QuadStencilComponent*
					 {
						 int n= 0;
						 for (auto& [id, wp] : s->getComponentMap())
							 if (n++ == i)
								 return wp.lock().get();
						 return nullptr;
					 })
		.endClass();
}

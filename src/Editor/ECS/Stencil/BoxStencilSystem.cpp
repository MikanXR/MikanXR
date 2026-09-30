#include "BoxStencilSystem.h"
#include "BoxStencilComponent.h"
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

// -- BoxStencilSystemDefinition -----
BoxStencilSystemDefinition::BoxStencilSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

configuru::Config BoxStencilSystemDefinition::writeToJSON()
{
	configuru::Config pt= Super::writeToJSON();

	return pt;
}

void BoxStencilSystemDefinition::readFromJSON(const configuru::Config& pt) { Super::readFromJSON(pt); }

// -- BoxStencilSystem ----
BoxStencilSystem::BoxStencilSystem(ProjectManagerPtr ownerObjectSystem)
	: Super::MikanTypedObjectSystem(ownerObjectSystem)
{
}

void BoxStencilSystem::getRelevantBoxStencilList(const std::vector<MikanStencilID>* allowedStencilIds,
												 const glm::vec3& cameraPosition, const glm::vec3& cameraForward,
												 std::vector<BoxStencilComponentPtr>& outStencilList) const
{
	outStencilList.clear();
	for (const auto& stencilPair : Super::getComponentMap())
	{
		MikanStencilID stencilId= stencilPair.first;
		BoxStencilComponentPtr componentPtr= stencilPair.second.lock();

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
			const glm::vec3 stencilCenter= glm::vec3(worldXform[3]); // position is 3rd column
			const glm::vec3 stencilZAxis= glm::vec3(worldXform[2]);  // Z is 2nd column
			const glm::vec3 stencilYAxis= glm::vec3(worldXform[1]);  // Y is 1st column
			const glm::vec3 stencilXAxis= glm::vec3(worldXform[0]);  // X is 0th column
			BoxStencilDefinitionConstPtr configPtr= componentPtr->getBoxStencilDefinition();
			const float boxXSize= configPtr->getBoxXSize();
			const float boxYSize= configPtr->getBoxYSize();
			const float boxZSize= configPtr->getBoxZSize();
			const glm::vec3 cameraToStencil= stencilCenter - cameraPosition;

			const bool bIsStencilInFrontOfCamera= glm::dot(cameraToStencil, cameraForward) > 0.f;
			const bool bIsCameraInStecil= fabsf(glm::dot(cameraToStencil, stencilXAxis)) <= boxXSize
										  && fabsf(glm::dot(cameraToStencil, stencilYAxis)) <= boxYSize
										  && fabsf(glm::dot(cameraToStencil, stencilZAxis)) <= boxZSize;

			if (bIsStencilInFrontOfCamera || bIsCameraInStecil)
			{
				outStencilList.push_back(componentPtr);
			}
		}
	}
}

void BoxStencilSystem::additionalComponentFactory(MikanObjectPtr ownerComponentObject,
												  ComponentDefinitionPtr componentDefinition)
{
	TransformComponentPtr rootComponent= ownerComponentObject->getRootComponent();
	assert(rootComponent);

	BoxStencilDefinitionPtr boxDefinition= std::static_pointer_cast<BoxStencilDefinition>(componentDefinition);

	// Attach a box collider component to the stencil
	BoxColliderComponentPtr boxColliderPtr= ownerComponentObject->addComponent<BoxColliderComponent>();
	boxColliderPtr->setHalfExtents(glm::vec3(boxDefinition->getBoxXSize() * 0.5f, boxDefinition->getBoxYSize() * 0.5f,
											 boxDefinition->getBoxZSize() * 0.5f));
	boxColliderPtr->attachToComponent(rootComponent);

	// Add a selection component
	ownerComponentObject->addComponent<SelectionComponent>();
}

bool BoxStencilSystem::isStencilFacingCamera(StencilComponentConstPtr stencil, const glm::vec3& cameraPosition,
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
void BoxStencilSystem::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	Super::getPropertyDescriptors(outDescriptors);
}

bool BoxStencilSystem::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	return Super::getPropertyValue(propertyName, outValue);
}

bool BoxStencilSystem::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	return MikanObjectSystem::setPropertyValue(propertyName, inValue);
}

// -- Lua Binding ----
void BoxStencilSystem::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.beginClass<BoxStencilSystem>("BoxStencilSystem")
		.addFunction("getBoxStencilById", [](BoxStencilSystem* s, int id) -> BoxStencilComponent*
					 { return s->getBoxStencilById(static_cast<MikanStencilID>(id)).get(); })
		.addFunction("getBoxStencilByName", [](BoxStencilSystem* s, const std::string& name) -> BoxStencilComponent*
					 { return s->getBoxStencilByName(name).get(); })
		.addFunction("getBoxStencilCount",
					 [](BoxStencilSystem* s) -> int { return static_cast<int>(s->getComponentMap().size()); })
		.addFunction("getBoxStencilAtIndex",
					 [](BoxStencilSystem* s, int i) -> BoxStencilComponent*
					 {
						 int n= 0;
						 for (auto& [id, wp] : s->getComponentMap())
							 if (n++ == i)
								 return wp.lock().get();
						 return nullptr;
					 })
		.endClass();
}

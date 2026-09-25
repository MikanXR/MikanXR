#include "AnchorObjectSystem.h"
#include "AnchorComponent.h"
#include "StencilComponent.h"
#include "SceneActorParenting.h"
#include "TransformComponent.h"
#include "MikanObject.h"
#include "MikanStencilTypes.h"
#include "StringUtils.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

// -- StencilComponentConfig -----
const std::string StencilComponentDefinition::k_stencilDisabledPropertyId= "is_disabled";
const std::string StencilComponentDefinition::k_stencilCullModePropertyId= "cull_mode";

StencilComponentDefinition::StencilComponentDefinition()
	: TransformComponentDefinition()
	, m_bIsDisabled(false)
{
}

StencilComponentDefinition::StencilComponentDefinition(MikanStencilID stencilId, const std::string& componentName,
													   const MikanTransform& xform)
	: TransformComponentDefinition(stencilId, componentName, xform)
	, m_bIsDisabled(false)
{
}

configuru::Config StencilComponentDefinition::writeToJSON()
{
	configuru::Config pt= TransformComponentDefinition::writeToJSON();

	pt[k_stencilDisabledPropertyId]= m_bIsDisabled;
	pt[k_stencilCullModePropertyId]= k_stencilCullModeStrings[(int)m_cullMode];

	return pt;
}

void StencilComponentDefinition::readFromJSON(const configuru::Config& pt)
{
	TransformComponentDefinition::readFromJSON(pt);

	m_bIsDisabled= pt.get_or<bool>(k_stencilDisabledPropertyId, false);

	const std::string modeName= pt.get_or<std::string>(k_stencilCullModePropertyId, k_stencilCullModeStrings[0]);
	m_cullMode= StringUtils::FindEnumValue<eStencilCullMode>(modeName, k_stencilCullModeStrings);
}

bool StencilComponentDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
													const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!TransformComponentDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanStencilComponentValues>();
	if (componentValues)
	{
		m_bIsDisabled= componentValues->is_disabled;
		m_cullMode= (eStencilCullMode)componentValues->cull_mode;
	}

	return true;
}

void StencilComponentDefinition::setIsDisabled(bool flag)
{
	if (m_bIsDisabled != flag)
	{
		m_bIsDisabled= flag;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_stencilDisabledPropertyId));
	}
}

void StencilComponentDefinition::setCullMode(eStencilCullMode mode)
{
	if (m_cullMode != mode)
	{
		m_cullMode= mode;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_stencilCullModePropertyId));
	}
}

// -- StencilComponent -----
StencilComponent::StencilComponent(MikanObjectWeakPtr owner)
	: TransformComponent(owner)
{
}

// -- TransformComponent ----
bool StencilComponent::canAttachToParent(TransformComponentConstPtr newParentComponent) const
{
	return isValidSceneActorParent(newParentComponent);
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle StencilComponent::getClientAPIValuesStructType() const
{
	return &MikanStencilComponentValues::staticGetArchetype();
}

// -- IPropertyInterface ----
void StencilComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	TransformComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
		StencilComponentDefinition::k_stencilDisabledPropertyId, MikanVariantType::BOOL));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 StencilComponentDefinition::k_stencilCullModePropertyId, MikanVariantType::INT)
								 ->setDefaultValue((int)eStencilCullMode::none));
}

bool StencilComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == StencilComponentDefinition::k_stencilDisabledPropertyId)
	{
		outValue= getStencilComponentDefinition()->getIsDisabled();
		return true;
	}
	else if (propertyName == StencilComponentDefinition::k_stencilCullModePropertyId)
	{
		outValue= (int)getStencilComponentDefinition()->getCullMode();
		return true;
	}

	return TransformComponent::getPropertyValue(propertyName, outValue);
}

bool StencilComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == StencilComponentDefinition::k_stencilDisabledPropertyId)
	{
		bool bIsDisabled= inValue.getBoolValue();

		getStencilComponentDefinition()->setIsDisabled(bIsDisabled);
		return true;
	}
	else if (propertyName == StencilComponentDefinition::k_stencilCullModePropertyId)
	{
		eStencilCullMode cullMode= (eStencilCullMode)inValue.getIntValue();

		getStencilComponentDefinition()->setCullMode(cullMode);
		return true;
	}

	return TransformComponent::setPropertyValue(propertyName, inValue);
}

// -- Lua Binding ----
void StencilComponent::bindLuaFunctions(lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<StencilComponent, TransformComponent>("StencilComponent")
		.addProperty(
			"isDisabled", [](StencilComponent* component) -> bool
			{ return component->getStencilComponentDefinition()->getIsDisabled(); },
			[](StencilComponent* component, bool isDisabled)
			{ component->getStencilComponentDefinition()->setIsDisabled(isDisabled); })
		.addProperty(
			"cullMode", [](StencilComponent* component) -> eStencilCullMode
			{ return component->getStencilComponentDefinition()->getCullMode(); },
			[](StencilComponent* component, eStencilCullMode cullMode)
			{ component->getStencilComponentDefinition()->setCullMode(cullMode); })
		.endClass();
}
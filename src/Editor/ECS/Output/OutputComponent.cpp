#include "OutputComponent.h"
#include "MikanOutputTypes.h"
#include "MikanVariantTypes.h"
#include "PropertyInterface.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

// -- OutputDefinition -----
const std::string OutputDefinition::k_enabledPropertyId= "enabled";

OutputDefinition::OutputDefinition()
	: MikanComponentDefinition()
{
}

OutputDefinition::OutputDefinition(MikanOutputID outputId)
	: MikanComponentDefinition(outputId, "")
{
}

configuru::Config OutputDefinition::writeToJSON()
{
	configuru::Config pt= MikanComponentDefinition::writeToJSON();

	pt[k_enabledPropertyId]= m_bIsEnabled;

	return pt;
}

void OutputDefinition::readFromJSON(const configuru::Config& pt)
{
	MikanComponentDefinition::readFromJSON(pt);

	m_bIsEnabled= pt.get_or<bool>(k_enabledPropertyId, m_bIsEnabled);
}

bool OutputDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
										  const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!MikanComponentDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanOutputComponentValues>();
	if (componentValues)
	{
		m_bIsEnabled= componentValues->enabled;
	}

	return true;
}

void OutputDefinition::setIsEnabled(bool bIsEnabled)
{
	if (m_bIsEnabled != bIsEnabled)
	{
		m_bIsEnabled= bIsEnabled;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_enabledPropertyId));
	}
}

// -- OutputComponent -----
OutputComponent::OutputComponent(MikanObjectWeakPtr owner)
	: MikanComponent(owner)
{
}

void OutputComponent::dispose()
{
	closeOutput();

	MikanComponent::dispose();
}

void OutputComponent::onDefinitionMarkedDirty(CommonConfigPtr configPtr,
											  const ConfigPropertyChangeSet& changedPropertySet)
{
	MikanComponent::onDefinitionMarkedDirty(configPtr, changedPropertySet);

	// A disabled output lets go at once rather than on the frame that skips it
	if (changedPropertySet.hasPropertyName(OutputDefinition::k_enabledPropertyId)
		&& !getOutputDefinition()->getIsEnabled())
	{
		closeOutput();
	}
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle OutputComponent::getClientAPIValuesStructType() const
{
	return &MikanOutputComponentValues::staticGetArchetype();
}

// -- IPropertyInterface ----
void OutputComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(OutputDefinition::k_enabledPropertyId, MikanVariantType::BOOL)
			->setDefaultValue(true));
}

bool OutputComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == OutputDefinition::k_enabledPropertyId)
	{
		outValue= getOutputDefinition()->getIsEnabled();
		return true;
	}

	return MikanComponent::getPropertyValue(propertyName, outValue);
}

bool OutputComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == OutputDefinition::k_enabledPropertyId)
	{
		getOutputDefinition()->setIsEnabled(inValue.getBoolValue());
		return true;
	}

	return MikanComponent::setPropertyValue(propertyName, inValue);
}

// -- Lua Binding ----
void OutputComponent::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<OutputComponent, MikanComponent>(OutputComponent::k_componentClassName.c_str())
		.addProperty(
			"isEnabled", [](OutputComponent* c) -> bool { return c->getOutputDefinition()->getIsEnabled(); },
			[](OutputComponent* c, bool v) { c->getOutputDefinition()->setIsEnabled(v); })
		.endClass();
}

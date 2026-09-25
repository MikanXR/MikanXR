#include "DMXPresetComponent.h"
#include "DMXFixtureComponent.h"
#include "DMXFixtureGroupComponent.h"
#include "DMXFixtureGroupSystem.h"
#include "FunctionInterface.h"
#include "Logger.h"
#include "MikanObjectSystem.h"
#include "MikanVariantTypes.h"

#include <algorithm>
#include <stdexcept>

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

namespace
{
const char* k_fixturesKey= "fixtures";
const char* k_fixtureIdKey= "fixture_id";
const char* k_channelsKey= "channels";

configuru::FormatOptions makeSingleLineJsonFormat()
{
	configuru::FormatOptions options= configuru::make_json_options();
	options.indentation= "";
	return options;
}
} // namespace

// -- DMXPresetDefinition -----
const std::string DMXPresetDefinition::k_groupIdPropertyId= "group_id";
const std::string DMXPresetDefinition::k_presetDataPropertyId= "preset_data";

DMXPresetDefinition::DMXPresetDefinition()
	: MikanComponentDefinition()
{
}

DMXPresetDefinition::DMXPresetDefinition(MikanDMXPresetID presetId)
	: MikanComponentDefinition(presetId, "")
{
}

configuru::Config DMXPresetDefinition::writeToJSON()
{
	configuru::Config pt= MikanComponentDefinition::writeToJSON();

	pt[k_groupIdPropertyId]= m_groupId;
	pt[k_fixturesKey]= writeFixtureValuesToJSON()[k_fixturesKey];

	return pt;
}

void DMXPresetDefinition::readFromJSON(const configuru::Config& pt)
{
	MikanComponentDefinition::readFromJSON(pt);

	m_groupId= pt.get_or<MikanDMXFixtureGroupID>(k_groupIdPropertyId, m_groupId);
	readFixtureValuesFromJSON(pt, m_fixtureValues);
}

bool DMXPresetDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
											 const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!MikanComponentDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* values= initParams.getTypedPointer<MikanDMXPresetComponentValues>();
	if (values)
	{
		m_groupId= values->group_id;

		// The flat wire form: one slice per fixture id, channel_counts long
		m_fixtureValues.clear();
		size_t offset= 0;
		for (size_t i= 0; i < values->fixture_ids.size() && i < values->channel_counts.size(); ++i)
		{
			const size_t count= values->channel_counts[i] > 0 ? static_cast<size_t>(values->channel_counts[i]) : 0;
			const size_t end= std::min(offset + count, static_cast<size_t>(values->channel_data.size()));
			m_fixtureValues[values->fixture_ids[i]]=
				std::vector<uint8_t>(values->channel_data.data() + offset, values->channel_data.data() + end);
			offset= end;
		}
	}

	if (m_groupId == INVALID_MIKAN_ID)
	{
		// If no group was specified, use the first one
		auto groupSystem= ownerObjectSystem->getObjectSystemOfType<DMXFixtureGroupSystem>();

		m_groupId= groupSystem->getFirstComponentId();
	}

	return true;
}

void DMXPresetDefinition::setGroupId(MikanDMXFixtureGroupID groupId)
{
	if (groupId != m_groupId)
	{
		m_groupId= groupId;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_groupIdPropertyId));
	}
}

bool DMXPresetDefinition::hasFixtureValues(MikanLightID fixtureId) const
{
	return m_fixtureValues.find(fixtureId) != m_fixtureValues.end();
}

bool DMXPresetDefinition::getFixtureValues(MikanLightID fixtureId, std::vector<uint8_t>& outValues) const
{
	auto it= m_fixtureValues.find(fixtureId);
	if (it == m_fixtureValues.end())
		return false;

	outValues= it->second;
	return true;
}

void DMXPresetDefinition::setFixtureValues(MikanLightID fixtureId, const std::vector<uint8_t>& values)
{
	auto it= m_fixtureValues.find(fixtureId);
	if (it != m_fixtureValues.end() && it->second == values)
		return;

	m_fixtureValues[fixtureId]= values;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_presetDataPropertyId));
}

void DMXPresetDefinition::setFixtureChannelRange(MikanLightID fixtureId, size_t offset,
												 const std::vector<uint8_t>& bytes)
{
	std::vector<uint8_t> values;
	getFixtureValues(fixtureId, values);
	if (values.size() < offset + bytes.size())
		values.resize(offset + bytes.size(), 0);
	std::copy(bytes.begin(), bytes.end(), values.begin() + offset);

	setFixtureValues(fixtureId, values);
}

bool DMXPresetDefinition::removeFixtureValues(MikanLightID fixtureId)
{
	if (m_fixtureValues.erase(fixtureId) == 0)
		return false;

	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_presetDataPropertyId));
	return true;
}

void DMXPresetDefinition::setAllFixtureValues(const FixtureValueMap& values)
{
	if (values == m_fixtureValues)
		return;

	m_fixtureValues= values;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_presetDataPropertyId));
}

std::string DMXPresetDefinition::fixtureValuesToJsonString() const
{
	return configuru::dump_string(writeFixtureValuesToJSON(), makeSingleLineJsonFormat());
}

bool DMXPresetDefinition::fixtureValuesFromJsonString(const std::string& jsonText)
{
	try
	{
		configuru::Config pt= configuru::parse_string(jsonText.c_str(), configuru::JSON, "DMXPresetDefinition");
		FixtureValueMap values;
		if (!readFixtureValuesFromJSON(pt, values))
			return false;

		setAllFixtureValues(values);
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

configuru::Config DMXPresetDefinition::writeFixtureValuesToJSON() const
{
	configuru::Config fixtures= configuru::Config::array();
	for (const auto& [fixtureId, values] : m_fixtureValues)
	{
		configuru::Config channels= configuru::Config::array();
		for (const uint8_t value : values)
		{
			channels.push_back(static_cast<int>(value));
		}

		configuru::Config entry= configuru::Config::object();
		entry[k_fixtureIdKey]= fixtureId;
		entry[k_channelsKey]= channels;
		fixtures.push_back(entry);
	}

	configuru::Config pt= configuru::Config::object();
	pt[k_fixturesKey]= fixtures;
	return pt;
}

bool DMXPresetDefinition::readFixtureValuesFromJSON(const configuru::Config& pt, FixtureValueMap& outValues)
{
	outValues.clear();

	if (!pt.is_object() || !pt.has_key(k_fixturesKey) || !pt[k_fixturesKey].is_array())
		return false;

	for (const configuru::Config& entry : pt[k_fixturesKey].as_array())
	{
		if (!entry.is_object() || !entry.has_key(k_fixtureIdKey) || !entry[k_fixtureIdKey].is_int()
			|| !entry.has_key(k_channelsKey) || !entry[k_channelsKey].is_array())
		{
			MIKAN_LOG_WARNING("DMXPresetDefinition::readFromJSON") << "Skipping malformed preset fixture entry";
			continue;
		}

		std::vector<uint8_t> values;
		for (const configuru::Config& channel : entry[k_channelsKey].as_array())
		{
			const int channelValue= channel.is_number() ? static_cast<int>(channel.as_float()) : 0;
			values.push_back(static_cast<uint8_t>(std::clamp(channelValue, 0, 255)));
		}
		outValues[entry[k_fixtureIdKey].as_integer<MikanLightID>()]= values;
	}

	return true;
}

// -- DMXPresetComponent -----
const std::string DMXPresetComponent::k_fixtureIdsPropertyId= "fixture_ids";
const std::string DMXPresetComponent::k_channelCountsPropertyId= "channel_counts";
const std::string DMXPresetComponent::k_channelDataPropertyId= "channel_data";
const std::string DMXPresetComponent::k_captureFunctionId= "capture_preset";
const std::string DMXPresetComponent::k_applyFunctionId= "apply_preset";

DMXPresetComponent::DMXPresetComponent(MikanObjectWeakPtr owner)
	: MikanComponent(owner)
{
}

DMXFixtureGroupComponentPtr DMXPresetComponent::getGroup() const
{
	// A component built without an owner (the unit tests) has no systems to ask
	if (!getOwnerObject())
		return nullptr;

	DMXFixtureGroupSystemPtr groupSystem= getObjectSystemOfType<DMXFixtureGroupSystem>();
	if (!groupSystem)
		return nullptr;

	return groupSystem->getGroupById(getDMXPresetDefinition()->getGroupId());
}

void DMXPresetComponent::capture()
{
	DMXFixtureGroupComponentPtr group= getGroup();
	if (!group)
		return;

	DMXPresetDefinition::FixtureValueMap values;
	for (const MikanLightID fixtureId : group->getDMXFixtureGroupDefinition()->getFixtureIds())
	{
		if (DMXFixtureComponentPtr fixture= group->resolveFixture(fixtureId))
		{
			fixture->getChannelValues(values[fixtureId]);
		}
	}

	getDMXPresetDefinition()->setAllFixtureValues(values);
}

void DMXPresetComponent::apply()
{
	DMXFixtureGroupComponentPtr group= getGroup();
	if (!group)
		return;

	DMXPresetDefinitionPtr definition= getDMXPresetDefinition();
	for (const MikanLightID fixtureId : group->getDMXFixtureGroupDefinition()->getFixtureIds())
	{
		DMXFixtureComponentPtr fixture= group->resolveFixture(fixtureId);
		if (!fixture)
			continue;

		// A member the preset holds nothing for lands on zeros, so the group
		// always ends in a fully defined state
		std::vector<uint8_t> values;
		definition->getFixtureValues(fixtureId, values);
		values.resize(fixture->getDMXFixtureDefinition()->getDMXChannelCount(), 0);
		fixture->setChannelValues(values);
	}
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle DMXPresetComponent::getClientAPIValuesStructType() const
{
	return &MikanDMXPresetComponentValues::staticGetArchetype();
}

// -- IPropertyInterface ----
void DMXPresetComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(DMXPresetDefinition::k_groupIdPropertyId, MikanVariantType::INT));
	// The table as JSON: the panel draws the values itself, and the string
	// form is what undo re-applies verbatim
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(DMXPresetDefinition::k_presetDataPropertyId, MikanVariantType::STRING)
			->setUIHidden()
			->setClientAPIHidden());
	// Read-only flat views for clients: one slice per fixture id, channel_counts long
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(k_fixtureIdsPropertyId, MikanVariantType::INT_ARRAY)
								 ->setReadOnly()
								 ->setUIHidden());
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(k_channelCountsPropertyId, MikanVariantType::INT_ARRAY)
			->setReadOnly()
			->setUIHidden());
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(k_channelDataPropertyId, MikanVariantType::UBYTE_ARRAY)
			->setReadOnly()
			->setUIHidden());
}

bool DMXPresetComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	DMXPresetDefinitionPtr definition= getDMXPresetDefinition();

	if (propertyName == DMXPresetDefinition::k_groupIdPropertyId)
	{
		outValue= static_cast<int>(definition->getGroupId());
		return true;
	}
	else if (propertyName == DMXPresetDefinition::k_presetDataPropertyId)
	{
		outValue= definition->fixtureValuesToJsonString();
		return true;
	}
	else if (propertyName == k_fixtureIdsPropertyId)
	{
		std::vector<int> fixtureIds;
		for (const auto& [fixtureId, values] : definition->getFixtureValues())
			fixtureIds.push_back(fixtureId);
		outValue= fixtureIds;
		return true;
	}
	else if (propertyName == k_channelCountsPropertyId)
	{
		std::vector<int> channelCounts;
		for (const auto& [fixtureId, values] : definition->getFixtureValues())
			channelCounts.push_back(static_cast<int>(values.size()));
		outValue= channelCounts;
		return true;
	}
	else if (propertyName == k_channelDataPropertyId)
	{
		std::vector<uint8_t> channelData;
		for (const auto& [fixtureId, values] : definition->getFixtureValues())
			channelData.insert(channelData.end(), values.begin(), values.end());
		outValue= channelData;
		return true;
	}

	return MikanComponent::getPropertyValue(propertyName, outValue);
}

bool DMXPresetComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	DMXPresetDefinitionPtr definition= getDMXPresetDefinition();

	if (propertyName == DMXPresetDefinition::k_groupIdPropertyId)
	{
		if (inValue.value_type != MikanVariantType::INT)
			return false;

		definition->setGroupId(inValue.getIntValue());
		return true;
	}
	else if (propertyName == DMXPresetDefinition::k_presetDataPropertyId)
	{
		if (inValue.value_type != MikanVariantType::STRING)
			return false;

		// Malformed text is rejected without touching the table
		return definition->fixtureValuesFromJsonString(inValue.getUtf8Value());
	}

	return MikanComponent::setPropertyValue(propertyName, inValue);
}

// -- IFunctionInterface ----
void DMXPresetComponent::getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getFunctionDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<FunctionDescriptor>(k_captureFunctionId, "Capture From Live"));
	outDescriptors.push_back(std::make_shared<FunctionDescriptor>(k_applyFunctionId, "Apply"));
}

bool DMXPresetComponent::invokeFunction(const std::string& functionName)
{
	if (functionName == k_captureFunctionId)
	{
		capture();
		return true;
	}
	else if (functionName == k_applyFunctionId)
	{
		apply();
		return true;
	}

	return MikanComponent::invokeFunction(functionName);
}

// -- Lua Binding ----
void DMXPresetComponent::bindLuaFunctions(lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<DMXPresetComponent, MikanComponent>(DMXPresetComponent::k_componentClassName.c_str())
		.addProperty("groupId", [](DMXPresetComponent* c) -> int { return c->getDMXPresetDefinition()->getGroupId(); })
		.addFunction("apply", [](DMXPresetComponent* c) { c->apply(); })
		.addFunction("capture", [](DMXPresetComponent* c) { c->capture(); })
		.endClass();
}

#pragma once

#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "DMXFixtureComponent.h"
#include "LightSystemFwd.h"
#include "MikanComponent.h"
#include "MikanLightTypes.h"
#include "MikanTypeFwd.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// -- DMXPresetDefinition -----
// A fixed set of DMX channel bytes for the fixtures of one group: captured
// from the live fixtures or edited in the panel, and applied on demand.
// Values are keyed by fixture component id, one byte per DMX channel.
class DMXPresetDefinition : public MikanComponentDefinition
{
public:
	using FixtureValueMap= DMXFixtureValueMap;

	DMXPresetDefinition();
	DMXPresetDefinition(MikanDMXPresetID presetId);

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	inline MikanDMXPresetID getPresetId() const { return getComponentId(); }

	static const std::string k_groupIdPropertyId;
	inline MikanDMXFixtureGroupID getGroupId() const { return m_groupId; }
	void setGroupId(MikanDMXFixtureGroupID groupId);

	// Every value mutation notifies this one name, so the whole table is the
	// unit the transaction recorder captures and re-applies
	static const std::string k_presetDataPropertyId;
	inline const FixtureValueMap& getFixtureValues() const { return m_fixtureValues; }
	bool hasFixtureValues(MikanLightID fixtureId) const;
	bool getFixtureValues(MikanLightID fixtureId, std::vector<uint8_t>& outValues) const;
	void setFixtureValues(MikanLightID fixtureId, const std::vector<uint8_t>& values);
	// Write bytes at an offset inside a fixture's slice, growing it with zeros
	void setFixtureChannelRange(MikanLightID fixtureId, size_t offset, const std::vector<uint8_t>& bytes);
	bool removeFixtureValues(MikanLightID fixtureId);
	// Replace the whole table with one notification
	void setAllFixtureValues(const FixtureValueMap& values);

	// The table as a single-line JSON object, the text the preset_data
	// property carries; false and untouched when the text does not parse
	std::string fixtureValuesToJsonString() const;
	bool fixtureValuesFromJsonString(const std::string& jsonText);

private:
	configuru::Config writeFixtureValuesToJSON() const;
	static bool readFixtureValuesFromJSON(const configuru::Config& pt, FixtureValueMap& outValues);

	MikanDMXFixtureGroupID m_groupId= INVALID_MIKAN_ID;
	FixtureValueMap m_fixtureValues;
};

// -- DMXPresetComponent -----
class DMXPresetComponent : public MikanComponent
{
public:
	DMXPresetComponent(MikanObjectWeakPtr owner);

	inline static const std::string k_componentClassName= "DMXPresetComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	inline DMXPresetDefinitionPtr getDMXPresetDefinition() const
	{
		return std::static_pointer_cast<DMXPresetDefinition>(m_definition);
	}

	// The group this preset addresses, null when the id resolves to nothing
	DMXFixtureGroupComponentPtr getGroup() const;
	// Snapshot every member fixture's live channel bytes into the preset (recorded)
	void capture();
	// Write the preset to every member fixture, zeros where the preset holds
	// nothing for a member (runtime only, not recorded)
	void apply();

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static const std::string k_fixtureIdsPropertyId;
	static const std::string k_channelCountsPropertyId;
	static const std::string k_channelDataPropertyId;
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static const std::string k_captureFunctionId;
	static const std::string k_applyFunctionId;
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors);
	virtual bool invokeFunction(const std::string& functionName) override;

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);
};

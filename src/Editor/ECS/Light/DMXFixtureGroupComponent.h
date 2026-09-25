#pragma once

#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "LightSystemFwd.h"
#include "MikanComponent.h"
#include "MikanLightTypes.h"
#include "MikanTypeFwd.h"

#include <string>
#include <vector>

// -- DMXFixtureGroupDefinition -----
// A named set of DMX fixtures on one stage: the unit a preset or a sequence
// addresses. Membership is a plain set of fixture component ids, so a fixture
// may belong to any number of groups.
class DMXFixtureGroupDefinition : public MikanComponentDefinition
{
public:
	DMXFixtureGroupDefinition();
	DMXFixtureGroupDefinition(MikanDMXFixtureGroupID groupId);

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	inline MikanDMXFixtureGroupID getGroupId() const { return getComponentId(); }

	static const std::string k_ownerStageIdPropertyId;
	inline MikanStageID getOwnerStageId() const { return m_stageId; }
	void setOwnerStageId(MikanStageID stageId);

	static const std::string k_fixtureIdsPropertyId;
	inline const std::vector<MikanLightID>& getFixtureIds() const { return m_fixtureIds; }
	bool containsFixture(MikanLightID fixtureId) const;
	// Set semantics: false when the id is already present
	bool addFixture(MikanLightID fixtureId);
	// False when the id is absent
	bool removeFixture(MikanLightID fixtureId);
	// Replace the whole list with one notification
	void setFixtureIds(const std::vector<MikanLightID>& fixtureIds);

private:
	MikanStageID m_stageId= INVALID_MIKAN_ID;
	std::vector<MikanLightID> m_fixtureIds;
};

// -- DMXFixtureGroupComponent -----
class DMXFixtureGroupComponent : public MikanComponent
{
public:
	DMXFixtureGroupComponent(MikanObjectWeakPtr owner);

	inline static const std::string k_componentClassName= "DMXFixtureGroupComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	inline DMXFixtureGroupDefinitionPtr getDMXFixtureGroupDefinition() const
	{
		return std::static_pointer_cast<DMXFixtureGroupDefinition>(m_definition);
	}

	// A member id resolved against the spot light and pixel grid systems, null
	// when neither holds it
	DMXFixtureComponentPtr resolveFixture(MikanLightID fixtureId) const;
	// The members that resolve, in list order
	void getFixtures(std::vector<DMXFixtureComponentPtr>& outFixtures) const;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
	{
		MikanComponent::getFunctionDescriptors(outDescriptors);
	}

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);
};

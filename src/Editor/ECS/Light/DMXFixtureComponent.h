#pragma once

#include "CommonConfig.h"
#include "LightSystemFwd.h"
#include "MikanLightTypes.h"
#include "ProjectConfigConstants.h"
#include "SceneFwd.h"
#include "TransformComponent.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class IDMXManager;

// Channel bytes keyed by fixture component id, the unit presets store and
// sequences fill each frame
using DMXFixtureValueMap= std::map<MikanLightID, std::vector<uint8_t>>;

// -- DMXFixtureComponentDefinition -----
class DMXFixtureComponentDefinition : public TransformComponentDefinition
{
public:
	DMXFixtureComponentDefinition();
	DMXFixtureComponentDefinition(MikanLightID lightId);

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	static const std::string k_ownerStageIdPropertyId;
	inline MikanStageID getOwnerStageId() const { return m_stageId; }
	void setOwnerStageId(MikanStageID stageId);

	// -- DMX universe / channel address --

	static const std::string k_dmxUniversePropertyId;
	uint16_t getDMXUniverse() const { return m_dmxUniverse; }
	void setDMXUniverse(uint16_t universe);

	static const std::string k_dmxStartChannelPropertyId;
	uint16_t getDMXStartChannel() const { return m_dmxStartChannel; }
	void setDMXStartChannel(uint16_t startChannel);

	static const std::string k_dmxChannelCountPropertyId;
	uint16_t getDMXChannelCount() const { return m_dmxChannelCount; }

	static const std::string k_isDisabledPropertyId;
	bool getIsDisabled() const { return m_bIsDisabled; }
	void setIsDisabled(bool flag);

	// -- Emitter spec --
	// What the fixture physically is, rather than what it is currently doing. A client turns
	// a channel byte into a renderable light with these, so they belong to the project
	// alongside the DMX address rather than being guessed per integration.

	static const std::string k_maxWattagePropertyId;
	float getMaxWattage() const { return m_maxWattage; }
	void setMaxWattage(float watts);

	static const std::string k_lumensPerWattPropertyId;
	float getLumensPerWatt() const { return m_lumensPerWatt; }
	void setLumensPerWatt(float lumensPerWatt);

protected:
	void setDMXChannelCount(uint16_t count);

	MikanStageID m_stageId= INVALID_MIKAN_ID;
	uint16_t m_dmxUniverse= 1;
	uint16_t m_dmxStartChannel= 1;
	uint16_t m_dmxChannelCount= 3;
	bool m_bIsDisabled= false;
	float m_maxWattage= 0.3f;
	float m_lumensPerWatt= 83.f;
};

// -- DMXFixtureComponent -----
class DMXFixtureComponent : public TransformComponent
{
public:
	DMXFixtureComponent(MikanObjectWeakPtr owner);

	inline static const std::string k_componentClassName= "DMXFixtureComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	inline DMXFixtureComponentDefinitionPtr getDMXFixtureDefinition() const
	{
		return std::static_pointer_cast<DMXFixtureComponentDefinition>(m_definition);
	}
	inline DMXObjectSystemPtr getDMXObjectSystem() const { return m_dmxObjectSystem.lock(); }

	// -- TransformComponent ----
	virtual bool canAttachToParent(TransformComponentConstPtr newParentComponent) const override;

	StageComponentConstPtr getOwnerStageComponent() const;
	eTrackingVolumeType getTrackingVolumeType() const;

	virtual void init() override;

	// -- Channel data --
	// The fixture's current DMX channel bytes, getDMXChannelCount() long
	virtual void getChannelValues(std::vector<uint8_t>& outValues) const {}
	// Write the fixture's channel bytes (padded with zeros or truncated to the
	// channel count), update its visuals, and send them over DMX
	virtual void setChannelValues(const std::vector<uint8_t>& values) {}
	// Push the fixture's current channel bytes to the DMX system
	virtual void sendDMXData() const {}

	// -- Lua Binding --
	static void bindLuaFunctions(struct lua_State* L);

protected:
	/// Cached pointer to the DMXObjectSystem. Set during init().
	DMXObjectSystemWeakPtr m_dmxObjectSystem;

	// -- IEntityAccessor --
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface --
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static const std::string k_triangulateLightFunctionId;
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors);
	virtual bool invokeFunction(const std::string& functionName) override;

	void triangulateLight();
};

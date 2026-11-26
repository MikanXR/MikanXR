#pragma once

#include "OutputComponent.h"
#include "SharedTextureFwd.h"

class SpoutOutputDefinition : public OutputDefinition
{
public:
	SpoutOutputDefinition();
	SpoutOutputDefinition(MikanOutputID outputId);

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	// The name receivers find the sender by
	static const std::string k_spoutOutputNamePropertyId;
	inline const std::string& getSpoutOutputName() const { return m_spoutOutputName; }
	void setSpoutOutputName(const std::string& spoutOutputName);

private:
	std::string m_spoutOutputName;
};

// Publishes the composited frame as a Spout sender. The sender opens on the first
// frame, survives scene switches, and reopens when the frame size or its name changes.
class SpoutOutputComponent : public OutputComponent
{
public:
	SpoutOutputComponent(MikanObjectWeakPtr owner);

	inline SpoutOutputDefinitionPtr getSpoutOutputDefinition() const
	{
		return std::static_pointer_cast<SpoutOutputDefinition>(m_definition);
	}

	inline static const std::string k_componentClassName= "SpoutOutputComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	// -- Output interface ----
	virtual void writeFrame(const OutputFrame& frame) override;
	virtual void closeOutput() override;
	bool getIsSenderOpen() const;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

protected:
	virtual void onDefinitionMarkedDirty(CommonConfigPtr configPtr,
										 const ConfigPropertyChangeSet& changedPropertySet) override;

private:
	bool openSender(int width, int height);

	ISharedTextureWriteAccessorPtr m_writeAccessor;
	int m_senderWidth= 0;
	int m_senderHeight= 0;
};

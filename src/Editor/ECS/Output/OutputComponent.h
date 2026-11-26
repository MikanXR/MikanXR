#pragma once

#include "ComponentFwd.h"
#include "MikanComponent.h"
#include "MikanTypeFwd.h"
#include "MkRendererFwd.h"

#include <string>

// One composited frame on its way to the project outputs
struct OutputFrame
{
	IMkTexturePtr texture;
	// The rate frames arrive at, which an encoder's rate control needs up front
	float frameRate= 30.f;
	// The compositor that composited it, the one publishing right now
	MikanCompositorID compositorId= INVALID_MIKAN_ID;
};

// The persisted state every project output shares
class OutputDefinition : public MikanComponentDefinition
{
public:
	OutputDefinition();
	OutputDefinition(MikanOutputID outputId);

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	inline MikanOutputID getOutputId() const { return getComponentId(); }

	// Whether the output takes frames. A disabled output holds no sender.
	static const std::string k_enabledPropertyId;
	inline bool getIsEnabled() const { return m_bIsEnabled; }
	void setIsEnabled(bool bIsEnabled);

private:
	bool m_bIsEnabled= true;
};

// A place the project's composited frame goes: a Spout sender, an RTP stream.
// Outputs belong to the project rather than to a scene. The current scene's
// compositor hands each frame it composites to every enabled output, so a scene
// switch changes the picture an output carries and not the output itself.
class OutputComponent : public MikanComponent
{
public:
	OutputComponent(MikanObjectWeakPtr owner);

	inline OutputDefinitionPtr getOutputDefinition() const
	{
		return std::static_pointer_cast<OutputDefinition>(getDefinition());
	}

	inline static const std::string k_componentClassName= "OutputComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	virtual void dispose() override;

	// -- Output interface ----
	// Takes one composited frame. Called once per composited frame while a
	// compositor publishes, for an enabled output only.
	virtual void writeFrame(const OutputFrame& frame) {}
	// Releases whatever the output holds open. Called when the output is disabled
	// or disposed and when no compositor publishes any more. The next frame reopens it.
	virtual void closeOutput() {}

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
};

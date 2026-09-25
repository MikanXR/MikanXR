#pragma once

#include "TextureSourceComponent.h"

using SharedTextureReadAccessorPtr= std::shared_ptr<class SharedTextureReadAccessor>;

class SpoutTextureSourceDefinition : public TextureSourceDefinition
{
public:
	SpoutTextureSourceDefinition();
	SpoutTextureSourceDefinition(MikanTextureSourceID TextureSourceId);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	static const std::string k_spoutSourcePropertyId;
	inline const std::string& getSpoutSource() const { return m_spoutSource; }
	void setSpoutSource(const std::string& spoutSource);

private:
	std::string m_spoutSource;
};

class SpoutTextureSourceComponent : public TextureSourceComponent
{
public:
	SpoutTextureSourceComponent(MikanObjectWeakPtr owner);

	inline SpoutTextureSourceDefinitionPtr getSpoutTextureSourceDefinition() const
	{
		return std::static_pointer_cast<SpoutTextureSourceDefinition>(m_definition);
	}
	virtual void setDefinition(MikanComponentDefinitionPtr definition) override;
	virtual void update(float deltaSeconds) override;
	virtual void dispose() override;

	inline static const std::string k_componentClassName= "SpoutTextureSourceComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }
	const std::string& getSpoutSourceName() const;

	// Texture Source Interface
	virtual IMkTexturePtr getClientColorSourceTexture(MikanCameraID cameraId,
													  eTextureSourceColorType textureSourceColorType,
													  int64_t frameIndex= -1) const;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
	{
		TextureSourceComponent::getFunctionDescriptors(outDescriptors);
	}
	virtual void showTextureSourceSettings() override;

protected:
	virtual void onDefinitionMarkedDirty(CommonConfigPtr configPtr,
										 const ConfigPropertyChangeSet& changedPropertySet) override;
	void closeTextureSource();
	void openTextureSource();

private:
	SharedTextureReadAccessorPtr m_colorTextureReadAccessor;
	IMkTexturePtr m_colorTexture;
	struct SPOUTLIBRARY* m_spoutColorFrame;
};
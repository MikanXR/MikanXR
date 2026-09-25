#pragma once

#include "CompositorConstants.h"
#include "TextureSourceComponent.h"
#include "MkRendererFwd.h"

class ClientTextureSourceDefinition : public TextureSourceDefinition
{
public:
	ClientTextureSourceDefinition();
	ClientTextureSourceDefinition(MikanTextureSourceID TextureSourceId);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	static const std::string k_clientSourcePropertyId;
	inline const std::string& getClientSource() const { return m_clientSource; }
	void setClientSource(const std::string& clientSource);

private:
	std::string m_clientSource;
};

class ClientTextureSourceComponent : public TextureSourceComponent
{
public:
	ClientTextureSourceComponent(MikanObjectWeakPtr owner);

	inline static const std::string k_componentClassName= "ClientTextureSourceComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	inline ClientTextureSourceDefinitionPtr getClientTextureSourceDefinition() const
	{
		return std::static_pointer_cast<ClientTextureSourceDefinition>(m_definition);
	}
	virtual void setDefinition(MikanComponentDefinitionPtr definition) override;
	const std::string& getClientSourceName() const;

	// Texture Source Interface
	IMkTexturePtr getClientColorSourceTexture(MikanCameraID cameraId, eTextureSourceColorType textureSourceColorType,
											  int64_t frameIndex= -1) const;
	IMkTexturePtr getClientDepthSourceTexture(MikanCameraID cameraId, eTextureSourceDepthType textureSourceDepthType,
											  int64_t frameIndex= -1) const;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static void getFunctionNamesStatic(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
	{
		TextureSourceComponent::getFunctionDescriptors(outDescriptors);
	}
	virtual void showTextureSourceSettings() override;

protected:
	class ClientSourceManager* getClientSourceManager() const;

private:
	bool m_bIsClientSourceOpened= false;
	bool m_bIsClientSourceStreaming= false;
};
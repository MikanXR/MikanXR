#pragma once

#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "CompositorConstants.h"
#include "MikanComponent.h"
#include "MikanTypeFwd.h"
#include "MikanTextureSourceTypes.h"
#include "MkRendererFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectFwd.h"
#include "VideoDisplayConstants.h"

#include <map>
#include <memory>
#include <string>

class TextureSourceDefinition : public MikanComponentDefinition
{
public:
	TextureSourceDefinition();
	TextureSourceDefinition(MikanTextureSourceID TextureSourceId);

	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	inline MikanTextureSourceID getTextureSourceId() const { return getComponentId(); }
};

class TextureSourceComponent : public MikanComponent
{
public:
	TextureSourceComponent(MikanObjectWeakPtr owner);

	inline TextureSourceDefinitionPtr getTextureSourceDefinition() const
	{
		return std::static_pointer_cast<TextureSourceDefinition>(getDefinition());
	}
	virtual void setDefinition(MikanComponentDefinitionPtr definition) override;

	inline static const std::string k_componentClassName= "TextureSourceComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	// Texture Source Interface
	MikanTextureSourceID getTextureSourceId() const;
	virtual IMkTexturePtr getClientColorSourceTexture(MikanCameraID cameraId,
													  eTextureSourceColorType textureSourceColorType,
													  int64_t frameIndex= -1) const;
	virtual IMkTexturePtr getClientDepthSourceTexture(MikanCameraID cameraId,
													  eTextureSourceDepthType textureSourceDepthType,
													  int64_t frameIndex= -1) const;

	// Video Source Events
	MulticastDelegate<void(TextureSourceComponentPtr TextureSource)> OnOpened;
	MulticastDelegate<void(TextureSourceComponentPtr TextureSource)> OnClosed;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IFunctionInterface ----
	static const std::string k_showTextureSourceSettingsFunctionId;
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outPropertyNames);
	virtual bool invokeFunction(const std::string& functionName) override;

	virtual void showTextureSourceSettings() {};
};
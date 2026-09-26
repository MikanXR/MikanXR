#include "CameraMath.h"
#include "MikanCoreTypes.h"
#include "MikanObject.h"
#include "MikanTextureSourceTypes.h"
#include "TextureSourceComponent.h"

#include <easy/profiler.h>

// -- TextureSourceDefinition -----
TextureSourceDefinition::TextureSourceDefinition()
	: MikanComponentDefinition()
{
}

TextureSourceDefinition::TextureSourceDefinition(MikanTextureSourceID textureSourceId)
	: MikanComponentDefinition(textureSourceId, "")
{
}

bool TextureSourceDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
												 const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!MikanComponentDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	// MikanTextureSourceValues has no additional fields beyond base class
	return true;
}

// -- TextureSourceComponent -----
TextureSourceComponent::TextureSourceComponent(MikanObjectWeakPtr owner)
	: MikanComponent(owner)
{
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle TextureSourceComponent::getClientAPIValuesStructType() const
{
	return &MikanTextureSourceValues::staticGetArchetype();
}

void TextureSourceComponent::setDefinition(MikanComponentDefinitionPtr definition)
{
	MikanComponent::setDefinition(definition);

	auto TextureSourceDefinitionPtr= std::static_pointer_cast<TextureSourceDefinition>(definition);
}

MikanTextureSourceID TextureSourceComponent::getTextureSourceId() const
{
	return getTextureSourceDefinition()->getTextureSourceId();
}

IMkTexturePtr TextureSourceComponent::getClientColorSourceTexture(MikanCameraID cameraId,
																  eTextureSourceColorType textureSourceColorType,
																  int64_t frameIndex) const
{
	return IMkTexturePtr();
}

IMkTexturePtr TextureSourceComponent::getClientDepthSourceTexture(MikanCameraID cameraId,
																  eTextureSourceDepthType textureSourceDepthType,
																  int64_t frameIndex) const
{
	return IMkTexturePtr();
}

// -- IFunctionInterface ----
const std::string TextureSourceComponent::k_showTextureSourceSettingsFunctionId= "show_texture_source_settings";

void TextureSourceComponent::getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getFunctionDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<FunctionDescriptor>(k_showTextureSourceSettingsFunctionId, "Show Texture Source Settings"));
}

bool TextureSourceComponent::invokeFunction(const std::string& functionName)
{
	if (functionName == k_showTextureSourceSettingsFunctionId)
	{
		showTextureSourceSettings();
		return true;
	}

	return MikanComponent::invokeFunction(functionName);
}
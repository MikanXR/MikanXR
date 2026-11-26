#include "SpoutOutputComponent.h"
#include "IMkTexture.h"
#include "MikanOutputTypes.h"
#include "MikanVariantTypes.h"
#include "ProjectConfigConstants.h"
#include "PropertyInterface.h"
#include "SharedTextureWriter.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

#include <assert.h>

// -- SpoutOutputDefinition -----
const std::string SpoutOutputDefinition::k_spoutOutputNamePropertyId= "spout_output_name";

SpoutOutputDefinition::SpoutOutputDefinition()
	: OutputDefinition()
	, m_spoutOutputName(DEFAULT_SPOUT_OUTPUT_NAME)
{
}

SpoutOutputDefinition::SpoutOutputDefinition(MikanOutputID outputId)
	: OutputDefinition(outputId)
	, m_spoutOutputName(DEFAULT_SPOUT_OUTPUT_NAME)
{
}

configuru::Config SpoutOutputDefinition::writeToJSON()
{
	configuru::Config pt= OutputDefinition::writeToJSON();

	pt[k_spoutOutputNamePropertyId]= m_spoutOutputName;

	return pt;
}

void SpoutOutputDefinition::readFromJSON(const configuru::Config& pt)
{
	OutputDefinition::readFromJSON(pt);

	m_spoutOutputName= pt.get_or<std::string>(k_spoutOutputNamePropertyId, m_spoutOutputName);
}

bool SpoutOutputDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
											   const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!OutputDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanSpoutOutputComponentValues>();
	if (componentValues)
	{
		m_spoutOutputName= componentValues->spout_output_name.getUtf8Value();
	}

	return true;
}

void SpoutOutputDefinition::setSpoutOutputName(const std::string& spoutOutputName)
{
	if (m_spoutOutputName != spoutOutputName)
	{
		m_spoutOutputName= spoutOutputName;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_spoutOutputNamePropertyId));
	}
}

// -- SpoutOutputComponent -----
SpoutOutputComponent::SpoutOutputComponent(MikanObjectWeakPtr owner)
	: OutputComponent(owner)
{
}

void SpoutOutputComponent::onDefinitionMarkedDirty(CommonConfigPtr configPtr,
												   const ConfigPropertyChangeSet& changedPropertySet)
{
	OutputComponent::onDefinitionMarkedDirty(configPtr, changedPropertySet);

	// A sender keeps the name it opened with, so a rename reopens it on the next frame
	if (changedPropertySet.hasPropertyName(SpoutOutputDefinition::k_spoutOutputNamePropertyId))
	{
		closeOutput();
	}
}

// -- Output interface ----
void SpoutOutputComponent::writeFrame(const OutputFrame& frame)
{
	IMkTexturePtr compositedTexture= frame.texture;
	if (!compositedTexture)
		return;

	const int width= compositedTexture->getTextureWidth();
	const int height= compositedTexture->getTextureHeight();
	if (width <= 0 || height <= 0)
		return;

	// A scene switch can bring a camera of another size, and a sender is sized at open
	if (getIsSenderOpen() && (width != m_senderWidth || height != m_senderHeight))
	{
		closeOutput();
	}

	if (!getIsSenderOpen())
	{
		// The composite is always RGBA 32BPP, one of the two formats Spout carries
		assert(compositedTexture->getBufferFormat() == MK_RGBA);

		if (!openSender(width, height))
			return;
	}

	m_writeAccessor->writeColorFrameTexture(compositedTexture->getPlatformTexture());
}

bool SpoutOutputComponent::openSender(int width, int height)
{
	const std::string& spoutOutputName= getSpoutOutputDefinition()->getSpoutOutputName();
	if (spoutOutputName.empty())
		return false;

	SharedTextureDescriptor sharedTextureDescriptor;
	sharedTextureDescriptor.color_buffer_type= SharedColorBufferType::RGBA32;
	sharedTextureDescriptor.depth_buffer_type= SharedDepthBufferType::NODEPTH;
	sharedTextureDescriptor.width= width;
	sharedTextureDescriptor.height= height;
	sharedTextureDescriptor.graphicsAPI= SharedClientGraphicsApi::OpenGL;

	m_writeAccessor= createSharedTextureWriteAccessor(spoutOutputName, INVALID_MIKAN_ID);
	if (!m_writeAccessor->initialize(&sharedTextureDescriptor, true, nullptr))
	{
		closeOutput();
		return false;
	}

	m_senderWidth= width;
	m_senderHeight= height;

	return true;
}

void SpoutOutputComponent::closeOutput()
{
	if (m_writeAccessor)
	{
		m_writeAccessor->dispose();
		m_writeAccessor= nullptr;
	}

	m_senderWidth= 0;
	m_senderHeight= 0;
}

bool SpoutOutputComponent::getIsSenderOpen() const { return m_writeAccessor && m_writeAccessor->getIsInitialized(); }

// -- IEntityAccessor ----
Serialization::StructTypeHandle SpoutOutputComponent::getClientAPIValuesStructType() const
{
	return &MikanSpoutOutputComponentValues::staticGetArchetype();
}

// -- IPropertyInterface ----
void SpoutOutputComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	OutputComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(SpoutOutputDefinition::k_spoutOutputNamePropertyId,
																  MikanVariantType::STRING));
}

bool SpoutOutputComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == SpoutOutputDefinition::k_spoutOutputNamePropertyId)
	{
		outValue= getSpoutOutputDefinition()->getSpoutOutputName().c_str();
		return true;
	}

	return OutputComponent::getPropertyValue(propertyName, outValue);
}

bool SpoutOutputComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == SpoutOutputDefinition::k_spoutOutputNamePropertyId)
	{
		getSpoutOutputDefinition()->setSpoutOutputName(inValue.getUtf8Value());
		return true;
	}

	return OutputComponent::setPropertyValue(propertyName, inValue);
}

// -- Lua Binding ----
void SpoutOutputComponent::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<SpoutOutputComponent, OutputComponent>(SpoutOutputComponent::k_componentClassName.c_str())
		.addProperty(
			"spoutOutputName",
			[](SpoutOutputComponent* c) -> std::string { return c->getSpoutOutputDefinition()->getSpoutOutputName(); },
			[](SpoutOutputComponent* c, const std::string& v) { c->getSpoutOutputDefinition()->setSpoutOutputName(v); })
		.addProperty("isSenderOpen", [](SpoutOutputComponent* c) -> bool { return c->getIsSenderOpen(); })
		.endClass();
}

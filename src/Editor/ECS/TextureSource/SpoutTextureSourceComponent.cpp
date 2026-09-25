#include "IMkTexture.h"
#include "Logger.h"
#include "MikanTextureSourceTypes.h"
#include "SpoutTextureSourceComponent.h"
#include "SpoutLibrary.h"
#include "StringUtils.h"
#include "TextureSourceSettings/AppStage_TextureSourceSettings.h"

#include <easy/profiler.h>

// -- SpoutTextureSourceDefinition ------
const std::string SpoutTextureSourceDefinition::k_spoutSourcePropertyId= "spout_source";

SpoutTextureSourceDefinition::SpoutTextureSourceDefinition()
	: TextureSourceDefinition()
{
}

SpoutTextureSourceDefinition::SpoutTextureSourceDefinition(MikanTextureSourceID textureSourceId)
	: TextureSourceDefinition(textureSourceId)
{
}

configuru::Config SpoutTextureSourceDefinition::writeToJSON()
{
	configuru::Config pt= TextureSourceDefinition::writeToJSON();

	pt["spout_source"]= m_spoutSource;

	return pt;
}

void SpoutTextureSourceDefinition::readFromJSON(const configuru::Config& pt)
{
	TextureSourceDefinition::readFromJSON(pt);

	m_spoutSource= pt.get_or<std::string>("spout_source", m_spoutSource);
}

bool SpoutTextureSourceDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
													  const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!TextureSourceDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanSpoutTextureSourceValues>();
	if (componentValues)
	{
		m_spoutSource= componentValues->spout_source.getUtf8Value();
	}

	return true;
}

void SpoutTextureSourceDefinition::setSpoutSource(const std::string& spoutSource)
{
	if (spoutSource != m_spoutSource)
	{
		m_spoutSource= spoutSource;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_spoutSourcePropertyId));
	}
}

// -- SpoutTextureSourceComponent -----
SpoutTextureSourceComponent::SpoutTextureSourceComponent(MikanObjectWeakPtr owner)
	: TextureSourceComponent(owner)
	, m_spoutColorFrame(nullptr)
{
	m_bWantsUpdate= true;
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle SpoutTextureSourceComponent::getClientAPIValuesStructType() const
{
	return &MikanSpoutTextureSourceValues::staticGetArchetype();
}

void SpoutTextureSourceComponent::setDefinition(MikanComponentDefinitionPtr definition)
{
	MikanComponent::setDefinition(definition);

	// re-open the texture source
	openTextureSource();
}

void SpoutTextureSourceComponent::onDefinitionMarkedDirty(CommonConfigPtr configPtr,
														  const ConfigPropertyChangeSet& changedPropertySet)
{
	TextureSourceComponent::onDefinitionMarkedDirty(configPtr, changedPropertySet);

	if (changedPropertySet.hasPropertyName(SpoutTextureSourceDefinition::k_spoutSourcePropertyId))
	{
		openTextureSource();
	}
}

void SpoutTextureSourceComponent::update(float deltaSeconds)
{
	if (m_spoutColorFrame != nullptr)
	{
		EASY_BLOCK("SpoutTextureSource: receive color texture");

		// Try and (re)initialize the receiver info if
		// * We haven't created a texture yet
		// * The sender has changed properties
		if (!m_colorTexture || m_spoutColorFrame->IsUpdated())
		{
			m_spoutColorFrame->ReceiveTexture();
		}

		// Fetch the (now hopefully valid) sender size
		const int senderWidth= m_spoutColorFrame->GetSenderWidth();
		const int senderHeight= m_spoutColorFrame->GetSenderHeight();

		// Get the current texture size
		const int textureWidth= m_colorTexture ? m_colorTexture->getTextureWidth() : 0;
		const int textureHeight= m_colorTexture ? m_colorTexture->getTextureHeight() : 0;

		// If the read texture size doesn't match the sender size, reallocate it
		if (senderWidth != textureWidth || senderHeight != textureHeight)
		{
			// Free any existing texture
			if (m_colorTexture)
			{
				m_colorTexture->disposeTexture();
				m_colorTexture= nullptr;
			}

			// Allocate a new texture to match the spout source shared texture
			// (unless the sender size is new invalid)
			if (senderWidth > 0 && senderHeight > 0)
			{
				m_colorTexture= CreateMkTexture();
				m_colorTexture->setSize(senderWidth, senderHeight);
				m_colorTexture->setTextureFormat(MK_RGBA);
				m_colorTexture->setBufferFormat(MK_RGBA);
				m_colorTexture->createTexture();
			}
		}

		// Read in the shared texture from Spout
		if (m_colorTexture)
		{
			const GLuint textureId= m_colorTexture->getGlTextureId();

			if (textureId != 0)
			{
				m_spoutColorFrame->ReceiveTexture(textureId, GL_TEXTURE_2D);
			}
		}
	}
}

void SpoutTextureSourceComponent::dispose()
{
	closeTextureSource();

	TextureSourceComponent::dispose();
}

const std::string& SpoutTextureSourceComponent::getSpoutSourceName() const
{
	return getSpoutTextureSourceDefinition()->getSpoutSource();
}

void SpoutTextureSourceComponent::closeTextureSource()
{
	if (m_spoutColorFrame)
	{
		m_spoutColorFrame->ReleaseReceiver();
		m_spoutColorFrame->Release();
		m_spoutColorFrame= nullptr;
	}

	if (m_colorTexture)
	{
		m_colorTexture->disposeTexture();
		m_colorTexture= nullptr;
	}
}

void SpoutTextureSourceComponent::openTextureSource()
{
	closeTextureSource();

	const std::string& spoutSourceName= getSpoutSourceName();

	if (!spoutSourceName.empty())
	{
		m_spoutColorFrame= GetSpout();
		if (m_spoutColorFrame != nullptr)
		{
			m_spoutColorFrame->SetReceiverName(spoutSourceName.c_str());
		}
		else
		{
			MIKAN_LOG_ERROR("SpoutTextureSourceComponent") << "Failed to open spout for sender: " << spoutSourceName;
		}
	}
}

// -- Texture Source Interface ---
IMkTexturePtr SpoutTextureSourceComponent::getClientColorSourceTexture(MikanCameraID cameraId,
																	   eTextureSourceColorType textureSourceColorType,
																	   int64_t frameIndex) const
{
	return m_colorTexture;
}

// -- IPropertyInterface ----
void SpoutTextureSourceComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	TextureSourceComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(SpoutTextureSourceDefinition::k_spoutSourcePropertyId,
																  MikanVariantType::STRING));
}

bool SpoutTextureSourceComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == SpoutTextureSourceDefinition::k_spoutSourcePropertyId)
	{
		outValue= getSpoutTextureSourceDefinition()->getSpoutSource();
		return true;
	}

	return TextureSourceComponent::getPropertyValue(propertyName, outValue);
}

bool SpoutTextureSourceComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == SpoutTextureSourceDefinition::k_spoutSourcePropertyId)
	{
		const std::string devicePath= inValue.getUtf8Value();
		getSpoutTextureSourceDefinition()->setSpoutSource(devicePath);
		return true;
	}

	return TextureSourceComponent::setPropertyValue(propertyName, inValue);
}

void SpoutTextureSourceComponent::showTextureSourceSettings()
{
	auto* appStage= getOwnerEditorWindow()->pushAppStageOfType<AppStage_TextureSourceSettings>();

	appStage->setSourceCameraId(INVALID_MIKAN_ID); // No camera needed for spout sources
	appStage->setTextureSourceComponent(getSelfPtr<TextureSourceComponent>());
}
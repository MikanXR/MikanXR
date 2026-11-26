#include "RTPOutputComponent.h"
#include "MikanOutputTypes.h"
#include "MikanVariantTypes.h"
#include "NetworkVideoSourceSystem.h"
#include "ProjectConfigConstants.h"
#include "PropertyInterface.h"
#include "RtpOutputStream.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

#include <algorithm>

// -- RTPOutputDefinition -----
const std::string RTPOutputDefinition::k_rtpOutputHostPropertyId= "rtp_output_host";
const std::string RTPOutputDefinition::k_rtpOutputPortPropertyId= "rtp_output_port";
const std::string RTPOutputDefinition::k_rtpOutputHeightPropertyId= "rtp_output_height";
const std::string RTPOutputDefinition::k_rtpOutputBitrateKbpsPropertyId= "rtp_output_bitrate_kbps";

RTPOutputDefinition::RTPOutputDefinition()
	: OutputDefinition()
	, m_rtpOutputPort(DEFAULT_RTP_OUTPUT_PORT)
	, m_rtpOutputHeight(DEFAULT_RTP_OUTPUT_HEIGHT)
	, m_rtpOutputBitrateKbps(DEFAULT_RTP_OUTPUT_BITRATE_KBPS)
{
}

RTPOutputDefinition::RTPOutputDefinition(MikanOutputID outputId)
	: OutputDefinition(outputId)
	, m_rtpOutputPort(DEFAULT_RTP_OUTPUT_PORT)
	, m_rtpOutputHeight(DEFAULT_RTP_OUTPUT_HEIGHT)
	, m_rtpOutputBitrateKbps(DEFAULT_RTP_OUTPUT_BITRATE_KBPS)
{
}

configuru::Config RTPOutputDefinition::writeToJSON()
{
	configuru::Config pt= OutputDefinition::writeToJSON();

	pt[k_rtpOutputHostPropertyId]= m_rtpOutputHost;
	pt[k_rtpOutputPortPropertyId]= m_rtpOutputPort;
	pt[k_rtpOutputHeightPropertyId]= m_rtpOutputHeight;
	pt[k_rtpOutputBitrateKbpsPropertyId]= m_rtpOutputBitrateKbps;

	return pt;
}

void RTPOutputDefinition::readFromJSON(const configuru::Config& pt)
{
	OutputDefinition::readFromJSON(pt);

	m_rtpOutputHost= pt.get_or<std::string>(k_rtpOutputHostPropertyId, m_rtpOutputHost);
	m_rtpOutputPort= pt.get_or<int>(k_rtpOutputPortPropertyId, m_rtpOutputPort);
	m_rtpOutputHeight= pt.get_or<int>(k_rtpOutputHeightPropertyId, m_rtpOutputHeight);
	m_rtpOutputBitrateKbps= pt.get_or<int>(k_rtpOutputBitrateKbpsPropertyId, m_rtpOutputBitrateKbps);
}

bool RTPOutputDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
											 const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!OutputDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanRTPOutputComponentValues>();
	if (componentValues)
	{
		m_rtpOutputHost= componentValues->rtp_output_host.getUtf8Value();
		m_rtpOutputPort= componentValues->rtp_output_port;
		m_rtpOutputHeight= componentValues->rtp_output_height;
		m_rtpOutputBitrateKbps= componentValues->rtp_output_bitrate_kbps;
	}

	return true;
}

void RTPOutputDefinition::setRtpOutputHost(const std::string& host)
{
	if (m_rtpOutputHost != host)
	{
		m_rtpOutputHost= host;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_rtpOutputHostPropertyId));
	}
}

void RTPOutputDefinition::setRtpOutputPort(int port)
{
	if (m_rtpOutputPort != port)
	{
		m_rtpOutputPort= port;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_rtpOutputPortPropertyId));
	}
}

void RTPOutputDefinition::setRtpOutputHeight(int height)
{
	if (m_rtpOutputHeight != height)
	{
		m_rtpOutputHeight= height;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_rtpOutputHeightPropertyId));
	}
}

void RTPOutputDefinition::setRtpOutputBitrateKbps(int bitrateKbps)
{
	if (m_rtpOutputBitrateKbps != bitrateKbps)
	{
		m_rtpOutputBitrateKbps= bitrateKbps;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_rtpOutputBitrateKbpsPropertyId));
	}
}

// -- RTPOutputComponent -----
RTPOutputComponent::RTPOutputComponent(MikanObjectWeakPtr owner)
	: OutputComponent(owner)
{
	// The frame sender's readback and its statistics advance per frame
	m_bWantsUpdate= true;
}

void RTPOutputComponent::update(float deltaSeconds)
{
	OutputComponent::update(deltaSeconds);

	if (m_stream)
	{
		m_stream->update(deltaSeconds);
	}
}

void RTPOutputComponent::onDefinitionMarkedDirty(CommonConfigPtr configPtr,
												 const ConfigPropertyChangeSet& changedPropertySet)
{
	OutputComponent::onDefinitionMarkedDirty(configPtr, changedPropertySet);

	// Any RTP setting rebuilds the stream on the next frame, since the sender's caps
	// are fixed at open and a rebuild is what clears a failed sender
	if (changedPropertySet.hasPropertyName(RTPOutputDefinition::k_rtpOutputHostPropertyId)
		|| changedPropertySet.hasPropertyName(RTPOutputDefinition::k_rtpOutputPortPropertyId)
		|| changedPropertySet.hasPropertyName(RTPOutputDefinition::k_rtpOutputHeightPropertyId)
		|| changedPropertySet.hasPropertyName(RTPOutputDefinition::k_rtpOutputBitrateKbpsPropertyId))
	{
		closeOutput();
	}
}

// -- Output interface ----
void RTPOutputComponent::writeFrame(const OutputFrame& frame)
{
	RTPOutputDefinitionConstPtr definition= getRTPOutputDefinition();

	RtpOutputStream::Config config;
	config.height= definition->getRtpOutputHeight();
	config.bitrateKbps= definition->getRtpOutputBitrateKbps();
	if (!definition->getRtpOutputHost().empty())
	{
		RtpDestination destination;
		destination.host= definition->getRtpOutputHost();
		destination.port= static_cast<uint16_t>(std::clamp(definition->getRtpOutputPort(), 0, 65535));
		config.destinations.push_back(destination);
	}

	if (!m_stream)
	{
		m_stream=
			std::make_shared<RtpOutputStream>(getGraphicsContext(), getObjectSystemOfType<NetworkVideoSourceSystem>());
	}

	m_stream->writeFrame(frame, config);
}

void RTPOutputComponent::closeOutput()
{
	if (m_stream)
	{
		m_stream->close();
	}
}

bool RTPOutputComponent::getHasDestination() const
{
	RTPOutputDefinitionConstPtr definition= getRTPOutputDefinition();

	return definition->getIsEnabled() && !definition->getRtpOutputHost().empty();
}

bool RTPOutputComponent::getStreamFormat(int& outWidth, int& outHeight, float& outFps) const
{
	return m_stream && m_stream->getStreamFormat(outWidth, outHeight, outFps);
}

std::string RTPOutputComponent::getStatusText() const
{
	RTPOutputDefinitionConstPtr definition= getRTPOutputDefinition();
	if (!definition->getIsEnabled())
		return "off";
	if (definition->getRtpOutputHost().empty())
		return "no destination";
	if (!m_stream)
		return "idle";

	return m_stream->getStatusText();
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle RTPOutputComponent::getClientAPIValuesStructType() const
{
	return &MikanRTPOutputComponentValues::staticGetArchetype();
}

// -- IPropertyInterface ----
const std::string RTPOutputComponent::k_rtpOutputStatusPropertyId= "rtp_output_status";

void RTPOutputComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	OutputComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(RTPOutputDefinition::k_rtpOutputHostPropertyId, MikanVariantType::STRING));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(RTPOutputDefinition::k_rtpOutputPortPropertyId, MikanVariantType::INT)
			->setDefaultValue(DEFAULT_RTP_OUTPUT_PORT));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(RTPOutputDefinition::k_rtpOutputHeightPropertyId, MikanVariantType::INT)
			->setDefaultValue(DEFAULT_RTP_OUTPUT_HEIGHT));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(RTPOutputDefinition::k_rtpOutputBitrateKbpsPropertyId,
																  MikanVariantType::INT)
								 ->setDefaultValue(DEFAULT_RTP_OUTPUT_BITRATE_KBPS));
	// Runtime state for the panel, so it is read only and stays out of the client values struct
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(k_rtpOutputStatusPropertyId, MikanVariantType::STRING)
								 ->setReadOnly()
								 ->setClientAPIHidden());
}

bool RTPOutputComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	RTPOutputDefinitionConstPtr definition= getRTPOutputDefinition();

	if (propertyName == RTPOutputDefinition::k_rtpOutputHostPropertyId)
	{
		outValue= definition->getRtpOutputHost().c_str();
		return true;
	}
	else if (propertyName == RTPOutputDefinition::k_rtpOutputPortPropertyId)
	{
		outValue= definition->getRtpOutputPort();
		return true;
	}
	else if (propertyName == RTPOutputDefinition::k_rtpOutputHeightPropertyId)
	{
		outValue= definition->getRtpOutputHeight();
		return true;
	}
	else if (propertyName == RTPOutputDefinition::k_rtpOutputBitrateKbpsPropertyId)
	{
		outValue= definition->getRtpOutputBitrateKbps();
		return true;
	}
	else if (propertyName == k_rtpOutputStatusPropertyId)
	{
		outValue= getStatusText().c_str();
		return true;
	}

	return OutputComponent::getPropertyValue(propertyName, outValue);
}

bool RTPOutputComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	RTPOutputDefinitionPtr definition= getRTPOutputDefinition();

	if (propertyName == RTPOutputDefinition::k_rtpOutputHostPropertyId)
	{
		definition->setRtpOutputHost(inValue.getUtf8Value());
		return true;
	}
	else if (propertyName == RTPOutputDefinition::k_rtpOutputPortPropertyId)
	{
		definition->setRtpOutputPort(inValue.getIntValue());
		return true;
	}
	else if (propertyName == RTPOutputDefinition::k_rtpOutputHeightPropertyId)
	{
		definition->setRtpOutputHeight(inValue.getIntValue());
		return true;
	}
	else if (propertyName == RTPOutputDefinition::k_rtpOutputBitrateKbpsPropertyId)
	{
		definition->setRtpOutputBitrateKbps(inValue.getIntValue());
		return true;
	}

	return OutputComponent::setPropertyValue(propertyName, inValue);
}

// -- Lua Binding ----
void RTPOutputComponent::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<RTPOutputComponent, OutputComponent>(RTPOutputComponent::k_componentClassName.c_str())
		.addProperty(
			"rtpOutputHost",
			[](RTPOutputComponent* c) -> std::string { return c->getRTPOutputDefinition()->getRtpOutputHost(); },
			[](RTPOutputComponent* c, const std::string& v) { c->getRTPOutputDefinition()->setRtpOutputHost(v); })
		.addProperty(
			"rtpOutputPort",
			[](RTPOutputComponent* c) -> int { return c->getRTPOutputDefinition()->getRtpOutputPort(); },
			[](RTPOutputComponent* c, int v) { c->getRTPOutputDefinition()->setRtpOutputPort(v); })
		.addProperty("statusText", [](RTPOutputComponent* c) -> std::string { return c->getStatusText(); })
		.endClass();
}

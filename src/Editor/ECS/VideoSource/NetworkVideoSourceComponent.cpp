#include "CameraMath.h"
#include "INetworkVideoDeviceManager.h"
#include "NetworkVideoSourceComponent.h"
#include "NetworkVideoSourceSystem.h"
#include "MikanObject.h"
#include "MikanServer.h"
#include "MikanVideoSourceTypes.h"
#include "StringUtils.h"
#include "ThreadUtils.h"
#include "VideoSourceRequestHandler.h"
#include "ProjectConfigConstants.h"

#include "opencv2/opencv.hpp"
#include "opencv2/calib3d/calib3d.hpp"

#include <easy/profiler.h>

#include <assert.h>

// -- NetworkVideoSourceDefinition -----
const std::string NetworkVideoSourceDefinition::k_addressPropertyId= "ip_address";
const std::string NetworkVideoSourceDefinition::k_pathPropertyId= "path";
const std::string NetworkVideoSourceDefinition::k_protocolPropertyId= "protocol";
const std::string NetworkVideoSourceDefinition::k_portPropertyId= "port";

const std::string g_NetworkVideoProtocol[(int)eNetworkVideoProtocol::COUNT]= {
	"RTMP",
	"RTSP",
};
const std::string* k_NetworkVideoProtocol= g_NetworkVideoProtocol;

NetworkVideoSourceDefinition::NetworkVideoSourceDefinition()
	: VideoSourceDefinition()
	, m_protocol(eNetworkVideoProtocol::RTSP)
	, m_address(DEFAULT_NETWORKED_CAMERA_ADDRESS)
	, m_path(DEFAULT_NETWORKED_CAMERA_PATH)
	, m_port(DEFAULT_RTSP_PORT)
{
}

NetworkVideoSourceDefinition::NetworkVideoSourceDefinition(MikanVideoSourceID videoSourceId)
	: VideoSourceDefinition(videoSourceId)
	, m_protocol(eNetworkVideoProtocol::RTSP)
	, m_address(DEFAULT_NETWORKED_CAMERA_ADDRESS)
	, m_path(DEFAULT_NETWORKED_CAMERA_PATH)
	, m_port(DEFAULT_RTSP_PORT)
{
}

configuru::Config NetworkVideoSourceDefinition::writeToJSON()
{
	configuru::Config pt= VideoSourceDefinition::writeToJSON();

	const std::string protocolString= m_protocol != eNetworkVideoProtocol::INVALID
										  ? k_NetworkVideoProtocol[(int)m_protocol]
										  : k_NetworkVideoProtocol[(int)eNetworkVideoProtocol::RTSP];
	pt[NetworkVideoSourceDefinition::k_protocolPropertyId]= protocolString;
	pt[NetworkVideoSourceDefinition::k_addressPropertyId]= m_address;
	pt[NetworkVideoSourceDefinition::k_pathPropertyId]= m_path;
	pt[NetworkVideoSourceDefinition::k_portPropertyId]= m_port;

	return pt;
}

void NetworkVideoSourceDefinition::readFromJSON(const configuru::Config& pt)
{
	VideoSourceDefinition::readFromJSON(pt);

	const std::string protocolString= pt.get_or<std::string>(NetworkVideoSourceDefinition::k_protocolPropertyId,
															 k_NetworkVideoProtocol[(int)eNetworkVideoProtocol::RTSP]);
	m_protocol= StringUtils::FindEnumValue<eNetworkVideoProtocol>(protocolString, k_NetworkVideoProtocol);
	m_address= pt.get_or<std::string>(NetworkVideoSourceDefinition::k_addressPropertyId, m_address);
	m_path= pt.get_or<std::string>(NetworkVideoSourceDefinition::k_pathPropertyId, m_path);
	m_port= pt.get_or<int>(NetworkVideoSourceDefinition::k_portPropertyId, m_port);
}

bool NetworkVideoSourceDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
													  const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!VideoSourceDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanNetworkVideoSourceValues>();
	if (componentValues)
	{
		const std::string protocolString= componentValues->protocol.getUtf8Value();
		m_protocol= StringUtils::FindEnumValue<eNetworkVideoProtocol>(protocolString, k_NetworkVideoProtocol);
		m_address= componentValues->ip_address.getUtf8Value();
		m_port= componentValues->port;
		m_path= componentValues->path.getUtf8Value();
	}

	return true;
}

void NetworkVideoSourceDefinition::setURL(const std::string& URL)
{
	eNetworkVideoProtocol protocol;
	std::string address;
	std::string path;
	int port= 0;

	if (NetworkVideoSourceDefinition::parseUrl(URL, protocol, address, port, path))
	{
		m_protocol= protocol;
		m_address= address;
		m_path= path;
		m_port= port;
		notifyPropertyChanged(ConfigPropertyChangeSet()
								  .addPropertyName(k_addressPropertyId)
								  .addPropertyName(k_pathPropertyId)
								  .addPropertyName(k_protocolPropertyId)
								  .addPropertyName(k_portPropertyId));
	}
}

void NetworkVideoSourceDefinition::setAddress(const std::string& address)
{
	if (address != m_address)
	{
		m_address= address;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_addressPropertyId));
	}
}

void NetworkVideoSourceDefinition::setPath(const std::string& path)
{
	if (path != m_path)
	{
		m_path= path;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_pathPropertyId));
	}
}

const std::string NetworkVideoSourceDefinition::getProtocolString() const
{
	return m_protocol != eNetworkVideoProtocol::INVALID ? k_NetworkVideoProtocol[(int)m_protocol] : "";
}

void NetworkVideoSourceDefinition::setProtocol(eNetworkVideoProtocol protocol)
{
	if (protocol != m_protocol && protocol != eNetworkVideoProtocol::INVALID)
	{
		m_protocol= protocol;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_protocolPropertyId));
	}
}

void NetworkVideoSourceDefinition::setProtocol(const std::string& protocolString)
{
	eNetworkVideoProtocol protocol=
		StringUtils::FindEnumValue<eNetworkVideoProtocol>(protocolString, k_NetworkVideoProtocol);
	setProtocol(protocol);
}

void NetworkVideoSourceDefinition::setPort(int port)
{
	if (port != m_port)
	{
		m_port= port;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_portPropertyId));
	}
}

bool NetworkVideoSourceDefinition::parseUrl(const std::string& url, eNetworkVideoProtocol& outProtocol,
											std::string& outAddress, int& outPort, std::string& outPath)
{
	// Find protocol separator "://"
	size_t protocolEnd= url.find("://");
	if (protocolEnd == std::string::npos)
		return false;

	// Extract and parse protocol
	std::string protocolStr= url.substr(0, protocolEnd);
	outProtocol= StringUtils::FindEnumValue<eNetworkVideoProtocol>(protocolStr, k_NetworkVideoProtocol);
	if (outProtocol == eNetworkVideoProtocol::COUNT)
		return false;

	// Start parsing after "://"
	size_t addressStart= protocolEnd + 3;
	if (addressStart >= url.length())
		return false;

	// Find path separator "/"
	size_t pathStart= url.find('/', addressStart);
	std::string addressAndPort;

	if (pathStart != std::string::npos)
	{
		addressAndPort= url.substr(addressStart, pathStart - addressStart);
		outPath= url.substr(pathStart);
	}
	else
	{
		addressAndPort= url.substr(addressStart);
		outPath= "/";
	}

	// Parse address and port
	size_t portStart= addressAndPort.find(':');
	if (portStart != std::string::npos)
	{
		outAddress= addressAndPort.substr(0, portStart);
		std::string portStr= addressAndPort.substr(portStart + 1);
		try
		{
			outPort= std::stoi(portStr);
		}
		catch (const std::exception&)
		{
			return false;
		}
	}
	else
	{
		outAddress= addressAndPort;
		// Set default ports based on protocol
		outPort= (outProtocol == eNetworkVideoProtocol::RTMP) ? DEFAULT_RTMP_PORT : 554;
	}

	return true;
}

// -- NetworkVideoSourceComponent -----
NetworkVideoSourceComponent::NetworkVideoSourceComponent(MikanObjectWeakPtr owner)
	: VideoSourceComponent(owner)
	, m_networkVideoDevice(nullptr)
{
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle NetworkVideoSourceComponent::getClientAPIValuesStructType() const
{
	return &MikanNetworkVideoSourceValues::staticGetArchetype();
}

void NetworkVideoSourceComponent::init()
{
	m_bWantsUpdate= true;
	MikanComponent::init();

	// Attempt to open the video source
	openVideoSource();
}

void NetworkVideoSourceComponent::dispose()
{
	// Close the video source if it is open
	closeVideoSource();

	// Call the base class dispose method
	MikanComponent::dispose();
}

void NetworkVideoSourceComponent::setDefinition(MikanComponentDefinitionPtr definition)
{
	MikanComponent::setDefinition(definition);

	// Close any open video source that was open
	closeVideoSource();
}

std::string NetworkVideoSourceComponent::getDevicePath() const
{
	if (m_networkVideoDevice != nullptr)
	{
		return m_networkVideoDevice->getDevicePath();
	}

	return "";
}

std::string NetworkVideoSourceComponent::getDeviceAPI() const { return "NetworkVideoSource"; }

bool NetworkVideoSourceComponent::openVideoSource()
{
	// If the video source is already open, do nothing
	if (m_networkVideoDevice != nullptr)
		return true;

	// If the device path is empty, return false
	NetworkVideoSourceDefinitionPtr definition= getNetworkVideoSourceDefinition();
	const std::string& address= definition->getAddress();
	if (address.empty())
		return false;

	// Create the network video device
	auto networkVideoSourceSystem=
		std::static_pointer_cast<NetworkVideoSourceSystem>(getOwnerObject()->getOwnerSystem());
	INetworkVideoDeviceManagerPtr networkVideoDeviceManager= networkVideoSourceSystem->getNetworkVideoDeviceManager();
	if (!networkVideoDeviceManager)
	{
		// If the manager is still initializing, mark this component as pending so the
		// system will retry openVideoSource() once the manager is ready
		auto networkVideoManagerState= networkVideoSourceSystem->getNetworkVideoManagerState();
		if (networkVideoManagerState == NetworkVideoSourceSystem::eNetworkVideoManagerState::uninitialized
			|| networkVideoManagerState == NetworkVideoSourceSystem::eNetworkVideoManagerState::initializing)
		{
			m_bPendingOpen= true;
			return true;
		}
		else
		{
			return false;
		}
	}
	m_bPendingOpen= false;

	NetworkVideoConnectionSettings connectionSettings;
	connectionSettings.protocol= definition->getProtocol();
	connectionSettings.address= address.c_str();
	connectionSettings.path= definition->getPath().c_str();
	connectionSettings.port= definition->getPort();

	m_networkVideoDevice= networkVideoDeviceManager->createVideoDevice(connectionSettings);
	if (m_networkVideoDevice == nullptr)
		return false;

	// Listen for events from the Network video device before calling open()
	// so that notifyVideoDeviceOpened() is received when the async open completes
	m_networkVideoDevice->addListener(this);

	// Kick off the async open — post-open setup happens in notifyVideoDeviceOpened()
	if (m_networkVideoDevice->open() == eVideoOpeningStatus::failed)
	{
		m_networkVideoDevice->removeListener(this);
		return false;
	}

	return true;
}

void NetworkVideoSourceComponent::closeVideoSource()
{
	m_bPendingOpen= false;

	if (m_networkVideoDevice != nullptr)
	{
		// Remove the listener for the Network video device
		m_networkVideoDevice->removeListener(this);

		// Stop the video stream if it is running
		forceStopVideoStream();

		// Close the Network video device
		m_networkVideoDevice->close();

		// Tell the Network video device manager to destroy the device
		auto networkVideoSourceSystem=
			std::static_pointer_cast<NetworkVideoSourceSystem>(getOwnerObject()->getOwnerSystem());
		INetworkVideoDeviceManagerPtr networkVideoDeviceManager=
			networkVideoSourceSystem->getNetworkVideoDeviceManager();
		networkVideoDeviceManager->destroyVideoDevice(m_networkVideoDevice);

		// Clear the Network video device pointer
		m_networkVideoDevice= nullptr;
	}

	// Let any connected clients know that the video source closed
	MikanServer::getInstance()->getVideoSourceRequestHandler()->publishVideoSourceClosedEvent();
	if (OnClosed)
	{
		OnClosed(getSelfPtr<VideoSourceComponent>());
	}
}

eVideoStreamingStatus NetworkVideoSourceComponent::startVideoStreamInternal()
{
	if (m_networkVideoDevice != nullptr)
	{
		eVideoStreamingStatus status= m_networkVideoDevice->startVideoStream();
		// TODO: Move to INetworkVideoDeviceListener event
		if (OnStarted && (status == eVideoStreamingStatus::started || status == eVideoStreamingStatus::pendingStart))
		{
			OnStarted(getSelfPtr<VideoSourceComponent>());
		}

		return status;
	}

	return eVideoStreamingStatus::failed;
}

eVideoStreamingStatus NetworkVideoSourceComponent::getVideoStreamingStatus() const
{
	if (m_networkVideoDevice != nullptr)
	{
		return m_networkVideoDevice->getVideoStreamingStatus();
	}

	return eVideoStreamingStatus::failed;
}

void NetworkVideoSourceComponent::stopVideoStreamInternal()
{
	if (m_networkVideoDevice != nullptr)
	{
		m_networkVideoDevice->stopVideoStream();

		if (OnStopped)
		{
			OnStopped(getSelfPtr<VideoSourceComponent>());
		}
	}
}

bool NetworkVideoSourceComponent::getVideoPixelDimensions(int& outPixelWidth, int& outPixelHeight) const
{
	if (m_networkVideoDevice != nullptr)
	{
		NetworkVideoStreamProperties streamProperties;
		if (m_networkVideoDevice->getStreamProperties(streamProperties))
		{
			outPixelWidth= streamProperties.width;
			outPixelHeight= streamProperties.height;
			return true;
		}
	}

	return false;
}

bool NetworkVideoSourceComponent::getVideoColorimetry(VideoColorimetry& outColorimetry) const
{
	if (m_networkVideoDevice != nullptr)
	{
		NetworkVideoStreamProperties streamProperties;
		if (m_networkVideoDevice->getStreamProperties(streamProperties))
		{
			outColorimetry= streamProperties.colorimetry;
			return true;
		}
	}

	return false;
}

// -- INetworkVideoDeviceListener ----
void NetworkVideoSourceComponent::notifyVideoDeviceOpened(const INetworkVideoDevice* device)
{
	if (device != m_networkVideoDevice.get())
		return;

	// Let any connected clients know that the video source opened
	MikanServer::getInstance()->getVideoSourceRequestHandler()->publishVideoSourceOpenedEvent();
	if (OnOpened)
	{
		OnOpened(getSelfPtr<VideoSourceComponent>());
	}
}

void NetworkVideoSourceComponent::notifyVideoDeviceClosed(const INetworkVideoDevice* device)
{
	if (device == m_networkVideoDevice.get())
	{
		// The video source is now invalidated, so we can clear the pointer
		// but we still want to clean up the video source state
		m_networkVideoDevice= nullptr;
		closeVideoSource();
	}
}

void NetworkVideoSourceComponent::notifyVideoModePropertiesChanged(const INetworkVideoDevice* device)
{
	// Make sure the device is the one we are currently using
	assert(m_networkVideoDevice.get() == device);

	// At the moment, this function should only be called from video sources that
	// update their video frame size on the main thread.
	// If this changes, we will need to refactor this function to be thread safe.
	assert(ThreadUtils::isRunningInMainThread());

	// Recompute the projection matrix
	recomputeCameraProjectionMatrix();

	// Let any listeners know that the video frame sized changed
	MikanServer::getInstance()->getVideoSourceRequestHandler()->publishVideoSourceModeChangedEvent();
	if (OnFrameSizeChanged)
	{
		OnFrameSizeChanged(getSelfPtr<VideoSourceComponent>());
	}
}

void NetworkVideoSourceComponent::notifyVideoFrameReceived(const NetworkVideoFrameBuffer& bufferInfo)
{
	assert(m_networkVideoDevice != nullptr);

	NetworkVideoStreamProperties streamProperties;
	bool bVaildStream= m_networkVideoDevice->getStreamProperties(streamProperties);
	assert(bVaildStream);

	NetworkVideoSourceDefinitionPtr definition= getNetworkVideoSourceDefinition();
	const MikanVideoSourceIntrinsics& intrinsics= definition->getCameraIntrinsics();

	const bool is_frame_flipped= definition->getIsFrameMirrored();
	const bool is_buffer_flipped= definition->getIsBufferMirrored();
	cv::Size bufferDimensions(bufferInfo.width, bufferInfo.height);

	// Fetch the latest video buffer frame from the device
	if (intrinsics.intrinsics_type == MikanIntrinsicsType::STEREO_CAMERA_INTRINSICS)
	{
		const auto& stereoIntrinsics= intrinsics.getStereoIntrinsics();
		const int section_width= (int)stereoIntrinsics.pixel_width;
		const int section_height= (int)stereoIntrinsics.pixel_height;

		cv::Rect left_bounds= cv::Rect(0, 0, section_width, section_height);
		cv::Rect right_bounds= cv::Rect(section_width, 0, section_width, section_height);

		// Cache the left raw video frame
		writeStereoVideoFrameSection(bufferInfo.data, bufferDimensions, is_frame_flipped, VideoFrameSection::Left,
									 is_buffer_flipped ? right_bounds : left_bounds);

		// Cache the right raw video frame
		writeStereoVideoFrameSection(bufferInfo.data, bufferDimensions, is_frame_flipped, VideoFrameSection::Right,
									 is_buffer_flipped ? left_bounds : right_bounds);
	}
	else
	{
		// Cache the raw video frame
		writeVideoFrame(bufferInfo.data, bufferDimensions, is_frame_flipped);
	}
}

// -- IPropertyInterface ----
void NetworkVideoSourceComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	VideoSourceComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(NetworkVideoSourceDefinition::k_addressPropertyId,
																  MikanVariantType::STRING)
								 ->setDefaultValue("192.168.1.1"));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(NetworkVideoSourceDefinition::k_pathPropertyId, MikanVariantType::STRING)
			->setDefaultValue(""));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(NetworkVideoSourceDefinition::k_protocolPropertyId,
																  MikanVariantType::STRING)
								 ->setDefaultValue(k_NetworkVideoProtocol[(int)eNetworkVideoProtocol::RTSP]));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(NetworkVideoSourceDefinition::k_portPropertyId, MikanVariantType::INT)
			->setDefaultValue(DEFAULT_RTMP_PORT));
}

bool NetworkVideoSourceComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == NetworkVideoSourceDefinition::k_addressPropertyId)
	{
		outValue= getNetworkVideoSourceDefinition()->getAddress();
		return true;
	}
	else if (propertyName == NetworkVideoSourceDefinition::k_pathPropertyId)
	{
		outValue= getNetworkVideoSourceDefinition()->getPath();
		return true;
	}
	else if (propertyName == NetworkVideoSourceDefinition::k_protocolPropertyId)
	{
		outValue= getNetworkVideoSourceDefinition()->getProtocolString();
		return true;
	}
	else if (propertyName == NetworkVideoSourceDefinition::k_portPropertyId)
	{
		outValue= getNetworkVideoSourceDefinition()->getPort();
		return true;
	}

	return VideoSourceComponent::getPropertyValue(propertyName, outValue);
}

bool NetworkVideoSourceComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == NetworkVideoSourceDefinition::k_addressPropertyId)
	{
		const std::string url= inValue.getUtf8Value();
		getNetworkVideoSourceDefinition()->setAddress(url);
		return true;
	}
	else if (propertyName == NetworkVideoSourceDefinition::k_portPropertyId)
	{
		const int port= inValue.getIntValue();
		getNetworkVideoSourceDefinition()->setPort(port);
		return true;
	}
	else if (propertyName == NetworkVideoSourceDefinition::k_protocolPropertyId)
	{
		const std::string protocolString= inValue.getUtf8Value();
		getNetworkVideoSourceDefinition()->setProtocol(protocolString);
		return true;
	}
	else if (propertyName == NetworkVideoSourceDefinition::k_pathPropertyId)
	{
		const std::string path= inValue.getUtf8Value();
		getNetworkVideoSourceDefinition()->setPath(path);
		return true;
	}

	return VideoSourceComponent::setPropertyValue(propertyName, inValue);
}

void NetworkVideoSourceComponent::onDefinitionMarkedDirty(CommonConfigPtr configPtr,
														  const ConfigPropertyChangeSet& changedPropertySet)
{
	// If the device path changed, reopen the video source
	if (changedPropertySet.hasPropertyName(NetworkVideoSourceDefinition::k_addressPropertyId)
		|| changedPropertySet.hasPropertyName(NetworkVideoSourceDefinition::k_pathPropertyId)
		|| changedPropertySet.hasPropertyName(NetworkVideoSourceDefinition::k_protocolPropertyId)
		|| changedPropertySet.hasPropertyName(NetworkVideoSourceDefinition::k_portPropertyId))
	{
		closeVideoSource();
		openVideoSource();
	}
	else
	{
		VideoSourceComponent::onDefinitionMarkedDirty(configPtr, changedPropertySet);
	}
}
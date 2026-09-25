//-- includes -----
#include "WebsocketInterprocessMessageClient.h"
#include "MikanClientLogger.h"
#include "MikanClient.h"
#include "MikanCoreCAPI.h"
#include "MikanCoreTypes.h"
#include "MikanCoreVulkanTypes.h"
#include "JsonUtils.h"
#include "JsonSerializer.h"
#include "SharedTextureWriter.h"

#include "ixwebsocket/IXNetSystem.h"

#include <assert.h>

namespace
{
// A Vulkan client's frame image crosses into the shared texture library in that library's own
// vocabulary, converted on the stack since the write completes before the call returns. Every
// other graphics API's texture pointer passes through untouched.
void* resolveApiTexturePtr(const ISharedTextureWriteAccessorPtr& writer, void* apiTexturePtr,
						   SharedVulkanTexture& outVulkanTexture)
{
	if (writer->getRenderTargetDescriptor()->graphicsAPI != SharedClientGraphicsApi::Vulkan || apiTexturePtr == nullptr)
		return apiTexturePtr;

	const auto* vulkanTexture= static_cast<const MikanVulkanTexture*>(apiTexturePtr);
	outVulkanTexture.image= vulkanTexture->image;
	outVulkanTexture.layout= (int32_t)vulkanTexture->layout;
	outVulkanTexture.format= (int32_t)vulkanTexture->format;
	outVulkanTexture.width= vulkanTexture->width;
	outVulkanTexture.height= vulkanTexture->height;

	return &outVulkanTexture;
}
} // namespace

// -- methods -----
MikanClient::MikanClient()
	: m_messageClient(new WebsocketInterprocessMessageClient(MikanConstants_ClientAPIVersion))
{
	for (int i= 0; i < MikanClientGraphicsApi_COUNT; i++)
	{
		m_graphicsDeviceInterfaces[i]= nullptr;
		m_graphicsCommandQueueInterfaces[i]= nullptr;
	};

	m_messageClient->setTextResponseHandler([this](const std::string& utf8ResponseString)
											{ textResponseHandler(utf8ResponseString); });
	m_messageClient->setBinaryResponseHandler([this](const uint8_t* buffer, size_t bufferSize)
											  { binaryResponseHandler(buffer, bufferSize); });
}

MikanClient::~MikanClient()
{
	freeAllCameraRenderTargetTextures();
	delete m_messageClient;
}

// -- ClientMikanAPI System -----
MikanCoreResult MikanClient::startup(const char* client_name, ClientLogSeverityLevel log_level,
									 t_logCallback log_callback)
{
	MikanCoreResult resultCode= MikanCoreResult_Success;

	// Store the client name
	m_clientName= client_name;

	// Reset status flags
	m_bIsConnected= false;

	ClientLoggerSettings settings= {};
	settings.min_log_level= log_level;
	settings.log_callback= log_callback;

	client_log_init(settings);

	if (!ix::initNetSystem())
	{
		MIKAN_LOG_WARNING("WebsocketInterprocessMessageServer::initialize()") << "Failed to initialize net system";
		resultCode= MikanCoreResult_RequestFailed;
	}

	return resultCode;
}

MikanCoreResult MikanClient::connect(const std::string& host, const std::string& port)
{
	return m_messageClient->connect(host, port);
}

bool MikanClient::getIsConnected() const { return m_messageClient->getIsConnected(); }

const std::string& MikanClient::getClientName() const { return m_clientName; }

MikanCoreResult MikanClient::disconnect(uint16_t code, const std::string& reason)
{
	MikanCoreResult resultCode= MikanCoreResult_NotConnected;

	// Free any existing buffer if we called allocate already
	freeAllCameraRenderTargetTextures();

	if (m_messageClient->getIsConnected())
	{
		m_messageClient->disconnect(code, reason);
		resultCode= MikanCoreResult_Success;
	}

	return resultCode;
}

MikanCoreResult MikanClient::fetchNextEvent(size_t utf8_buffer_size, char* out_utf8_buffer,
											size_t* out_utf8_bytes_written)
{
	// Events can arrive even when not connected (e.g. disconnect event)
	// So we don't check for connection here
	return m_messageClient->fetchNextEvent(utf8_buffer_size, out_utf8_buffer, out_utf8_bytes_written);
}

MikanCoreResult MikanClient::setTextResponseCallback(MikanTextResponseCallback callback, void* callback_userdata)
{
	m_textResponseCallback= callback;
	m_textResponseCallbackUserData= callback_userdata;

	return MikanCoreResult_Success;
}

MikanCoreResult MikanClient::setBinaryResponseCallback(MikanBinaryResponseCallback callback, void* callback_userdata)
{
	m_binaryResponseCallback= callback;
	m_binaryResponseCallbackUserData= callback_userdata;

	return MikanCoreResult_Success;
}

MikanCoreResult MikanClient::sendRequestJSON(const char* utf8_request_json)
{
	if (m_messageClient->getIsConnected())
	{
		return m_messageClient->sendRequest(utf8_request_json);
	}

	return MikanCoreResult_NotConnected;
}

void MikanClient::textResponseHandler(const std::string& utf8ResponseString)
{
	if (m_textResponseCallback != nullptr)
	{
		JsonSaxIntegerValueSearcher searcher;
		int requestId= -1;

		if (searcher.fetchKeyValuePair(utf8ResponseString, "requestId", requestId))
		{
			m_textResponseCallback((MikanRequestID)requestId, utf8ResponseString.c_str(),
								   m_textResponseCallbackUserData);
		}
		else
		{
			MIKAN_MT_LOG_WARNING("MikanClient::responseHandler()")
				<< "Received response missing a request id: " << utf8ResponseString;
		}
	}
	else
	{
		MIKAN_MT_LOG_WARNING("MikanClient::responseHandler()") << "No response callback set";
	}
}

void MikanClient::binaryResponseHandler(const uint8_t* buffer, size_t bufferSize)
{
	if (m_binaryResponseCallback != nullptr)
	{
		m_binaryResponseCallback(buffer, bufferSize, m_binaryResponseCallbackUserData);
	}
	else
	{
		MIKAN_MT_LOG_WARNING("MikanClient::binaryResponseHandler()") << "No binary response callback set";
	}
}

MikanCoreResult MikanClient::shutdown()
{
	if (m_messageClient != nullptr)
	{
		m_messageClient->disconnect(0, "");
	}
	ix::uninitNetSystem();
	client_log_dispose();

	return MikanCoreResult_Success;
}

ISharedTextureWriteAccessorPtr MikanClient::getSharedTextureWriteAccessor(MikanCameraID camera_id) const
{
	auto it= m_renderTargetWriterCameraMap.find(camera_id);
	if (it != m_renderTargetWriterCameraMap.end())
	{
		return it->second;
	}

	return ISharedTextureWriteAccessorPtr();
}

ISharedTextureWriteAccessorPtr MikanClient::addSharedTextureWriteAccessor(MikanCameraID camera_id)
{
	ISharedTextureWriteAccessorPtr writeAccessor= createSharedTextureWriteAccessor(m_clientName, camera_id);

	// Route the writer's diagnostics through the client log, so a failed shared texture
	// allocation names its reason to the client instead of failing silently
	writeAccessor->setLogCallback(
		[](SharedTextureLogLevel level, const std::string& message)
		{
			if (level == SharedTextureLogLevel::error)
			{
				MIKAN_LOG_ERROR("SharedTextureWriter") << message;
			}
			else
			{
				MIKAN_LOG_INFO("SharedTextureWriter") << message;
			}
		});

	m_renderTargetWriterCameraMap.insert({camera_id, writeAccessor});

	return writeAccessor;
}

MikanCoreResult MikanClient::allocateCameraRenderTargetTextures(MikanCameraID cameraId,
																const MikanRenderTargetDescriptor& mkDesiredDescriptor)
{
	MikanCoreResult resultCode;

	// Fetch the cached graphics API interface, if any
	void* apiInterface= nullptr;
	void* apiCommandQueueInterface= nullptr;
	if (mkDesiredDescriptor.graphicsAPI != MikanClientGraphicsApi_UNKNOWN)
	{
		Mikan_GetGraphicsDeviceInterface(this, mkDesiredDescriptor.graphicsAPI, &apiInterface);
		Mikan_GetGraphicsCommandQueueInterface(this, mkDesiredDescriptor.graphicsAPI, &apiCommandQueueInterface);
	}

	// A Vulkan client's device crosses in the shared texture library's own vocabulary,
	// so the client SDK header is the only side naming a Vulkan type
	if (mkDesiredDescriptor.graphicsAPI == MikanClientGraphicsApi_Vulkan && apiInterface != nullptr)
	{
		const auto* vulkanInterface= static_cast<const MikanVulkanDeviceInterface*>(apiInterface);

		m_vulkanDeviceInterface.instance= vulkanInterface->instance;
		m_vulkanDeviceInterface.physicalDevice= vulkanInterface->physicalDevice;
		m_vulkanDeviceInterface.device= vulkanInterface->device;
		m_vulkanDeviceInterface.queue= vulkanInterface->queue;
		m_vulkanDeviceInterface.queueFamilyIndex= vulkanInterface->queueFamilyIndex;
		apiInterface= &m_vulkanDeviceInterface;
	}

	SharedTextureDescriptor descriptor;

	switch (mkDesiredDescriptor.color_buffer_type)
	{
	case MikanColorBuffer_NOCOLOR:
		descriptor.color_buffer_type= SharedColorBufferType::NOCOLOR;
		break;
	case MikanColorBuffer_RGB24:
		descriptor.color_buffer_type= SharedColorBufferType::RGB24;
		break;
	case MikanColorBuffer_RGBA32:
		descriptor.color_buffer_type= SharedColorBufferType::RGBA32;
		break;
	case MikanColorBuffer_BGRA32:
		descriptor.color_buffer_type= SharedColorBufferType::BGRA32;
		break;
	case MikanColorBuffer_RGBA16F:
		descriptor.color_buffer_type= SharedColorBufferType::RGBA16F;
		break;
	}

	switch (mkDesiredDescriptor.depth_buffer_type)
	{
	case MikanDepthBuffer_NODEPTH:
		descriptor.depth_buffer_type= SharedDepthBufferType::NODEPTH;
		break;
	case MikanDepthBuffer_FLOAT_DEVICE_DEPTH:
		descriptor.depth_buffer_type= SharedDepthBufferType::FLOAT_DEVICE_DEPTH;
		break;
	case MikanDepthBuffer_FLOAT_SCENE_DEPTH:
		descriptor.depth_buffer_type= SharedDepthBufferType::FLOAT_SCENE_DEPTH;
		break;
	case MikanDepthBuffer_PACK_DEPTH_RGBA:
		descriptor.depth_buffer_type= SharedDepthBufferType::PACK_DEPTH_RGBA;
		break;
	}

	switch (mkDesiredDescriptor.shadow_buffer_type)
	{
	case MikanShadowBuffer_NOSHADOW:
		descriptor.shadow_buffer_type= SharedShadowBufferType::NOSHADOW;
		break;
	case MikanShadowBuffer_RGB24:
		descriptor.shadow_buffer_type= SharedShadowBufferType::RGB24;
		break;
	case MikanShadowBuffer_RGBA32:
		descriptor.shadow_buffer_type= SharedShadowBufferType::RGBA32;
		break;
	case MikanShadowBuffer_BGRA32:
		descriptor.shadow_buffer_type= SharedShadowBufferType::BGRA32;
		break;
	case MikanShadowBuffer_RGBA16F:
		descriptor.shadow_buffer_type= SharedShadowBufferType::RGBA16F;
		break;
	}

	switch (mkDesiredDescriptor.graphicsAPI)
	{
	case MikanClientGraphicsApi_Direct3D9:
		descriptor.graphicsAPI= SharedClientGraphicsApi::Direct3D9;
		break;
	case MikanClientGraphicsApi_Direct3D11:
		descriptor.graphicsAPI= SharedClientGraphicsApi::Direct3D11;
		break;
	case MikanClientGraphicsApi_Direct3D12:
		descriptor.graphicsAPI= SharedClientGraphicsApi::Direct3D12;
		break;
	case MikanClientGraphicsApi_OpenGL:
		descriptor.graphicsAPI= SharedClientGraphicsApi::OpenGL;
		break;
	case MikanClientGraphicsApi_Metal:
		descriptor.graphicsAPI= SharedClientGraphicsApi::Metal;
		break;
	case MikanClientGraphicsApi_Vulkan:
		descriptor.graphicsAPI= SharedClientGraphicsApi::Vulkan;
		break;
	default:
		descriptor.graphicsAPI= SharedClientGraphicsApi::UNKNOWN;
		break;
	}

	descriptor.width= mkDesiredDescriptor.width;
	descriptor.height= mkDesiredDescriptor.height;
	descriptor.aux_width= mkDesiredDescriptor.aux_width;
	descriptor.aux_height= mkDesiredDescriptor.aux_height;

	// Create a unique write for each camera id
	ISharedTextureWriteAccessorPtr renderTargetWriter= getSharedTextureWriteAccessor(cameraId);
	if (!renderTargetWriter)
	{
		renderTargetWriter= addSharedTextureWriteAccessor(cameraId);
	}

	// Create the shared texture
	bool bSuccess= false;
	const bool bEnableFrameCounter= false; // use frameRendered RPC to send frame index
	if (renderTargetWriter->initialize(&descriptor, bEnableFrameCounter, apiInterface, apiCommandQueueInterface))
	{
		resultCode= MikanCoreResult_Success;
	}
	else
	{
		resultCode= MikanCoreResult_RequestFailed;
	}

	return resultCode;
}

MikanCoreResult MikanClient::getCameraRenderTargetDescriptor(MikanCameraID cameraId,
															 MikanRenderTargetDescriptor& outDescriptor)
{
	ISharedTextureWriteAccessorPtr renderTargetWriter= getSharedTextureWriteAccessor(cameraId);
	if (renderTargetWriter)
	{
		const SharedTextureDescriptor* desc= renderTargetWriter->getRenderTargetDescriptor();
		if (desc != nullptr)
		{
			switch (desc->color_buffer_type)
			{
			case SharedColorBufferType::NOCOLOR:
				outDescriptor.color_buffer_type= MikanColorBuffer_NOCOLOR;
				break;
			case SharedColorBufferType::RGB24:
				outDescriptor.color_buffer_type= MikanColorBuffer_RGB24;
				break;
			case SharedColorBufferType::RGBA32:
				outDescriptor.color_buffer_type= MikanColorBuffer_RGBA32;
				break;
			case SharedColorBufferType::BGRA32:
				outDescriptor.color_buffer_type= MikanColorBuffer_BGRA32;
				break;
			case SharedColorBufferType::RGBA16F:
				outDescriptor.color_buffer_type= MikanColorBuffer_RGBA16F;
				break;
			}

			switch (desc->depth_buffer_type)
			{
			case SharedDepthBufferType::NODEPTH:
				outDescriptor.depth_buffer_type= MikanDepthBuffer_NODEPTH;
				break;
			case SharedDepthBufferType::FLOAT_DEVICE_DEPTH:
				outDescriptor.depth_buffer_type= MikanDepthBuffer_FLOAT_DEVICE_DEPTH;
				break;
			case SharedDepthBufferType::FLOAT_SCENE_DEPTH:
				outDescriptor.depth_buffer_type= MikanDepthBuffer_FLOAT_SCENE_DEPTH;
				break;
			case SharedDepthBufferType::PACK_DEPTH_RGBA:
				outDescriptor.depth_buffer_type= MikanDepthBuffer_PACK_DEPTH_RGBA;
				break;
			}

			switch (desc->shadow_buffer_type)
			{
			case SharedShadowBufferType::NOSHADOW:
				outDescriptor.shadow_buffer_type= MikanShadowBuffer_NOSHADOW;
				break;
			case SharedShadowBufferType::RGB24:
				outDescriptor.shadow_buffer_type= MikanShadowBuffer_RGB24;
				break;
			case SharedShadowBufferType::RGBA32:
				outDescriptor.shadow_buffer_type= MikanShadowBuffer_RGBA32;
				break;
			case SharedShadowBufferType::BGRA32:
				outDescriptor.shadow_buffer_type= MikanShadowBuffer_BGRA32;
				break;
			case SharedShadowBufferType::RGBA16F:
				outDescriptor.shadow_buffer_type= MikanShadowBuffer_RGBA16F;
				break;
			}

			switch (desc->graphicsAPI)
			{
			case SharedClientGraphicsApi::Direct3D9:
				outDescriptor.graphicsAPI= MikanClientGraphicsApi_Direct3D9;
				break;
			case SharedClientGraphicsApi::Direct3D11:
				outDescriptor.graphicsAPI= MikanClientGraphicsApi_Direct3D11;
				break;
			case SharedClientGraphicsApi::Direct3D12:
				outDescriptor.graphicsAPI= MikanClientGraphicsApi_Direct3D12;
				break;
			case SharedClientGraphicsApi::OpenGL:
				outDescriptor.graphicsAPI= MikanClientGraphicsApi_OpenGL;
				break;
			case SharedClientGraphicsApi::Metal:
				outDescriptor.graphicsAPI= MikanClientGraphicsApi_Metal;
				break;
			case SharedClientGraphicsApi::Vulkan:
				outDescriptor.graphicsAPI= MikanClientGraphicsApi_Vulkan;
				break;
			default:
				outDescriptor.graphicsAPI= MikanClientGraphicsApi_UNKNOWN;
				break;
			}

			outDescriptor.width= desc->width;
			outDescriptor.height= desc->height;
			outDescriptor.aux_width= desc->aux_width;
			outDescriptor.aux_height= desc->aux_height;

			return MikanCoreResult_Success;
		}
	}

	return MikanCoreResult_Uninitialized;
}

MikanCoreResult MikanClient::freeCameraRenderTargetTextures(MikanCameraID cameraId)
{
	ISharedTextureWriteAccessorPtr renderTargetWriter= getSharedTextureWriteAccessor(cameraId);
	if (renderTargetWriter)
	{
		renderTargetWriter->dispose();
	}

	return MikanCoreResult_Success;
}

MikanCoreResult MikanClient::freeAllCameraRenderTargetTextures()
{
	for (auto it : m_renderTargetWriterCameraMap)
	{
		it.second->dispose();
	}

	return MikanCoreResult_Success;
}

MikanCoreResult MikanClient::writeCameraColorRenderTargetTexture(MikanCameraID cameraId, void* apiColorTexturePtr)
{
	ISharedTextureWriteAccessorPtr renderTargetWriter= getSharedTextureWriteAccessor(cameraId);
	if (renderTargetWriter)
	{
		SharedVulkanTexture vulkanTexture;
		apiColorTexturePtr= resolveApiTexturePtr(renderTargetWriter, apiColorTexturePtr, vulkanTexture);

		if (renderTargetWriter->writeColorFrameTexture(apiColorTexturePtr))
		{
			return MikanCoreResult_Success;
		}
	}

	return MikanCoreResult_RequestFailed;
}

MikanCoreResult MikanClient::writeCameraDepthRenderTargetTexture(MikanCameraID cameraId, void* apiDepthTexturePtr,
																 float zNear, float zFar)
{
	ISharedTextureWriteAccessorPtr renderTargetWriter= getSharedTextureWriteAccessor(cameraId);
	if (renderTargetWriter)
	{
		SharedDepthBufferType depthBufferType= renderTargetWriter->getRenderTargetDescriptor()->depth_buffer_type;

		if (depthBufferType != SharedDepthBufferType::NODEPTH)
		{
			SharedVulkanTexture vulkanTexture;
			apiDepthTexturePtr= resolveApiTexturePtr(renderTargetWriter, apiDepthTexturePtr, vulkanTexture);

			if (renderTargetWriter->writeDepthFrameTexture(apiDepthTexturePtr, zNear, zFar))
			{
				return MikanCoreResult_Success;
			}
		}
	}

	return MikanCoreResult_RequestFailed;
}

MikanCoreResult MikanClient::writeCameraShadowRenderTargetTexture(MikanCameraID cameraId, void* apiShadowTexturePtr)
{
	ISharedTextureWriteAccessorPtr renderTargetWriter= getSharedTextureWriteAccessor(cameraId);
	if (renderTargetWriter)
	{
		SharedShadowBufferType shadowBufferType= renderTargetWriter->getRenderTargetDescriptor()->shadow_buffer_type;

		if (shadowBufferType != SharedShadowBufferType::NOSHADOW)
		{
			SharedVulkanTexture vulkanTexture;
			apiShadowTexturePtr= resolveApiTexturePtr(renderTargetWriter, apiShadowTexturePtr, vulkanTexture);

			if (renderTargetWriter->writeShadowFrameTexture(apiShadowTexturePtr))
			{
				return MikanCoreResult_Success;
			}
		}
	}

	return MikanCoreResult_RequestFailed;
}

void* MikanClient::getCameraPackDepthTextureResourcePtr(MikanCameraID cameraId) const
{
	ISharedTextureWriteAccessorPtr renderTargetWriter= getSharedTextureWriteAccessor(cameraId);
	if (renderTargetWriter)
	{
		return renderTargetWriter->getPackDepthTextureResourcePtr();
	}

	return nullptr;
}

MikanCoreResult MikanClient::setGraphicsDeviceInterface(MikanClientGraphicsApi api, void* graphicsDeviceInterface)
{
	if (api < 0 || api >= MikanClientGraphicsApi_COUNT)
		return MikanCoreResult_InvalidParam;

	m_graphicsDeviceInterfaces[api]= graphicsDeviceInterface;

	return MikanCoreResult_Success;
}

MikanCoreResult MikanClient::getGraphicsDeviceInterface(MikanClientGraphicsApi api, void** outGraphicsDeviceInterface)
{
	if (api < 0 || api >= MikanClientGraphicsApi_COUNT)
		return MikanCoreResult_InvalidParam;
	if (outGraphicsDeviceInterface == nullptr)
		return MikanCoreResult_NullParam;

	*outGraphicsDeviceInterface= m_graphicsDeviceInterfaces[api];
	return MikanCoreResult_Success;
}

MikanCoreResult MikanClient::setGraphicsCommandQueueInterface(MikanClientGraphicsApi api,
															  void* graphicsCommandQueueInterface)
{
	if (api < 0 || api >= MikanClientGraphicsApi_COUNT)
		return MikanCoreResult_InvalidParam;

	m_graphicsCommandQueueInterfaces[api]= graphicsCommandQueueInterface;

	return MikanCoreResult_Success;
}

MikanCoreResult MikanClient::getGraphicsCommandQueueInterface(MikanClientGraphicsApi api,
															  void** outGraphicsCommandQueueInterface)
{
	if (api < 0 || api >= MikanClientGraphicsApi_COUNT)
		return MikanCoreResult_InvalidParam;
	if (outGraphicsCommandQueueInterface == nullptr)
		return MikanCoreResult_NullParam;

	*outGraphicsCommandQueueInterface= m_graphicsCommandQueueInterfaces[api];
	return MikanCoreResult_Success;
}
#include "MikanAPI.h"
#include "MikanCoreCAPI.h"
#include "MikanRequestManager.h"
#include "MikanRenderTargetAPI.h"
#include "MikanEventManager.h"
#include "JsonSerializer.h"
#include "TypeRegistry.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanAnchorTypes.rfks.h"

#include "MikanAPITypes.rfks.h"

#include "MikanCameraEvents.rfks.h"
#include "MikanCameraTypes.rfks.h"
#include "MikanCameraRequests.rfks.h"

#include "MikanClientEvents.rfks.h"
#include "MikanClientTypes.rfks.h"
#include "MikanClientRequests.rfks.h"

#include "MikanComponentTypes.rfks.h"

#include "MikanCompositorEvents.rfks.h"
#include "MikanCompositorTypes.rfks.h"

#include "MikanEditorTypes.rfks.h"

#include "MikanFunctionTypes.rfks.h"
#include "MikanFunctionRequests.rfks.h"

#include "MikanLightEvents.rfks.h"
#include "MikanLightTypes.rfks.h"
#include "MikanLightRequests.rfks.h"

#include "MikanMarkerTypes.rfks.h"
#include "MikanMarkerRequests.rfks.h"

#include "MikanMathTypes.rfks.h"

#include "MikanPropertyEvents.rfks.h"
#include "MikanPropertyTypes.rfks.h"
#include "MikanPropertyRequests.rfks.h"

#include "MikanRemoteControlEvents.rfks.h"
#include "MikanRemoteControlTypes.rfks.h"
#include "MikanRemoteControlRequests.rfks.h"

#include "MikanSceneTypes.rfks.h"

#include "MikanScriptEvents.rfks.h"
#include "MikanScriptTypes.rfks.h"
#include "MikanScriptRequests.rfks.h"

#include "MikanStageTypes.rfks.h"

#include "MikanShapeTypes.rfks.h"
#include "MikanShapeRequests.rfks.h"

#include "MikanStencilTypes.rfks.h"
#include "MikanStencilRequests.rfks.h"

#include "MikanTextureSourceEvents.rfks.h"
#include "MikanTextureSourceTypes.rfks.h"

#include "MikanTrackingMountTypes.rfks.h"

#include "MikanTrackingVolumeTypes.rfks.h"

#include "MikanTransformTypes.rfks.h"

#include "MikanVariantTypes.rfks.h"

#include "MikanVideoSourceEvents.rfks.h"
#include "MikanVideoSourceRequests.rfks.h"
#include "MikanVideoSourceTypes.rfks.h"

#include "MikanVRDeviceTypes.rfks.h"
#endif // MIKANAPI_REFLECTION_ENABLED

#include "MikanAnchorTypes.h"
#include "MikanEditorTypes.h"
#include "MikanFunctionTypes.h"
#include "MikanCameraTypes.h"
#include "MikanCompositorTypes.h"
#include "MikanLightTypes.h"
#include "MikanMarkerTypes.h"
#include "MikanSceneTypes.h"
#include "MikanStageTypes.h"
#include "MikanScriptTypes.h"
#include "MikanShapeTypes.h"
#include "MikanStencilTypes.h"
#include "MikanTextureSourceTypes.h"
#include "MikanTrackingMountTypes.h"
#include "MikanTrackingVolumeTypes.h"
#include "MikanVideoSourceTypes.h"
#include "MikanVRDeviceTypes.h"

// Static system class name definitions
const char* MikanAnchorSystemValues::k_systemName= "AnchorObjectSystem";
const char* MikanEditorSystemValues::k_systemName= "EditorObjectSystem";
const char* MikanBoxStencilSystemValues::k_systemName= "BoxStencilSystem";
const char* MikanQuadStencilSystemValues::k_systemName= "QuadStencilSystem";
const char* MikanModelStencilSystemValues::k_systemName= "ModelStencilSystem";
const char* MikanQuadShapeSystemValues::k_systemName= "QuadShapeSystem";
const char* MikanBoxShapeSystemValues::k_systemName= "BoxShapeSystem";
const char* MikanModelShapeSystemValues::k_systemName= "ModelShapeSystem";
const char* MikanMarkerSystemValues::k_systemName= "MarkerObjectSystem";
const char* MikanSceneSystemValues::k_systemName= "SceneObjectSystem";
const char* MikanDMXObjectSystemValues::k_systemName= "DMXObjectSystem";
const char* MikanRGBSpotLightSystemValues::k_systemName= "RGBSpotLightSystem";
const char* MikanRGBPixelGridSystemValues::k_systemName= "RGBPixelGridSystem";
const char* MikanDMXFixtureGroupSystemValues::k_systemName= "DMXFixtureGroupSystem";
const char* MikanDMXPresetSystemValues::k_systemName= "DMXPresetSystem";
const char* MikanDMXSequenceSystemValues::k_systemName= "DMXSequenceSystem";
const char* MikanLightEnvironmentSystemValues::k_systemName= "LightEnvironmentSystem";
const char* MikanVRObjectSystemValues::k_systemName= "VRObjectSystem";

// Static component class name and owner system name definitions
// TODO: Make these come from the System and component classes
const char* MikanAnchorComponentValues::k_componentClassName= "AnchorComponent";
const char* MikanAnchorComponentValues::k_ownerSystemName= MikanAnchorSystemValues::k_systemName;

const char* MikanCameraComponentValues::k_componentClassName= "CameraComponent";
const char* MikanCameraComponentValues::k_ownerSystemName= "CameraObjectSystem";

const char* MikanCompositorComponentValues::k_componentClassName= "CompositorComponent";
const char* MikanCompositorComponentValues::k_ownerSystemName= "CompositorObjectSystem";

const char* MikanMarkerComponentValues::k_componentClassName= "MarkerComponent";
const char* MikanMarkerComponentValues::k_ownerSystemName= MikanMarkerSystemValues::k_systemName;

const char* MikanSceneComponentValues::k_componentClassName= "SceneComponent";
const char* MikanSceneComponentValues::k_ownerSystemName= MikanSceneSystemValues::k_systemName;

const char* MikanStageComponentValues::k_componentClassName= "StageComponent";
const char* MikanStageComponentValues::k_ownerSystemName= "StageObjectSystem";

const char* MikanQuadStencilComponentValues::k_componentClassName= "QuadStencilComponent";
const char* MikanQuadStencilComponentValues::k_ownerSystemName= MikanQuadStencilSystemValues::k_systemName;

const char* MikanBoxStencilComponentValues::k_componentClassName= "BoxStencilComponent";
const char* MikanBoxStencilComponentValues::k_ownerSystemName= MikanBoxStencilSystemValues::k_systemName;

const char* MikanRGBSpotLightComponentValues::k_componentClassName= "RGBSpotLightComponent";
const char* MikanRGBSpotLightComponentValues::k_ownerSystemName= MikanRGBSpotLightSystemValues::k_systemName;

const char* MikanRGBPixelGridComponentValues::k_componentClassName= "RGBPixelGridComponent";
const char* MikanRGBPixelGridComponentValues::k_ownerSystemName= MikanRGBPixelGridSystemValues::k_systemName;

const char* MikanDMXFixtureGroupComponentValues::k_componentClassName= "DMXFixtureGroupComponent";
const char* MikanDMXFixtureGroupComponentValues::k_ownerSystemName= MikanDMXFixtureGroupSystemValues::k_systemName;

const char* MikanDMXPresetComponentValues::k_componentClassName= "DMXPresetComponent";
const char* MikanDMXPresetComponentValues::k_ownerSystemName= MikanDMXPresetSystemValues::k_systemName;

const char* MikanDMXSequenceComponentValues::k_componentClassName= "DMXSequenceComponent";
const char* MikanDMXSequenceComponentValues::k_ownerSystemName= MikanDMXSequenceSystemValues::k_systemName;

const char* MikanLightEnvironmentComponentValues::k_componentClassName= "LightEnvironmentComponent";
const char* MikanLightEnvironmentComponentValues::k_ownerSystemName= MikanLightEnvironmentSystemValues::k_systemName;

const char* MikanModelStencilComponentValues::k_componentClassName= "ModelStencilComponent";
const char* MikanModelStencilComponentValues::k_ownerSystemName= MikanModelStencilSystemValues::k_systemName;

const char* MikanQuadShapeComponentValues::k_componentClassName= "QuadShapeComponent";
const char* MikanQuadShapeComponentValues::k_ownerSystemName= MikanQuadShapeSystemValues::k_systemName;

const char* MikanBoxShapeComponentValues::k_componentClassName= "BoxShapeComponent";
const char* MikanBoxShapeComponentValues::k_ownerSystemName= MikanBoxShapeSystemValues::k_systemName;

const char* MikanModelShapeComponentValues::k_componentClassName= "ModelShapeComponent";
const char* MikanModelShapeComponentValues::k_ownerSystemName= MikanModelShapeSystemValues::k_systemName;

const char* MikanMarkerTrackingVolumeComponentValues::k_componentClassName= "MarkerTrackingVolumeComponent";
const char* MikanMarkerTrackingVolumeComponentValues::k_ownerSystemName= "MarkerTrackingVolumeSystem";

const char* MikanVRTrackingVolumeComponentValues::k_componentClassName= "VRTrackingVolumeComponent";
const char* MikanVRTrackingVolumeComponentValues::k_ownerSystemName= "VRTrackingVolumeSystem";

const char* MikanTrackingMountComponentValues::k_componentClassName= "TrackingMountComponent";
const char* MikanTrackingMountComponentValues::k_ownerSystemName= "TrackingMountObjectSystem";

const char* MikanScriptComponentValues::k_componentClassName= "ScriptComponent";
const char* MikanScriptComponentValues::k_ownerSystemName= "ScriptObjectSystem";

const char* MikanClientTextureSourceValues::k_componentClassName= "ClientTextureSourceComponent";
const char* MikanClientTextureSourceValues::k_ownerSystemName= "ClientTextureSourceSystem";

const char* MikanSpoutTextureSourceValues::k_componentClassName= "SpoutTextureSourceComponent";
const char* MikanSpoutTextureSourceValues::k_ownerSystemName= "SpoutTextureSourceSystem";

const char* MikanCEFTextureSourceValues::k_componentClassName= "CEFTextureSourceComponent";
const char* MikanCEFTextureSourceValues::k_ownerSystemName= "CEFTextureSourceSystem";

const char* MikanNetworkVideoSourceValues::k_componentClassName= "NetworkVideoSourceComponent";
const char* MikanNetworkVideoSourceValues::k_ownerSystemName= "NetworkVideoSourceSystem";

const char* MikanARKitVideoSourceValues::k_componentClassName= "ARKitVideoSourceComponent";
const char* MikanARKitVideoSourceValues::k_ownerSystemName= "ARKitVideoSourceSystem";

const char* MikanFileVideoSourceValues::k_componentClassName= "FileVideoSourceComponent";
const char* MikanFileVideoSourceValues::k_ownerSystemName= "FileVideoSourceSystem";

const char* MikanUSBVideoSourceValues::k_componentClassName= "USBVideoSourceComponent";
const char* MikanUSBVideoSourceValues::k_ownerSystemName= "USBVideoSourceSystem";

const char* MikanVRDeviceComponentValues::k_componentClassName= "VRDeviceComponent";
const char* MikanVRDeviceComponentValues::k_ownerSystemName= MikanVRObjectSystemValues::k_systemName;

// -- MikanAPI Implementation -----
class MikanAPI : public IMikanAPI
{
public:
	MikanAPI()
		: m_context(nullptr)
		, m_requestManager(std::make_unique<MikanRequestManager>())
		, m_eventManager(std::make_unique<MikanEventManager>())
		, m_renderTargetAPI(std::make_unique<MikanRenderTargetAPI>(m_requestManager.get()))
	{
	}

	virtual ~MikanAPI() { shutdown(); }

	// Initialize the Mikan API
	virtual MikanAPIResult init(const char* client_name, MikanLogLevel min_log_level,
								MikanLogCallback log_callback) override
	{
		MikanAPIResult result= (MikanAPIResult)Mikan_Initialize(client_name, min_log_level, log_callback, &m_context);
		if (result != MikanAPIResult::Success)
		{
			return result;
		}

		result= m_eventManager->init(m_context);
		if (result != MikanAPIResult::Success)
		{
			return result;
		}

		result= m_requestManager->init(m_context);
		if (result != MikanAPIResult::Success)
		{
			return result;
		}

#ifdef MIKANAPI_REFLECTION_ENABLED
		Serialization::TypeRegistry::build();
#endif // MIKANAPI_REFLECTION_ENABLED

		return MikanAPIResult::Success;
	}

	virtual bool getIsInitialized() override { return Mikan_GetIsInitialized(m_context); }

	virtual int getClientAPIVersion() const override { return MikanConstants_ClientAPIVersion; }

	virtual std::string getClientName() const override { return Mikan_GetClientName(m_context); }

	virtual MikanClientInfo allocateClientInfo() const override
	{
		MikanClientInfo clientInfo= {};

		// Stamp the request with the core sdk version and client id
		clientInfo.clientId= getClientName().c_str();

		return clientInfo;
	}

	// Send a request to the Mikan API
	virtual MikanResponseFuture sendRequest(MikanRequest& request) override
	{
		MikanResponseFuture responseFuture= m_renderTargetAPI->tryProcessRequest(request);
		if (responseFuture.isValid())
		{
			return responseFuture;
		}

		return m_requestManager->sendRequest(request);
	}

	virtual MikanAPIResult cancelRequest(const MikanRequestID& requestId) override
	{
		return m_requestManager->cancelRequest(requestId);
	}

	// Set client properties before calling connect
	virtual MikanAPIResult setGraphicsDeviceInterface(MikanClientGraphicsApi api,
													  void* graphicsDeviceInterface) override
	{
		return m_renderTargetAPI->setGraphicsDeviceInterface(api, graphicsDeviceInterface);
	}
	virtual MikanAPIResult getGraphicsDeviceInterface(MikanClientGraphicsApi api,
													  void** outGraphicsDeviceInterface) override
	{
		return m_renderTargetAPI->getGraphicsDeviceInterface(api, outGraphicsDeviceInterface);
	}
	virtual MikanAPIResult setGraphicsCommandQueueInterface(MikanClientGraphicsApi api,
															void* graphicsCommandQueueInterface) override
	{
		return m_renderTargetAPI->setGraphicsCommandQueueInterface(api, graphicsCommandQueueInterface);
	}
	virtual MikanAPIResult getGraphicsCommandQueueInterface(MikanClientGraphicsApi api,
															void** outGraphicsCommandQueueInterface) override
	{
		return m_renderTargetAPI->getGraphicsCommandQueueInterface(api, outGraphicsCommandQueueInterface);
	}

	virtual MikanAPIResult getCameraPackDepthTextureResourcePtr(MikanCameraID cameraId, void** outResourcePtr) override
	{
		return m_renderTargetAPI->getCameraPackDepthTextureResourcePtr(cameraId, outResourcePtr);
	}

	virtual MikanAPIResult connect() override { return (MikanAPIResult)Mikan_Connect(m_context, "", ""); }

	virtual MikanAPIResult connect(const std::string& host, const std::string& port) override
	{
		return (MikanAPIResult)Mikan_Connect(m_context, host.c_str(), port.c_str());
	}

	virtual bool getIsConnected() override { return Mikan_GetIsConnected(m_context); }

	virtual MikanAPIResult fetchNextEvent(MikanEventPtr& out_event) override
	{
		return m_eventManager->fetchNextEvent(out_event);
	}

	virtual MikanAPIResult disconnect() override { return (MikanAPIResult)Mikan_Disconnect(m_context, 0, ""); }

	virtual MikanAPIResult disconnect(uint16_t code, const std::string& reason) override
	{
		return (MikanAPIResult)Mikan_Disconnect(m_context, code, reason.c_str());
	}

	virtual MikanAPIResult shutdown() override
	{
		auto result= (MikanAPIResult)Mikan_Shutdown(m_context);
		m_context= nullptr;
		return result;
	}

private:
	MikanContext m_context;
	std::unique_ptr<MikanRequestManager> m_requestManager;
	std::unique_ptr<MikanEventManager> m_eventManager;
	std::unique_ptr<MikanRenderTargetAPI> m_renderTargetAPI;
};

// -- Mikan API Factory -----
IMikanAPIPtr IMikanAPI::createMikanAPI() { return std::make_shared<MikanAPI>(); }
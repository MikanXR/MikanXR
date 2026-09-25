#pragma once

#include "MikanCoreTypes.h"
#include "MikanTypeFwd.h"
#include "MulticastDelegate.h"
#include "IServerRequestHandler.h"

#include <map>
#include <memory>

using SharedTextureReadAccessorPtr= std::shared_ptr<class SharedTextureReadAccessor>;
using SharedTextureReadAccessorCameraMap= std::map<MikanCameraID, SharedTextureReadAccessorPtr>;

class RenderTargetClientState
{
public:
	RenderTargetClientState()= default;
	RenderTargetClientState(class MikanClientConnectionState* owner);
	virtual ~RenderTargetClientState();

	MikanClientGraphicsApi getClientGraphicsAPI(MikanCameraID cameraId) const;

	void disposeAllRenderTargetAccessors();

	inline const SharedTextureReadAccessorCameraMap& getRenderTargetAccessorMap() const
	{
		return m_renderTargetReadAccessorCameraMap;
	}
	class SharedTextureReadAccessor* getRenderTargetReadAccessor(MikanCameraID cameraId) const;
	bool hasAllocatedRenderTarget(MikanCameraID cameraId) const;
	bool allocateRenderTargetTextures(MikanCameraID cameraId, const MikanRenderTargetDescriptor& desc);
	void disposeRenderTargetAccessor(MikanCameraID cameraId);
	bool readRenderTargetTextures(MikanCameraID cameraId, const int64_t newFrameIndex);

protected:
	class SharedTextureReadAccessor* getOrAllocateRenderTargetAccessor(MikanCameraID cameraId,
																	   const MikanRenderTargetDescriptor& desc);
	void notifyRenderTargetReleased(class SharedTextureReadAccessor* readAccessor);

private:
	class MikanClientConnectionState* m_owner= nullptr;
	SharedTextureReadAccessorCameraMap m_renderTargetReadAccessorCameraMap;
};

class CameraRequestHandler : public IServerRequestHandler
{
public:
	CameraRequestHandler(class MikanServer* owner)
		: IServerRequestHandler(owner)
	{
	}

	virtual bool startup(class MainWindow* mainWindow) override;
	virtual void shutdown() override {}

	void publishCameraNewFrameEvent(const struct MikanCameraNewFrameEvent& newFrameEvent);
	void publishCameraNewPropertiesEvent(const struct MikanCameraNewPropertiesEvent& propertiesEvent);
	void publishCompositorStartedEvent(MikanCompositorID compositorId, MikanCameraID cameraId);
	void publishCompositorStoppedEvent(MikanCompositorID compositorId, MikanCameraID cameraId);

	MulticastDelegate<void(const char* clientId, const struct MikanClientInfo& clientInfo,
						   class SharedTextureReadAccessor* readAccessor)>
		OnClientRenderTargetAllocated;
	MulticastDelegate<void(const char* clientId, class SharedTextureReadAccessor* readAccessor)>
		OnClientRenderTargetReleased;
	MulticastDelegate<void(const char* clientId, MikanCameraID cameraId, int64_t frameIndex)>
		OnClientRenderTargetUpdated;

protected:
	void allocateRenderTargetTexturesHandler(const ClientRequest& request, ClientResponse& response);
	void freeRenderTargetTexturesHandler(const ClientRequest& request, ClientResponse& response);
	void frameRenderedHandler(const ClientRequest& request, ClientResponse& response);
	void getCameraPropertiesHandler(const ClientRequest& request, ClientResponse& response);
};
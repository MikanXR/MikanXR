#pragma once

#include "ClientTextureFrameQueue.h"
#include "CompositorConstants.h"
#include "MikanAPITypes.h"
#include "MikanClientTypes.h"
#include "MulticastDelegate.h"
#include "MikanRendererFwd.h"
#include "NamedValueTable.h"

#include <functional>
#include <string>

class ClientSourceManager
{
public:
	struct ClientSource
	{
		std::string clientId;
		MikanClientInfo clientInfo;
		MikanRenderTargetDescriptor desc;
		class SharedTextureReadAccessor* readAccessor= nullptr;
		ClientTextureFrameQueue* textureQueue= nullptr;
		int64_t frameIndex= 0;
	};

	explicit ClientSourceManager(int textureQueueSize= 3);
	virtual ~ClientSourceManager()= default;

	// Answers the frame queue depth the camera's video source runs the compositor
	// at, or 0 when unknown. A client's texture ring must hold at least that many
	// frames or the compositor can never find the frame it is about to composite.
	using TextureQueueSizeResolver= std::function<int(MikanCameraID cameraId)>;
	void setTextureQueueSizeResolver(TextureQueueSizeResolver resolver) { m_queueSizeResolver= resolver; }

	bool startup();
	void shutdown();

	inline const NamedValueTable<ClientSource*>& getClientSources() const { return m_clientSources; }
	bool hasClientSource(const std::string& clientId, MikanCameraID cameraId) const;
	bool getClientSourceDimensions(const std::string& clientId, MikanCameraID cameraId, int& outWidth,
								   int& outHeight) const;
	IMkTexturePtr getClientColorSourceTexture(const std::string& clientId, MikanCameraID cameraId,
											  eTextureSourceColorType textureSourceColorType,
											  int64_t frameIndex= -1) const;
	IMkTexturePtr getClientDepthSourceTexture(const std::string& clientId, MikanCameraID cameraId,
											  eTextureSourceDepthType textureSourceDepthType,
											  int64_t frameIndex= -1) const;

	MulticastDelegate<void(const std::string& clientId, MikanCameraID cameraId)> OnClientSourceConnected;
	MulticastDelegate<void(const std::string& clientId, MikanCameraID cameraId)> OnClientSourceDisconnected;

protected:
	static std::string makeClientSourceTableKey(const char* clientId, MikanCameraID cameraId);
	ClientSource* getClientSource(const char* clientId, MikanCameraID cameraId) const;
	bool addClientSource(const char* clientId, const MikanClientInfo& clientInfo,
						 class SharedTextureReadAccessor* readAccessor);
	bool removeClientSource(const char* clientId, class SharedTextureReadAccessor* readAccessor);
	void destroyClientSource(const std::string& tableKey, ClientSource* clientSource);

	// MikanServer Events
	void onClientRenderTargetAllocated(const char* clientId, const MikanClientInfo& clientInfo,
									   class SharedTextureReadAccessor* readAccessor);
	void onClientRenderTargetReleased(const char* clientId, class SharedTextureReadAccessor* readAccessor);
	void onClientRenderTargetUpdated(const char* clientId, MikanCameraID cameraId, int64_t frameIndex);

private:
	int m_textureQueueSize= 3;
	TextureQueueSizeResolver m_queueSizeResolver;

	// Data sources used by the compositor layers
	NamedValueTable<ClientSource*> m_clientSources;
};
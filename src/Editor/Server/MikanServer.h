#pragma once

//-- includes -----
#include "CommonConfigFwd.h"
#include "InterprocessMessageServerInterface.h"
#include "ScriptingFwd.h"
#include "MikanAPITypes.h"
#include "MikanClientTypes.h"
#include "MikanClientEvents.h"
#include "MikanCameraEvents.h"
#include "MikanScriptEvents.h"
#include "MikanVideoSourceEvents.h"
#include "MulticastDelegate.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectSystemFwd.h"

#include "glm/ext/matrix_float4x4.hpp"
#include "stdint.h"

#include <map>
#include <memory>

class MikanClientConnectionState;
using MikanClientConnectionStatePtr= std::shared_ptr<MikanClientConnectionState>;
using MikanClientConnectionStateConstPtr= std::shared_ptr<const MikanClientConnectionState>;
using MikanClientConnectionStateMap= std::map<std::string, MikanClientConnectionStatePtr>;

//-- definitions -----
class MikanServer
{
public:
	MikanServer();
	virtual ~MikanServer();

	static MikanServer* getInstance() { return m_instance; }
	class MainWindow* getOwnerWindow() const { return m_ownerWindow; }
	ProjectManagerPtr getProjectManager() const;
	ProjectConfigPtr getProjectConfig() const;
	inline class IInterprocessMessageServer* getMessageServer() { return m_messageServer; }
	inline class HttpInterprocessMessageServer* getHttpMessageServer() { return m_httpMessageServer; }

	// Stops and restarts the HTTP server on the port and bind scope the app settings hold,
	// and the websocket server when its bind scope changed. Registered routes and request
	// handlers are preserved, since dispose() only stops the listeners.
	void restartMessageServers();
	inline class AssetUploadRequestHandler* getAssetUploadRequestHandler() const { return m_assetUploadRequestHandler; }
	inline class CompositorStreamRequestHandler* getCompositorStreamRequestHandler() const
	{
		return m_compositorStreamRequestHandler;
	}
	inline class CameraRequestHandler* getCameraRequestHandler() const { return m_cameraRequestHandler; }
	inline class FunctionRequestHandler* getFunctionRequestHandler() const { return m_functionRequestHandler; }
	inline class LightRequestHandler* getLightRequestHandler() const { return m_lightRequestHandler; }
	inline class PropertyRequestHandler* getPropertyRequestHandler() const { return m_propertyRequestHandler; }
	inline class ScriptRequestHandler* getScriptRequestHandler() const { return m_scriptRequestHandler; }
	inline class RemoteControlManager* getRemoteControlManager() const { return m_remoteControlManager; }
	inline class ModelGeometryRequestHandler* getModelGeometryRequestHandler() const
	{
		return m_modelGeometryRequestHandler;
	}
	inline class TextureSourceRequestHandler* getTextureSourceRequestHandler() const
	{
		return m_textureSourceRequestHandler;
	}
	inline class MarkerRequestHandler* getMarkerRequestHandler() const { return m_markerRequestHandler; }
	inline class VideoSourceRequestHandler* getVideoSourceRequestHandler() const { return m_videoSourceRequestHandler; }

	bool startup(class MainWindow* mainWindow);
	void update();
	void shutdown();

	void publishMikanJsonEvent(const std::string& mikanJsonEvent);

	MikanClientConnectionStatePtr getConnectedClientState(const std::string& connectionId) const;
	void getConnectedClientStateList(std::vector<MikanClientConnectionStateConstPtr>& outClientList) const;
	const MikanClientConnectionStateMap& getConnectedClientStateMap() { return m_clientConnections; }
	// Whether any connected client asked the editor to watch the source for the origin marker
	bool isVideoSourceAlignmentWatched(MikanVideoSourceID videoSourceId) const;

	MulticastDelegate<void(const std::string& clientId, const MikanClientInfo& clientInfo)> OnClientInitialized;
	MulticastDelegate<void(const std::string& clientId)> OnClientDisposed;

protected:
	// Connection State Management
	MikanClientConnectionStatePtr allocateClientConnectionState(const std::string& connectionId);
	void disposeClientConnectionState(const std::string& connectionId);
	void initClientInfo(MikanClientConnectionStatePtr connectionState, const MikanClientInfo& clientInfo);
	bool disposeClientInfo(MikanClientConnectionStatePtr connectionState);

	// Websocket Event Handlers
	void onClientConnectedHandler(const ClientSocketEvent& event);
	void onClientDisconnectedHandler(const ClientSocketEvent& event);
	void onClientErrorHandler(const ClientSocketEvent& event);

	// Request Callbacks
	void initClientHandler(const ClientRequest& request, ClientResponse& response);
	void disposeClientHandler(const ClientRequest& request, ClientResponse& response);

private:
	static MikanServer* m_instance;
	class MainWindow* m_ownerWindow= nullptr;

	class IInterprocessMessageServer* m_messageServer;
	class HttpInterprocessMessageServer* m_httpMessageServer;

	class AssetUploadRequestHandler* m_assetUploadRequestHandler;
	class CompositorStreamRequestHandler* m_compositorStreamRequestHandler;
	class CameraRequestHandler* m_cameraRequestHandler;
	class FunctionRequestHandler* m_functionRequestHandler;
	class LightRequestHandler* m_lightRequestHandler;
	class PropertyRequestHandler* m_propertyRequestHandler;
	class RemoteControlManager* m_remoteControlManager;
	class ScriptRequestHandler* m_scriptRequestHandler;
	class MarkerRequestHandler* m_markerRequestHandler;
	class ModelGeometryRequestHandler* m_modelGeometryRequestHandler;
	class TextureSourceRequestHandler* m_textureSourceRequestHandler;
	class VideoSourceRequestHandler* m_videoSourceRequestHandler;

	MikanClientConnectionStateMap m_clientConnections;

	// The rpc.discover answer, built once every request handler is registered
	nlohmann::json m_openRpcDocument;
};
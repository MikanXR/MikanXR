//-- includes -----
#include "App.h"
#include "AppSettingsConfig.h"
#include "AssetUploadRequestHandler.h"
#include "CameraRequestHandler.h"
#include "FunctionRequestHandler.h"
#include "LightRequestHandler.h"
#include "MarkerRequestHandler.h"
#include "JsonDeserializer.h"
#include "JsonSerializer.h"
#include "Logger.h"
#include "MainWindow.h"
#include "MikanAPITypes.h"
#include "MikanClientConnectionState.h"
#include "MikanClientRequests.h"
#include "MikanCoreTypes.h"
#include "MikanCameraRequests.h"
#include "MikanScriptRequests.h"
#include "MikanServer.h"
#include "MikanStencilRequests.h"
#include "ProjectConfig.h"
#include "PropertyRequestHandler.h"
#include "RemoteControlManager.h"
#include "ScriptRequestHandler.h"
#include "ServerResponseHelpers.h"
#include "ShapeRequestHandler.h"
#include "StencilRequestHandler.h"
#include "StringUtils.h"
#include "TextureSourceRequestHandler.h"
#include "VideoSourceRequestHandler.h"
#include "Version.h"
#include "WebsocketInterprocessMessageServer.h"
#include "HttpInterprocessMessageServer.h"

#include <Refureku/Refureku.h>
#include <easy/profiler.h>

#include <nlohmann/json.hpp>

using json= nlohmann::json;

#ifdef _MSC_VER
#pragma warning(disable : 4996) // 'This function or variable may be unsafe': strncpy
#endif

using namespace std::placeholders;

// -- MikanServer -----
MikanServer* MikanServer::m_instance= nullptr;

MikanServer::MikanServer()
	: m_messageServer(new WebsocketInterprocessMessageServer())
	, m_httpMessageServer(new HttpInterprocessMessageServer())
	, m_assetUploadRequestHandler(new AssetUploadRequestHandler(this))
	, m_cameraRequestHandler(new CameraRequestHandler(this))
	, m_functionRequestHandler(new FunctionRequestHandler(this))
	, m_lightRequestHandler(new LightRequestHandler(this))
	, m_propertyRequestHandler(new PropertyRequestHandler(this))
	, m_remoteControlManager(new RemoteControlManager(this))
	, m_markerRequestHandler(new MarkerRequestHandler(this))
	, m_shapeRequestHandler(new ShapeRequestHandler(this))
	, m_scriptRequestHandler(new ScriptRequestHandler(this))
	, m_stencilRequestHandler(new StencilRequestHandler(this))
	, m_textureSourceRequestHandler(new TextureSourceRequestHandler(this))
	, m_videoSourceRequestHandler(new VideoSourceRequestHandler(this))
{
	m_instance= this;
}

MikanServer::~MikanServer()
{
	delete m_videoSourceRequestHandler;
	delete m_textureSourceRequestHandler;
	delete m_markerRequestHandler;
	delete m_shapeRequestHandler;
	delete m_stencilRequestHandler;
	delete m_scriptRequestHandler;
	delete m_remoteControlManager;
	delete m_propertyRequestHandler;
	delete m_lightRequestHandler;
	delete m_functionRequestHandler;
	delete m_cameraRequestHandler;
	delete m_assetUploadRequestHandler;
	delete m_httpMessageServer;
	delete m_messageServer;
	m_instance= nullptr;
}

// -- ClientMikanAPI System -----
bool MikanServer::startup(MainWindow* mainWindow)
{
	EASY_FUNCTION();

	m_ownerWindow= mainWindow;

	if (!m_messageServer->initialize())
	{
		MIKAN_LOG_ERROR("MikanServer::startup()") << "Failed to initialize interprocess message server";
		return false;
	}

	// The HTTP trigger server is a secondary, best-effort feature (e.g. for Stream Deck style
	// integrations) - failing to bind its port shouldn't prevent the primary websocket RPC server
	// (and the rest of the app) from starting up.
	{
		AppSettingsConfigPtr appSettings= App::getInstance()->getAppSettings();
		const int httpPort= appSettings->getHttpServerPort();
		if (!m_httpMessageServer->initialize(httpPort, appSettings->getHttpServerAllowRemote()))
		{
			MIKAN_LOG_WARNING("MikanServer::startup()")
				<< "Failed to initialize HTTP interprocess message server on port " << httpPort;
		}
	}

	if (!m_assetUploadRequestHandler->startup(mainWindow))
	{
		MIKAN_LOG_ERROR("MikanServer::startup()") << "Failed to bind asset upload request handler";
		return false;
	}

	if (!m_cameraRequestHandler->startup(mainWindow))
	{
		MIKAN_LOG_ERROR("MikanServer::startup()") << "Failed to bind camera request handlers";
		return false;
	}

	if (!m_functionRequestHandler->startup(mainWindow))
	{
		MIKAN_LOG_ERROR("MikanServer::startup()") << "Failed to bind function request handlers";
		return false;
	}

	if (!m_lightRequestHandler->startup(mainWindow))
	{
		MIKAN_LOG_ERROR("MikanServer::startup()") << "Failed to bind light request handlers";
		return false;
	}

	if (!m_propertyRequestHandler->startup(mainWindow))
	{
		MIKAN_LOG_ERROR("MikanServer::startup()") << "Failed to bind property request handlers";
		return false;
	}

	if (!m_scriptRequestHandler->startup(mainWindow))
	{
		MIKAN_LOG_ERROR("MikanServer::startup()") << "Failed to bind script request handlers";
		return false;
	}

	if (!m_markerRequestHandler->startup(mainWindow))
	{
		MIKAN_LOG_ERROR("MikanServer::startup()") << "Failed to bind marker request handlers";
		return false;
	}

	if (!m_shapeRequestHandler->startup(mainWindow))
	{
		MIKAN_LOG_ERROR("MikanServer::startup()") << "Failed to bind shape request handlers";
		return false;
	}

	if (!m_stencilRequestHandler->startup(mainWindow))
	{
		MIKAN_LOG_ERROR("MikanServer::startup()") << "Failed to bind stencil request handlers";
		return false;
	}

	if (!m_remoteControlManager->startup(mainWindow))
	{
		MIKAN_LOG_ERROR("MikanServer::startup()") << "Failed to bind remote control request handlers";
		return false;
	}

	if (!m_textureSourceRequestHandler->startup(mainWindow))
	{
		MIKAN_LOG_ERROR("MikanServer::startup()") << "Failed to bind texture source request handlers";
		return false;
	}

	if (!m_videoSourceRequestHandler->startup(mainWindow))
	{
		MIKAN_LOG_ERROR("MikanServer::startup()") << "Failed to bind video source request handlers";
		return false;
	}

	// Websocket Event Handlers
	m_messageServer->setSocketEventHandler(WEBSOCKET_CONNECT_EVENT,
										   std::bind(&MikanServer::onClientConnectedHandler, this, _1));
	m_messageServer->setSocketEventHandler(WEBSOCKET_DISCONNECT_EVENT,
										   std::bind(&MikanServer::onClientDisconnectedHandler, this, _1));
	m_messageServer->setSocketEventHandler(WEBSOCKET_ERROR_EVENT,
										   std::bind(&MikanServer::onClientErrorHandler, this, _1));

	// Client Init/Dispose Requests
	m_messageServer->setRequestHandler(InitClientRequest::staticGetArchetype().getName(),
									   std::bind(&MikanServer::initClientHandler, this, _1, _2));
	m_messageServer->setRequestHandler(DisposeClientRequest::staticGetArchetype().getName(),
									   std::bind(&MikanServer::disposeClientHandler, this, _1, _2));

	return true;
}

void MikanServer::update()
{
	EASY_FUNCTION();

	// Process incoming function calls from clients
	{
		EASY_BLOCK("processRemoteFunctionCalls");

		m_messageServer->processSocketEvents();
		m_messageServer->processRequests();
		m_httpMessageServer->processRequests();
	}
}

void MikanServer::shutdown()
{
	m_clientConnections.clear();
	m_messageServer->dispose();
	m_httpMessageServer->dispose();

	m_assetUploadRequestHandler->shutdown();
	m_cameraRequestHandler->shutdown();
	m_functionRequestHandler->shutdown();
	m_lightRequestHandler->shutdown();
	m_propertyRequestHandler->shutdown();
	m_scriptRequestHandler->shutdown();
	m_markerRequestHandler->shutdown();
	m_shapeRequestHandler->shutdown();
	m_stencilRequestHandler->shutdown();
	m_remoteControlManager->shutdown();
	m_textureSourceRequestHandler->shutdown();
	m_videoSourceRequestHandler->shutdown();

	m_ownerWindow= nullptr;
}

void MikanServer::restartHttpMessageServer()
{
	AppSettingsConfigPtr appSettings= App::getInstance()->getAppSettings();
	const int port= appSettings->getHttpServerPort();

	m_httpMessageServer->dispose();
	if (!m_httpMessageServer->initialize(port, appSettings->getHttpServerAllowRemote()))
	{
		MIKAN_LOG_WARNING("MikanServer::restartHttpMessageServer") << "Failed to restart HTTP server on port " << port;
	}
}

ProjectManagerPtr MikanServer::getProjectManager() const { return m_ownerWindow->getProjectManager(); }

// Fetched live rather than cached: loading a project replaces the ProjectConfig instance
ProjectConfigPtr MikanServer::getProjectConfig() const
{
	return m_ownerWindow != nullptr ? getProjectManager()->getProjectConfig() : nullptr;
}

void MikanServer::publishMikanJsonEvent(const std::string& mikanJsonEvent)
{
	for (auto& connection_it : m_clientConnections)
	{
		connection_it.second->publishMikanJsonEvent(mikanJsonEvent);
	}
}

// RPC Callbacks
MikanClientConnectionStatePtr MikanServer::getConnectedClientState(const std::string& connectionId) const
{
	auto connection_it= m_clientConnections.find(connectionId);
	if (connection_it != m_clientConnections.end())
	{
		return connection_it->second;
	}

	return MikanClientConnectionStatePtr();
}

void MikanServer::getConnectedClientStateList(std::vector<MikanClientConnectionStateConstPtr>& outClientList) const
{
	outClientList.clear();
	for (auto& connection_it : m_clientConnections)
	{
		outClientList.push_back(connection_it.second);
	}
}

// Connection State Management
MikanClientConnectionStatePtr MikanServer::allocateClientConnectionState(const std::string& connectionId)
{
	MikanClientConnectionStatePtr clientState;

	auto connection_it= m_clientConnections.find(connectionId);
	if (connection_it != m_clientConnections.end())
	{
		// Client already exists
		clientState= connection_it->second;
	}
	else
	{
		// Create a new client state
		clientState= std::make_shared<MikanClientConnectionState>(this, connectionId);

		m_clientConnections.insert({connectionId, clientState});
	}

	return clientState;
}

void MikanServer::disposeClientConnectionState(const std::string& connectionId)
{
	auto connection_it= m_clientConnections.find(connectionId);
	if (connection_it != m_clientConnections.end())
	{
		const std::string& clientId= connection_it->first;
		MikanClientConnectionStatePtr connectionState= connection_it->second;

		// Make sure the client info is disposed before removing the client connection
		// (Client may have already done this)
		disposeClientInfo(connectionState);

		// Finally, remove the client connection from the connection list
		// (which will delete the client state)
		m_clientConnections.erase(connection_it);
	}
}

void MikanServer::initClientInfo(MikanClientConnectionStatePtr connectionState, const MikanClientInfo& clientInfo)
{
	const std::string clientId= clientInfo.clientId.getUtf8Value();

	// Fill in the client info and allocate render target read accessor
	// After this point, the connection can allocate render target textures
	connectionState->setMikanClientInfo(clientInfo);

	// Tell any listeners that the given client ID has initialized new client info
	if (OnClientInitialized)
	{
		OnClientInitialized(clientId, clientInfo);
	}
}

bool MikanServer::disposeClientInfo(MikanClientConnectionStatePtr connectionState)
{
	if (connectionState->isClientInfoValid())
	{
		const std::string& clientId= connectionState->getClientId();

		// Make sure all render target textures are freed before disposing the client info
		// (Client may have already done this)
		connectionState->getRenderTargetClientState()->disposeAllRenderTargetAccessors();

		// Tell any listeners that the given client ID has initialized is clearing its client info
		if (OnClientDisposed)
		{
			OnClientDisposed(clientId);
		}

		// Dispose any render target textures and reset the client info to defaults
		connectionState->clearMikanClientInfo();
		return true;
	}

	return false;
}

// Websocket Event Handlers
void MikanServer::onClientConnectedHandler(const ClientSocketEvent& event)
{
	MIKAN_LOG_INFO("onClientConnected") << "connectionId: " << event.connectionId
										<< ", protocol: " << event.eventArgs[0];

	// Determine if the client is compatible with the server
	// by checking the protocol version
	bool bIsClientCompatible= false;
	std::vector<std::string> protocols= StringUtils::splitString(event.eventArgs[0], ',');
	int clientProtocol= -1;
	for (const std::string& protocol : protocols)
	{
		std::string prefix= WEBSOCKET_PROTOCOL_PREFIX;
		if (protocol.rfind(prefix.c_str(), 0) == 0)
		{
			std::string versionString= protocol.substr(prefix.length());

			if (!versionString.empty())
			{
				int clientProtocol= std::atoi(versionString.c_str());

				bIsClientCompatible= clientProtocol >= MIKAN_MIN_ALLOWED_CLIENT_API_VERSION;
				break;
			}
		}
	}

	// Create a new client state for the connection
	MikanClientConnectionStatePtr clientState= allocateClientConnectionState(event.connectionId);

	// Tell the client if they are compatible with the server
	// Up to the client to trigger disconnect in response
	if (clientState)
	{
		MikanConnectedEvent connectedEvent= {};
		connectedEvent.serverVersion.version= MIKAN_SERVER_API_VERSION;
		connectedEvent.minClientVersion.version= MIKAN_MIN_ALLOWED_CLIENT_API_VERSION;
		connectedEvent.isClientCompatible= bIsClientCompatible;

		m_messageServer->sendMessageToClient(event.connectionId, mikanTypeToJsonString(connectedEvent));
	}
}

void MikanServer::onClientDisconnectedHandler(const ClientSocketEvent& event)
{
	MIKAN_LOG_INFO("onClientDisconnected") << "connectionId: " << event.connectionId << ", code: " << event.eventArgs[0]
										   << ", reason: " << event.eventArgs[1];

	disposeClientConnectionState(event.connectionId);
}

void MikanServer::onClientErrorHandler(const ClientSocketEvent& event)
{
	MIKAN_LOG_ERROR("onClientError") << "connectionId: " << event.connectionId << ", error: " << event.eventArgs[0];
}

// Request Callbacks
void MikanServer::initClientHandler(const ClientRequest& request, ClientResponse& response)
{
	InitClientRequest initClientRequest;
	if (!readTypedRequest(request.utf8RequestString, initClientRequest)
		|| initClientRequest.clientInfo.clientId.isEmpty())
	{
		MIKAN_LOG_ERROR("connectHandler") << "Failed to parse client info";
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	const std::string& connectionId= request.connectionId;
	const MikanClientInfo& clientInfo= initClientRequest.clientInfo;
	const char* clientId= clientInfo.clientId.getUtf8Value();

	auto connection_it= m_clientConnections.find(connectionId);
	if (connection_it != m_clientConnections.end())
	{
		MikanClientConnectionStatePtr connectionState= connection_it->second;

		MIKAN_LOG_INFO("e") << "Client (connectionId: " << connectionId << ", clientId: " << clientId
							<< ") allocated client info";

		initClientInfo(connectionState, clientInfo);

		writeSimpleJsonResponse(request.requestId, MikanAPIResult::Success, response);
	}
	else
	{
		MIKAN_LOG_ERROR("initClientHandler")
			<< "Client (connectionId: " << connectionId << ", clientId: " << clientId << ") already connected";
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::AlreadyConnected, response);
	}
}

void MikanServer::disposeClientHandler(const ClientRequest& request, ClientResponse& response)
{
	const std::string& connectionId= request.connectionId;

	auto connection_it= m_clientConnections.find(connectionId);
	if (connection_it != m_clientConnections.end())
	{
		MikanClientConnectionStatePtr connectionState= connection_it->second;
		RenderTargetClientState* renderTargetClientState= connectionState->getRenderTargetClientState();

		// Tear down any active render target textures before destroying the client info
		renderTargetClientState->disposeAllRenderTargetAccessors();

		if (disposeClientInfo(connectionState))
		{
			const std::string& clientId= connectionState->getClientId();

			MIKAN_LOG_INFO("disposeClientHandler") << "Client (connectionId: " << connectionId
												   << ", clientId: " << clientId << ") deallocated client info";

			writeSimpleJsonResponse(request.requestId, MikanAPIResult::Success, response);
		}
		else
		{
			MIKAN_LOG_ERROR("disposeClientHandler")
				<< "Client (connection id: " << connectionId << ") already deallocated client info";

			writeSimpleJsonResponse(request.requestId, MikanAPIResult::UnknownClient, response);
		}
	}
	else
	{
		MIKAN_LOG_ERROR("disposeClientHandler") << "Client (connection id: " << connectionId << ") not connected";
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::UnknownClient, response);
	}
}
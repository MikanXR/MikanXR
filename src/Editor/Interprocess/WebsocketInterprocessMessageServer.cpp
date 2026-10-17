#include "WebsocketInterprocessMessageServer.h"
#include "JsonUtils.h"
#include "MikanClientRequests.h"
#include "MikanClientEvents.h"
#include "MikanScriptEvents.h"
#include "MikanVideoSourceEvents.h"
#include "Logger.h"
#include "StringUtils.h"
#include "ThreadUtils.h"
#include "JsonSerializer.h"
#include "JsonDeserializer.h"
#include "Version.h"

#include "IxWebSocket/IXConnectionState.h"
#include "IxWebSocket/IxNetSystem.h"
#include "IxWebSocket/IXWebSocket.h"
#include "IxWebSocket/IXWebSocketServer.h"
#include "IxWebSocket/IXWebSocketSendData.h"

#include "nlohmann/json.hpp"

#include "readerwriterqueue.h"

#include <chrono>

using json= nlohmann::json;

using LockFreeMessageQueue= moodycamel::ReaderWriterQueue<std::string>;
using LockFreeMessageQueuePtr= std::shared_ptr<LockFreeMessageQueue>;
using WebSocketWeakPtr= std::weak_ptr<ix::WebSocket>;
using WebSocketPtr= std::shared_ptr<ix::WebSocket>;

//-- WebSocketClientConnection -----
class WebSocketClientConnection : public ix::ConnectionState
{
public:
	WebSocketClientConnection(WebsocketInterprocessMessageServer* ownerMessageServer)
		: ix::ConnectionState()
		, m_ownerMessageServer(ownerMessageServer)
		, m_socketEventQueue(std::make_shared<LockFreeMessageQueue>())
		, m_requestQueue(std::make_shared<LockFreeMessageQueue>())
	{
	}

	void bindWebSocket(WebSocketWeakPtr websocket) { m_websocket= websocket; }

	bool disconnect()
	{
		WebSocketPtr websocket= m_websocket.lock();
		if (websocket)
		{
			websocket->close();
			return true;
		}

		return false;
	}

	inline LockFreeMessageQueuePtr getSocketEventQueue() { return m_socketEventQueue; }
	inline LockFreeMessageQueuePtr getRequestQueue() { return m_requestQueue; }

	void handleClientMessage(ConnectionStatePtr connectionState, const ix::WebSocketMessagePtr& msg)
	{
		switch (msg->type)
		{
		case ix::WebSocketMessageType::Open:
		{
			auto remoteIp= connectionState->getRemoteIp();

			MIKAN_MT_LOG_TRACE("WebSocketClientConnection::handleClientMessage") << "New connection";
			MIKAN_MT_LOG_TRACE("WebSocketClientConnection::handleClientMessage") << "remote ip: " << remoteIp;
			MIKAN_MT_LOG_TRACE("WebSocketClientConnection::handleClientMessage") << "id: " << connectionState->getId();
			MIKAN_MT_LOG_TRACE("WebSocketClientConnection::handleClientMessage") << "Uri: " << msg->openInfo.uri;

			std::string protocol;
			auto it= msg->openInfo.headers.find("Sec-WebSocket-Protocol");
			if (it != msg->openInfo.headers.end())
			{
				protocol= it->second;
				MIKAN_MT_LOG_TRACE("WebSocketClientConnection::handleClientMessage") << "Protocol: " << protocol;
			}
			else
			{
				MIKAN_MT_LOG_WARNING("WebSocketClientConnection::handleClientMessage") << "No protocols specified";
			}

			// Enqueue the client connect event
			std::stringstream ss;
			ss << WEBSOCKET_CONNECT_EVENT;
			ss << ":" << protocol;
			m_socketEventQueue->enqueue(ss.str());
		}
		break;
		case ix::WebSocketMessageType::Close:
		{
			MIKAN_MT_LOG_TRACE("WebSocketClientConnection::handleClientMessage") << "Close connection";
			MIKAN_MT_LOG_TRACE("WebSocketClientConnection::handleClientMessage") << "id: " << connectionState->getId();
			MIKAN_MT_LOG_TRACE("WebSocketClientConnection::handleClientMessage") << "reason: " << msg->closeInfo.reason;
			MIKAN_MT_LOG_TRACE("WebSocketClientConnection::handleClientMessage") << "code: " << msg->closeInfo.code;

			// Enqueue the client disconnect event
			std::stringstream ss;
			ss << WEBSOCKET_DISCONNECT_EVENT;
			ss << ":" << msg->closeInfo.code;
			ss << ":" << msg->closeInfo.reason;
			m_socketEventQueue->enqueue(ss.str());
		}
		break;
		case ix::WebSocketMessageType::Message:
		{
			if (!msg->binary)
			{
				// Enqueue the request json string
				m_requestQueue->enqueue(msg->str);
			}
			else
			{
				MIKAN_MT_LOG_ERROR("WebSocketClientConnection::handleClientMessage")
					<< "Received unsupported binary message";
			}
		}
		break;
		case ix::WebSocketMessageType::Error:
		{
			MIKAN_MT_LOG_TRACE("WebSocketClientConnection::handleClientMessage") << "Error: " << msg->errorInfo.reason;

			// Enqueue the client error event
			std::stringstream ss;
			ss << WEBSOCKET_ERROR_EVENT;
			ss << ":" << msg->errorInfo.reason;
			m_socketEventQueue->enqueue(ss.str());
		}
		break;
		case ix::WebSocketMessageType::Ping:
		{
			MIKAN_MT_LOG_TRACE("WebSocketClientConnection::handleClientMessage") << "Ping";
		}
		break;
		case ix::WebSocketMessageType::Pong:
		{
			MIKAN_MT_LOG_TRACE("WebSocketClientConnection::handleClientMessage") << "Pong";
		}
		break;
		case ix::WebSocketMessageType::Fragment:
		{
			MIKAN_MT_LOG_TRACE("WebSocketClientConnection::handleClientMessage") << "Fragment";
		}
		break;
		}
	}

	template <typename t_message_type>
	bool sendSimpleMessage()
	{
		t_message_type event= {};
		json eventJson= event;
		std::string eventJsonString= eventJson.dump();

		return sendText(eventJsonString);
	}

	bool sendText(const std::string& textData)
	{
		WebSocketPtr websocket= m_websocket.lock();

		if (websocket)
		{
			auto sendInfo= websocket->sendText(textData);

			return sendInfo.success;
		}

		return false;
	}

private:
	WebsocketInterprocessMessageServer* m_ownerMessageServer= nullptr;
	LockFreeMessageQueuePtr m_socketEventQueue;
	LockFreeMessageQueuePtr m_requestQueue;
	WebSocketWeakPtr m_websocket;
};

//-- WebsocketInterprocessMessageServer -----
WebsocketInterprocessMessageServer::WebsocketInterprocessMessageServer()
	: m_server(nullptr)
{
}

WebsocketInterprocessMessageServer::~WebsocketInterprocessMessageServer() { dispose(); }

bool WebsocketInterprocessMessageServer::initialize()
{
	bool bSuccess= true;

	if (!ix::initNetSystem())
	{
		MIKAN_LOG_WARNING("WebsocketInterprocessMessageServer::initialize()") << "Failed to initialize net system";
		bSuccess= false;
	}

	if (bSuccess)
	{
		WebsocketInterprocessMessageServer* ownerMessageServer= this;

		m_server= std::make_shared<ix::WebSocketServer>();

		auto connectionStateFactory= [ownerMessageServer]() -> WebSocketClientConnectionPtr
		{ return std::make_shared<WebSocketClientConnection>(ownerMessageServer); };

		auto clientConnectCallback=
			[ownerMessageServer](WebSocketWeakPtr webSocketWeakPtr, ConnectionStatePtr connectionState)
		{
			WebSocketPtr webSocket= webSocketWeakPtr.lock();
			if (!webSocket)
				return;

			WebSocketClientConnectionPtr clientConnectionState=
				std::static_pointer_cast<WebSocketClientConnection>(connectionState);

			// Bind the websocket to the connection state
			clientConnectionState->bindWebSocket(webSocketWeakPtr);

			// TODO: Need to modify ixwebsocket to respond with the same protocol that the client requested
			// For now we just respond with the latest version of the protocol
			{
				std::stringstream ss;
				ss << WEBSOCKET_PROTOCOL_PREFIX << MIKAN_SERVER_API_VERSION;

				ix::WebSocketHttpHeaders extraHeaders;
				extraHeaders["Sec-WebSocket-Protocol"]= ss.str();
				webSocket->setExtraHeaders(extraHeaders);
			}

			// Bind the message handler to the connection
			webSocket->setOnMessageCallback(
				[clientConnectionState](const ix::WebSocketMessagePtr& msg)
				{ clientConnectionState->handleClientMessage(clientConnectionState, msg); });

			// Add the connection to the list of connections
			{
				std::lock_guard<std::mutex> lock(ownerMessageServer->m_connectionsMutex);

				ownerMessageServer->m_connections.push_back(clientConnectionState);
			}
		};

		m_server->setConnectionStateFactory(connectionStateFactory);
		m_server->setOnConnectionCallback(clientConnectCallback);

		// Start listening for connections
		std::pair<bool, std::string> result= m_server->listen();
		if (result.first)
		{
			m_server->start();
		}
		else
		{
			MIKAN_LOG_INFO("WebsocketInterprocessMessageServer::initialize()") << "Listen error: " << result.second;
			bSuccess= false;
		}
	}

	return bSuccess;
}

void WebsocketInterprocessMessageServer::dispose()
{
	if (m_server)
	{
		// Close down all connections
		std::vector<WebSocketClientConnectionPtr> connections;
		getConnectionList(connections);

		// Disconnect all clients
		for (WebSocketClientConnectionPtr connection : m_connections)
		{
			connection->disconnect();
		}

		// Clear the connection list
		{
			std::lock_guard<std::mutex> lock(m_connectionsMutex);

			m_connections.clear();
		}

		// Give us 500ms for the server to notice that clients went away
		ThreadUtils::sleepMilliseconds(500);
		m_server->stop();
	}

	ix::uninitNetSystem();
}

void WebsocketInterprocessMessageServer::setSocketEventHandler(const std::string& eventType, SocketEventHandler handler)
{
	m_socketEventHandlers[eventType]= handler;
}

void WebsocketInterprocessMessageServer::setRequestHandler(const std::string& requestTypeName, RequestHandler handler)
{
	m_requestHandlers[requestTypeName]= handler;
}

void WebsocketInterprocessMessageServer::getConnectionList(std::vector<WebSocketClientConnectionPtr>& outConnections)
{
	std::lock_guard<std::mutex> lock(m_connectionsMutex);

	for (WebSocketClientConnectionPtr connection : m_connections)
	{
		if (connection)
		{
			outConnections.push_back(connection);
		}
	}
}

WebSocketClientConnectionPtr WebsocketInterprocessMessageServer::findConnection(const std::string& connectionId)
{
	std::lock_guard<std::mutex> lock(m_connectionsMutex);

	for (WebSocketClientConnectionPtr connection : m_connections)
	{
		if (connection && connection->getId() == connectionId)
		{
			return connection;
		}
	}

	return nullptr;
}

void WebsocketInterprocessMessageServer::sendMessageToClient(const std::string& connectionId,
															 const std::string& message)
{
	WebSocketClientConnectionPtr connection= findConnection(connectionId);

	if (connection)
	{
		connection->sendText(message);
	}
}

void WebsocketInterprocessMessageServer::sendMessageToAllClients(const std::string& message)
{
	std::vector<WebSocketClientConnectionPtr> connections;
	getConnectionList(connections);

	for (WebSocketClientConnectionPtr connection : connections)
	{
		connection->sendText(message);
	}
}

void WebsocketInterprocessMessageServer::processSocketEvents()
{
	std::vector<WebSocketClientConnectionPtr> connections;
	getConnectionList(connections);

	// Process all connections
	for (WebSocketClientConnectionPtr connection : connections)
	{
		// Read all pending socket events in the queue
		std::string inRequestString;
		while (connection->getSocketEventQueue()->try_dequeue(inRequestString))
		{
			// Split on the argument separator
			std::vector<std::string> eventArgs= StringUtils::splitString(inRequestString, ':');
			if (eventArgs.size() == 0)
			{
				continue;
			}

			// First argument is the event type
			std::string eventType= eventArgs[0];
			eventArgs.erase(eventArgs.begin());

			// Fine the handler for this event type
			auto handler_it= m_socketEventHandlers.find(eventType);
			if (handler_it != m_socketEventHandlers.end())
			{
				// NOTE: Connection ID here is a unique ID for the websocket connection on the server
				// and is not the same as the client ID that the client sends to identify itself
				const std::string connectionId= connection->getId();

				ClientSocketEvent socketEvent= {connectionId, eventType, eventArgs};
				handler_it->second(socketEvent);
			}
		}
	}
}

void WebsocketInterprocessMessageServer::processRequests()
{
	std::vector<WebSocketClientConnectionPtr> connections;
	getConnectionList(connections);

	if (connections.empty())
		return;

	// Start on a different connection each tick so no connection is always answered first
	const size_t connectionCount= connections.size();
	const size_t startIndex= m_nextRequestConnectionIndex % connectionCount;
	m_nextRequestConnectionIndex= startIndex + 1;

	for (size_t offset= 0; offset < connectionCount; ++offset)
	{
		WebSocketClientConnectionPtr connection= connections[(startIndex + offset) % connectionCount];

		std::string inRequestString;
		while (connection->getRequestQueue()->try_dequeue(inRequestString))
		{
			processRequest(connection, inRequestString);
		}
	}
}

void WebsocketInterprocessMessageServer::processRequest(const WebSocketClientConnectionPtr& connection,
														const std::string& requestString)
{
	std::string requestTypeName;
	JsonSaxStringValueSearcher typeNameSearcher;
	if (!typeNameSearcher.fetchKeyValuePair(requestString, "requestTypeName", requestTypeName))
	{
		MIKAN_LOG_WARNING("processRequest") << "Request missing/invalid requestTypeName field: " << requestString;
		return;
	}

	// Request ID is optional if the request doesn't expect a response
	int requestId;
	JsonSaxIntegerValueSearcher requestIdSearcher;
	if (!requestIdSearcher.fetchKeyValuePair(requestString, "requestId", requestId))
	{
		requestId= INVALID_MIKAN_ID;
	}

	// Get the response from a registered function handler, if any
	ClientResponse outResponse;
	auto handler_it= m_requestHandlers.find(requestTypeName);
	if (handler_it != m_requestHandlers.end())
	{
		// NOTE: Connection ID here is a unique ID for the websocket connection on the server
		// and is not the same as the client ID that the client sends to identify itself
		const std::string connectionId= connection->getId();
		ClientRequest request= {connectionId, requestId, requestString};

		handler_it->second(request, outResponse);
	}
	else
	{
		MikanResponse outResult;
		outResult.responseTypeName= MikanResponse::staticGetArchetype().getName();
		outResult.requestId= requestId;
		outResult.resultCode= MikanAPIResult::UnknownFunction;

		std::string errorMsg;
		if (!Serialization::serializeToJsonString(outResult, outResponse.utf8String, errorMsg))
		{
			MIKAN_LOG_WARNING("processRequest")
				<< "Failed to serialize response for unknown request type: " << requestTypeName
				<< ", error: " << errorMsg;
			return;
		}
	}

	// Send the response back to the client
	if (!outResponse.utf8String.empty())
	{
		connection->sendText(outResponse.utf8String);
	}

	if (requestId != INVALID_MIKAN_ID && outResponse.utf8String.empty())
	{
		MIKAN_LOG_WARNING("processRequest")
			<< "Request handler for " << requestTypeName << " returned empty response, but response expected!";
	}
}
#pragma once

#include "InterprocessMessageServerInterface.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace ix
{
class WebSocketServer;
class ConnectionState;
} // namespace ix
using WebSocketServerPtr= std::shared_ptr<ix::WebSocketServer>;
using ConnectionStatePtr= std::shared_ptr<ix::ConnectionState>;
using WebSocketClientConnectionPtr= std::shared_ptr<class WebSocketClientConnection>;
using WebsocketClientEventQueuePtr= std::shared_ptr<class WebsocketClientEventQueue>;

class WebsocketInterprocessMessageServer : public IInterprocessMessageServer
{
public:
	WebsocketInterprocessMessageServer();
	virtual ~WebsocketInterprocessMessageServer();

	bool initialize(bool bAllowRemote) override;
	bool getIsAllowRemote() const override { return m_bAllowRemote; }
	void dispose() override;
	void setSocketEventHandler(const std::string& eventType, SocketEventHandler handler) override;
	void setRequestHandler(const std::string& method, RequestHandler handler) override;
	std::vector<std::string> getRequestHandlerNames() const override;

	void sendMessageToClient(const std::string& connectionId, const std::string& message) override;
	void sendMessageToAllClients(const std::string& message) override;
	void processSocketEvents() override;
	void processRequests() override;

	// Dispatches one JSON-RPC frame from a connection to the handler its method names and
	// returns the reply, empty when there is none: a notification is never answered.
	// Separate from the socket so the dispatch rules can be exercised without a connection.
	static std::string dispatchRequestFrame(const std::map<std::string, RequestHandler>& requestHandlers,
											const std::string& connectionId, const std::string& requestFrame);

protected:
	void getConnectionList(std::vector<WebSocketClientConnectionPtr>& outConnections);
	WebSocketClientConnectionPtr findConnection(const std::string& clientId);

	// Dispatches one request and sends its response
	void processRequest(const WebSocketClientConnectionPtr& connection, const std::string& requestString);

private:
	WebSocketServerPtr m_server;
	bool m_bAllowRemote= false;
	std::vector<WebSocketClientConnectionPtr> m_connections;
	std::mutex m_connectionsMutex;
	std::map<std::string, SocketEventHandler> m_socketEventHandlers;
	std::map<std::string, RequestHandler> m_requestHandlers;

	// Round-robin cursor into the connection list, advanced once per processRequests() so no
	// connection is always answered first.
	size_t m_nextRequestConnectionIndex= 0;
};

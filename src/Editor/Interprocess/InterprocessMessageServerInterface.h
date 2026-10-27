#pragma once

#include "JsonRpc.h"

#include <functional>
#include <string>
#include <vector>

#define WEBSOCKET_SERVER_ADDRESS "ws://127.0.0.1"
#define WEBSOCKET_SERVER_PORT "8080"
#define WEBSOCKET_SERVER_PORT_NUMBER 8080
#define WEBSOCKET_PROTOCOL_PREFIX "Mikan-"

#define WEBSOCKET_CONNECT_EVENT "connect"
#define WEBSOCKET_DISCONNECT_EVENT "disconnect"
#define WEBSOCKET_ERROR_EVENT "error"
#define WEBSOCKET_PING_EVENT "ping"
#define WEBSOCKET_PONG_EVENT "pong"

struct ClientSocketEvent
{
	std::string connectionId;
	std::string eventType;
	std::vector<std::string> eventArgs;
};

// One JSON-RPC request or notification, as a request handler receives it
struct ClientRequest
{
	std::string connectionId;
	// The request struct's name
	std::string method;
	// Empty for a notification, which is answered with nothing, not even an error
	JsonRpcId requestId;
	// The params object as JSON text, which is the request struct's fields
	std::string utf8RequestString;
};

struct ClientResponse
{
	std::string utf8String;
};

using SocketEventHandler= std::function<void(const ClientSocketEvent& event)>;
using RequestHandler= std::function<void(const ClientRequest& request, ClientResponse& response)>;

class IInterprocessMessageServer
{
public:
	virtual ~IInterprocessMessageServer() {}

	// bAllowRemote binds every interface instead of loopback, the scope the HTTP server
	// takes from the same setting, so a client on another machine can connect
	virtual bool initialize(bool bAllowRemote)= 0;
	virtual bool getIsAllowRemote() const= 0;
	virtual void dispose()= 0;
	virtual void setSocketEventHandler(const std::string& eventType, SocketEventHandler handler)= 0;
	// method is the request struct's name, which a request frame carries as its method
	virtual void setRequestHandler(const std::string& method, RequestHandler handler)= 0;
	virtual std::vector<std::string> getRequestHandlerNames() const= 0;

	virtual void sendMessageToClient(const std::string& connectionId, const std::string& message)= 0;
	virtual void sendMessageToAllClients(const std::string& message)= 0;
	virtual void processSocketEvents()= 0;
	virtual void processRequests()= 0;
};
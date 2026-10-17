#include "WebsocketInterprocessMessageClient.h"
#include "JsonUtils.h"
#include "MikanClientLogger.h"
#include "StringUtils.h"

#include "readerwriterqueue.h"

#include <ixwebsocket/IXWebSocket.h>

using LockFreeEventQueue= moodycamel::ReaderWriterQueue<std::string>;
using LockFreeEventQueuePtr= std::shared_ptr<LockFreeEventQueue>;

using WebSocketWeakPtr= std::weak_ptr<ix::WebSocket>;
using WebSocketPtr= std::shared_ptr<ix::WebSocket>;

//-- WebsocketConnectionState -----
class WebsocketConnectionState
{
public:
	WebsocketConnectionState(int protocolVersion)
		: m_protocolVersion(protocolVersion)
		, m_websocket(std::make_shared<ix::WebSocket>())
		, m_eventQueue(std::make_shared<LockFreeEventQueue>())
	{
		// Register the subprotocol once, here rather than per connect: ix::WebSocket only ever
		// appends to its subprotocol list, so adding it on each connect attempt grows the
		// Sec-WebSocket-Protocol header until the handshake is large enough for the server to abort
		// the connection mid-session.
		std::stringstream protocolName;
		protocolName << WEBSOCKET_PROTOCOL_PREFIX << m_protocolVersion;
		m_websocket->addSubProtocol(protocolName.str());

		m_websocket->setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) { handleWebSocketMessage(msg); });
	}

	~WebsocketConnectionState() { disconnect(); }

	const bool getIsConnected() const
	{
		auto readyState= m_websocket->getReadyState();
		return readyState == ix::ReadyState::Open || readyState == ix::ReadyState::Connecting;
	}

	inline WebSocketPtr getWebSocket() { return m_websocket; }

	inline LockFreeEventQueuePtr getServerEventQueue() { return m_eventQueue; }
	inline void setTextResponseHandler(IInterprocessMessageClient::TextResponseHandler handler)
	{
		m_textResponseHandler= handler;
	}

	MikanCoreResult connect(const std::string& host, const std::string& port)
	{
		if (getIsConnected())
		{
			MIKAN_MT_LOG_ERROR("WebsocketConnectionState::connect()") << "Already connected";
			return MikanCoreResult_AlreadyConnected;
		}

		std::string hostAddress= host.empty() ? WEBSOCKET_SERVER_ADDRESS : host;
		std::string hostPort= port.empty() ? WEBSOCKET_SERVER_PORT : port;
		m_websocket->setUrl(hostAddress + ":" + hostPort);
		m_websocket->start();

		return MikanCoreResult_Success;
	}

	bool disconnect(uint16_t code= 0, const std::string& reason= "")
	{
		if (getIsConnected())
		{
			if (code != 0)
			{
				m_websocket->stop(code, reason);
			}
			else
			{
				m_websocket->stop(ix::WebSocketCloseConstants::kNormalClosureCode,
								  ix::WebSocketCloseConstants::kNormalClosureMessage);
			}

			return true;
		}

		return false;
	}

	void handleWebSocketMessage(const ix::WebSocketMessagePtr& msg)
	{
		switch (msg->type)
		{
		case ix::WebSocketMessageType::Open:
		{
			MIKAN_MT_LOG_INFO("handleWebSocketMessage")
				<< "New connection"
				<< ", uri: " << msg->openInfo.uri << ", protocol: " << msg->openInfo.protocol;
		}
		break;
		case ix::WebSocketMessageType::Close:
		{
			std::stringstream ss;

			ss << WEBSOCKET_DISCONNECT_EVENT;
			ss << ":" << msg->closeInfo.code;
			ss << ":" << msg->closeInfo.reason;

			m_eventQueue->enqueue(ss.str());
		}
		break;
		case ix::WebSocketMessageType::Message:
		{
			if (!msg->binary)
			{
				JsonSaxStringValueSearcher searcher;

				if (searcher.hasKey(msg->str, "eventTypeName"))
				{
					m_eventQueue->enqueue(msg->str);
				}
				else if (searcher.hasKey(msg->str, "responseTypeName"))
				{
					if (m_textResponseHandler != nullptr)
					{
						m_textResponseHandler(msg->str);
					}
					else
					{
						MIKAN_MT_LOG_ERROR("ClientConnectionState::handleClientMessage")
							<< "Received response message but no handler set: " << msg->str;
					}
				}
				else
				{
					MIKAN_MT_LOG_ERROR("ClientConnectionState::handleClientMessage")
						<< "Received unsupported message: " << msg->str;
				}
			}
			else
			{
				// The protocol sends no binary frames
				MIKAN_MT_LOG_WARNING("ClientConnectionState::handleClientMessage")
					<< "Dropped a " << msg->str.size() << " byte binary message";
			}
		}
		break;
		case ix::WebSocketMessageType::Error:
		{
			MIKAN_MT_LOG_ERROR("handleWebSocketMessage") << "Error: " << msg->errorInfo.reason;
		}
		break;
		case ix::WebSocketMessageType::Ping:
		{
			MIKAN_MT_LOG_TRACE("handleWebSocketMessage") << "Ping";
		}
		break;
		case ix::WebSocketMessageType::Pong:
		{
			MIKAN_MT_LOG_TRACE("handleWebSocketMessage") << "Pong";
		}
		break;
		case ix::WebSocketMessageType::Fragment:
		{
			MIKAN_MT_LOG_TRACE("handleWebSocketMessage") << "Fragment";
		}
		break;
		}
	}

private:
	int m_protocolVersion= 0;
	WebSocketPtr m_websocket;
	ix::WebSocketHttpHeaders m_headers;
	LockFreeEventQueuePtr m_eventQueue;
	IInterprocessMessageClient::TextResponseHandler m_textResponseHandler;
	std::string m_connectionRequestJson;
};

//-- WebsocketInterprocessMessageClient -----
WebsocketInterprocessMessageClient::WebsocketInterprocessMessageClient(int protocolVersion)
	: m_connectionState(std::make_shared<WebsocketConnectionState>(protocolVersion))
{
}

WebsocketInterprocessMessageClient::~WebsocketInterprocessMessageClient() { dispose(); }

MikanCoreResult WebsocketInterprocessMessageClient::initialize() { return MikanCoreResult_Success; }

void WebsocketInterprocessMessageClient::dispose() { disconnect(0, ""); }

const bool WebsocketInterprocessMessageClient::getIsConnected() const { return m_connectionState->getIsConnected(); }

void WebsocketInterprocessMessageClient::setTextResponseHandler(IInterprocessMessageClient::TextResponseHandler handler)
{
	m_connectionState->setTextResponseHandler(handler);
}

MikanCoreResult WebsocketInterprocessMessageClient::connect(const std::string& host, const std::string& port)
{
	return m_connectionState->connect(host, port);
}

void WebsocketInterprocessMessageClient::disconnect(uint16_t code, const std::string& reason)
{
	m_connectionState->disconnect(code, reason);
}

MikanCoreResult WebsocketInterprocessMessageClient::fetchNextEvent(size_t utf8BufferSize, char* outUtf8Buffer,
																   size_t* outUtf8BufferSizeNeeded)
{
	auto eventQueue= m_connectionState->getServerEventQueue();
	const std::string* nextEvent= eventQueue->peek();

	if (nextEvent == nullptr)
		return MikanCoreResult_NoData;

	const size_t eventSize= nextEvent->size();
	const size_t bytesNeeded= eventSize + 1; // Include null terminator

	if (outUtf8Buffer != nullptr)
	{
		if (bytesNeeded > utf8BufferSize)
			return MikanCoreResult_BufferTooSmall;

		// Copy the utf-8 buffer from the event queue into the output buffer
		memcpy(outUtf8Buffer, nextEvent->c_str(), eventSize);

		// Null terminate the end of the string
		outUtf8Buffer[eventSize]= '\0';

		// Remove the event string from the queue
		eventQueue->pop();

		if (outUtf8BufferSizeNeeded != nullptr)
			*outUtf8BufferSizeNeeded= bytesNeeded;

		return MikanCoreResult_Success;
	}
	else
	{
		if (outUtf8BufferSizeNeeded == nullptr)
			return MikanCoreResult_NullParam;

		*outUtf8BufferSizeNeeded= bytesNeeded;
		return MikanCoreResult_Success;
	}
}

MikanCoreResult WebsocketInterprocessMessageClient::sendRequest(const std::string& utf8RequestString)
{
	ix::WebSocketSendInfo sendInfo= m_connectionState->getWebSocket()->sendText(utf8RequestString);

	if (!sendInfo.success)
	{
		MIKAN_LOG_ERROR("WebsocketInterprocessMessageClient::sendRequest()")
			<< "Failed to send request: " << utf8RequestString;
		return MikanCoreResult_SocketError;
	}

	return MikanCoreResult_Success;
}
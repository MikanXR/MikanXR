#include "MikanEventManager.h"
#include "MikanAPITypes.h"
#include "MikanCoreCAPI.h"
#include "MikanClientEvents.h"
#include "Logger.h"
#include "JsonDeserializer.h"
#include "StringUtils.h"
#include "SerializableObjectPtr.h"
#include "TypeRegistry.h"
#include "JsonRpc.h"

#include <string>

#include "nlohmann/json.hpp"

#define WEBSOCKET_CONNECT_EVENT "connect"
#define WEBSOCKET_DISCONNECT_EVENT "disconnect"
#define WEBSOCKET_ERROR_EVENT "error"
#define WEBSOCKET_PING_EVENT "ping"
#define WEBSOCKET_PONG_EVENT "pong"

using json= nlohmann::json;

MikanAPIResult MikanEventManager::init(MikanContext context)
{
	m_context= context;

	return MikanAPIResult::Success;
}

MikanAPIResult MikanEventManager::fetchNextEvent(MikanEventPtr& out_event)
{
	size_t utf8BytesWritten= 0;

	MikanAPIResult result=
		(MikanAPIResult)Mikan_FetchNextEvent(m_context, m_eventBuffer.size(), m_eventBuffer.data(), &utf8BytesWritten);
	if (result == MikanAPIResult::BufferTooSmall)
	{
		// A null buffer reports the size of the event at the head of the queue, so one larger
		// than the buffer is fetched into a buffer grown to fit rather than left wedged there
		size_t bytesNeeded= 0;
		result= (MikanAPIResult)Mikan_FetchNextEvent(m_context, 0, nullptr, &bytesNeeded);
		if (result != MikanAPIResult::Success)
		{
			return result;
		}

		m_eventBuffer.resize(bytesNeeded);
		result= (MikanAPIResult)Mikan_FetchNextEvent(m_context, m_eventBuffer.size(), m_eventBuffer.data(),
													 &utf8BytesWritten);
	}

	if (result == MikanAPIResult::Success)
	{
		out_event= parseEventString(m_eventBuffer.data());
		if (!out_event)
		{
			MIKAN_MT_LOG_WARNING("MikanClient::fetchNextEvent()")
				<< "Failed to parse event string: " << m_eventBuffer.data();
			result= MikanAPIResult::MalformedResponse;
		}
	}

	return result;
}

MikanEventPtr MikanEventManager::parseEventString(const char* szUtf8EventString)
{
	MikanEventPtr eventPtr;

	try
	{
		std::string eventString= szUtf8EventString;

		if (eventString.rfind(WEBSOCKET_DISCONNECT_EVENT, 0) == 0)
		{
			int disconnectCode= 0;
			std::string disconnectReason= "";

			std::vector<std::string> tokens= StringUtils::splitString(eventString, ':');
			if (tokens.size() >= 3)
			{
				disconnectCode= std::atoi(tokens[1].c_str());
				disconnectReason= tokens[2].c_str();
			}

			MIKAN_MT_LOG_INFO("MikanClient::parseEventString()")
				<< "Received websocket DISCONNECT"
				<< ", disconnectCode: " << disconnectCode << ", protocol: " << disconnectReason;

			auto disconnectEventPtr= std::make_shared<MikanDisconnectedEvent>();
			disconnectEventPtr->code= (MikanDisconnectCode)disconnectCode;
			disconnectEventPtr->reason.setUtf8Value(disconnectReason.c_str());

			eventPtr= disconnectEventPtr;
		}
		else
		{
			// An event is a notification from the server whose method is the event struct's name
			JsonRpcFrame frame;
			if (JsonRpc::parseIncoming(eventString, frame) != eJsonRpcFrameKind::notification)
			{
				MIKAN_MT_LOG_ERROR("MikanClient::parseEventString()") << "Not an event notification: " << eventString;
				return nullptr;
			}

			const JsonRpcRequest& notification= frame.request;
			Serialization::StructTypeHandle eventStruct=
				Serialization::TypeRegistry::getStructByName(notification.method);
			if (eventStruct != nullptr)
			{
				eventPtr= eventStruct->makeSharedInstance<MikanEvent>();

				std::string parseEventError;
				if (Serialization::deserializeFromJson(notification.params, eventPtr.get(), eventStruct,
													   parseEventError))
				{
					eventPtr->eventTypeName.setUtf8Value(notification.method.c_str());
				}
				else
				{
					MIKAN_MT_LOG_ERROR("MikanClient::parseEventString()")
						<< "Failed to parse event: " << parseEventError;
					eventPtr= nullptr;
				}
			}
			else
			{
				MIKAN_MT_LOG_WARNING("MikanClient::parseEventString()")
					<< "Received an event of unknown type: " << notification.method;
			}
		}
	}
	catch (json::parse_error& e)
	{
		MIKAN_MT_LOG_ERROR("MikanClient::parseEventString()") << "Failed to parse event: " << e.what();
	}
	catch (json::exception& e)
	{
		MIKAN_MT_LOG_ERROR("MikanClient::parseEventString()") << "Failed to parse event: " << e.what();
	}

	return eventPtr;
}
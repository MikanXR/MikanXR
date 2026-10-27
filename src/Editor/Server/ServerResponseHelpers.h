#pragma once

#include "IEntityAccessor.h"
#include "JsonDeserializer.h"
#include "JsonRpc.h"
#include "JsonSerializer.h"
#include "InterprocessMessageServerInterface.h"
#include "Logger.h"
#include "MikanAPITypes.h"
#include "ReflectionHandles.h"

#include <nlohmann/json.hpp>

using json= nlohmann::json;

// The error a MikanAPIResult travels as: the mapped code, the result's name as the message,
// and the name again as data.resultCode, which is what a client fills its response in from
JsonRpcError makeMikanJsonRpcError(MikanAPIResult result);

// An event as the notification that carries it, the event struct's name as the method
template <typename t_mikan_event>
std::string mikanEventToJsonRpc(const t_mikan_event& mikanEvent)
{
	json params;
	std::string errorMsg;
	if (!Serialization::serializeToJson(mikanEvent, params, errorMsg))
	{
		MIKAN_LOG_ERROR("ServerResponseHelpers::mikanEventToJsonRpc")
			<< "Failed to serialize Mikan event to JSON: " << errorMsg;
		return std::string();
	}

	// An event struct with no fields serializes to null, and params must be an object
	if (params.is_null())
	{
		params= json::object();
	}

	return JsonRpc::formatNotification(Serialization::getStructTypeName(&t_mikan_event::staticGetArchetype()), params);
}

template <typename t_mikan_type>
bool readTypedRequest(const std::string& utf8RequestString, t_mikan_type& outParameters)
{
	std::string errorMsg;
	if (!Serialization::deserializeFromJsonString(utf8RequestString, outParameters, errorMsg))
	{
		MIKAN_LOG_ERROR("MikanServer::readRequestPayload") << "Failed to parse JSON: " << errorMsg;
		return false;
	}

	return true;
}

// Answers with an arbitrary JSON result. Nothing is written for a notification.
void writeJsonResultResponse(const JsonRpcId& requestId, const json& result, ClientResponse& response);

// Answers with the response struct as the result. Nothing is written for a notification.
template <typename t_mikan_type>
void writeTypedJsonResponse(const JsonRpcId& requestId, t_mikan_type& result, ClientResponse& response)
{
	if (!requestId.has_value())
	{
		response.utf8String.clear();
		return;
	}

	json resultJson;
	std::string errorMsg;
	if (!Serialization::serializeToJson(result, resultJson, errorMsg))
	{
		MIKAN_LOG_ERROR("ServerResponseHelpers::writeTypedJsonResponse")
			<< "Failed to serialize MikanResponse to JSON: " << errorMsg;
		response.utf8String= JsonRpc::formatError(*requestId, makeMikanJsonRpcError(MikanAPIResult::GeneralError));
		return;
	}

	writeJsonResultResponse(requestId, resultJson, response);
}

// Answers Success with an empty result and any other result with an error. Nothing is written
// for a notification.
void writeSimpleJsonResponse(const JsonRpcId& requestId, MikanAPIResult result, ClientResponse& response);

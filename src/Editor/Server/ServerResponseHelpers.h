#pragma once

#include "IEntityAccessor.h"
#include "JsonDeserializer.h"
#include "JsonSerializer.h"
#include "InterprocessMessageServerInterface.h"
#include "Logger.h"
#include "MikanAPITypes.h"

#include <nlohmann/json.hpp>

using json= nlohmann::json;

template <typename t_mikan_type>
std::string mikanTypeToJsonString(const t_mikan_type& mikanType)
{
	std::string jsonStr;
	std::string errorMsg;
	if (!Serialization::serializeToJsonString(mikanType, jsonStr, errorMsg))
	{
		MIKAN_LOG_ERROR("ServerResponseHelpers::mikanTypeToJsonString")
			<< "Failed to serialize Mikan type to JSON string: " << errorMsg;
	}

	return jsonStr;
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

template <typename t_mikan_type>
void writeTypedJsonResponse(MikanRequestID requestId, t_mikan_type& result, ClientResponse& response)
{
	result.requestId= requestId;
	result.resultCode= MikanAPIResult::Success;

	std::string errorMsg;
	if (!Serialization::serializeToJsonString(result, response.utf8String, errorMsg))
	{
		MIKAN_LOG_ERROR("ServerResponseHelpers::writeTypedJsonResponse")
			<< "Failed to serialize MikanResponse to JSON string: " << errorMsg;
	}
}

void writeSimpleJsonResponse(MikanRequestID requestId, MikanAPIResult result, ClientResponse& response);

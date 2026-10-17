#include "ServerResponseHelpers.h"

void writeSimpleJsonResponse(MikanRequestID requestId, MikanAPIResult result, ClientResponse& response)
{
	// Only write a response if the request ID is valid (i.e. the client expects a response)
	if (requestId != INVALID_MIKAN_ID)
	{
		MikanResponse mikanResponse;
		mikanResponse.requestId= requestId;
		mikanResponse.resultCode= result;

		std::string errorMsg;
		if (!Serialization::serializeToJsonString(mikanResponse, response.utf8String, errorMsg))
		{
			MIKAN_LOG_ERROR("ServerResponseHelpers::writeSimpleJsonResponse")
				<< "Failed to serialize MikanResponse to JSON string: " << errorMsg;
		}
	}
	else
	{
		response.utf8String= "";
	}
}

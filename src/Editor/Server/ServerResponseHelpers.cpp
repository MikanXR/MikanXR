#include "ServerResponseHelpers.h"
#include "TypeRegistry.h"

JsonRpcError makeMikanJsonRpcError(MikanAPIResult result)
{
	const char* resultName=
		Serialization::getEnumValueName(Serialization::TypeRegistry::getEnumByName("MikanAPIResult"), (int64_t)result);
	const std::string resultNameString= resultName != nullptr ? resultName : std::to_string((int)result);

	JsonRpcError error;
	error.code= jsonRpcErrorCode(result);
	error.message= resultNameString;
	error.data= json{{"resultCode", resultNameString}};

	return error;
}

void writeJsonResultResponse(const JsonRpcId& requestId, const json& result, ClientResponse& response)
{
	if (!requestId.has_value())
	{
		response.utf8String.clear();
		return;
	}

	// A response struct with no fields serializes to null, and the plain response is {}
	response.utf8String= JsonRpc::formatResult(*requestId, result.is_null() ? json::object() : result);
}

void writeSimpleJsonResponse(const JsonRpcId& requestId, MikanAPIResult result, ClientResponse& response)
{
	if (!requestId.has_value())
	{
		response.utf8String.clear();
		return;
	}

	if (result == MikanAPIResult::Success)
	{
		response.utf8String= JsonRpc::formatResult(*requestId, json::object());
	}
	else
	{
		response.utf8String= JsonRpc::formatError(*requestId, makeMikanJsonRpcError(result));
	}
}

#include "JsonRpc.h"

using json= nlohmann::json;

namespace
{
const char* k_jsonRpcVersion= "2.0";

eJsonRpcFrameKind rejectFrame(JsonRpcFrame& outFrame, int code, const char* message, const json& id= nullptr)
{
	outFrame.kind= eJsonRpcFrameKind::invalid;
	outFrame.error.code= code;
	outFrame.error.message= message;
	outFrame.error.data= nullptr;
	outFrame.errorId= id;

	return outFrame.kind;
}

// The spec allows a string, a number or null. A number with a fraction is refused, since
// nothing that sends one can be answered with the same id it expects.
bool isValidId(const json& id) { return id.is_string() || id.is_number_integer() || id.is_null(); }

bool parseError(const json& errorJson, JsonRpcError& outError)
{
	if (!errorJson.is_object())
	{
		return false;
	}

	auto codeIt= errorJson.find("code");
	auto messageIt= errorJson.find("message");
	if (codeIt == errorJson.end() || !codeIt->is_number_integer() || messageIt == errorJson.end()
		|| !messageIt->is_string())
	{
		return false;
	}

	outError.code= codeIt->get<int>();
	outError.message= messageIt->get<std::string>();

	auto dataIt= errorJson.find("data");
	outError.data= (dataIt != errorJson.end()) ? *dataIt : json(nullptr);

	return true;
}
} // namespace

namespace JsonRpc
{
eJsonRpcFrameKind parseIncoming(const std::string& utf8Frame, JsonRpcFrame& outFrame)
{
	outFrame= JsonRpcFrame();

	json frameJson= json::parse(utf8Frame, nullptr, false);
	if (frameJson.is_discarded())
	{
		return rejectFrame(outFrame, k_jsonRpcParseError, "Parse error");
	}

	// Batches are not supported, so a batch is answered with one error rather than one per call
	if (!frameJson.is_object())
	{
		return rejectFrame(outFrame, k_jsonRpcInvalidRequest, "Invalid Request");
	}

	// The id is read first so an otherwise invalid frame is answered to the id it carried
	json readableId= nullptr;
	auto idIt= frameJson.find("id");
	const bool bHasId= idIt != frameJson.end();
	if (bHasId && isValidId(*idIt))
	{
		readableId= *idIt;
	}

	auto versionIt= frameJson.find("jsonrpc");
	if (versionIt == frameJson.end() || !versionIt->is_string() || versionIt->get<std::string>() != k_jsonRpcVersion)
	{
		return rejectFrame(outFrame, k_jsonRpcInvalidRequest, "Invalid Request", readableId);
	}

	if (bHasId && !isValidId(*idIt))
	{
		return rejectFrame(outFrame, k_jsonRpcInvalidRequest, "Invalid Request");
	}

	auto methodIt= frameJson.find("method");
	if (methodIt != frameJson.end())
	{
		if (!methodIt->is_string())
		{
			return rejectFrame(outFrame, k_jsonRpcInvalidRequest, "Invalid Request", readableId);
		}

		JsonRpcRequest& request= outFrame.request;
		request.method= methodIt->get<std::string>();

		auto paramsIt= frameJson.find("params");
		if (paramsIt != frameJson.end())
		{
			if (!paramsIt->is_object() && !paramsIt->is_array())
			{
				return rejectFrame(outFrame, k_jsonRpcInvalidRequest, "Invalid Request", readableId);
			}

			request.params= *paramsIt;
		}

		if (bHasId)
		{
			request.id= *idIt;
		}

		outFrame.kind= bHasId ? eJsonRpcFrameKind::request : eJsonRpcFrameKind::notification;
		return outFrame.kind;
	}

	auto resultIt= frameJson.find("result");
	auto errorIt= frameJson.find("error");
	const bool bHasResult= resultIt != frameJson.end();
	const bool bHasError= errorIt != frameJson.end();
	if (bHasId && bHasResult != bHasError)
	{
		JsonRpcResponse& response= outFrame.response;
		response.id= *idIt;

		if (bHasResult)
		{
			response.result= *resultIt;
		}
		else
		{
			JsonRpcError error;
			if (!parseError(*errorIt, error))
			{
				return rejectFrame(outFrame, k_jsonRpcInvalidRequest, "Invalid Request", readableId);
			}

			response.error= error;
		}

		outFrame.kind= eJsonRpcFrameKind::response;
		return outFrame.kind;
	}

	return rejectFrame(outFrame, k_jsonRpcInvalidRequest, "Invalid Request", readableId);
}

std::string formatRequest(const std::string& method, const json& params, const json& id)
{
	json frameJson= {{"jsonrpc", k_jsonRpcVersion}, {"method", method}, {"params", params}, {"id", id}};

	return frameJson.dump();
}

std::string formatNotification(const std::string& method, const json& params)
{
	json frameJson= {{"jsonrpc", k_jsonRpcVersion}, {"method", method}, {"params", params}};

	return frameJson.dump();
}

std::string formatResult(const json& id, const json& result)
{
	json frameJson= {{"jsonrpc", k_jsonRpcVersion}, {"id", id}, {"result", result}};

	return frameJson.dump();
}

std::string formatError(const json& id, const JsonRpcError& error)
{
	json errorJson= {{"code", error.code}, {"message", error.message}};
	if (!error.data.is_null())
	{
		errorJson["data"]= error.data;
	}

	json frameJson= {{"jsonrpc", k_jsonRpcVersion}, {"id", id}, {"error", errorJson}};

	return frameJson.dump();
}
} // namespace JsonRpc

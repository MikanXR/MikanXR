#pragma once

#include "MikanUtilityExport.h"

#include "nlohmann/json.hpp"

#include <optional>
#include <string>

// The JSON-RPC 2.0 envelope every websocket text frame between the editor and a client travels in.
// A request names a method, carries its params by name and carries an id. A request without an id
// is a notification and is never answered, not even with an error. A response carries the id it
// answers and exactly one of result or error. The editor's events are notifications it sends.

// The error codes JSON-RPC reserves. Every other code on the wire is a result enum's own value.
constexpr int k_jsonRpcParseError= -32700;
constexpr int k_jsonRpcInvalidRequest= -32600;
constexpr int k_jsonRpcMethodNotFound= -32601;
constexpr int k_jsonRpcInvalidParams= -32602;

// A request id as the client sent it: an integer, a string, or null. Empty on a notification.
using JsonRpcId= std::optional<nlohmann::json>;

struct JsonRpcRequest
{
	std::string method;
	// An object or an array. A request that carried no params holds an empty object.
	nlohmann::json params= nlohmann::json::object();
	JsonRpcId id;

	bool isNotification() const { return !id.has_value(); }
};

struct JsonRpcError
{
	int code= 0;
	std::string message;
	// Null when the error carries no data
	nlohmann::json data;
};

struct JsonRpcResponse
{
	// Null when it answers a frame whose id could not be read
	nlohmann::json id;
	// Exactly one of these is set
	std::optional<nlohmann::json> result;
	std::optional<JsonRpcError> error;
};

enum class eJsonRpcFrameKind
{
	request,
	notification,
	response,
	invalid
};

struct JsonRpcFrame
{
	eJsonRpcFrameKind kind= eJsonRpcFrameKind::invalid;
	// Filled for a request or a notification
	JsonRpcRequest request;
	// Filled for a response
	JsonRpcResponse response;
	// Filled for an invalid frame: the error that answers it (parse error or invalid request),
	// addressed to the frame's id when it could be read and to null otherwise
	JsonRpcError error;
	nlohmann::json errorId;
};

// The error code a result enum travels as. UnknownFunction and MalformedParameters take the
// reserved codes that mean the same thing. Every other result keeps its own value, which lies
// outside the reserved range. A template over the enum, so this library needs no API types.
template <typename t_result_enum>
int jsonRpcErrorCode(t_result_enum result)
{
	switch (result)
	{
	case t_result_enum::UnknownFunction:
		return k_jsonRpcMethodNotFound;
	case t_result_enum::MalformedParameters:
		return k_jsonRpcInvalidParams;
	default:
		return (int)result;
	}
}

namespace JsonRpc
{
// Classifies a frame. A batch (a top level array) is invalid, since batches are not supported.
MIKAN_UTILITY_FUNC(eJsonRpcFrameKind) parseIncoming(const std::string& utf8Frame, JsonRpcFrame& outFrame);

MIKAN_UTILITY_FUNC(std::string)
formatRequest(const std::string& method, const nlohmann::json& params, const nlohmann::json& id);
MIKAN_UTILITY_FUNC(std::string) formatNotification(const std::string& method, const nlohmann::json& params);
MIKAN_UTILITY_FUNC(std::string) formatResult(const nlohmann::json& id, const nlohmann::json& result);
MIKAN_UTILITY_FUNC(std::string) formatError(const nlohmann::json& id, const JsonRpcError& error);
} // namespace JsonRpc

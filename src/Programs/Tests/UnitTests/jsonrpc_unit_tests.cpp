//-- includes -----
#include "JsonRpc.h"
#include "MikanAPITypes.h"

#include "unit_test.h"

#include <stdio.h>
#include <string>

using json= nlohmann::json;

//-- public interface -----
bool run_jsonrpc_unit_tests()
{
	UNIT_TEST_MODULE_BEGIN("jsonrpc")
	UNIT_TEST_MODULE_CALL_TEST(jsonrpc_test_request_round_trip);
	UNIT_TEST_MODULE_CALL_TEST(jsonrpc_test_notification_has_no_id);
	UNIT_TEST_MODULE_CALL_TEST(jsonrpc_test_string_and_null_ids);
	UNIT_TEST_MODULE_CALL_TEST(jsonrpc_test_result_round_trip);
	UNIT_TEST_MODULE_CALL_TEST(jsonrpc_test_error_round_trip);
	UNIT_TEST_MODULE_CALL_TEST(jsonrpc_test_parse_error_has_null_id);
	UNIT_TEST_MODULE_CALL_TEST(jsonrpc_test_batch_rejected);
	UNIT_TEST_MODULE_CALL_TEST(jsonrpc_test_invalid_requests);
	UNIT_TEST_MODULE_CALL_TEST(jsonrpc_test_error_code_mapping);
	UNIT_TEST_MODULE_END()
}

namespace
{
bool jsonRpcCheck(bool condition, const char* what)
{
	if (!condition)
	{
		fprintf(stdout, "      failed: %s\n", what);
	}

	return condition;
}
} // namespace

bool jsonrpc_test_request_round_trip()
{
	UNIT_TEST_BEGIN("request round trip")

	const json params= {{"command", "test_command"}, {"parameters", json::array({"a", "b"})}};
	const std::string frameText= JsonRpc::formatRequest("MikanRemoteControlCommand", params, 7);

	const json frameJson= json::parse(frameText);
	success&= jsonRpcCheck(frameJson["jsonrpc"] == "2.0", "jsonrpc is 2.0");
	success&= jsonRpcCheck(frameJson["id"] == 7, "id written");

	JsonRpcFrame frame;
	success&=
		jsonRpcCheck(JsonRpc::parseIncoming(frameText, frame) == eJsonRpcFrameKind::request, "classified as request");
	success&= jsonRpcCheck(frame.request.method == "MikanRemoteControlCommand", "method survives");
	success&= jsonRpcCheck(frame.request.params == params, "params survive");
	success&= jsonRpcCheck(frame.request.id.has_value() && *frame.request.id == 7, "id survives");
	success&= jsonRpcCheck(!frame.request.isNotification(), "not a notification");

	UNIT_TEST_COMPLETE()
}

bool jsonrpc_test_notification_has_no_id()
{
	UNIT_TEST_BEGIN("notification without id")

	const std::string frameText= JsonRpc::formatNotification("MikanConnectedEvent", {{"httpPort", 8090}});
	success&= jsonRpcCheck(!json::parse(frameText).contains("id"), "no id written");

	JsonRpcFrame frame;
	success&= jsonRpcCheck(JsonRpc::parseIncoming(frameText, frame) == eJsonRpcFrameKind::notification,
						   "classified as notification");
	success&= jsonRpcCheck(frame.request.isNotification(), "request has no id");
	success&= jsonRpcCheck(frame.request.params["httpPort"] == 8090, "params survive");

	// A notification without params carries an empty object
	success&= jsonRpcCheck(JsonRpc::parseIncoming(R"({"jsonrpc":"2.0","method":"Ping"})", frame)
							   == eJsonRpcFrameKind::notification,
						   "params are optional");
	success&= jsonRpcCheck(frame.request.params.is_object() && frame.request.params.empty(), "missing params are {}");

	UNIT_TEST_COMPLETE()
}

bool jsonrpc_test_string_and_null_ids()
{
	UNIT_TEST_BEGIN("string and null ids are requests")

	JsonRpcFrame frame;
	success&=
		jsonRpcCheck(JsonRpc::parseIncoming(R"({"jsonrpc":"2.0","method":"rpc.discover","params":[],"id":"1"})", frame)
						 == eJsonRpcFrameKind::request,
					 "string id is a request");
	success&= jsonRpcCheck(frame.request.id.has_value() && *frame.request.id == "1", "string id survives");
	success&=
		jsonRpcCheck(frame.request.params.is_array() && frame.request.params.empty(), "empty array params survive");

	success&= jsonRpcCheck(JsonRpc::parseIncoming(R"({"jsonrpc":"2.0","method":"X","id":null})", frame)
							   == eJsonRpcFrameKind::request,
						   "null id is a request, not a notification");

	UNIT_TEST_COMPLETE()
}

bool jsonrpc_test_result_round_trip()
{
	UNIT_TEST_BEGIN("result round trip")

	const std::string frameText= JsonRpc::formatResult(3, {{"value", 1.5}});

	JsonRpcFrame frame;
	success&=
		jsonRpcCheck(JsonRpc::parseIncoming(frameText, frame) == eJsonRpcFrameKind::response, "classified as response");
	success&= jsonRpcCheck(frame.response.id == 3, "id survives");
	success&=
		jsonRpcCheck(frame.response.result.has_value() && (*frame.response.result)["value"] == 1.5, "result survives");
	success&= jsonRpcCheck(!frame.response.error.has_value(), "no error");

	// The plain response is an empty result
	success&= jsonRpcCheck(JsonRpc::parseIncoming(JsonRpc::formatResult(4, json::object()), frame)
							   == eJsonRpcFrameKind::response,
						   "empty result is a response");
	success&=
		jsonRpcCheck(frame.response.result.has_value() && frame.response.result->empty(), "empty result survives");

	UNIT_TEST_COMPLETE()
}

bool jsonrpc_test_error_round_trip()
{
	UNIT_TEST_BEGIN("error round trip")

	JsonRpcError error;
	error.code= 105;
	error.message= "InvalidCameraID";
	error.data= {{"resultCode", "InvalidCameraID"}};
	const std::string frameText= JsonRpc::formatError(5, error);

	JsonRpcFrame frame;
	success&=
		jsonRpcCheck(JsonRpc::parseIncoming(frameText, frame) == eJsonRpcFrameKind::response, "classified as response");
	success&= jsonRpcCheck(frame.response.id == 5, "id survives");
	success&= jsonRpcCheck(!frame.response.result.has_value(), "no result");
	success&= jsonRpcCheck(frame.response.error.has_value(), "error present");
	if (frame.response.error.has_value())
	{
		success&= jsonRpcCheck(frame.response.error->code == 105, "code survives");
		success&= jsonRpcCheck(frame.response.error->message == "InvalidCameraID", "message survives");
		success&= jsonRpcCheck(frame.response.error->data["resultCode"] == "InvalidCameraID", "data survives");
	}

	// An error without data writes none
	success&= jsonRpcCheck(
		!json::parse(JsonRpc::formatError(6, JsonRpcError{-32600, "Invalid Request", nullptr}))["error"].contains(
			"data"),
		"null data is omitted");

	UNIT_TEST_COMPLETE()
}

bool jsonrpc_test_parse_error_has_null_id()
{
	UNIT_TEST_BEGIN("parse error answers with a null id")

	JsonRpcFrame frame;
	success&= jsonRpcCheck(JsonRpc::parseIncoming(R"({"jsonrpc":"2.0","method":"X","id":1)", frame)
							   == eJsonRpcFrameKind::invalid,
						   "truncated frame is invalid");
	success&= jsonRpcCheck(frame.error.code == k_jsonRpcParseError, "code is -32700");
	success&= jsonRpcCheck(frame.errorId.is_null(), "id is null");

	const json reply= json::parse(JsonRpc::formatError(frame.errorId, frame.error));
	success&= jsonRpcCheck(reply.contains("id") && reply["id"].is_null(), "reply carries id null");
	success&= jsonRpcCheck(reply["error"]["code"] == -32700, "reply carries -32700");

	UNIT_TEST_COMPLETE()
}

bool jsonrpc_test_batch_rejected()
{
	UNIT_TEST_BEGIN("batch answered with one invalid request")

	JsonRpcFrame frame;
	success&=
		jsonRpcCheck(JsonRpc::parseIncoming(
						 R"([{"jsonrpc":"2.0","method":"A","id":1},{"jsonrpc":"2.0","method":"B","id":2}])", frame)
						 == eJsonRpcFrameKind::invalid,
					 "batch is invalid");
	success&= jsonRpcCheck(frame.error.code == k_jsonRpcInvalidRequest, "code is -32600");
	success&= jsonRpcCheck(frame.errorId.is_null(), "id is null");

	success&= jsonRpcCheck(JsonRpc::parseIncoming("[]", frame) == eJsonRpcFrameKind::invalid, "empty batch is invalid");
	success&= jsonRpcCheck(frame.error.code == k_jsonRpcInvalidRequest, "empty batch code is -32600");

	UNIT_TEST_COMPLETE()
}

bool jsonrpc_test_invalid_requests()
{
	UNIT_TEST_BEGIN("invalid requests")

	JsonRpcFrame frame;

	// The id an invalid frame carried is the one it is answered to
	success&= jsonRpcCheck(JsonRpc::parseIncoming(R"({"method":"X","id":9})", frame) == eJsonRpcFrameKind::invalid,
						   "missing jsonrpc is invalid");
	success&= jsonRpcCheck(frame.error.code == k_jsonRpcInvalidRequest && frame.errorId == 9, "answered to its id");

	success&= jsonRpcCheck(JsonRpc::parseIncoming(R"({"jsonrpc":"1.0","method":"X","id":1})", frame)
							   == eJsonRpcFrameKind::invalid,
						   "wrong version is invalid");
	success&= jsonRpcCheck(JsonRpc::parseIncoming(R"({"jsonrpc":"2.0","method":5,"id":1})", frame)
							   == eJsonRpcFrameKind::invalid,
						   "non-string method is invalid");
	success&= jsonRpcCheck(JsonRpc::parseIncoming(R"({"jsonrpc":"2.0","method":"X","params":3,"id":1})", frame)
							   == eJsonRpcFrameKind::invalid,
						   "scalar params are invalid");
	success&= jsonRpcCheck(JsonRpc::parseIncoming(R"({"jsonrpc":"2.0","method":"X","id":1.5})", frame)
							   == eJsonRpcFrameKind::invalid,
						   "fractional id is invalid");
	success&= jsonRpcCheck(frame.errorId.is_null(), "fractional id is not echoed");
	success&= jsonRpcCheck(
		JsonRpc::parseIncoming(R"({"jsonrpc":"2.0","id":1,"result":{},"error":{"code":1,"message":"x"}})", frame)
			== eJsonRpcFrameKind::invalid,
		"result and error together are invalid");
	success&= jsonRpcCheck(JsonRpc::parseIncoming(R"({"jsonrpc":"2.0","id":1,"error":{"code":"x"}})", frame)
							   == eJsonRpcFrameKind::invalid,
						   "malformed error is invalid");
	success&= jsonRpcCheck(JsonRpc::parseIncoming(R"("text")", frame) == eJsonRpcFrameKind::invalid,
						   "scalar frame is invalid");
	success&= jsonRpcCheck(frame.error.code == k_jsonRpcInvalidRequest, "scalar frame code is -32600");

	UNIT_TEST_COMPLETE()
}

bool jsonrpc_test_error_code_mapping()
{
	UNIT_TEST_BEGIN("MikanAPIResult error code mapping")

	success&= jsonRpcCheck(jsonRpcErrorCode(MikanAPIResult::UnknownFunction) == k_jsonRpcMethodNotFound,
						   "UnknownFunction is -32601");
	success&= jsonRpcCheck(jsonRpcErrorCode(MikanAPIResult::MalformedParameters) == k_jsonRpcInvalidParams,
						   "MalformedParameters is -32602");

	// Every other result keeps its value, outside the range JSON-RPC reserves
	const MikanAPIResult keptResults[]= {
		MikanAPIResult::GeneralError,     MikanAPIResult::Uninitialized,
		MikanAPIResult::NullParam,        MikanAPIResult::InvalidParam,
		MikanAPIResult::RequestFailed,    MikanAPIResult::NotConnected,
		MikanAPIResult::AlreadyConnected, MikanAPIResult::SocketError,
		MikanAPIResult::Timeout,          MikanAPIResult::Canceled,
		MikanAPIResult::NoData,           MikanAPIResult::BufferTooSmall,
		MikanAPIResult::UnknownClient,    MikanAPIResult::MalformedResponse,
		MikanAPIResult::NoVideoSource,    MikanAPIResult::NoVideoSourceAssignedTracker,
		MikanAPIResult::InvalidDeviceId,  MikanAPIResult::InvalidStencilID,
		MikanAPIResult::InvalidAnchorID,  MikanAPIResult::InvalidCameraID,
		MikanAPIResult::InvalidSceneID,   MikanAPIResult::InvalidStageID,
		MikanAPIResult::InvalidShapeID,
	};
	for (MikanAPIResult result : keptResults)
	{
		const int code= jsonRpcErrorCode(result);
		if (code != (int)result || (code <= -32000 && code >= -32768))
		{
			fprintf(stdout, "      result %d maps to %d\n", (int)result, code);
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

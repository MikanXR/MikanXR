//-- includes -----
#include <assert.h>
#include <stdio.h>
#include <string>

#include "MikanAPITypes.h"
#include "MikanRemoteControlEvents.h"
#include "MikanRemoteControlRequests.h"
#include "ReflectionHandles.h"
#include "TypeRegistry.h"
#include "JsonRpc.h"
#include "JsonSerializer.h"
#include "JsonDeserializer.h"

#include "unit_test.h"

#include <nlohmann/json.hpp>

using json= nlohmann::json;

//-- public interface -----
bool run_mikan_api_unit_tests()
{
	UNIT_TEST_MODULE_BEGIN("mikan_api")
	UNIT_TEST_MODULE_CALL_TEST(mikan_api_test_type_registry_build);
	UNIT_TEST_MODULE_CALL_TEST(mikan_api_test_type_registry_lookup);
	UNIT_TEST_MODULE_CALL_TEST(mikan_api_test_type_registry_unknown_returns_null);
	UNIT_TEST_MODULE_CALL_TEST(mikan_api_test_type_name_set_on_construct);
	UNIT_TEST_MODULE_CALL_TEST(mikan_api_test_request_envelope);
	UNIT_TEST_MODULE_CALL_TEST(mikan_api_test_response_round_trip);
	UNIT_TEST_MODULE_CALL_TEST(mikan_api_test_error_response_result_code);
	UNIT_TEST_MODULE_CALL_TEST(mikan_api_test_event_notification_round_trip);
	UNIT_TEST_MODULE_END()
}

//-- TypeRegistry tests -----

bool mikan_api_test_type_registry_build()
{
	UNIT_TEST_BEGIN("type registry build")

	Serialization::TypeRegistry::build();

	// After building, core Mikan types must be registered
	Serialization::StructTypeHandle requestStruct= Serialization::TypeRegistry::getStructByName("MikanRequest");
	assert(requestStruct != nullptr);

	Serialization::StructTypeHandle responseStruct= Serialization::TypeRegistry::getStructByName("MikanResponse");
	assert(responseStruct != nullptr);

	Serialization::StructTypeHandle eventStruct= Serialization::TypeRegistry::getStructByName("MikanEvent");
	assert(eventStruct != nullptr);

	UNIT_TEST_COMPLETE()
}

bool mikan_api_test_type_registry_lookup()
{
	UNIT_TEST_BEGIN("type registry lookup by name")

	Serialization::TypeRegistry::build();

	// Concrete request type must also be found
	Serialization::StructTypeHandle cmdStruct=
		Serialization::TypeRegistry::getStructByName("MikanRemoteControlCommand");
	assert(cmdStruct != nullptr);

	// The returned struct's name must match what we queried
	assert(std::string(cmdStruct->getName()) == "MikanRemoteControlCommand");

	// The returned struct must be usable for dynamic instantiation
	auto instance= cmdStruct->makeSharedInstance<MikanRequest>();
	assert(instance != nullptr);
	assert(std::string(instance->requestTypeName.getUtf8Value()) == "MikanRemoteControlCommand");

	UNIT_TEST_COMPLETE()
}

bool mikan_api_test_type_registry_unknown_returns_null()
{
	UNIT_TEST_BEGIN("type registry unknown type returns null")

	Serialization::TypeRegistry::build();

	Serialization::StructTypeHandle unknownStruct= Serialization::TypeRegistry::getStructByName("ThisTypeDoesNotExist");
	assert(unknownStruct == nullptr);

	UNIT_TEST_COMPLETE()
}

//-- MikanAPITypes constructor tests -----

bool mikan_api_test_type_name_set_on_construct()
{
	UNIT_TEST_BEGIN("type name set in constructor")

	// Base types set their own name
	MikanRequest request;
	assert(std::string(request.requestTypeName.getUtf8Value()) == "MikanRequest");

	MikanResponse response;
	assert(std::string(response.responseTypeName.getUtf8Value()) == "MikanResponse");

	MikanEvent event;
	assert(std::string(event.eventTypeName.getUtf8Value()) == "MikanEvent");

	// Derived types set the derived type name, not the base name
	MikanRemoteControlCommand cmd;
	assert(std::string(cmd.requestTypeName.getUtf8Value()) == "MikanRemoteControlCommand");

	UNIT_TEST_COMPLETE()
}

//-- Wire format tests -----

namespace
{
// Unlike assert, still checks in a release build
bool envelopeCheck(bool condition, const char* what)
{
	if (!condition)
	{
		fprintf(stdout, "      failed: %s\n", what);
	}

	return condition;
}
} // namespace

bool mikan_api_test_request_envelope()
{
	UNIT_TEST_BEGIN("request is a JSON-RPC request whose params are the struct's fields")

	MikanRemoteControlCommand cmd;
	cmd.command= "test_command";
	cmd.parameters.push_back("first");
	cmd.requestId= 42;

	json params;
	std::string serializeError;
	success&= envelopeCheck(Serialization::serializeToJson(cmd, params, serializeError), "request serializes");

	// The envelope carries the type name and the id, so neither is a field
	success&= envelopeCheck(params.contains("command"), "params carry the fields");
	success&= envelopeCheck(!params.contains("requestTypeName"), "params carry no type name");
	success&= envelopeCheck(!params.contains("requestId"), "params carry no request id");

	const std::string frameText= JsonRpc::formatRequest(cmd.requestTypeName.getUtf8Value(), params, cmd.requestId);

	JsonRpcFrame frame;
	success&=
		envelopeCheck(JsonRpc::parseIncoming(frameText, frame) == eJsonRpcFrameKind::request, "frame is a request");
	success&= envelopeCheck(frame.request.method == "MikanRemoteControlCommand", "method is the struct name");
	success&= envelopeCheck(frame.request.id.has_value() && *frame.request.id == 42, "id is the request id");

	// The server reads the params straight into the struct the method names
	MikanRemoteControlCommand received;
	std::string deserializeError;
	success&= envelopeCheck(Serialization::deserializeFromJson(frame.request.params, received, deserializeError),
							"params deserialize into the request");
	success&= envelopeCheck(received.command == cmd.command, "command survives");
	success&= envelopeCheck(received.parameters.size() == 1, "parameters survive");

	UNIT_TEST_COMPLETE()
}

//-- Round-trip deserialization test -----

bool mikan_api_test_response_round_trip()
{
	UNIT_TEST_BEGIN("response result round-trip into the request's ResponseType")

	Serialization::TypeRegistry::build();

	// Simulate the server answering a remote control command
	MikanRemoteControlCommandResult expected;
	expected.results.push_back("ok");

	json resultJson;
	std::string serializeError;
	success&=
		envelopeCheck(Serialization::serializeToJson(expected, resultJson, serializeError), "response serializes");
	success&= envelopeCheck(!resultJson.contains("responseTypeName"), "result carries no type name");
	success&= envelopeCheck(!resultJson.contains("requestId"), "result carries no request id");
	success&= envelopeCheck(!resultJson.contains("resultCode"), "result carries no result code");

	// Simulate the client: the result carries no type, so the request names it
	JsonRpcFrame frame;
	success&= envelopeCheck(JsonRpc::parseIncoming(JsonRpc::formatResult(99, resultJson), frame)
								== eJsonRpcFrameKind::response,
							"frame is a response");
	success&= envelopeCheck(frame.response.id == 99, "id survives");
	if (!envelopeCheck(frame.response.result.has_value(), "result present"))
	{
		success= false;
		UNIT_TEST_COMPLETE()
	}

	const char* responseTypeName= Serialization::getResponseTypeName(&MikanRemoteControlCommand::staticGetArchetype());
	Serialization::StructTypeHandle responseStruct=
		responseTypeName != nullptr ? Serialization::TypeRegistry::getStructByName(responseTypeName) : nullptr;
	if (!envelopeCheck(responseStruct == &MikanRemoteControlCommandResult::staticGetArchetype(),
					   "ResponseType names the response struct"))
	{
		success= false;
		UNIT_TEST_COMPLETE()
	}

	auto instance= responseStruct->makeSharedInstance<MikanResponse>();
	std::string deserializeError;
	success&= envelopeCheck(
		Serialization::deserializeFromJson(*frame.response.result, instance.get(), responseStruct, deserializeError),
		"result deserializes");
	success&= envelopeCheck(static_cast<MikanRemoteControlCommandResult*>(instance.get())->results.size() == 1,
							"results survive");
	success&= envelopeCheck(std::string(instance->responseTypeName.getUtf8Value()) == "MikanRemoteControlCommandResult",
							"constructor stamps the type name");

	// A request without the property is answered with the plain response
	success&= envelopeCheck(Serialization::getResponseTypeName(&MikanRequest::staticGetArchetype()) == nullptr,
							"absent ResponseType reads null");

	UNIT_TEST_COMPLETE()
}

bool mikan_api_test_error_response_result_code()
{
	UNIT_TEST_BEGIN("error response carries the MikanAPIResult name")

	Serialization::TypeRegistry::build();
	Serialization::EnumTypeHandle resultEnum= Serialization::TypeRegistry::getEnumByName("MikanAPIResult");
	success&= envelopeCheck(resultEnum != nullptr, "MikanAPIResult is registered");

	JsonRpcError error;
	error.code= jsonRpcErrorCode(MikanAPIResult::InvalidCameraID);
	error.message= "InvalidCameraID";
	error.data= {{"resultCode", "InvalidCameraID"}};

	JsonRpcFrame frame;
	success&=
		envelopeCheck(JsonRpc::parseIncoming(JsonRpc::formatError(12, error), frame) == eJsonRpcFrameKind::response,
					  "frame is a response");
	if (!envelopeCheck(frame.response.error.has_value(), "error present"))
	{
		success= false;
		UNIT_TEST_COMPLETE()
	}

	// The client fills the response's resultCode from the name in data
	const std::string resultCodeName= frame.response.error->data["resultCode"].get<std::string>();
	int64_t resultCodeValue= 0;
	success&= envelopeCheck(Serialization::findEnumValueByName(resultEnum, resultCodeName.c_str(), resultCodeValue),
							"resultCode names a MikanAPIResult");
	success&= envelopeCheck((MikanAPIResult)resultCodeValue == MikanAPIResult::InvalidCameraID, "resultCode survives");

	// And the name is what the enum reflection writes for the value
	const char* valueName= Serialization::getEnumValueName(resultEnum, (int64_t)MikanAPIResult::UnknownFunction);
	success&=
		envelopeCheck(valueName != nullptr && std::string(valueName) == "UnknownFunction", "value name reads back");

	UNIT_TEST_COMPLETE()
}

bool mikan_api_test_event_notification_round_trip()
{
	UNIT_TEST_BEGIN("event is a notification typed by its method")

	Serialization::TypeRegistry::build();

	MikanRemoteControlEvent sent;
	sent.remoteControlEvent= "stage_changed";

	json params;
	std::string serializeError;
	success&= envelopeCheck(Serialization::serializeToJson(sent, params, serializeError), "event serializes");
	success&= envelopeCheck(!params.contains("eventTypeName"), "params carry no type name");

	JsonRpcFrame frame;
	success&= envelopeCheck(
		JsonRpc::parseIncoming(JsonRpc::formatNotification(sent.eventTypeName.getUtf8Value(), params), frame)
			== eJsonRpcFrameKind::notification,
		"frame is a notification");

	Serialization::StructTypeHandle eventStruct= Serialization::TypeRegistry::getStructByName(frame.request.method);
	if (!envelopeCheck(eventStruct == &MikanRemoteControlEvent::staticGetArchetype(), "method names the event struct"))
	{
		success= false;
		UNIT_TEST_COMPLETE()
	}

	auto received= eventStruct->makeSharedInstance<MikanEvent>();
	std::string deserializeError;
	success&= envelopeCheck(
		Serialization::deserializeFromJson(frame.request.params, received.get(), eventStruct, deserializeError),
		"params deserialize into the event");
	success&= envelopeCheck(static_cast<MikanRemoteControlEvent*>(received.get())->remoteControlEvent
								== sent.remoteControlEvent,
							"fields survive");

	UNIT_TEST_COMPLETE()
}

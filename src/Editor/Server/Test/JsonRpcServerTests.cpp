#include "JsonRpcServerTests.h"
#include "unit_test.h"

#include "HttpInterprocessMessageServer.h"
#include "MikanClientEvents.h"
#include "MikanRemoteControlRequests.h"
#include "OpenRpcDocument.h"
#include "ServerResponseHelpers.h"
#include "WebsocketInterprocessMessageServer.h"

#include "IxWebSocket/IXHttpClient.h"

#include <functional>
#include <map>
#include <stdio.h>
#include <string>
#include <thread>
#include <vector>

namespace
{
bool jsonRpcServerCheck(bool condition, const char* what)
{
	if (!condition)
	{
		printf("      failed: %s\n", what);
	}

	return condition;
}

// A stub dispatcher table: a typed answer, a plain answer, an error, and a handler that
// records that it ran so a notification can be seen to reach it
struct StubHandlers
{
	std::map<std::string, RequestHandler> handlers;
	int remoteControlCalls= 0;
	std::string lastParams;

	StubHandlers()
	{
		handlers["MikanRemoteControlCommand"]= [this](const ClientRequest& request, ClientResponse& response)
		{
			++remoteControlCalls;
			lastParams= request.utf8RequestString;

			MikanRemoteControlCommand command;
			if (!readTypedRequest(request.utf8RequestString, command))
			{
				writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
				return;
			}

			MikanRemoteControlCommandResult result;
			result.results.push_back(command.command);
			writeTypedJsonResponse(request.requestId, result, response);
		};
		handlers["PopAppStage"]= [](const ClientRequest& request, ClientResponse& response)
		{ writeSimpleJsonResponse(request.requestId, MikanAPIResult::Success, response); };
		handlers["GetCameraProperties"]= [](const ClientRequest& request, ClientResponse& response)
		{ writeSimpleJsonResponse(request.requestId, MikanAPIResult::InvalidCameraID, response); };
	}

	json dispatch(const std::string& frame)
	{
		const std::string reply=
			WebsocketInterprocessMessageServer::dispatchRequestFrame(handlers, "connection", frame);

		return reply.empty() ? json() : json::parse(reply, nullptr, false);
	}
};

// A member of a JSON object, or null when the node is not an object or has no such member, so a
// missing reply fails a check instead of throwing
json valueAt(const json& node, const char* key)
{
	if (!node.is_object())
	{
		return json();
	}

	auto it= node.find(key);
	return it != node.end() ? *it : json();
}

// Every $ref in the document must name something the document defines
void collectUnresolvedRefs(const json& document, const json& node, std::vector<std::string>& outUnresolved)
{
	if (node.is_object())
	{
		auto refIt= node.find("$ref");
		if (refIt != node.end() && refIt->is_string())
		{
			const std::string ref= refIt->get<std::string>();
			const std::string prefix= "#/";
			bool bResolved= false;
			if (ref.rfind(prefix, 0) == 0)
			{
				const json::json_pointer pointer(ref.substr(1));
				bResolved= document.contains(pointer);
			}
			if (!bResolved)
			{
				outUnresolved.push_back(ref);
			}
		}

		for (const auto& item : node.items())
		{
			collectUnresolvedRefs(document, item.value(), outUnresolved);
		}
	}
	else if (node.is_array())
	{
		for (const json& element : node)
		{
			collectUnresolvedRefs(document, element, outUnresolved);
		}
	}
}

const json* findNamed(const json& list, const std::string& name)
{
	for (const json& entry : list)
	{
		if (valueAt(entry, "name") == name)
		{
			return &entry;
		}
	}

	return nullptr;
}

// ix::SocketServer cannot report an ephemeral port, so a small fixed range is tried
int bindOpenRpcTestServer(HttpInterprocessMessageServer& server)
{
	for (int port= 28090; port < 28100; ++port)
	{
		if (server.initialize(port, false))
			return port;
	}
	return 0;
}

int fetchOnWorker(int port, const char* verb, std::string& outBody)
{
	int statusCode= 0;
	std::thread worker(
		[&]()
		{
			ix::HttpClient client;
			ix::HttpRequestArgsPtr args= client.createRequest();
			args->connectTimeout= 5;
			args->transferTimeout= 20;
			const std::string url= "http://127.0.0.1:" + std::to_string(port) + OpenRpcDocument::k_routePath;
			ix::HttpResponsePtr response= client.request(url, verb, "", args);
			statusCode= response->statusCode;
			outBody= response->body;
		});
	worker.join();

	return statusCode;
}
} // namespace

bool run_jsonrpc_server_tests()
{
	UNIT_TEST_MODULE_BEGIN("jsonrpc_server")
	UNIT_TEST_MODULE_CALL_TEST(jsonrpc_server_test_results);
	UNIT_TEST_MODULE_CALL_TEST(jsonrpc_server_test_errors);
	UNIT_TEST_MODULE_CALL_TEST(jsonrpc_server_test_notifications_get_no_reply);
	UNIT_TEST_MODULE_CALL_TEST(jsonrpc_server_test_params);
	UNIT_TEST_MODULE_CALL_TEST(jsonrpc_server_test_event_notification);
	UNIT_TEST_MODULE_CALL_TEST(jsonrpc_server_test_openrpc_document);
	UNIT_TEST_MODULE_CALL_TEST(jsonrpc_server_test_openrpc_route);
	UNIT_TEST_MODULE_END()
}

bool jsonrpc_server_test_results()
{
	UNIT_TEST_BEGIN("a request is answered with its response struct or {} as the result")

	StubHandlers stub;

	const json typedReply= stub.dispatch(
		R"({"jsonrpc":"2.0","method":"MikanRemoteControlCommand","params":{"command":"go","parameters":[]},"id":3})");
	success&= jsonRpcServerCheck(valueAt(typedReply, "jsonrpc") == "2.0", "reply is JSON-RPC 2.0");
	success&= jsonRpcServerCheck(valueAt(typedReply, "id") == 3, "reply carries the id");
	success&= jsonRpcServerCheck(typedReply.contains("result") && !typedReply.contains("error"), "reply is a result");
	success&= jsonRpcServerCheck(valueAt(valueAt(typedReply, "result"), "results") == json::array({"go"}),
								 "result is the response struct's fields");
	success&= jsonRpcServerCheck(typedReply.contains("result") && !typedReply["result"].contains("resultCode"),
								 "result carries no envelope fields");

	const json simpleReply= stub.dispatch(R"({"jsonrpc":"2.0","method":"PopAppStage","params":{},"id":"text-id"})");
	success&= jsonRpcServerCheck(valueAt(simpleReply, "id") == "text-id", "a string id is echoed");
	success&= jsonRpcServerCheck(simpleReply.contains("result") && simpleReply["result"] == json::object(),
								 "the plain response is an empty result");

	UNIT_TEST_COMPLETE()
}

bool jsonrpc_server_test_errors()
{
	UNIT_TEST_BEGIN("failures are error objects carrying the MikanAPIResult name")

	StubHandlers stub;

	const json handlerError= stub.dispatch(R"({"jsonrpc":"2.0","method":"GetCameraProperties","params":{},"id":4})");
	success&= jsonRpcServerCheck(valueAt(handlerError, "id") == 4, "error carries the id");
	success&=
		jsonRpcServerCheck(handlerError.contains("error") && !handlerError.contains("result"), "reply is an error");
	if (handlerError.contains("error"))
	{
		const json& error= handlerError["error"];
		success&= jsonRpcServerCheck(valueAt(error, "code") == (int)MikanAPIResult::InvalidCameraID,
									 "code is the result's value");
		success&= jsonRpcServerCheck(valueAt(error, "message") == "InvalidCameraID", "message is the result's name");
		success&=
			jsonRpcServerCheck(error.contains("data") && valueAt(error["data"], "resultCode") == "InvalidCameraID",
							   "data.resultCode is the result's name");
	}

	const json unknown= stub.dispatch(R"({"jsonrpc":"2.0","method":"NoSuchRequest","params":{},"id":5})");
	success&=
		jsonRpcServerCheck(unknown.contains("error") && valueAt(unknown["error"], "code") == k_jsonRpcMethodNotFound,
						   "an unknown method is -32601");
	success&= jsonRpcServerCheck(unknown.contains("error") && unknown["error"].contains("data")
									 && valueAt(unknown["error"]["data"], "resultCode") == "UnknownFunction",
								 "an unknown method's resultCode is UnknownFunction");

	const json parseError= stub.dispatch(R"({"jsonrpc":"2.0","method":)");
	success&=
		jsonRpcServerCheck(parseError.contains("id") && parseError["id"].is_null(), "a parse error has a null id");
	success&=
		jsonRpcServerCheck(parseError.contains("error") && valueAt(parseError["error"], "code") == k_jsonRpcParseError,
						   "a parse error is -32700");
	success&= jsonRpcServerCheck(parseError.contains("error") && parseError["error"].contains("data"),
								 "a parse error carries a resultCode");

	const json batch= stub.dispatch(R"([{"jsonrpc":"2.0","method":"PopAppStage","id":1}])");
	success&= jsonRpcServerCheck(batch.contains("error") && valueAt(batch["error"], "code") == k_jsonRpcInvalidRequest,
								 "a batch is one -32600");
	success&= jsonRpcServerCheck(batch.is_object(), "a batch is answered with one object, not an array");

	// A response sent to the server answers nothing and is not answered
	success&= jsonRpcServerCheck(stub.dispatch(R"({"jsonrpc":"2.0","id":1,"result":{}})").is_null(),
								 "a response frame gets no reply");

	UNIT_TEST_COMPLETE()
}

bool jsonrpc_server_test_notifications_get_no_reply()
{
	UNIT_TEST_BEGIN("a notification runs its handler and is never answered")

	StubHandlers stub;

	const json typedReply= stub.dispatch(
		R"({"jsonrpc":"2.0","method":"MikanRemoteControlCommand","params":{"command":"go","parameters":[]}})");
	success&= jsonRpcServerCheck(typedReply.is_null(), "a typed answer is suppressed");
	success&= jsonRpcServerCheck(stub.remoteControlCalls == 1, "the handler ran");

	success&= jsonRpcServerCheck(stub.dispatch(R"({"jsonrpc":"2.0","method":"PopAppStage","params":{}})").is_null(),
								 "a plain answer is suppressed");
	success&= jsonRpcServerCheck(stub.dispatch(R"({"jsonrpc":"2.0","method":"GetCameraProperties"})").is_null(),
								 "an error is suppressed");
	success&= jsonRpcServerCheck(stub.dispatch(R"({"jsonrpc":"2.0","method":"NoSuchRequest","params":{}})").is_null(),
								 "an unknown method is suppressed");

	UNIT_TEST_COMPLETE()
}

bool jsonrpc_server_test_params()
{
	UNIT_TEST_BEGIN("params are by name, and an empty array stands for none")

	StubHandlers stub;

	const json malformed=
		stub.dispatch(R"({"jsonrpc":"2.0","method":"MikanRemoteControlCommand","params":{"command":"go"},"id":1})");
	success&=
		jsonRpcServerCheck(malformed.contains("error") && valueAt(malformed["error"], "code") == k_jsonRpcInvalidParams,
						   "a missing field is -32602");

	const json positional=
		stub.dispatch(R"({"jsonrpc":"2.0","method":"MikanRemoteControlCommand","params":["go",[]],"id":2})");
	success&= jsonRpcServerCheck(positional.contains("error")
									 && valueAt(positional["error"], "code") == k_jsonRpcInvalidParams,
								 "positional params are -32602");

	const int callsBefore= stub.remoteControlCalls;
	stub.dispatch(R"({"jsonrpc":"2.0","method":"MikanRemoteControlCommand","params":[],"id":3})");
	success&= jsonRpcServerCheck(stub.remoteControlCalls == callsBefore + 1 && stub.lastParams == "{}",
								 "an empty array reaches the handler as {}");

	stub.dispatch(R"({"jsonrpc":"2.0","method":"MikanRemoteControlCommand","id":4})");
	success&= jsonRpcServerCheck(stub.lastParams == "{}", "missing params reach the handler as {}");

	UNIT_TEST_COMPLETE()
}

bool jsonrpc_server_test_event_notification()
{
	UNIT_TEST_BEGIN("an event is a notification named by its struct")

	MikanConnectedEvent connectedEvent;
	connectedEvent.httpPort= 8090;

	const json frame= json::parse(mikanEventToJsonRpc(connectedEvent), nullptr, false);
	success&= jsonRpcServerCheck(valueAt(frame, "jsonrpc") == "2.0", "event is JSON-RPC 2.0");
	success&= jsonRpcServerCheck(valueAt(frame, "method") == "MikanConnectedEvent", "method is the struct name");
	success&= jsonRpcServerCheck(!frame.contains("id"), "event has no id");
	success&= jsonRpcServerCheck(frame.contains("params") && valueAt(frame["params"], "httpPort") == 8090,
								 "params are the event's fields");
	success&= jsonRpcServerCheck(frame.contains("params") && !frame["params"].contains("eventTypeName"),
								 "params carry no type name");

	UNIT_TEST_COMPLETE()
}

bool jsonrpc_server_test_openrpc_document()
{
	UNIT_TEST_BEGIN("the OpenRPC document describes the registered methods from reflection")

	const std::vector<std::string> names= {"rpc.discover", "MikanRemoteControlCommand", "InitClientRequest",
										   "PropertyGetValueRequest", "NotAReflectedRequest"};
	const json document= OpenRpcDocument::buildDocument(names);

	success&= jsonRpcServerCheck(valueAt(document, "openrpc") == "1.3.2", "openrpc is 1.3.2");
	success&= jsonRpcServerCheck(document.contains("info") && valueAt(document["info"], "title") == "MikanXR",
								 "info names MikanXR");
	success&= jsonRpcServerCheck(document.contains("servers") && document["servers"].size() == 1, "one server");

	// rpc.discover and the unreflected name are not methods
	const json& methods= document["methods"];
	success&= jsonRpcServerCheck(methods.is_array() && methods.size() == 3, "one method per reflected request");
	success&= jsonRpcServerCheck(findNamed(methods, "rpc.discover") == nullptr, "rpc.discover is not listed");

	const json* command= findNamed(methods, "MikanRemoteControlCommand");
	if (jsonRpcServerCheck(command != nullptr, "MikanRemoteControlCommand is listed"))
	{
		success&= jsonRpcServerCheck(valueAt(*command, "paramStructure") == "by-name", "params are by name");
		const json& params= (*command)["params"];
		success&=
			jsonRpcServerCheck(findNamed(params, "command") != nullptr && findNamed(params, "parameters") != nullptr,
							   "params are the request's fields");
		success&= jsonRpcServerCheck(findNamed(params, "requestId") == nullptr, "no envelope field is a param");
		success&= jsonRpcServerCheck(valueAt((*command)["result"], "name") == "MikanRemoteControlCommandResult",
									 "result is the ResponseType");
		const json* parameters= findNamed(params, "parameters");
		success&= jsonRpcServerCheck(parameters != nullptr && valueAt((*parameters)["schema"], "type") == "array",
									 "a List is an array");
		success&= jsonRpcServerCheck(!(*command)["errors"].empty(), "errors are listed");
	}
	else
	{
		success= false;
	}

	const json* initClient= findNamed(methods, "InitClientRequest");
	success&= jsonRpcServerCheck(initClient != nullptr && valueAt((*initClient)["result"], "name") == "MikanResponse",
								 "an absent ResponseType is the plain MikanResponse");

	// Every reference resolves inside the document
	std::vector<std::string> unresolved;
	collectUnresolvedRefs(document, document, unresolved);
	for (const std::string& ref : unresolved)
	{
		printf("      unresolved %s\n", ref.c_str());
	}
	success&= jsonRpcServerCheck(unresolved.empty(), "every $ref resolves");

	// The serializer's shapes: enums as their names, the polymorphic pointer as {class_name, value}
	const json& schemas= document["components"]["schemas"];
	success&= jsonRpcServerCheck(schemas.contains("MikanDisconnectCode")
									 && valueAt(schemas["MikanDisconnectCode"], "type") == "string"
									 && !schemas["MikanDisconnectCode"]["enum"].empty(),
								 "an enum is a string with its names");
	success&=
		jsonRpcServerCheck(schemas.contains("PolymorphicObjectPtr") && schemas["PolymorphicObjectPtr"].contains("oneOf")
							   && schemas["PolymorphicObjectPtr"]["oneOf"].size() > 1,
						   "the polymorphic pointer is one of its subclasses");
	success&= jsonRpcServerCheck(schemas.contains("MikanResponse") && schemas["MikanResponse"]["properties"].empty(),
								 "the plain response has no properties");

	// One error per distinct MikanAPIResult value, the reserved codes where they apply
	const json& errors= document["components"]["errors"];
	success&= jsonRpcServerCheck(errors.contains("UnknownFunction")
									 && valueAt(errors["UnknownFunction"], "code") == k_jsonRpcMethodNotFound,
								 "UnknownFunction is -32601");
	success&= jsonRpcServerCheck(!errors.contains("Success"), "Success is not an error");
	success&= jsonRpcServerCheck(errors.contains("InvalidSceneID") != errors.contains("InvalidStageID"),
								 "a shared value is listed once");

	// Events are listed apart from the methods
	success&= jsonRpcServerCheck(document.contains("x-mikan-events")
									 && findNamed(document["x-mikan-events"], "MikanConnectedEvent") != nullptr,
								 "events are under x-mikan-events");

	UNIT_TEST_COMPLETE()
}

bool jsonrpc_server_test_openrpc_route()
{
	UNIT_TEST_BEGIN("the OpenRPC document is served at GET /openrpc.json")

	HttpInterprocessMessageServer server;
	const int port= bindOpenRpcTestServer(server);
	if (port == 0)
	{
		printf("      skipped: no test port free\n");
		UNIT_TEST_COMPLETE()
	}

	const std::string documentText= OpenRpcDocument::buildDocument({"PopAppStage"}).dump();
	success&= jsonRpcServerCheck(OpenRpcDocument::registerRoute(&server, documentText), "route registers");

	std::string body;
	success&= jsonRpcServerCheck(fetchOnWorker(port, "GET", body) == 200, "GET is 200");
	success&= jsonRpcServerCheck(body == documentText, "the body is the document");
	success&= jsonRpcServerCheck(fetchOnWorker(port, "POST", body) == 405, "POST is 405");

	server.removeBackgroundRouteHandler(OpenRpcDocument::k_routePath);
	server.dispose();

	UNIT_TEST_COMPLETE()
}

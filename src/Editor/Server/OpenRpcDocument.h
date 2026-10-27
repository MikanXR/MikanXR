#pragma once

#include "HttpInterprocessMessageServer.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

// The OpenRPC 1.3.2 document describing the websocket API, built from reflection, so tools
// such as the OpenRPC inspector list every method with its schemas. It is the answer to the
// rpc.discover method and is also served over HTTP:
//
//   GET /openrpc.json
//
// Each registered method's params are the request struct's fields by name, its result is the
// response struct its ResponseType property names (the plain MikanResponse when it names none),
// and its errors are the MikanAPIResult codes. components.schemas holds every struct and enum a
// method or an event reaches, in the JSON shapes the serializer writes. Events are not callable,
// so they are listed under x-mikan-events rather than as methods.
//
// The document depends only on the registered method names and reflection, both fixed once the
// server has started, so it is built once and the route serves that text from the connection
// thread.
class OpenRpcDocument
{
public:
	static const char* k_discoverMethod;
	static const char* k_routePath;

	// methodNames are the dispatcher's registered names. rpc.discover itself is not a method of
	// the document, and a name with no reflected request struct is skipped.
	static nlohmann::json buildDocument(const std::vector<std::string>& methodNames);

	static bool registerRoute(HttpInterprocessMessageServer* server, const std::string& documentText);
	static HttpRouteResponse buildResponse(const HttpRouteRequest& request, const std::string& documentText);
};

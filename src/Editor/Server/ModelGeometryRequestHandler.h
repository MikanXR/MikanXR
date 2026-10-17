#pragma once

#include "HttpInterprocessMessageServer.h"
#include "IServerRequestHandler.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

enum class eModelGeometryKind : int
{
	stencil,
	shape,
};

// What a geometry request resolves to
enum class eModelGeometryQueryResult : int
{
	found,
	// No component of that kind has the id, or it has no loaded model
	notFound,
	// The editor did not answer in time
	unavailable,
	// The model loaded but its GLB could not be built
	buildFailed,
};

struct ModelGeometryPayload
{
	std::string etag;
	std::shared_ptr<const std::vector<uint8_t>> glbBytes;
};

// Serves model stencil and model shape geometry as GLB over the HTTP message server:
//
//   GET /geometry/stencil/<componentId>
//   GET /geometry/shape/<componentId>
//
// Each component publishes its path, with ?v=<etag> appended, as its read-only geometry_url
// property, which clients join to the HTTP base URL from the connected event. The reply is the
// model's GLB with an ETag, and 304 when If-None-Match carries that etag.
//
// The route runs on the connection thread and hops to the main thread once to resolve the
// component and take the shared GLB bytes, built on the resource's first request. The bytes are
// sent from the connection thread with no editor state held. The reply is built by
// buildResponse so the tests can drive the route through a server with a stub resolver.
class ModelGeometryRequestHandler : public IServerRequestHandler
{
public:
	ModelGeometryRequestHandler(class MikanServer* owner)
		: IServerRequestHandler(owner)
	{
	}

	virtual bool startup(class MainWindow* mainWindow) override;
	virtual void shutdown() override;

	static const char* k_routePrefix;

	// The server-relative geometry_url for a component, or empty without an etag
	static std::string makeGeometryUrl(eModelGeometryKind kind, int32_t componentId, const std::string& etag);

	using GeometryResolver= std::function<eModelGeometryQueryResult(eModelGeometryKind kind, int32_t componentId,
																	ModelGeometryPayload& outPayload)>;
	// The reply for one request: 405 for a verb other than GET, 404 for an unknown path or id,
	// 304 for a matching If-None-Match, 504 when the editor did not answer, 500 when the GLB could
	// not be built, otherwise the GLB
	static HttpRouteResponse buildResponse(const HttpRouteRequest& request, const GeometryResolver& resolver);
	// Registers the prefix route on a server with the given resolver
	static bool registerRoute(HttpInterprocessMessageServer* server, const GeometryResolver& resolver);

private:
	eModelGeometryQueryResult resolveOnMainThread(eModelGeometryKind kind, int32_t componentId,
												  ModelGeometryPayload& outPayload);
	// Runs on the main thread
	eModelGeometryQueryResult resolveNow(eModelGeometryKind kind, int32_t componentId,
										 ModelGeometryPayload& outPayload);
};

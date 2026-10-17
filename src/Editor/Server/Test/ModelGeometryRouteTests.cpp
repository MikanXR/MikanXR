#include "ModelGeometryRouteTests.h"
#include "unit_test.h"

#include "HttpInterprocessMessageServer.h"
#include "MikanRenderModelResource.h"
#include "ModelGeometryRequestHandler.h"

#include "IxWebSocket/IXHttpClient.h"

#include <memory>
#include <stdio.h>
#include <string>
#include <thread>
#include <vector>

namespace
{
// ix::SocketServer cannot report an ephemeral port, so a small fixed range is tried
int bindGeometryTestServer(HttpInterprocessMessageServer& server)
{
	for (int port= 28090; port < 28100; ++port)
	{
		if (server.initialize(port, false))
			return port;
	}
	return 0;
}

// One request on a worker thread. The route is a background route with a stub
// resolver, so nothing needs the test thread to pump.
struct GeometryRequestOnWorker
{
	int statusCode= 0;
	std::string body;
	ix::WebSocketHttpHeaders headers;

	GeometryRequestOnWorker(int port, const std::string& verb, const std::string& path,
							const std::string& ifNoneMatch= std::string())
	{
		std::thread worker(
			[&]()
			{
				ix::HttpClient client;
				ix::HttpRequestArgsPtr args= client.createRequest();
				args->connectTimeout= 5;
				args->transferTimeout= 20;
				if (!ifNoneMatch.empty())
				{
					args->extraHeaders["If-None-Match"]= ifNoneMatch;
				}

				const std::string url= "http://127.0.0.1:" + std::to_string(port) + path;
				ix::HttpResponsePtr response= client.request(url, verb, "", args);
				statusCode= response->statusCode;
				body= response->body;
				headers= response->headers;
			});
		worker.join();
	}

	std::string header(const char* name) const
	{
		const auto it= headers.find(name);
		return it != headers.end() ? it->second : std::string();
	}
};
} // namespace

bool model_geometry_route_test_url_shape()
{
	UNIT_TEST_BEGIN("geometry urls name the kind, the id and the etag")

	success&= (ModelGeometryRequestHandler::makeGeometryUrl(eModelGeometryKind::stencil, 7, "00ff")
			   == "/geometry/stencil/7?v=00ff");
	success&= (ModelGeometryRequestHandler::makeGeometryUrl(eModelGeometryKind::shape, 12, "ab")
			   == "/geometry/shape/12?v=ab");
	// No etag means no loaded model, which publishes an empty url
	success&= ModelGeometryRequestHandler::makeGeometryUrl(eModelGeometryKind::stencil, 7, "").empty();

	UNIT_TEST_COMPLETE()
}

bool model_geometry_route_test_etag_covers_textures()
{
	UNIT_TEST_BEGIN("the geometry etag changes with the model or any texture file")

	using SourceFile= MikanRenderModelResource::GeometrySourceFile;
	const std::vector<SourceFile> baseline= {{"models/scan.glb", 1000, 42}, {"models/scan.png", 5000, 43}};
	const std::string baselineETag= MikanRenderModelResource::computeGeometryETag(baseline);

	success&= (baselineETag.size() == 16);
	success&= (MikanRenderModelResource::computeGeometryETag(baseline) == baselineETag);

	std::vector<SourceFile> textureResized= baseline;
	textureResized[1].fileSize= 5001;
	success&= (MikanRenderModelResource::computeGeometryETag(textureResized) != baselineETag);

	std::vector<SourceFile> textureRewritten= baseline;
	textureRewritten[1].lastWriteTime= 44;
	success&= (MikanRenderModelResource::computeGeometryETag(textureRewritten) != baselineETag);

	std::vector<SourceFile> modelRewritten= baseline;
	modelRewritten[0].lastWriteTime= 41;
	success&= (MikanRenderModelResource::computeGeometryETag(modelRewritten) != baselineETag);

	std::vector<SourceFile> textureDropped= {baseline[0]};
	success&= (MikanRenderModelResource::computeGeometryETag(textureDropped) != baselineETag);

	UNIT_TEST_COMPLETE()
}

bool model_geometry_route_test_replies()
{
	UNIT_TEST_BEGIN("the geometry route answers the glb, 304, 404, 405, 504 and preflight")

	HttpInterprocessMessageServer server;
	const int port= bindGeometryTestServer(server);
	if (port == 0)
	{
		printf("      skipped: no test port free\n");
		UNIT_TEST_COMPLETE()
	}

	// A stub editor with model stencil 3 and model shape 5, and a wedged main thread for stencil 9.
	// The bytes carry an embedded zero so a string-terminated copy would show.
	auto glbBytes= std::make_shared<std::vector<uint8_t>>(std::vector<uint8_t>{'g', 'l', 'T', 'F', 0, 2, 0, 0});
	const std::string etag= "0123456789abcdef";
	eModelGeometryKind lastKind= eModelGeometryKind::shape;
	success&= ModelGeometryRequestHandler::registerRoute(
		&server,
		[&](eModelGeometryKind kind, int32_t componentId, ModelGeometryPayload& outPayload)
		{
			lastKind= kind;
			if (kind == eModelGeometryKind::stencil && componentId == 9)
				return eModelGeometryQueryResult::unavailable;
			if ((kind == eModelGeometryKind::stencil && componentId == 3)
				|| (kind == eModelGeometryKind::shape && componentId == 5))
			{
				outPayload.etag= etag;
				outPayload.glbBytes= glbBytes;
				return eModelGeometryQueryResult::found;
			}
			return eModelGeometryQueryResult::notFound;
		});

	GeometryRequestOnWorker get(port, "GET", "/geometry/stencil/3?v=" + etag);
	success&= (get.statusCode == 200);
	success&= (get.header("Content-Type") == "model/gltf-binary");
	success&= (get.header("ETag") == "\"" + etag + "\"");
	success&= (get.header("Cache-Control") == "no-cache");
	success&= (get.header("Access-Control-Allow-Origin") == "*");
	success&= (get.body == std::string(glbBytes->begin(), glbBytes->end()));
	success&= (lastKind == eModelGeometryKind::stencil);

	GeometryRequestOnWorker shape(port, "GET", "/geometry/shape/5");
	success&= (shape.statusCode == 200 && lastKind == eModelGeometryKind::shape);

	// A matching etag, strong, weak or in a list, answers 304 with no body
	GeometryRequestOnWorker notModified(port, "GET", "/geometry/stencil/3", "\"" + etag + "\"");
	success&= (notModified.statusCode == 304 && notModified.body.empty());
	success&= (notModified.header("ETag") == "\"" + etag + "\"");
	GeometryRequestOnWorker weakInList(port, "GET", "/geometry/stencil/3", "\"stale\", W/\"" + etag + "\"");
	success&= (weakInList.statusCode == 304);
	GeometryRequestOnWorker stale(port, "GET", "/geometry/stencil/3", "\"stale\"");
	success&= (stale.statusCode == 200 && stale.body.size() == glbBytes->size());

	GeometryRequestOnWorker unknownId(port, "GET", "/geometry/stencil/4");
	success&= (unknownId.statusCode == 404);
	GeometryRequestOnWorker unknownKind(port, "GET", "/geometry/x/3");
	success&= (unknownKind.statusCode == 404);
	GeometryRequestOnWorker badId(port, "GET", "/geometry/stencil/3a");
	success&= (badId.statusCode == 404);
	GeometryRequestOnWorker noId(port, "GET", "/geometry/stencil/");
	success&= (noId.statusCode == 404);

	GeometryRequestOnWorker post(port, "POST", "/geometry/stencil/3");
	success&= (post.statusCode == 405);

	GeometryRequestOnWorker wedged(port, "GET", "/geometry/stencil/9");
	success&= (wedged.statusCode == 504);

	GeometryRequestOnWorker preflight(port, "OPTIONS", "/geometry/stencil/3");
	success&= (preflight.statusCode == 204);
	success&= (preflight.header("Access-Control-Allow-Headers") == "Content-Type, If-None-Match");

	server.dispose();

	UNIT_TEST_COMPLETE()
}

bool run_model_geometry_route_tests()
{
	UNIT_TEST_MODULE_BEGIN("model_geometry_route")
	UNIT_TEST_MODULE_CALL_TEST(model_geometry_route_test_url_shape);
	UNIT_TEST_MODULE_CALL_TEST(model_geometry_route_test_etag_covers_textures);
	UNIT_TEST_MODULE_CALL_TEST(model_geometry_route_test_replies);
	UNIT_TEST_MODULE_END()
}

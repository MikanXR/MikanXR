#include "ARKitAlignmentRouteTests.h"
#include "unit_test.h"

#include "ARKitAlignmentRequestHandler.h"
#include "FrameCoupledAlignment.h"
#include "HttpInterprocessMessageServer.h"
#include "JsonUtils.h"

#include "IxWebSocket/IXHttpClient.h"

#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

#include <atomic>
#include <cmath>
#include <stdio.h>
#include <string>
#include <thread>

namespace
{
// ix::SocketServer cannot report an ephemeral port, so a small fixed range is tried
int bindTestServer(HttpInterprocessMessageServer& server)
{
	for (int port= 28090; port < 28100; ++port)
	{
		if (server.initialize(port, false))
			return port;
	}
	return 0;
}

// One request on a worker thread. The route is a background route with a stub
// query, so nothing needs the test thread to pump.
struct RequestOnWorker
{
	int statusCode= 0;
	std::string body;

	RequestOnWorker(int port, const char* verb, const std::string& pathAndQuery)
	{
		std::thread worker(
			[&]()
			{
				ix::HttpClient client;
				ix::HttpRequestArgsPtr args= client.createRequest();
				args->connectTimeout= 5;
				args->transferTimeout= 20;
				const std::string url= "http://127.0.0.1:" + std::to_string(port) + pathAndQuery;
				ix::HttpResponsePtr response= client.request(url, verb, "", args);
				statusCode= response->statusCode;
				body= response->body;
			});
		worker.join();
	}
};

bool matricesClose(const glm::dmat4& a, const glm::dmat4& b, double tolerance)
{
	for (int column= 0; column < 4; ++column)
	{
		for (int row= 0; row < 4; ++row)
		{
			if (std::abs(a[column][row] - b[column][row]) > tolerance)
				return false;
		}
	}
	return true;
}
} // namespace

bool run_arkit_alignment_route_tests()
{
	UNIT_TEST_MODULE_BEGIN("arkit_alignment_route")
	UNIT_TEST_MODULE_CALL_TEST(arkit_alignment_route_test_frame_coupled_averaging);
	UNIT_TEST_MODULE_CALL_TEST(arkit_alignment_route_test_replies);
	UNIT_TEST_MODULE_END()
}

bool arkit_alignment_route_test_frame_coupled_averaging()
{
	UNIT_TEST_BEGIN("the shared frame-coupled solve recovers the offset and averages across the hemisphere")

	// A camera at a known stage pose seeing the marker at the origin: the solved
	// offset maps the camera's world pose onto that stage pose
	const glm::dmat4 cameraInStage= glm::rotate(glm::translate(glm::dmat4(1.0), glm::dvec3(0.5, 1.2, -2.0)),
												glm::radians(30.0), glm::dvec3(0.0, 1.0, 0.0));
	const glm::dmat4 markerInCamera= glm::inverse(cameraInStage);
	const glm::mat4 cameraInWorld= glm::mat4(glm::rotate(glm::translate(glm::dmat4(1.0), glm::dvec3(3.0, 0.0, 1.0)),
														 glm::radians(-80.0), glm::dvec3(0.0, 1.0, 0.0)));
	const glm::dmat4 worldToStage= FrameCoupledAlignment::computeWorldToStageSample(markerInCamera, cameraInWorld);
	const bool bRecovers= matricesClose(worldToStage * glm::dmat4(cameraInWorld), cameraInStage, 1e-6);
	if (!bRecovers)
		printf("      the sample does not map the world pose onto the stage pose\n");
	success&= bRecovers;

	// Identical samples average to themselves, and no samples to identity. The
	// sample's rotation came through a float pose, so it is orthonormal only to
	// float precision and the quaternion round trip in the average cleans it up
	// by about that much.
	const bool bIdentical= matricesClose(
		FrameCoupledAlignment::averageWorldToStage({worldToStage, worldToStage, worldToStage}), worldToStage, 1e-5);
	if (!bIdentical)
		printf("      identical samples did not average to themselves\n");
	success&= bIdentical;
	success&= matricesClose(FrameCoupledAlignment::averageWorldToStage({}), glm::dmat4(1.0), 0.0);

	// Two rotations either side of a mean average onto it, with their translations
	const glm::dquat mean= glm::angleAxis(glm::radians(40.0), glm::normalize(glm::dvec3(0.0, 1.0, 0.0)));
	const glm::dquat left= glm::angleAxis(glm::radians(30.0), glm::dvec3(0.0, 1.0, 0.0));
	const glm::dquat right= glm::angleAxis(glm::radians(50.0), glm::dvec3(0.0, 1.0, 0.0));
	glm::dmat4 leftSample= glm::mat4_cast(left);
	leftSample[3]= glm::dvec4(1.0, 0.0, 0.0, 1.0);
	// The same rotation with its quaternion sign flipped, which a naive average
	// would cancel against the first
	glm::dmat4 rightSample= glm::mat4_cast(-right);
	rightSample[3]= glm::dvec4(3.0, 2.0, 0.0, 1.0);
	glm::dmat4 expected= glm::mat4_cast(mean);
	expected[3]= glm::dvec4(2.0, 1.0, 0.0, 1.0);
	const bool bAveraged=
		matricesClose(FrameCoupledAlignment::averageWorldToStage({leftSample, rightSample}), expected, 1e-6);
	if (!bAveraged)
		printf("      two rotations did not average onto their mean\n");
	success&= bAveraged;

	UNIT_TEST_COMPLETE()
}

bool arkit_alignment_route_test_replies()
{
	UNIT_TEST_BEGIN("the alignment route answers 404 for an unknown port and tracks a solve")

	HttpInterprocessMessageServer server;
	const int port= bindTestServer(server);
	if (port == 0)
	{
		printf("      skipped: no test port free\n");
		UNIT_TEST_COMPLETE()
	}

	// A stub editor with one ARKit source on port 9000, aligned on demand
	std::atomic<bool> bAligned{false};
	std::atomic<bool> bMarkerVisible{false};
	std::atomic<bool> bAvailable{true};
	int lastQueriedPort= 0;
	success&=
		ARKitAlignmentRequestHandler::registerRoute(&server,
													[&](int basePort, bool& outMarkerVisible, bool& outAlignedFlag)
													{
														lastQueriedPort= basePort;
														if (!bAvailable)
															return eARKitAlignmentQueryResult::unavailable;
														if (basePort != 9000)
															return eARKitAlignmentQueryResult::unknownPort;
														outMarkerVisible= bMarkerVisible;
														outAlignedFlag= bAligned;
														return eARKitAlignmentQueryResult::found;
													});
	// The same path cannot be registered twice
	success&= !ARKitAlignmentRequestHandler::registerRoute(&server, [](int, bool&, bool&)
														   { return eARKitAlignmentQueryResult::unknownPort; });

	// Only GET, and only with a usable port
	success&= (RequestOnWorker(port, "POST", "/arkit/alignment?port=9000").statusCode == 405);
	success&= (RequestOnWorker(port, "GET", "/arkit/alignment").statusCode == 400);
	success&= (RequestOnWorker(port, "GET", "/arkit/alignment?port=abc").statusCode == 400);
	success&= (RequestOnWorker(port, "GET", "/arkit/alignment?port=0").statusCode == 400);

	// An unknown port is a 404, a known one answers the status
	success&= (RequestOnWorker(port, "GET", "/arkit/alignment?port=9001").statusCode == 404);
	success&= (lastQueriedPort == 9001);
	{
		RequestOnWorker before(port, "GET", "/arkit/alignment?port=9000");
		success&= (before.statusCode == 200);
		const json reply= json::parse(before.body, nullptr, false);
		success&=
			reply.is_object() && reply.value("aligned", true) == false && reply.value("marker_visible", true) == false;
	}

	// The marker comes into view, then the solve lands
	bMarkerVisible= true;
	{
		RequestOnWorker seeing(port, "GET", "/arkit/alignment?port=9000");
		const json reply= json::parse(seeing.body, nullptr, false);
		success&= (seeing.statusCode == 200) && reply.value("marker_visible", false) && !reply.value("aligned", true);
	}
	bAligned= true;
	{
		RequestOnWorker after(port, "GET", "/arkit/alignment?port=9000");
		const json reply= json::parse(after.body, nullptr, false);
		success&= (after.statusCode == 200) && reply.value("aligned", false);
	}

	// An editor that does not answer is a 504
	bAvailable= false;
	success&= (RequestOnWorker(port, "GET", "/arkit/alignment?port=9000").statusCode == 504);

	server.dispose();

	UNIT_TEST_COMPLETE()
}

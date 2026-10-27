#include "CompositorStreamRouteTests.h"
#include "unit_test.h"

#include "CompositorRtpOutput.h"
#include "CompositorStreamRequestHandler.h"
#include "HttpInterprocessMessageServer.h"
#include "IRtpVideoSender.h"

#include "IxWebSocket/IXHttpClient.h"

#include <atomic>
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
	std::string contentType;

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
				const auto typeIt= response->headers.find("Content-Type");
				if (typeIt != response->headers.end())
					contentType= typeIt->second;
			});
		worker.join();
	}
};

bool sizeIs(int sourceWidth, int sourceHeight, int requestedHeight, int expectedWidth, int expectedHeight)
{
	int width= 0;
	int height= 0;
	CompositorRtpOutput::computeOutputSize(sourceWidth, sourceHeight, requestedHeight, width, height);
	const bool bMatches= (width == expectedWidth && height == expectedHeight);
	if (!bMatches)
	{
		printf("      %dx%d at height %d gave %dx%d, expected %dx%d\n", sourceWidth, sourceHeight, requestedHeight,
			   width, height, expectedWidth, expectedHeight);
	}
	return bMatches;
}

RtpDestination makeDestination(const char* host, uint16_t port)
{
	RtpDestination destination;
	destination.host= host;
	destination.port= port;
	return destination;
}
} // namespace

bool run_compositor_stream_route_tests()
{
	UNIT_TEST_MODULE_BEGIN("compositor_stream_route")
	UNIT_TEST_MODULE_CALL_TEST(compositor_stream_route_test_output_size);
	UNIT_TEST_MODULE_CALL_TEST(compositor_stream_route_test_sdp);
	UNIT_TEST_MODULE_CALL_TEST(compositor_stream_route_test_replies);
	UNIT_TEST_MODULE_CALL_TEST(compositor_stream_route_test_destination_host);
	UNIT_TEST_MODULE_CALL_TEST(compositor_stream_route_test_merge_destinations);
	UNIT_TEST_MODULE_END()
}

bool compositor_stream_route_test_output_size()
{
	UNIT_TEST_BEGIN("the RTP output size keeps the source aspect, stays even, and floors at the encoder minimum")

	// 0 keeps the source size
	success&= sizeIs(1920, 1440, 0, 1920, 1440);
	// A requested height scales the width with the aspect
	success&= sizeIs(1920, 1440, 720, 960, 720);
	success&= sizeIs(1920, 1080, 720, 1280, 720);
	success&= sizeIs(1920, 1080, 480, 852, 480);
	// Odd results round down to even
	success&= sizeIs(1921, 1081, 0, 1920, 1080);
	// Too small clamps up to the encoder floor, too tall clamps to its ceiling
	success&= sizeIs(1920, 1080, 10, 160, 64);
	success&= sizeIs(1920, 1080, 9000, 7282, 4096);
	// A source with no size gives none
	success&= sizeIs(0, 0, 720, 0, 0);

	UNIT_TEST_COMPLETE()
}

bool compositor_stream_route_test_sdp()
{
	UNIT_TEST_BEGIN("the SDP names the destination, the port, the H.264 payload and the picture")

	CompositorStreamInfo info;
	info.compositorId= 7;
	info.host= "192.168.1.50";
	info.port= 5004;
	info.width= 1280;
	info.height= 720;
	info.fps= 30.f;

	const std::string sdp= CompositorStreamRequestHandler::buildSdp(info);
	success&= (sdp.rfind("v=0\r\n", 0) == 0);
	success&= (sdp.find("c=IN IP4 192.168.1.50\r\n") != std::string::npos);
	success&= (sdp.find("m=video 5004 RTP/AVP 96\r\n") != std::string::npos);
	success&= (sdp.find("a=rtpmap:96 H264/90000\r\n") != std::string::npos);
	success&= (sdp.find("a=fmtp:96 packetization-mode=1\r\n") != std::string::npos);
	success&= (sdp.find("a=framesize:96 1280-720\r\n") != std::string::npos);
	success&= (sdp.find("a=framerate:30\r\n") != std::string::npos);
	// Every line ends in CRLF, with no bare LF
	success&= (sdp.find("\n") != std::string::npos);
	for (size_t pos= sdp.find('\n'); pos != std::string::npos; pos= sdp.find('\n', pos + 1))
	{
		success&= (pos > 0 && sdp[pos - 1] == '\r');
	}

	// An unknown picture leaves the size and rate lines out
	CompositorStreamInfo unsized= info;
	unsized.width= 0;
	unsized.height= 0;
	unsized.fps= 0.f;
	const std::string unsizedSdp= CompositorStreamRequestHandler::buildSdp(unsized);
	success&= (unsizedSdp.find("a=framesize") == std::string::npos);
	success&= (unsizedSdp.find("a=framerate") == std::string::npos);

	UNIT_TEST_COMPLETE()
}

bool compositor_stream_route_test_replies()
{
	UNIT_TEST_BEGIN("the stream route answers the SDP for a streaming compositor and errors otherwise")

	HttpInterprocessMessageServer server;
	const int port= bindTestServer(server);
	if (port == 0)
	{
		printf("      skipped: no test port free\n");
		UNIT_TEST_COMPLETE()
	}

	// A stub editor with compositor 3 streaming and compositor 4 with its output off
	std::atomic<bool> bAvailable{true};
	std::atomic<int> compositorCount{2};
	int lastQueriedId= -2;
	success&= CompositorStreamRequestHandler::registerRoute(
		&server,
		[&](int compositorId, CompositorStreamInfo& outInfo)
		{
			lastQueriedId= compositorId;
			if (!bAvailable)
				return eCompositorStreamQueryResult::unavailable;
			if (compositorId < 0)
			{
				if (compositorCount == 0)
					return eCompositorStreamQueryResult::unknownCompositor;
				if (compositorCount > 1)
					return eCompositorStreamQueryResult::ambiguousCompositor;
				compositorId= 3;
			}
			if (compositorId == 4)
				return eCompositorStreamQueryResult::notStreaming;
			if (compositorId != 3)
				return eCompositorStreamQueryResult::unknownCompositor;

			outInfo.compositorId= compositorId;
			outInfo.host= "10.0.0.9";
			outInfo.port= 6000;
			outInfo.width= 960;
			outInfo.height= 720;
			outInfo.fps= 29.97f;
			return eCompositorStreamQueryResult::found;
		});
	// The same path cannot be registered twice
	success&= !CompositorStreamRequestHandler::registerRoute(
		&server, [](int, CompositorStreamInfo&) { return eCompositorStreamQueryResult::unknownCompositor; });

	// Only GET, and only with a usable id
	success&= (RequestOnWorker(port, "POST", "/compositor/stream.sdp?compositor=3").statusCode == 405);
	success&= (RequestOnWorker(port, "GET", "/compositor/stream.sdp?compositor=abc").statusCode == 400);
	success&= (RequestOnWorker(port, "GET", "/compositor/stream.sdp?compositor=-1").statusCode == 400);

	// No id with several compositors is ambiguous, with one it resolves
	success&= (RequestOnWorker(port, "GET", "/compositor/stream.sdp").statusCode == 400);
	success&= (lastQueriedId == -1);
	compositorCount= 1;
	success&= (RequestOnWorker(port, "GET", "/compositor/stream.sdp").statusCode == 200);
	compositorCount= 0;
	success&= (RequestOnWorker(port, "GET", "/compositor/stream.sdp").statusCode == 404);

	// Unknown and not streaming are both 404
	success&= (RequestOnWorker(port, "GET", "/compositor/stream.sdp?compositor=9").statusCode == 404);
	success&= (lastQueriedId == 9);
	success&= (RequestOnWorker(port, "GET", "/compositor/stream.sdp?compositor=4").statusCode == 404);

	// A streaming compositor answers its SDP
	{
		RequestOnWorker streaming(port, "GET", "/compositor/stream.sdp?compositor=3");
		success&= (streaming.statusCode == 200);
		success&= (streaming.contentType.rfind("application/sdp", 0) == 0);
		success&= (streaming.body.find("c=IN IP4 10.0.0.9\r\n") != std::string::npos);
		success&= (streaming.body.find("m=video 6000 RTP/AVP 96\r\n") != std::string::npos);
		success&= (streaming.body.find("a=framesize:96 960-720\r\n") != std::string::npos);
	}

	// An editor that does not answer is a 504
	bAvailable= false;
	success&= (RequestOnWorker(port, "GET", "/compositor/stream.sdp?compositor=3").statusCode == 504);

	server.dispose();

	UNIT_TEST_COMPLETE()
}

bool compositor_stream_route_test_destination_host()
{
	UNIT_TEST_BEGIN("an RTP destination host is a unicast hostname or address")

	success&= isValidRtpDestinationHost("phone.local");
	success&= isValidRtpDestinationHost("my-phone");
	success&= isValidRtpDestinationHost("192.168.1.50");
	success&= isValidRtpDestinationHost("127.0.0.1");
	success&= isValidRtpDestinationHost("fe80::1");
	// The first octet below the multicast range
	success&= isValidRtpDestinationHost("223.1.1.1");
	// A leading number that is not an IPv4 octet
	success&= isValidRtpDestinationHost("239host");

	success&= !isValidRtpDestinationHost("");
	success&= !isValidRtpDestinationHost("phone local");
	success&= !isValidRtpDestinationHost("10.0.0.1/24");
	success&= !isValidRtpDestinationHost("host port=5004");
	success&= !isValidRtpDestinationHost("239.1.1.1");
	success&= !isValidRtpDestinationHost("224.0.0.251");

	UNIT_TEST_COMPLETE()
}

bool compositor_stream_route_test_merge_destinations()
{
	UNIT_TEST_BEGIN("the RTP destinations merge the persisted one with the client feeds, each once, in order")

	const RtpDestination monitor= makeDestination("192.168.1.10", 5004);
	const RtpDestination phone= makeDestination("192.168.1.50", 5006);
	const RtpDestination tablet= makeDestination("192.168.1.51", 5006);

	// The persisted destination leads, the client feeds follow in their order
	{
		const std::vector<RtpDestination> merged= CompositorRtpOutput::mergeDestinations({monitor}, {phone, tablet});
		success&= merged == std::vector<RtpDestination>({monitor, phone, tablet});
	}

	// A client feed naming the persisted destination is sent once
	{
		const std::vector<RtpDestination> merged=
			CompositorRtpOutput::mergeDestinations({monitor}, {phone, monitor, phone});
		success&= merged == std::vector<RtpDestination>({monitor, phone});
	}

	// The same host on another port is another destination
	{
		const RtpDestination phoneOtherPort= makeDestination("192.168.1.50", 5008);
		const std::vector<RtpDestination> merged= CompositorRtpOutput::mergeDestinations({}, {phone, phoneOtherPort});
		success&= merged == std::vector<RtpDestination>({phone, phoneOtherPort});
	}

	// Either side may be empty
	success&= CompositorRtpOutput::mergeDestinations({}, {}).empty();
	success&= CompositorRtpOutput::mergeDestinations({monitor}, {}) == std::vector<RtpDestination>({monitor});

	UNIT_TEST_COMPLETE()
}

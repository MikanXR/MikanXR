#include "HttpServerTests.h"
#include "unit_test.h"

#include "HttpInterprocessMessageServer.h"

#include "IxWebSocket/IXHttpClient.h"

#include <atomic>
#include <chrono>
#include <stdio.h>
#include <string>
#include <thread>

namespace
{
// ix::SocketServer cannot report an ephemeral port, so a small fixed range is tried.
// Returns the bound port, or 0 when none in the range was free.
int bindTestServer(HttpInterprocessMessageServer& server)
{
	for (int port= 28090; port < 28100; ++port)
	{
		if (server.initialize(port, false))
		{
			return port;
		}
	}

	return 0;
}

std::string makeTestUrl(int port, const char* path) { return "http://127.0.0.1:" + std::to_string(port) + path; }

// Posts on a worker thread while the test thread pumps processRequests(), the way the
// editor's main thread does, until the worker has its response.
struct PostOnWorker
{
	std::thread worker;
	std::atomic<bool> bDone{false};
	int statusCode= 0;
	std::string responseBody;

	PostOnWorker(const std::string& url, const std::string& body)
	{
		worker= std::thread(
			[this, url, body]()
			{
				ix::HttpClient client;
				ix::HttpRequestArgsPtr args= client.createRequest();
				args->connectTimeout= 5;
				args->transferTimeout= 20;

				ix::HttpResponsePtr response= client.post(url, body, args);
				statusCode= response->statusCode;
				responseBody= response->body;
				bDone= true;
			});
	}

	void pumpUntilDone(HttpInterprocessMessageServer& server)
	{
		const auto deadline= std::chrono::steady_clock::now() + std::chrono::seconds(20);
		while (!bDone && std::chrono::steady_clock::now() < deadline)
		{
			server.processRequests();
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		worker.join();
	}
};
} // namespace

bool http_server_test_background_route_answers_off_main()
{
	UNIT_TEST_BEGIN("background route answers on the connection thread with a main-thread hop")

	HttpInterprocessMessageServer server;
	const int port= bindTestServer(server);
	if (port == 0)
	{
		printf("      skipped: no test port free\n");
		UNIT_TEST_COMPLETE()
	}

	const std::thread::id testThread= std::this_thread::get_id();
	std::thread::id handlerThread;
	std::thread::id jobThread;
	bool bHopRan= false;
	std::string receivedBody;

	server.setBackgroundRouteHandler("/echo",
									 [&](const HttpRouteRequest& request)
									 {
										 handlerThread= std::this_thread::get_id();
										 receivedBody= request.body;
										 bHopRan= server.runOnMainThread(
											 [&]() { jobThread= std::this_thread::get_id(); }, 5000);

										 HttpRouteResponse response;
										 response.contentType= "application/octet-stream";
										 response.body= request.body;
										 return response;
									 });

	// Binary with embedded zeros, so a text-only path would corrupt it
	std::string body;
	for (int i= 0; i < 70000; ++i)
	{
		body.push_back(static_cast<char>(i * 31));
	}

	PostOnWorker post(makeTestUrl(port, "/echo?folder=movies&name=a%20b.mp4"), body);
	post.pumpUntilDone(server);

	success&= (post.statusCode == 200);
	success&= (post.responseBody == body);
	success&= (receivedBody == body);
	success&= (handlerThread != testThread);
	success&= bHopRan;
	success&= (jobThread == testThread);

	server.dispose();

	UNIT_TEST_COMPLETE()
}

bool http_server_test_main_thread_route_answers_through_queue()
{
	UNIT_TEST_BEGIN("main-thread route answers through the request queue")

	HttpInterprocessMessageServer server;
	const int port= bindTestServer(server);
	if (port == 0)
	{
		printf("      skipped: no test port free\n");
		UNIT_TEST_COMPLETE()
	}

	const std::thread::id testThread= std::this_thread::get_id();
	std::thread::id handlerThread;
	std::string seenArg;

	server.setRouteHandler("/trigger/t",
						   [&](const HttpRouteRequest& request)
						   {
							   handlerThread= std::this_thread::get_id();
							   const auto it= request.queryArgs.find("user");
							   seenArg= it != request.queryArgs.end() ? it->second : "";

							   HttpRouteResponse response;
							   response.body= R"({"resultCode":"Success"})";
							   return response;
						   });

	// The same path may not be registered as a background route too
	success&=
		!server.setBackgroundRouteHandler("/trigger/t", [](const HttpRouteRequest&) { return HttpRouteResponse(); });
	// And background routes are not listed for the triggers panel
	server.setBackgroundRouteHandler("/upload", [](const HttpRouteRequest&) { return HttpRouteResponse(); });
	success&= (server.getRegisteredRoutePaths().size() == 1);

	PostOnWorker post(makeTestUrl(port, "/trigger/t?user=bob"), "");
	post.pumpUntilDone(server);

	success&= (post.statusCode == 200);
	success&= (post.responseBody == R"({"resultCode":"Success"})");
	success&= (handlerThread == testThread);
	success&= (seenArg == "bob");

	server.dispose();

	UNIT_TEST_COMPLETE()
}

bool http_server_test_dispose_releases_parked_handler()
{
	UNIT_TEST_BEGIN("dispose releases a handler parked in runOnMainThread")

	HttpInterprocessMessageServer server;
	const int port= bindTestServer(server);
	if (port == 0)
	{
		printf("      skipped: no test port free\n");
		UNIT_TEST_COMPLETE()
	}

	std::atomic<bool> bParked{false};
	std::atomic<bool> bHopRan{true};

	server.setBackgroundRouteHandler("/park",
									 [&](const HttpRouteRequest&)
									 {
										 bParked= true;
										 // Never pumped: the test disposes the server instead
										 bHopRan= server.runOnMainThread([]() {}, 30000);

										 HttpRouteResponse response;
										 response.statusCode= 503;
										 return response;
									 });

	PostOnWorker post(makeTestUrl(port, "/park"), "x");

	const auto parkDeadline= std::chrono::steady_clock::now() + std::chrono::seconds(10);
	while (!bParked && std::chrono::steady_clock::now() < parkDeadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	success&= bParked.load();

	const auto disposeStart= std::chrono::steady_clock::now();
	server.dispose();
	const auto disposeMs=
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - disposeStart).count();

	post.worker.join();

	success&= (disposeMs < 5000);
	success&= !bHopRan.load();
	success&= (post.statusCode == 503);
	if (!success)
	{
		printf("      parked=%d disposeMs=%lld hopRan=%d status=%d\n", (int)bParked.load(), (long long)disposeMs,
			   (int)bHopRan.load(), post.statusCode);
	}

	UNIT_TEST_COMPLETE()
}

bool run_http_server_tests()
{
	UNIT_TEST_MODULE_BEGIN("http_server")
	UNIT_TEST_MODULE_CALL_TEST(http_server_test_background_route_answers_off_main);
	UNIT_TEST_MODULE_CALL_TEST(http_server_test_main_thread_route_answers_through_queue);
	UNIT_TEST_MODULE_CALL_TEST(http_server_test_dispose_releases_parked_handler);
	UNIT_TEST_MODULE_END()
}

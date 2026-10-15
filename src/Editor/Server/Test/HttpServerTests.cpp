#include "HttpServerTests.h"
#include "unit_test.h"

#include "HttpInterprocessMessageServer.h"

#include "IxWebSocket/IXHttpClient.h"
#include "IxWebSocket/IXSocket.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdio.h>
#include <string>
#include <thread>
#include <vector>

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

// The byte at a position of the synthetic upload body, so the body can be streamed and
// checked without ever existing whole on either side
uint8_t uploadByteAt(uint64_t position) { return (uint8_t)((position * 31 + (position >> 12)) & 0xFF); }

// The process working set, 0 where it cannot be read
size_t readWorkingSetBytes()
{
#if defined(_WIN32)
	PROCESS_MEMORY_COUNTERS counters= {};
	counters.cb= sizeof(counters);
	if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
		return counters.WorkingSetSize;
#endif
	return 0;
}

// A raw HTTP client over ix::Socket: it writes the request head, then the body in chunks
// from uploadByteAt, so its own memory stays flat however large the body. sentBytes caps
// the body actually sent, which is how a stalled upload is staged.
struct RawUploadClient
{
	ix::Socket socket;
	int statusCode= 0;
	std::string responseBody;
	std::string error;

	bool connect(int port)
	{
		// The DNS lookup calls the cancellation request without a null check, so it
		// gets a real one
		return socket.connect("127.0.0.1", port, error, []() { return false; });
	}

	bool sendHead(const std::string& pathAndQuery, uint64_t contentLength)
	{
		const std::string head= "POST " + pathAndQuery + " HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: "
								+ std::to_string(contentLength) + "\r\n\r\n";
		return socket.writeBytes(head, nullptr);
	}

	bool sendBody(uint64_t byteCount)
	{
		std::string chunk;
		uint64_t position= 0;
		while (position < byteCount)
		{
			const size_t chunkSize= (size_t)std::min<uint64_t>(1 << 16, byteCount - position);
			chunk.resize(chunkSize);
			for (size_t i= 0; i < chunkSize; ++i)
				chunk[i]= (char)uploadByteAt(position + i);
			if (!socket.writeBytes(chunk, nullptr))
				return false;
			position+= chunkSize;
		}
		return true;
	}

	// Reads the status line, the headers, and a Content-Length body. False when the
	// server closed the connection first or the deadline passed.
	bool readResponse(int timeoutSecs)
	{
		std::atomic<bool> cancelFlag{false};
		const auto deadline= std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSecs);
		auto cancel= [&]() { return cancelFlag.load() || std::chrono::steady_clock::now() > deadline; };

		auto statusLine= socket.readLine(cancel);
		if (!statusLine.first)
			return false;
		statusCode= std::atoi(statusLine.second.c_str() + std::string("HTTP/1.1 ").size());

		size_t contentLength= 0;
		while (true)
		{
			auto header= socket.readLine(cancel);
			if (!header.first)
				return false;
			if (header.second == "\r\n" || header.second == "\n" || header.second.empty())
				break;
			const std::string key= "Content-Length: ";
			if (header.second.compare(0, key.size(), key) == 0)
				contentLength= (size_t)std::atoll(header.second.c_str() + key.size());
		}

		if (contentLength > 0)
		{
			auto body= socket.readBytes(contentLength, nullptr, nullptr, cancel);
			if (!body.first)
				return false;
			responseBody= body.second;
		}
		return true;
	}
};

// Streams an upload on a worker thread while the test thread pumps processRequests()
struct RawUploadOnWorker
{
	std::thread worker;
	std::atomic<bool> bDone{false};
	bool bConnected= false;
	bool bSent= false;
	bool bAnswered= false;
	int statusCode= 0;
	std::string responseBody;

	RawUploadOnWorker(int port, const std::string& pathAndQuery, uint64_t declaredLength, uint64_t sentLength)
	{
		worker= std::thread(
			[this, port, pathAndQuery, declaredLength, sentLength]()
			{
				RawUploadClient client;
				bConnected= client.connect(port);
				bSent= bConnected && client.sendHead(pathAndQuery, declaredLength) && client.sendBody(sentLength);
				bAnswered= bSent && client.readResponse(30);
				statusCode= client.statusCode;
				responseBody= client.responseBody;
				bDone= true;
			});
	}

	void pumpUntilDone(HttpInterprocessMessageServer& server, int timeoutSecs= 60)
	{
		const auto deadline= std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSecs);
		while (!bDone && std::chrono::steady_clock::now() < deadline)
		{
			server.processRequests();
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		if (worker.joinable())
			worker.join();
	}
};

// Whether the file holds exactly byteCount bytes of the synthetic body
bool fileMatchesUploadBody(const std::filesystem::path& path, uint64_t byteCount)
{
	std::error_code ec;
	if (std::filesystem::file_size(path, ec) != byteCount || ec)
		return false;

	std::ifstream file(path, std::ios::binary);
	std::vector<char> chunk(1 << 16);
	uint64_t position= 0;
	while (position < byteCount)
	{
		const size_t chunkSize= (size_t)std::min<uint64_t>(chunk.size(), byteCount - position);
		file.read(chunk.data(), (std::streamsize)chunkSize);
		if (!file)
			return false;
		for (size_t i= 0; i < chunkSize; ++i)
		{
			if ((uint8_t)chunk[i] != uploadByteAt(position + i))
				return false;
		}
		position+= chunkSize;
	}
	return true;
}

struct UploadFixtureDir
{
	std::filesystem::path root;

	UploadFixtureDir()
	{
		root= std::filesystem::temp_directory_path() / "mikan_http_server_tests";
		std::filesystem::remove_all(root);
		std::filesystem::create_directories(root);
	}

	~UploadFixtureDir()
	{
		std::error_code ignored;
		std::filesystem::remove_all(root, ignored);
	}
};
} // namespace

bool http_server_test_streaming_route_writes_body_to_file()
{
	UNIT_TEST_BEGIN("streaming route lands a large body on disk without holding it in memory")

	HttpInterprocessMessageServer server;
	const int port= bindTestServer(server);
	if (port == 0)
	{
		printf("      skipped: no test port free\n");
		UNIT_TEST_COMPLETE()
	}

	UploadFixtureDir fixture;
	const std::thread::id testThread= std::this_thread::get_id();
	std::thread::id destinationThread;
	std::thread::id handlerThread;
	uint64_t declaredAtDestination= 0;
	std::filesystem::path handedPath;
	bool bHandlerBodyEmpty= false;
	bool bFileMatched= false;
	size_t workingSetBefore= 0;
	size_t workingSetInHandler= 0;
	int handlerCalls= 0;
	int destinationCalls= 0;

	// 64 MiB plus a tail, well past what a test should ever hold in memory and far more
	// than the old in-memory path's double buffering would have added to the working set
	const uint64_t bodyLength= (64ull << 20) + 12345;

	server.setStreamingRouteHandler(
		"/upload",
		[&](const HttpRouteRequest& request, std::filesystem::path& outBodyFile, HttpRouteResponse& outRefusal)
		{
			++destinationCalls;
			destinationThread= std::this_thread::get_id();
			declaredAtDestination= request.bodyLength;
			if (request.queryArgs.count("refuse") != 0)
			{
				outRefusal.statusCode= 400;
				outRefusal.body= R"({"error":"refused before the body"})";
				return false;
			}
			outBodyFile= fixture.root / ("upload_" + request.queryArgs.at("name"));
			return true;
		},
		[&](const HttpRouteRequest& request)
		{
			++handlerCalls;
			handlerThread= std::this_thread::get_id();
			handedPath= request.bodyFilePath;
			bHandlerBodyEmpty= request.body.empty();
			workingSetInHandler= readWorkingSetBytes();
			bFileMatched= fileMatchesUploadBody(request.bodyFilePath, request.bodyLength);

			HttpRouteResponse response;
			response.body= R"({"ok":true})";
			return response;
		});

	workingSetBefore= readWorkingSetBytes();
	RawUploadOnWorker upload(port, "/upload?name=big.bin", bodyLength, bodyLength);
	upload.pumpUntilDone(server);

	success&= upload.bConnected && upload.bSent && upload.bAnswered;
	success&= (upload.statusCode == 200);
	success&= (upload.responseBody == R"({"ok":true})");
	success&= (destinationCalls == 1 && handlerCalls == 1);
	success&= (destinationThread != testThread && handlerThread != testThread);
	success&= (declaredAtDestination == bodyLength);
	success&= (handedPath == fixture.root / "upload_big.bin");
	success&= bHandlerBodyEmpty;
	success&= bFileMatched;
	if (workingSetBefore > 0 && workingSetInHandler > 0)
	{
		// The old path held the body twice; a quarter of it is a generous allowance for
		// the socket buffers and the allocator
		const size_t growth= workingSetInHandler > workingSetBefore ? workingSetInHandler - workingSetBefore : 0;
		success&= (growth < bodyLength / 4);
		if (growth >= bodyLength / 4)
			printf("      working set grew by %zu bytes during a %llu byte upload\n", growth,
				   (unsigned long long)bodyLength);
	}
	if (!success)
		printf("      connected=%d sent=%d answered=%d status=%d body=%s matched=%d\n", (int)upload.bConnected,
			   (int)upload.bSent, (int)upload.bAnswered, upload.statusCode, upload.responseBody.c_str(),
			   (int)bFileMatched);

	// A refused request still has its body drained and gets the refusal, with no file written
	// and no call to the handler
	RawUploadOnWorker refused(port, "/upload?name=nope.bin&refuse=1", 100000, 100000);
	refused.pumpUntilDone(server);
	success&= refused.bAnswered && (refused.statusCode == 400);
	success&= (refused.responseBody == R"({"error":"refused before the body"})");
	success&= (destinationCalls == 2 && handlerCalls == 1);
	success&= !std::filesystem::exists(fixture.root / "upload_nope.bin");

	// The same path may not be registered twice, nor as another kind
	success&= !server.setBackgroundRouteHandler("/upload", [](const HttpRouteRequest&) { return HttpRouteResponse(); });
	success&= !server.setStreamingRouteHandler(
		"/upload", [](const HttpRouteRequest&, std::filesystem::path&, HttpRouteResponse&) { return false; },
		[](const HttpRouteRequest&) { return HttpRouteResponse(); });

	server.dispose();

	UNIT_TEST_COMPLETE()
}

bool http_server_test_streaming_route_idle_timeout()
{
	UNIT_TEST_BEGIN("a stalled streamed body is cut by the idle timeout and its partial file removed")

	HttpInterprocessMessageServer server;
	int port= 0;
	for (int candidate= 28090; candidate < 28100 && port == 0; ++candidate)
	{
		if (server.initialize(candidate, false, 1))
			port= candidate;
	}
	if (port == 0)
	{
		printf("      skipped: no test port free\n");
		UNIT_TEST_COMPLETE()
	}

	UploadFixtureDir fixture;
	const std::filesystem::path bodyFile= fixture.root / "stalled.bin";
	int handlerCalls= 0;
	server.setStreamingRouteHandler(
		"/upload",
		[&](const HttpRouteRequest&, std::filesystem::path& outBodyFile, HttpRouteResponse&)
		{
			outBodyFile= bodyFile;
			return true;
		},
		[&](const HttpRouteRequest&)
		{
			++handlerCalls;
			return HttpRouteResponse();
		});

	// Declares a megabyte, sends a tenth of it, then waits for an answer that never comes
	const auto start= std::chrono::steady_clock::now();
	RawUploadOnWorker stalled(port, "/upload?name=stalled.bin", 1 << 20, 1 << 17);
	stalled.pumpUntilDone(server, 20);
	const auto elapsedMs=
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

	// The server closed the connection without a reply, in the idle timeout plus the
	// poll slack rather than the 30 s request deadline
	success&= stalled.bConnected && stalled.bSent;
	success&= !stalled.bAnswered;
	success&= (elapsedMs < 10000);
	success&= (handlerCalls == 0);
	success&= !std::filesystem::exists(bodyFile);
	if (!success)
		printf("      connected=%d sent=%d answered=%d elapsedMs=%lld handlerCalls=%d fileExists=%d\n",
			   (int)stalled.bConnected, (int)stalled.bSent, (int)stalled.bAnswered, (long long)elapsedMs, handlerCalls,
			   (int)std::filesystem::exists(bodyFile));

	server.dispose();

	UNIT_TEST_COMPLETE()
}

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
	UNIT_TEST_MODULE_CALL_TEST(http_server_test_streaming_route_writes_body_to_file);
	UNIT_TEST_MODULE_CALL_TEST(http_server_test_streaming_route_idle_timeout);
	UNIT_TEST_MODULE_END()
}

#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ix
{
class HttpServer;
struct HttpBodyPolicy;
struct HttpRequest;
struct HttpResponse;
using HttpRequestPtr= std::shared_ptr<HttpRequest>;
using HttpResponsePtr= std::shared_ptr<HttpResponse>;
} // namespace ix

#define HTTP_SERVER_PORT 8090

struct HttpRouteResponse
{
	int statusCode= 200;
	std::string body;
	std::string contentType= "application/json";
	// Extra response headers, written after Content-Type
	std::map<std::string, std::string> headers;
};

struct HttpRouteRequest
{
	// The HTTP verb (e.g. "GET"/"POST")
	std::string method= "GET";
	// The request URI with any "?query" stripped, which is what routes are matched on
	std::string path;
	// The decoded "?key=value&..." pairs, empty when the URI carried no query string
	std::map<std::string, std::string> queryArgs;
	// The request headers, keyed by lowercased name
	std::map<std::string, std::string> headers;
	// The raw request body (may be empty). Empty for a streaming route, whose body is on disk.
	std::string body;
	// For a streaming route: the file the body was streamed to, once it has arrived
	std::filesystem::path bodyFilePath;
	// The body length the request declared, which a streaming route knows before the body
	uint64_t bodyLength= 0;
};

using HttpRouteHandler= std::function<HttpRouteResponse(const HttpRouteRequest& request)>;
// A streaming route's first call, on the connection thread once the headers are in and
// before the body is read, with request.body empty and request.bodyLength set. Returns the
// file the body should be streamed to, whose directory must exist, or false with outRefusal
// filled, in which case the body is read and dropped and outRefusal is the reply.
using HttpBodyDestinationHandler= std::function<bool(
	const HttpRouteRequest& request, std::filesystem::path& outBodyFilePath, HttpRouteResponse& outRefusal)>;

struct PendingHttpRequest;
using PendingHttpRequestPtr= std::shared_ptr<PendingHttpRequest>;
struct PendingMainThreadJob;
using PendingMainThreadJobPtr= std::shared_ptr<PendingMainThreadJob>;

// Minimal stateless HTTP request/response server, distinct from IInterprocessMessageServer
// (which models persistent, bidirectional websocket connections). Wraps ix::HttpServer, which
// spawns one dedicated thread per accepted connection, reads the whole request there, and
// expects a synchronous response back from that thread.
//
// Two kinds of route:
//   - Main-thread routes (setRouteHandler) run inside processRequests(). Their handlers may
//     touch editor state and Lua script triggers, so the connection thread queues the request
//     and blocks on a promise/future pair until the main thread has answered or a short
//     timeout elapses. These are the script trigger routes the HTTP Triggers panel lists.
//   - Background routes (setBackgroundRouteHandler) run on the connection thread with the
//     request body moved in, so the body never crosses the main thread. A handler that
//     needs editor state hops with runOnMainThread(), which blocks the connection thread
//     until processRequests() has run the job.
//   - Background prefix routes (setBackgroundPrefixRouteHandler) are background routes that
//     answer every path under a prefix. They are matched after the exact paths, longest prefix
//     first, and the handler receives the full path. These serve the client geometry.
//   - Streaming routes (setStreamingRouteHandler) are background routes whose body goes
//     to a file instead of memory: the destination handler names the file before the body
//     is read, the library streams the body into it in chunks as it arrives, and the
//     route handler receives the path. The body is paced by an idle timeout rather than
//     the whole-request deadline, so a multi-gigabyte upload is not cut off while bytes
//     still arrive, and a stalled one is. This is the asset upload route.
//
// Every response allows any origin, so browser clients (the CEF GUI, TypeScript clients) can
// read it. An OPTIONS preflight to a registered path is answered on the connection thread.
// The server binds loopback unless the user enables remote access, which bounds that scope.
//
// dispose() drops every parked request and job before stopping the listener, because
// ix::HttpServer::stop() joins the connection threads and a thread parked in
// runOnMainThread() would otherwise wait on the main thread that is inside stop(). A
// connection still reading its body holds stop() until the read ends, hits the request
// deadline, or, for a streamed body, goes idle for the idle timeout, so a restart during an
// upload stalls the main thread for up to that long.
class HttpInterprocessMessageServer
{
public:
	HttpInterprocessMessageServer();
	~HttpInterprocessMessageServer();

	// bAllowRemote binds every interface instead of loopback, which is what lets another
	// machine (the phone uploading a take) reach the server. streamingIdleTimeoutSecs cuts
	// a streamed body when no byte arrives for that long.
	static constexpr int k_defaultStreamingIdleTimeoutSecs= 60;
	bool initialize(int port= HTTP_SERVER_PORT, bool bAllowRemote= false,
					int streamingIdleTimeoutSecs= k_defaultStreamingIdleTimeoutSecs);
	void dispose();

	// Registers a main-thread handler for an exact route path (e.g. "/trigger/toggle_light").
	// Returns false if the path is already registered.
	bool setRouteHandler(const std::string& path, HttpRouteHandler handler);
	void removeRouteHandler(const std::string& path);

	// Registers a handler that runs on the connection thread for an exact route path.
	// Returns false if the path is already registered as either kind.
	bool setBackgroundRouteHandler(const std::string& path, HttpRouteHandler handler);
	void removeBackgroundRouteHandler(const std::string& path);

	// Registers a connection-thread handler for every path that starts with prefix
	// (e.g. "/geometry/"). Returns false if the prefix is already registered.
	bool setBackgroundPrefixRouteHandler(const std::string& prefix, HttpRouteHandler handler);
	void removeBackgroundPrefixRouteHandler(const std::string& prefix);

	// Registers a streaming route: destinationHandler runs before the body and handler after
	// it, both on the connection thread. Returns false if the path is already registered.
	bool setStreamingRouteHandler(const std::string& path, HttpBodyDestinationHandler destinationHandler,
								  HttpRouteHandler handler);
	void removeStreamingRouteHandler(const std::string& path);

	// For background route handlers: runs the job inside the next processRequests() and
	// blocks the calling connection thread until it has run. Returns false when the job
	// did not run within the timeout or the server is disposing. A job that timed out may
	// still run later, so jobs must be safe to run late.
	bool runOnMainThread(std::function<void()> job, int timeoutMs);

	// Pumps queued HTTP requests and main-thread jobs on the calling thread (expected to be
	// the main thread), invoking any matched route handler and fulfilling the corresponding
	// connection thread's promise.
	void processRequests();

	// Returns the registered main-thread route paths (e.g. for a UI that lists them).
	// Background routes are left out: the HTTP Triggers panel fires each listed route with
	// no arguments, which only makes sense for a trigger.
	// Main-thread-only, same as processRequests() (route handlers may invoke Lua script triggers).
	std::vector<std::string> getRegisteredRoutePaths() const;

	// Invokes a registered route's handler directly, in-process, bypassing the HTTP
	// request queue entirely (e.g. for a UI button that fires a trigger without a real HTTP
	// round-trip). Returns false if no handler is registered for request.path. Main-thread-only.
	bool invokeRouteHandler(const HttpRouteRequest& request, HttpRouteResponse& outResponse);

protected:
	struct StreamingRoute
	{
		HttpBodyDestinationHandler destinationHandler;
		HttpRouteHandler handler;
	};

	// Both run on a per-connection background thread spawned by ix::SocketServer: the
	// first once the headers are in, deciding where the body goes, the second once the
	// whole request has arrived.
	ix::HttpBodyPolicy handleRequestHeaders(ix::HttpRequestPtr request);
	ix::HttpResponsePtr handleIncomingRequest(ix::HttpRequestPtr request);

	// The connection-thread handler for a path: an exact background or streaming route, else the
	// longest matching prefix route. Empty when the path has none.
	HttpRouteHandler findBackgroundHandler(const std::string& path) const;
	bool isRegisteredPath(const std::string& path) const;

private:
	std::shared_ptr<ix::HttpServer> m_server;
	int m_streamingIdleTimeoutSecs= k_defaultStreamingIdleTimeoutSecs;

	mutable std::mutex m_routeHandlersMutex;
	std::map<std::string, HttpRouteHandler> m_routeHandlers;
	std::map<std::string, HttpRouteHandler> m_backgroundRouteHandlers;
	std::map<std::string, StreamingRoute> m_streamingRouteHandlers;
	std::map<std::string, HttpRouteHandler> m_backgroundPrefixRouteHandlers;

	std::mutex m_pendingRequestsMutex;
	std::vector<PendingHttpRequestPtr> m_pendingRequests;
	std::vector<PendingMainThreadJobPtr> m_pendingJobs;
	std::atomic<bool> m_bDisposing{false};
};

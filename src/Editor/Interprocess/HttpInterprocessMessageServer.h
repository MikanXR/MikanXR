#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ix
{
class HttpServer;
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
};

struct HttpRouteRequest
{
	// The HTTP verb (e.g. "GET"/"POST")
	std::string method= "GET";
	// The request URI with any "?query" stripped, which is what routes are matched on
	std::string path;
	// The decoded "?key=value&..." pairs, empty when the URI carried no query string
	std::map<std::string, std::string> queryArgs;
	// The raw request body (may be empty)
	std::string body;
};

using HttpRouteHandler= std::function<HttpRouteResponse(const HttpRouteRequest& request)>;

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
//     request body moved in, so a large upload never crosses the main thread. A handler
//     that needs editor state hops with runOnMainThread(), which blocks the connection thread
//     until processRequests() has run the job.
//
// dispose() drops every parked request and job before stopping the listener, because
// ix::HttpServer::stop() joins the connection threads and a thread parked in
// runOnMainThread() would otherwise wait on the main thread that is inside stop(). A
// connection still reading its body holds stop() until the read ends or hits the request
// deadline, so a restart during an upload stalls the main thread for up to that deadline.
class HttpInterprocessMessageServer
{
public:
	HttpInterprocessMessageServer();
	~HttpInterprocessMessageServer();

	// bAllowRemote binds every interface instead of loopback, which is what lets another
	// machine (the phone uploading a take) reach the server
	bool initialize(int port= HTTP_SERVER_PORT, bool bAllowRemote= false);
	void dispose();

	// Registers a main-thread handler for an exact route path (e.g. "/trigger/toggle_light").
	// Returns false if the path is already registered.
	bool setRouteHandler(const std::string& path, HttpRouteHandler handler);
	void removeRouteHandler(const std::string& path);

	// Registers a handler that runs on the connection thread for an exact route path.
	// Returns false if the path is already registered as either kind.
	bool setBackgroundRouteHandler(const std::string& path, HttpRouteHandler handler);
	void removeBackgroundRouteHandler(const std::string& path);

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
	// Runs on a per-connection background thread spawned by ix::SocketServer.
	ix::HttpResponsePtr handleIncomingRequest(ix::HttpRequestPtr request);

private:
	std::shared_ptr<ix::HttpServer> m_server;

	mutable std::mutex m_routeHandlersMutex;
	std::map<std::string, HttpRouteHandler> m_routeHandlers;
	std::map<std::string, HttpRouteHandler> m_backgroundRouteHandlers;

	std::mutex m_pendingRequestsMutex;
	std::vector<PendingHttpRequestPtr> m_pendingRequests;
	std::vector<PendingMainThreadJobPtr> m_pendingJobs;
	std::atomic<bool> m_bDisposing{false};
};

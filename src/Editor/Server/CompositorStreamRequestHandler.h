#pragma once

#include "HttpInterprocessMessageServer.h"
#include "IServerRequestHandler.h"

#include <functional>
#include <string>

// What a compositor id resolves to for the stream route
enum class eCompositorStreamQueryResult : int
{
	found,
	// No compositor has the id, or the project has none
	unknownCompositor,
	// No id was given and the project has more than one compositor
	ambiguousCompositor,
	// The compositor exists but its RTP output is off or has no destination
	notStreaming,
	// The editor did not answer in time
	unavailable,
};

// What the SDP describes: the destination the RTP packets go to and the picture they carry
struct CompositorStreamInfo
{
	int compositorId= -1;
	std::string host;
	int port= 0;
	int width= 0;
	int height= 0;
	float fps= 0.f;
};

// Serves the session description for a compositor's RTP output over the HTTP
// message server:
//
//   GET /compositor/stream.sdp?compositor=<id>
//
// The id may be left off when the project has one compositor. The reply is an
// application/sdp document a player opens to receive the stream, 404 when the
// compositor is missing or its RTP output is off. The route runs on the connection
// thread and hops to the main thread once to read the compositor. The reply is
// built by buildResponse so the tests can drive the route with a stub query.
class CompositorStreamRequestHandler : public IServerRequestHandler
{
public:
	CompositorStreamRequestHandler(class MikanServer* owner)
		: IServerRequestHandler(owner)
	{
	}
	virtual ~CompositorStreamRequestHandler();

	virtual bool startup(class MainWindow* mainWindow) override;
	virtual void shutdown() override;

	static const char* k_routePath;

	// compositorId is -1 when the request named none
	using StreamQuery= std::function<eCompositorStreamQueryResult(int compositorId, CompositorStreamInfo& outInfo)>;

	// The SDP text for a stream, CRLF line endings as the format requires
	static std::string buildSdp(const CompositorStreamInfo& info);
	// The reply for one request: 405 for a verb other than GET, 400 for a malformed
	// or ambiguous compositor id, 404 for a missing compositor or one not streaming,
	// 504 when the editor did not answer, otherwise the SDP
	static HttpRouteResponse buildResponse(const HttpRouteRequest& request, const StreamQuery& query);
	// Registers the route on a server with the given query
	static bool registerRoute(HttpInterprocessMessageServer* server, const StreamQuery& query);

private:
	eCompositorStreamQueryResult queryOnMainThread(int compositorId, CompositorStreamInfo& outInfo);
	// Runs on the main thread: finds the compositor and reads its output settings
	eCompositorStreamQueryResult queryNow(int compositorId, CompositorStreamInfo& outInfo);
};

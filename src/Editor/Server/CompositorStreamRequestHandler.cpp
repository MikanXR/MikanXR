#include "CompositorStreamRequestHandler.h"
#include "CompositorComponent.h"
#include "CompositorObjectSystem.h"
#include "JsonUtils.h"
#include "MikanServer.h"

#include <cstdlib>
#include <memory>
#include <sstream>

const char* CompositorStreamRequestHandler::k_routePath= "/compositor/stream.sdp";

namespace
{
// Generous: it only trips when the main thread is wedged, not on a slow frame
constexpr int k_streamHopTimeoutMs= 2000;

HttpRouteResponse makeStreamErrorResponse(int statusCode, const std::string& error)
{
	HttpRouteResponse response;
	response.statusCode= statusCode;
	response.body= json{{"error", error}}.dump();
	return response;
}
} // namespace

CompositorStreamRequestHandler::~CompositorStreamRequestHandler() {}

bool CompositorStreamRequestHandler::startup(MainWindow* mainWindow)
{
	return registerRoute(m_owner->getHttpMessageServer(), [this](int compositorId, CompositorStreamInfo& outInfo)
						 { return queryOnMainThread(compositorId, outInfo); });
}

void CompositorStreamRequestHandler::shutdown()
{
	m_owner->getHttpMessageServer()->removeBackgroundRouteHandler(k_routePath);
}

bool CompositorStreamRequestHandler::registerRoute(HttpInterprocessMessageServer* server, const StreamQuery& query)
{
	return server->setBackgroundRouteHandler(k_routePath, [query](const HttpRouteRequest& request)
											 { return buildResponse(request, query); });
}

std::string CompositorStreamRequestHandler::buildSdp(const CompositorStreamInfo& info)
{
	// The connection address is where the receiver listens, which is the
	// destination the editor sends to. SPS and PPS ride every keyframe, so no
	// sprop-parameter-sets is needed.
	std::ostringstream ss;
	ss << "v=0\r\n";
	ss << "o=- 0 0 IN IP4 " << info.host << "\r\n";
	ss << "s=MikanXR compositor " << info.compositorId << "\r\n";
	ss << "c=IN IP4 " << info.host << "\r\n";
	ss << "t=0 0\r\n";
	ss << "m=video " << info.port << " RTP/AVP 96\r\n";
	ss << "a=rtpmap:96 H264/90000\r\n";
	ss << "a=fmtp:96 packetization-mode=1\r\n";
	if (info.width > 0 && info.height > 0)
	{
		ss << "a=framesize:96 " << info.width << "-" << info.height << "\r\n";
	}
	if (info.fps > 0.f)
	{
		ss << "a=framerate:" << info.fps << "\r\n";
	}
	ss << "a=recvonly\r\n";

	return ss.str();
}

HttpRouteResponse CompositorStreamRequestHandler::buildResponse(const HttpRouteRequest& request,
																const StreamQuery& query)
{
	if (request.method != "GET")
	{
		return makeStreamErrorResponse(405, "Query with GET");
	}

	int compositorId= -1;
	const auto idIt= request.queryArgs.find("compositor");
	if (idIt != request.queryArgs.end())
	{
		char* end= nullptr;
		const long parsedId= std::strtol(idIt->second.c_str(), &end, 10);
		if (idIt->second.empty() || end == idIt->second.c_str() || *end != '\0' || parsedId < 0)
		{
			return makeStreamErrorResponse(400, "compositor must be a component id");
		}
		compositorId= (int)parsedId;
	}

	CompositorStreamInfo info;
	switch (query(compositorId, info))
	{
	case eCompositorStreamQueryResult::found:
		break;
	case eCompositorStreamQueryResult::unknownCompositor:
		return makeStreamErrorResponse(404, compositorId >= 0 ? "No compositor with id " + std::to_string(compositorId)
															  : "The project has no compositor");
	case eCompositorStreamQueryResult::ambiguousCompositor:
		return makeStreamErrorResponse(400, "The project has several compositors, give a compositor id");
	case eCompositorStreamQueryResult::notStreaming:
		return makeStreamErrorResponse(404, "The compositor's RTP output is off");
	default:
		return makeStreamErrorResponse(504, "Timed out waiting for the editor");
	}

	HttpRouteResponse response;
	response.contentType= "application/sdp";
	response.body= buildSdp(info);
	return response;
}

eCompositorStreamQueryResult CompositorStreamRequestHandler::queryOnMainThread(int compositorId,
																			   CompositorStreamInfo& outInfo)
{
	struct QueryResult
	{
		eCompositorStreamQueryResult result= eCompositorStreamQueryResult::unavailable;
		CompositorStreamInfo info;
	};
	auto queryResult= std::make_shared<QueryResult>();

	// The compositors are main-thread state
	const bool bRan= m_owner->getHttpMessageServer()->runOnMainThread(
		[this, compositorId, queryResult]() { queryResult->result= queryNow(compositorId, queryResult->info); },
		k_streamHopTimeoutMs);
	if (!bRan)
		return eCompositorStreamQueryResult::unavailable;

	outInfo= queryResult->info;
	return queryResult->result;
}

eCompositorStreamQueryResult CompositorStreamRequestHandler::queryNow(int compositorId, CompositorStreamInfo& outInfo)
{
	CompositorObjectSystemPtr compositorSystem= getObjectSystemOfType<CompositorObjectSystem>();
	if (!compositorSystem)
		return eCompositorStreamQueryResult::unknownCompositor;

	if (compositorId < 0)
	{
		// Default to the project's single compositor; several need an explicit id
		std::vector<int> compositorIds;
		compositorSystem->getComponentIdList(CompositorComponent::k_componentClassName, compositorIds);
		if (compositorIds.empty())
			return eCompositorStreamQueryResult::unknownCompositor;
		if (compositorIds.size() > 1)
			return eCompositorStreamQueryResult::ambiguousCompositor;
		compositorId= compositorIds[0];
	}

	CompositorComponentPtr compositor= compositorSystem->getCompositorById(compositorId);
	if (!compositor)
		return eCompositorStreamQueryResult::unknownCompositor;

	CompositorDefinitionConstPtr definition= compositor->getCompositorDefinition();
	if (!definition->getIsRtpOutputStreaming() || definition->getRtpOutputHost().empty())
		return eCompositorStreamQueryResult::notStreaming;

	outInfo.compositorId= compositorId;
	outInfo.host= definition->getRtpOutputHost();
	outInfo.port= definition->getRtpOutputPort();
	outInfo.fps= compositor->getRtpOutputFrameRate();
	if (!compositor->getRtpOutputSize(outInfo.width, outInfo.height))
	{
		outInfo.width= 0;
		outInfo.height= 0;
	}

	return eCompositorStreamQueryResult::found;
}

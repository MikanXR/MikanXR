#include "CompositorStreamRequestHandler.h"
#include "RTPOutputComponent.h"
#include "RTPOutputSystem.h"
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
	return registerRoute(m_owner->getHttpMessageServer(), [this](int outputId, CompositorStreamInfo& outInfo)
						 { return queryOnMainThread(outputId, outInfo); });
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
	ss << "s=MikanXR output " << info.outputId << "\r\n";
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

	int outputId= -1;
	const auto idIt= request.queryArgs.find("output");
	if (idIt != request.queryArgs.end())
	{
		char* end= nullptr;
		const long parsedId= std::strtol(idIt->second.c_str(), &end, 10);
		if (idIt->second.empty() || end == idIt->second.c_str() || *end != '\0' || parsedId < 0)
		{
			return makeStreamErrorResponse(400, "output must be a component id");
		}
		outputId= (int)parsedId;
	}

	CompositorStreamInfo info;
	switch (query(outputId, info))
	{
	case eCompositorStreamQueryResult::found:
		break;
	case eCompositorStreamQueryResult::unknownOutput:
		return makeStreamErrorResponse(404, outputId >= 0 ? "No RTP output with id " + std::to_string(outputId)
														  : "The project has no RTP output");
	case eCompositorStreamQueryResult::ambiguousOutput:
		return makeStreamErrorResponse(400, "The project has several RTP outputs, give an output id");
	case eCompositorStreamQueryResult::notStreaming:
		return makeStreamErrorResponse(404, "The RTP output is off or has no destination");
	default:
		return makeStreamErrorResponse(504, "Timed out waiting for the editor");
	}

	HttpRouteResponse response;
	response.contentType= "application/sdp";
	response.body= buildSdp(info);
	return response;
}

eCompositorStreamQueryResult CompositorStreamRequestHandler::queryOnMainThread(int outputId,
																			   CompositorStreamInfo& outInfo)
{
	struct QueryResult
	{
		eCompositorStreamQueryResult result= eCompositorStreamQueryResult::unavailable;
		CompositorStreamInfo info;
	};
	auto queryResult= std::make_shared<QueryResult>();

	// The outputs are main-thread state
	const bool bRan= m_owner->getHttpMessageServer()->runOnMainThread(
		[this, outputId, queryResult]() { queryResult->result= queryNow(outputId, queryResult->info); },
		k_streamHopTimeoutMs);
	if (!bRan)
		return eCompositorStreamQueryResult::unavailable;

	outInfo= queryResult->info;
	return queryResult->result;
}

eCompositorStreamQueryResult CompositorStreamRequestHandler::queryNow(int outputId, CompositorStreamInfo& outInfo)
{
	RTPOutputSystemPtr rtpOutputSystem= getObjectSystemOfType<RTPOutputSystem>();
	if (!rtpOutputSystem)
		return eCompositorStreamQueryResult::unknownOutput;

	if (outputId < 0)
	{
		// Default to the project's single RTP output; several need an explicit id
		std::vector<int> outputIds;
		rtpOutputSystem->getComponentIdList(RTPOutputComponent::k_componentClassName, outputIds);
		if (outputIds.empty())
			return eCompositorStreamQueryResult::unknownOutput;
		if (outputIds.size() > 1)
			return eCompositorStreamQueryResult::ambiguousOutput;
		outputId= outputIds[0];
	}

	RTPOutputComponentPtr output= rtpOutputSystem->getOutputById(outputId);
	if (!output)
		return eCompositorStreamQueryResult::unknownOutput;

	if (!output->getHasDestination())
		return eCompositorStreamQueryResult::notStreaming;

	RTPOutputDefinitionConstPtr definition= output->getRTPOutputDefinition();
	outInfo.outputId= outputId;
	outInfo.host= definition->getRtpOutputHost();
	outInfo.port= definition->getRtpOutputPort();
	// The picture is known once a scene has published a frame through the output
	if (!output->getStreamFormat(outInfo.width, outInfo.height, outInfo.fps))
	{
		outInfo.width= 0;
		outInfo.height= 0;
		outInfo.fps= 0.f;
	}

	return eCompositorStreamQueryResult::found;
}

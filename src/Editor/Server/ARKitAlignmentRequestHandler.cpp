#include "ARKitAlignmentRequestHandler.h"
#include "ARKitMarkerAutoAligner.h"
#include "ARKitVideoSourceComponent.h"
#include "ARKitVideoSourceSystem.h"
#include "JsonUtils.h"
#include "Logger.h"
#include "MainWindow.h"
#include "MikanServer.h"

#include <chrono>
#include <cstdlib>
#include <memory>

const char* ARKitAlignmentRequestHandler::k_routePath= "/arkit/alignment";

namespace
{
// Generous: it only trips when the main thread is wedged, not on a slow frame
constexpr int k_alignmentHopTimeoutMs= 2000;

HttpRouteResponse makeAlignmentErrorResponse(int statusCode, const std::string& error)
{
	HttpRouteResponse response;
	response.statusCode= statusCode;
	response.body= json{{"error", error}}.dump();
	return response;
}
} // namespace

ARKitAlignmentRequestHandler::~ARKitAlignmentRequestHandler() {}

double ARKitAlignmentRequestHandler::nowSeconds()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool ARKitAlignmentRequestHandler::startup(MainWindow* mainWindow)
{
	return registerRoute(m_owner->getHttpMessageServer(), [this](int basePort, bool& outMarkerVisible, bool& outAligned)
						 { return queryOnMainThread(basePort, outMarkerVisible, outAligned); });
}

void ARKitAlignmentRequestHandler::shutdown()
{
	m_owner->getHttpMessageServer()->removeBackgroundRouteHandler(k_routePath);
	m_aligners.clear();
}

bool ARKitAlignmentRequestHandler::registerRoute(HttpInterprocessMessageServer* server, const AlignmentQuery& query)
{
	return server->setBackgroundRouteHandler(k_routePath, [query](const HttpRouteRequest& request)
											 { return buildResponse(request, query); });
}

HttpRouteResponse ARKitAlignmentRequestHandler::buildResponse(const HttpRouteRequest& request,
															  const AlignmentQuery& query)
{
	if (request.method != "GET")
	{
		return makeAlignmentErrorResponse(405, "Query with GET");
	}

	const auto portIt= request.queryArgs.find("port");
	if (portIt == request.queryArgs.end() || portIt->second.empty())
	{
		return makeAlignmentErrorResponse(400, "Missing port query argument");
	}

	char* end= nullptr;
	const long basePort= std::strtol(portIt->second.c_str(), &end, 10);
	if (end == portIt->second.c_str() || *end != '\0' || basePort <= 0 || basePort > 65535)
	{
		return makeAlignmentErrorResponse(400, "port must be a UDP port number");
	}

	bool bMarkerVisible= false;
	bool bAligned= false;
	switch (query((int)basePort, bMarkerVisible, bAligned))
	{
	case eARKitAlignmentQueryResult::found:
		break;
	case eARKitAlignmentQueryResult::unknownPort:
		return makeAlignmentErrorResponse(404, "No ARKit video source streams to port " + std::to_string(basePort));
	default:
		return makeAlignmentErrorResponse(504, "Timed out waiting for the editor");
	}

	HttpRouteResponse response;
	response.body= json{{"marker_visible", bMarkerVisible}, {"aligned", bAligned}}.dump();
	return response;
}

eARKitAlignmentQueryResult ARKitAlignmentRequestHandler::queryOnMainThread(int basePort, bool& outMarkerVisible,
																		   bool& outAligned)
{
	struct QueryResult
	{
		eARKitAlignmentQueryResult result= eARKitAlignmentQueryResult::unavailable;
		bool bMarkerVisible= false;
		bool bAligned= false;
	};
	auto queryResult= std::make_shared<QueryResult>();

	// The sources and the aligners are main-thread state
	const bool bRan= m_owner->getHttpMessageServer()->runOnMainThread(
		[this, basePort, queryResult]()
		{ queryResult->result= queryNow(basePort, queryResult->bMarkerVisible, queryResult->bAligned); },
		k_alignmentHopTimeoutMs);
	if (!bRan)
		return eARKitAlignmentQueryResult::unavailable;

	outMarkerVisible= queryResult->bMarkerVisible;
	outAligned= queryResult->bAligned;
	return queryResult->result;
}

eARKitAlignmentQueryResult ARKitAlignmentRequestHandler::queryNow(int basePort, bool& outMarkerVisible,
																  bool& outAligned)
{
	ARKitVideoSourceSystemPtr arkitSystem= getObjectSystemOfType<ARKitVideoSourceSystem>();
	if (!arkitSystem)
		return eARKitAlignmentQueryResult::unknownPort;

	ARKitVideoSourceComponentPtr source;
	for (const auto& entry : arkitSystem->getComponentMap())
	{
		ARKitVideoSourceComponentPtr candidate= entry.second.lock();
		if (candidate && candidate->getARKitVideoSourceDefinition()->getBasePort() == basePort)
		{
			source= candidate;
			break;
		}
	}
	if (!source)
		return eARKitAlignmentQueryResult::unknownPort;

	outAligned= source->hasPoseOffset();
	outMarkerVisible= false;

	// An unaligned source being asked about gets an aligner, kept alive by the polls
	const double now= nowSeconds();
	auto alignerIt= m_aligners.find(source->getComponentId());
	if (!outAligned)
	{
		if (alignerIt == m_aligners.end())
		{
			alignerIt= m_aligners
						   .emplace(source->getComponentId(),
									std::make_unique<ARKitMarkerAutoAligner>(m_owner->getOwnerWindow(), source))
						   .first;
			MIKAN_LOG_INFO("ARKitAlignmentRequestHandler")
				<< "Watching for the origin marker on " << source->getDevicePath() << " while the phone asks";
		}
		alignerIt->second->notifyPolled(now);
	}
	if (alignerIt != m_aligners.end())
	{
		outMarkerVisible= alignerIt->second->isMarkerVisible();
	}

	return eARKitAlignmentQueryResult::found;
}

void ARKitAlignmentRequestHandler::update()
{
	if (m_aligners.empty())
		return;

	const double now= nowSeconds();
	for (auto it= m_aligners.begin(); it != m_aligners.end();)
	{
		it->second->update(now);
		if (it->second->shouldRetire(now))
		{
			it= m_aligners.erase(it);
		}
		else
		{
			++it;
		}
	}
}

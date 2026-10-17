#include "ModelGeometryRequestHandler.h"
#include "JsonUtils.h"
#include "MikanRenderModelResource.h"
#include "MikanServer.h"
#include "ModelShapeComponent.h"
#include "ModelShapeSystem.h"
#include "ModelStencilComponent.h"
#include "ModelStencilSystem.h"

#include <cstdlib>

const char* ModelGeometryRequestHandler::k_routePrefix= "/geometry/";

namespace
{
// Generous: it only trips when the main thread is wedged, though a first request also builds the GLB
constexpr int k_geometryHopTimeoutMs= 2000;

const char* k_stencilKindSegment= "stencil";
const char* k_shapeKindSegment= "shape";

HttpRouteResponse makeGeometryErrorResponse(int statusCode, const std::string& error)
{
	HttpRouteResponse response;
	response.statusCode= statusCode;
	response.body= json{{"error", error}}.dump();
	return response;
}

// Parses "<kind>/<id>" after the route prefix
bool parseGeometryPath(const std::string& path, eModelGeometryKind& outKind, int32_t& outComponentId)
{
	const std::string prefix= ModelGeometryRequestHandler::k_routePrefix;
	if (path.compare(0, prefix.size(), prefix) != 0)
	{
		return false;
	}

	const std::string tail= path.substr(prefix.size());
	const size_t slashPos= tail.find('/');
	if (slashPos == std::string::npos)
	{
		return false;
	}

	const std::string kindSegment= tail.substr(0, slashPos);
	if (kindSegment == k_stencilKindSegment)
	{
		outKind= eModelGeometryKind::stencil;
	}
	else if (kindSegment == k_shapeKindSegment)
	{
		outKind= eModelGeometryKind::shape;
	}
	else
	{
		return false;
	}

	const std::string idSegment= tail.substr(slashPos + 1);
	if (idSegment.empty())
	{
		return false;
	}

	char* end= nullptr;
	const long componentId= std::strtol(idSegment.c_str(), &end, 10);
	if (*end != '\0' || componentId < 0 || componentId > INT32_MAX)
	{
		return false;
	}

	outComponentId= (int32_t)componentId;
	return true;
}

// True when an If-None-Match list names the etag, weak or strong, or is "*"
bool ifNoneMatchHasETag(const std::string& ifNoneMatch, const std::string& etag)
{
	size_t tokenStart= 0;
	while (tokenStart <= ifNoneMatch.size())
	{
		const size_t tokenEnd= std::min(ifNoneMatch.find(',', tokenStart), ifNoneMatch.size());
		std::string token= ifNoneMatch.substr(tokenStart, tokenEnd - tokenStart);
		tokenStart= tokenEnd + 1;

		const size_t first= token.find_first_not_of(" \t");
		const size_t last= token.find_last_not_of(" \t");
		if (first == std::string::npos)
		{
			continue;
		}
		token= token.substr(first, last - first + 1);

		if (token == "*")
		{
			return true;
		}
		if (token.compare(0, 2, "W/") == 0)
		{
			token= token.substr(2);
		}
		if (token.size() >= 2 && token.front() == '"' && token.back() == '"')
		{
			token= token.substr(1, token.size() - 2);
		}
		if (token == etag)
		{
			return true;
		}
	}

	return false;
}
} // namespace

bool ModelGeometryRequestHandler::startup(MainWindow* mainWindow)
{
	return registerRoute(m_owner->getHttpMessageServer(),
						 [this](eModelGeometryKind kind, int32_t componentId, ModelGeometryPayload& outPayload)
						 { return resolveOnMainThread(kind, componentId, outPayload); });
}

void ModelGeometryRequestHandler::shutdown()
{
	m_owner->getHttpMessageServer()->removeBackgroundPrefixRouteHandler(k_routePrefix);
}

std::string ModelGeometryRequestHandler::makeGeometryUrl(eModelGeometryKind kind, int32_t componentId,
														 const std::string& etag)
{
	if (etag.empty())
	{
		return std::string();
	}

	const char* kindSegment= kind == eModelGeometryKind::stencil ? k_stencilKindSegment : k_shapeKindSegment;
	return std::string(k_routePrefix) + kindSegment + "/" + std::to_string(componentId) + "?v=" + etag;
}

bool ModelGeometryRequestHandler::registerRoute(HttpInterprocessMessageServer* server, const GeometryResolver& resolver)
{
	return server->setBackgroundPrefixRouteHandler(k_routePrefix, [resolver](const HttpRouteRequest& request)
												   { return buildResponse(request, resolver); });
}

HttpRouteResponse ModelGeometryRequestHandler::buildResponse(const HttpRouteRequest& request,
															 const GeometryResolver& resolver)
{
	eModelGeometryKind kind;
	int32_t componentId;
	if (!parseGeometryPath(request.path, kind, componentId))
	{
		return makeGeometryErrorResponse(404, "Expected /geometry/stencil/<id> or /geometry/shape/<id>");
	}

	if (request.method != "GET")
	{
		return makeGeometryErrorResponse(405, "Fetch geometry with GET");
	}

	ModelGeometryPayload payload;
	switch (resolver(kind, componentId, payload))
	{
	case eModelGeometryQueryResult::found:
		break;
	case eModelGeometryQueryResult::notFound:
		return makeGeometryErrorResponse(404, "No loaded model for component " + std::to_string(componentId));
	case eModelGeometryQueryResult::buildFailed:
		return makeGeometryErrorResponse(500, "Failed to build the model's geometry");
	default:
		return makeGeometryErrorResponse(504, "Timed out waiting for the editor");
	}

	HttpRouteResponse response;
	response.headers["ETag"]= "\"" + payload.etag + "\"";
	response.headers["Cache-Control"]= "no-cache";

	const auto ifNoneMatchIt= request.headers.find("if-none-match");
	if (ifNoneMatchIt != request.headers.end() && ifNoneMatchHasETag(ifNoneMatchIt->second, payload.etag))
	{
		response.statusCode= 304;
		response.contentType= "model/gltf-binary";
		return response;
	}

	response.contentType= "model/gltf-binary";
	response.body.assign((const char*)payload.glbBytes->data(), payload.glbBytes->size());
	return response;
}

eModelGeometryQueryResult ModelGeometryRequestHandler::resolveOnMainThread(eModelGeometryKind kind, int32_t componentId,
																		   ModelGeometryPayload& outPayload)
{
	struct QueryResult
	{
		eModelGeometryQueryResult result= eModelGeometryQueryResult::unavailable;
		ModelGeometryPayload payload;
	};
	auto queryResult= std::make_shared<QueryResult>();

	// The components and their model resources are main-thread state
	const bool bRan= m_owner->getHttpMessageServer()->runOnMainThread(
		[this, kind, componentId, queryResult]()
		{ queryResult->result= resolveNow(kind, componentId, queryResult->payload); }, k_geometryHopTimeoutMs);
	if (!bRan)
		return eModelGeometryQueryResult::unavailable;

	outPayload= queryResult->payload;
	return queryResult->result;
}

eModelGeometryQueryResult ModelGeometryRequestHandler::resolveNow(eModelGeometryKind kind, int32_t componentId,
																  ModelGeometryPayload& outPayload)
{
	MikanRenderModelResourcePtr modelResource;
	if (kind == eModelGeometryKind::stencil)
	{
		ModelStencilSystemPtr stencilSystem= getObjectSystemOfType<ModelStencilSystem>();
		ModelStencilComponentPtr stencil= stencilSystem ? stencilSystem->getModelStencilById(componentId) : nullptr;
		modelResource= stencil ? stencil->getRenderModelResource() : nullptr;
	}
	else
	{
		ModelShapeSystemPtr shapeSystem= getObjectSystemOfType<ModelShapeSystem>();
		ModelShapeComponentPtr shape= shapeSystem ? shapeSystem->getModelShapeById(componentId) : nullptr;
		modelResource= shape ? shape->getRenderModelResource() : nullptr;
	}

	if (!modelResource)
	{
		return eModelGeometryQueryResult::notFound;
	}

	outPayload.etag= modelResource->getGeometryETag();
	outPayload.glbBytes= modelResource->getOrBuildGlbPayload();
	return outPayload.glbBytes ? eModelGeometryQueryResult::found : eModelGeometryQueryResult::buildFailed;
}

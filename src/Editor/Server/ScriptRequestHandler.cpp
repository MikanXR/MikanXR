#include "App.h"
#include "AppStage.h"
#include "CommonScriptContext.h"
#include "HttpInterprocessMessageServer.h"
#include "ScriptRequestHandler.h"
#include "Logger.h"
#include "MainWindow.h"
#include "MikanServer.h"
#include "MikanScriptEvents.h"
#include "MikanScriptRequests.h"
#include "ProjectManager.h"
#include "ScriptObjectSystem.h"
#include "ServerResponseHelpers.h"

#include <functional>

using namespace std::placeholders;

namespace
{
HttpRouteResponse makeTriggerResponse(MikanAPIResult result)
{
	HttpRouteResponse response;
	switch (result)
	{
	case MikanAPIResult::Success:
		response.statusCode= 200;
		response.body= "{\"resultCode\":\"Success\"}";
		break;
	case MikanAPIResult::MalformedParameters:
		response.statusCode= 400;
		response.body= "{\"resultCode\":\"MalformedParameters\"}";
		break;
	case MikanAPIResult::RequestFailed:
		response.statusCode= 422;
		response.body= "{\"resultCode\":\"RequestFailed\"}";
		break;
	default:
		response.statusCode= 500;
		response.body= "{\"resultCode\":\"GeneralError\"}";
		break;
	}

	return response;
}
} // namespace

// -- ScriptRequestHandler -- //
bool ScriptRequestHandler::startup(MainWindow* mainWindow)
{
	IInterprocessMessageServer* messageServer= m_owner->getMessageServer();

	// Script Requests
	messageServer->setRequestHandler(InvokeScriptTrigger::staticGetArchetype().getName(),
									 std::bind(&ScriptRequestHandler::invokeScriptTriggerHandler, this, _1, _2));
	messageServer->setRequestHandler(SendScriptMessage::staticGetArchetype().getName(),
									 std::bind(&ScriptRequestHandler::invokeScriptMessageHandler, this, _1, _2));

	return true;
}

void ScriptRequestHandler::shutdown() { removeHttpRoutes(); }

void ScriptRequestHandler::bindScriptContext(CommonScriptContextPtr scriptContext)
{
	if (CommonScriptContextPtr boundContext= m_scriptContext.lock())
	{
		unbindScriptContext(boundContext);
	}

	m_scriptContext= scriptContext;
	scriptContext->OnScriptMessage+= MakeDelegate(this, &ScriptRequestHandler::publishScriptMessageEvent);

	installHttpRoutes();
}

void ScriptRequestHandler::unbindScriptContext(CommonScriptContextPtr scriptContext)
{
	if (m_scriptContext.lock() != scriptContext)
		return;

	scriptContext->OnScriptMessage-= MakeDelegate(this, &ScriptRequestHandler::publishScriptMessageEvent);
	m_scriptContext.reset();

	removeHttpRoutes();
}

void ScriptRequestHandler::setHttpRoutes(const ScriptHttpRouteTable& routes)
{
	removeHttpRoutes();
	m_httpRoutes= routes;

	if (!m_scriptContext.expired())
	{
		installHttpRoutes();
	}
}

void ScriptRequestHandler::installHttpRoutes()
{
	if (m_bRoutesInstalled)
		return;

	for (const ScriptHttpRoute& route : m_httpRoutes.getRoutes())
	{
		registerHttpTriggerRoute(route);
	}
	m_bRoutesInstalled= true;
}

void ScriptRequestHandler::removeHttpRoutes()
{
	if (!m_bRoutesInstalled)
		return;

	for (const ScriptHttpRoute& route : m_httpRoutes.getRoutes())
	{
		unregisterHttpTriggerRoute(route);
	}
	m_bRoutesInstalled= false;
}

bool ScriptRequestHandler::registerHttpTriggerRoute(const ScriptHttpRoute& route)
{
	HttpInterprocessMessageServer* httpServer= m_owner->getHttpMessageServer();
	if (!httpServer)
	{
		MIKAN_LOG_WARNING("ScriptRequestHandler::registerHttpTriggerRoute")
			<< "No HTTP server to register route " << route.getRoutePath() << " on";
		return false;
	}

	// The route resolves its script at request time, so a reload that swaps
	// the instance or a table edit that retargets it needs no reinstall
	HttpRouteHandler handler= [this, route](const HttpRouteRequest& request) -> HttpRouteResponse
	{ return makeTriggerResponse(invokeScriptHttpTriggerInternal(route, request.queryArgs)); };

	return httpServer->setRouteHandler(route.getRoutePath(), handler);
}

void ScriptRequestHandler::unregisterHttpTriggerRoute(const ScriptHttpRoute& route)
{
	HttpInterprocessMessageServer* httpServer= m_owner->getHttpMessageServer();
	if (httpServer)
	{
		httpServer->removeRouteHandler(route.getRoutePath());
	}
}

// Scripting Events
void ScriptRequestHandler::publishScriptMessageEvent(const std::string& message)
{
	MikanScriptMessagePostedEvent messageInfo;
	messageInfo.message= message.c_str();

	m_owner->publishMikanJsonEvent(mikanTypeToJsonString(messageInfo));
}

// Scripting Requests
void ScriptRequestHandler::invokeScriptTriggerHandler(const ClientRequest& request, ClientResponse& response)
{
	InvokeScriptTrigger scriptTriggerRequest;
	if (!readTypedRequest(request.utf8RequestString, scriptTriggerRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	std::map<std::string, std::string> triggerArgs;
	for (const auto& entry : scriptTriggerRequest.trigger_args)
	{
		triggerArgs[entry.key.getUtf8Value()]= entry.value.getUtf8Value();
	}

	MikanAPIResult result= invokeScriptTriggerInternal(scriptTriggerRequest.script_name.getUtf8Value(),
													   scriptTriggerRequest.trigger_name.getUtf8Value(), triggerArgs);

	writeSimpleJsonResponse(request.requestId, result, response);
}

MikanAPIResult ScriptRequestHandler::invokeScriptTriggerInternal(const std::string& scriptName,
																 const std::string& triggerName,
																 const std::map<std::string, std::string>& args)
{
	CommonScriptContextPtr scriptContext= m_scriptContext.lock();
	if (!scriptContext)
	{
		return MikanAPIResult::RequestFailed;
	}

	MikanScriptID targetScriptId= INVALID_MIKAN_ID;
	if (!scriptName.empty())
	{
		ProjectManagerPtr projectManager= m_owner->getProjectManager();
		ScriptObjectSystemPtr scriptSystem=
			projectManager ? projectManager->getSystemOfType<ScriptObjectSystem>() : nullptr;
		ScriptComponentPtr script= scriptSystem ? scriptSystem->getTypedComponentByName(scriptName) : nullptr;
		if (!script)
		{
			return MikanAPIResult::MalformedParameters;
		}

		targetScriptId= script->getComponentId();
	}

	std::vector<MikanScriptID> targets;
	scriptContext->findBehaviorsWithTrigger(triggerName, targets);
	const bool bHasTrigger= targetScriptId == INVALID_MIKAN_ID
								? !targets.empty()
								: std::find(targets.begin(), targets.end(), targetScriptId) != targets.end();
	if (!bHasTrigger)
	{
		return MikanAPIResult::MalformedParameters;
	}

	if (!scriptContext->invokeScriptTrigger(triggerName, args, targetScriptId))
	{
		return MikanAPIResult::RequestFailed;
	}

	return MikanAPIResult::Success;
}

MikanAPIResult ScriptRequestHandler::invokeScriptHttpTriggerInternal(const ScriptHttpRoute& route,
																	 const std::map<std::string, std::string>& args)
{
	CommonScriptContextPtr scriptContext= m_scriptContext.lock();
	if (!scriptContext)
	{
		return MikanAPIResult::RequestFailed;
	}

	if (!scriptContext->behaviorHasMethod(route.scriptId,
										  CommonScriptContext::k_httpTriggerMethodPrefix + route.functionName))
	{
		return MikanAPIResult::MalformedParameters;
	}

	if (!scriptContext->invokeScriptHttpTrigger(route.scriptId, route.functionName, args))
	{
		return MikanAPIResult::RequestFailed;
	}

	return MikanAPIResult::Success;
}

void ScriptRequestHandler::invokeScriptMessageHandler(const ClientRequest& request, ClientResponse& response)
{
	SendScriptMessage scriptMessageRequest;
	if (!readTypedRequest(request.utf8RequestString, scriptMessageRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	// Offer the message to the project's script handlers; an unhandled message is not an error
	if (CommonScriptContextPtr scriptContext= m_scriptContext.lock())
	{
		scriptContext->invokeScriptMessageHandler(scriptMessageRequest.message.content.getUtf8Value());
	}

	writeSimpleJsonResponse(request.requestId, MikanAPIResult::Success, response);
}

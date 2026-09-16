#pragma once

#include "IServerRequestHandler.h"
#include "MikanAPITypes.h"
#include "ScriptHttpRouteTable.h"
#include "ScriptingFwd.h"

#include <map>
#include <string>
#include <vector>

class ScriptRequestHandler : public IServerRequestHandler
{
public:
	ScriptRequestHandler(class MikanServer* owner)
		: IServerRequestHandler(owner)
	{
	}

	virtual bool startup(class MainWindow* mainWindow) override;
	virtual void shutdown() override;

	// The project's one script context. Binding subscribes to its messages and
	// installs the HTTP routes of the current table; unbinding removes them.
	void bindScriptContext(CommonScriptContextPtr scriptContext);
	void unbindScriptContext(CommonScriptContextPtr scriptContext);
	CommonScriptContextPtr getScriptContext() const { return m_scriptContext.lock(); }

	// Replace the installed routes with this table. Each route is registered
	// under "/trigger/<route>" on the HTTP interprocess message server while a
	// context is bound; a route whose script or method is missing answers 400.
	void setHttpRoutes(const ScriptHttpRouteTable& routes);

protected:
	void installHttpRoutes();
	void removeHttpRoutes();
	bool registerHttpTriggerRoute(const ScriptHttpRoute& route);
	void unregisterHttpTriggerRoute(const ScriptHttpRoute& route);

	void publishScriptMessageEvent(const std::string& message);
	void invokeScriptTriggerHandler(const ClientRequest& request, ClientResponse& response);
	void invokeScriptMessageHandler(const ClientRequest& request, ClientResponse& response);

	// An empty script name fires every script that has the trigger
	MikanAPIResult invokeScriptTriggerInternal(const std::string& scriptName, const std::string& triggerName,
											   const std::map<std::string, std::string>& args);
	MikanAPIResult invokeScriptHttpTriggerInternal(const ScriptHttpRoute& route,
												   const std::map<std::string, std::string>& args);

private:
	CommonScriptContextWeakPtr m_scriptContext;
	ScriptHttpRouteTable m_httpRoutes;
	bool m_bRoutesInstalled= false;
};

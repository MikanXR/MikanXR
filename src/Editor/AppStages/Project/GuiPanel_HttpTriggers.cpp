#include "GuiPanel_HttpTriggers.h"
#include "App.h"
#include "AppSettingsConfig.h"
#include "HttpInterprocessMessageServer.h"
#include "LocText.h"
#include "MikanServer.h"
#include "Project/ProjectGuiPanelContext.h"

#include "imgui.h"

bool GuiPanel_HttpTriggers::init(ProjectGuiPanelContext* context)
{
	m_context= context;

	return true;
}

void GuiPanel_HttpTriggers::onGui()
{
	// Server port (e.g. for Stream Deck style integrations). Changing it
	// restarts the server, which re-registers every route. The edit commits on
	// Enter rather than per keystroke, so typing 8100 does not restart the
	// server on ports 8, 81 and 810 on the way there.
	{
		AppSettingsConfigPtr appSettings= App::getInstance()->getAppSettings();
		int httpPort= appSettings->getHttpServerPort();
		if (ImGui::InputInt(locLabel("httpTriggers.serverPort"), &httpPort, 1, 100,
							ImGuiInputTextFlags_EnterReturnsTrue))
		{
			if (httpPort < 1)
				httpPort= 1;
			if (httpPort > 65535)
				httpPort= 65535;

			addDeferredGuiEvent(
				[appSettings, httpPort]()
				{
					appSettings->setHttpServerPort(httpPort);
					MikanServer::getInstance()->restartHttpMessageServer();
				});
		}

		// Loopback only until asked, since the trigger routes and the upload endpoint
		// are then open to the network. The phone's capture upload needs this on.
		bool bAllowRemote= appSettings->getHttpServerAllowRemote();
		if (ImGui::Checkbox(locLabel("httpTriggers.allowRemote"), &bAllowRemote))
		{
			addDeferredGuiEvent(
				[appSettings, bAllowRemote]()
				{
					appSettings->setHttpServerAllowRemote(bAllowRemote);
					MikanServer::getInstance()->restartHttpMessageServer();
				});
		}
	}

	ImGui::Separator();

	// One button per route currently registered on the server
	// (see ScriptRequestHandler::registerHttpTriggerRoute)
	HttpInterprocessMessageServer* httpServer= MikanServer::getInstance()->getHttpMessageServer();
	if (httpServer == nullptr)
	{
		ImGui::TextUnformatted(locText("httpTriggers.noRoutesRegistered"));
		return;
	}

	const std::vector<std::string> routePaths= httpServer->getRegisteredRoutePaths();
	if (routePaths.empty())
	{
		ImGui::TextUnformatted(locText("httpTriggers.noRoutesRegistered"));
		return;
	}

	for (const std::string& path : routePaths)
	{
		if (ImGui::Button(path.c_str()))
		{
			// Deferred so the route handler does not mutate project state
			// while the panel is still drawing from it
			addDeferredGuiEvent(
				[httpServer, path]()
				{
					// No query args: the panel button fires the trigger with an empty arg table
					HttpRouteRequest request;
					request.path= path;

					HttpRouteResponse response;
					httpServer->invokeRouteHandler(request, response);
				});
		}
	}
}

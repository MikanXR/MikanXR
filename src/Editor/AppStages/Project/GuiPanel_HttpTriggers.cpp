#include "GuiPanel_HttpTriggers.h"
#include "App.h"
#include "AppSettingsConfig.h"
#include "AppStage.h"
#include "HttpInterprocessMessageServer.h"
#include "IconsForkAwesome.h"
#include "IEditorWindow.h"
#include "LocText.h"
#include "MikanServer.h"
#include "Project/ProjectGuiPanelContext.h"
#include "ProjectManager.h"
#include "ScriptComponent.h"
#include "ScriptHttpRouteTable.h"
#include "ScriptObjectSystem.h"
#include "TransactionHistory.h"

#include "imgui.h"

#include <float.h>
#include <string.h>

namespace
{
constexpr size_t k_routeTextBufferSize= 256;

ScriptObjectSystemPtr getScriptSystem(AppStage* appStage)
{
	ProjectManagerPtr projectManager= appStage->getProjectManager();
	return projectManager ? projectManager->getSystemOfType<ScriptObjectSystem>() : nullptr;
}
} // namespace

bool GuiPanel_HttpTriggers::init(ProjectGuiPanelContext* context)
{
	m_context= context;

	return true;
}

void GuiPanel_HttpTriggers::onGui()
{
	drawServerSettings();
	ImGui::Separator();
	drawRouteTable();
}

void GuiPanel_HttpTriggers::drawServerSettings()
{
	// Server port (e.g. for Stream Deck style integrations). Changing it
	// restarts the server, which re-registers every route. The edit commits on
	// Enter rather than per keystroke, so typing 8100 does not restart the
	// server on ports 8, 81 and 810 on the way there.
	AppSettingsConfigPtr appSettings= App::getInstance()->getAppSettings();
	int httpPort= appSettings->getHttpServerPort();
	if (ImGui::InputInt(locLabel("httpTriggers.serverPort"), &httpPort, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue))
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

void GuiPanel_HttpTriggers::drawRouteTable()
{
	ScriptObjectSystemPtr scriptSystem= getScriptSystem(getOwnerAppStage());
	if (!scriptSystem)
		return;

	TransactionHistory* transactionHistory= getOwnerAppStage()->getOwnerWindow()->getTransactionHistory();
	HttpInterprocessMessageServer* httpServer= MikanServer::getInstance()->getHttpMessageServer();

	ImGui::TextUnformatted(locText("httpTriggers.routes"));
	ImGui::SameLine();
	if (ImGui::SmallButton(locLabel("httpTriggers.addRoute")))
	{
		// A fresh row gets a unique placeholder route so it can be added
		// before the user names it
		const ScriptHttpRouteTable& routes= scriptSystem->getTypedDefinitionConst()->getHttpRoutes();
		ScriptHttpRoute newRoute;
		int suffix= routes.getRouteCount() + 1;
		do
		{
			newRoute.route= "route_" + std::to_string(suffix++);
		} while (routes.findRouteIndex(newRoute.route) >= 0);

		addDeferredGuiEvent(
			[scriptSystem, newRoute, transactionHistory]()
			{
				if (transactionHistory != nullptr)
					transactionHistory->beginGesture("http_route:" + newRoute.route);
				scriptSystem->addHttpRoute(newRoute);
				if (transactionHistory != nullptr)
					transactionHistory->endGesture();
			});
	}

	// A copy, since a deferred edit replaces the table under the loop
	const ScriptHttpRouteTable routes= scriptSystem->getTypedDefinitionConst()->getHttpRoutes();
	if (routes.empty())
	{
		ImGui::TextWrapped("%s", locText("httpTriggers.noRoutes"));
		return;
	}

	const ImGuiTableFlags tableFlags=
		ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
	if (!ImGui::BeginTable("http_routes", 4, tableFlags))
		return;

	ImGui::TableSetupColumn(locText("httpTriggers.route"), ImGuiTableColumnFlags_WidthStretch, 3.f);
	ImGui::TableSetupColumn(locText("httpTriggers.script"), ImGuiTableColumnFlags_WidthStretch, 2.f);
	ImGui::TableSetupColumn(locText("httpTriggers.function"), ImGuiTableColumnFlags_WidthStretch, 2.f);
	ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed);
	ImGui::TableHeadersRow();

	for (size_t rowIndex= 0; rowIndex < routes.getRouteCount(); ++rowIndex)
	{
		ScriptHttpRoute route= routes.getRoutes()[rowIndex];
		const bool bResolved= scriptSystem->isHttpRouteResolved(route);

		ImGui::TableNextRow();
		ImGui::PushID(static_cast<int>(rowIndex));

		if (drawRouteCells(rowIndex, route))
		{
			addDeferredGuiEvent(
				[scriptSystem, rowIndex, route, transactionHistory]()
				{
					if (transactionHistory != nullptr)
						transactionHistory->beginGesture("http_route:" + route.route);
					scriptSystem->setHttpRoute(rowIndex, route);
					if (transactionHistory != nullptr)
						transactionHistory->endGesture();
				});
		}

		ImGui::TableSetColumnIndex(3);
		ImGui::BeginDisabled(!bResolved || httpServer == nullptr);
		if (ImGui::SmallButton(locLabel("httpTriggers.fire")))
		{
			// Deferred so the route handler does not mutate project state
			// while the panel is still drawing from it. No query args: the
			// panel fires the trigger with an empty arg table.
			const std::string routePath= route.getRoutePath();
			addDeferredGuiEvent(
				[httpServer, routePath]()
				{
					HttpRouteRequest request;
					request.path= routePath;

					HttpRouteResponse response;
					httpServer->invokeRouteHandler(request, response);
				});
		}
		ImGui::EndDisabled();
		if (!bResolved && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("%s", locText("httpTriggers.routeUnresolved"));
		}

		ImGui::SameLine();
		if (ImGui::SmallButton(ICON_FK_TRASH_O))
		{
			const std::string routeName= route.route;
			addDeferredGuiEvent(
				[scriptSystem, rowIndex, routeName, transactionHistory]()
				{
					if (transactionHistory != nullptr)
						transactionHistory->beginGesture("http_route:" + routeName);
					scriptSystem->removeHttpRoute(rowIndex);
					if (transactionHistory != nullptr)
						transactionHistory->endGesture();
				});
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", locText("httpTriggers.removeRoute"));
		}

		ImGui::PopID();
	}

	ImGui::EndTable();
}

bool GuiPanel_HttpTriggers::drawRouteCells(size_t rowIndex, ScriptHttpRoute& inoutRoute)
{
	bool bChanged= false;

	// The route text commits on Enter or focus loss, so a half-typed name
	// never lands in the table and collides with another row
	ImGui::TableSetColumnIndex(0);
	char routeText[k_routeTextBufferSize];
	const bool bEditingThisRow= m_editingRow == static_cast<int>(rowIndex);
	const std::string& shownText= bEditingThisRow ? m_editingRouteText : inoutRoute.route;
	strncpy(routeText, shownText.c_str(), k_routeTextBufferSize - 1);
	routeText[k_routeTextBufferSize - 1]= '\0';

	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputText("##route", routeText, k_routeTextBufferSize);
	if (ImGui::IsItemActive())
	{
		m_editingRow= static_cast<int>(rowIndex);
		m_editingRouteText= routeText;
	}
	else if (bEditingThisRow)
	{
		if (ScriptHttpRouteTable::isValidRoute(m_editingRouteText) && m_editingRouteText != inoutRoute.route)
		{
			inoutRoute.route= m_editingRouteText;
			bChanged= true;
		}
		m_editingRow= -1;
		m_editingRouteText.clear();
	}

	ImGui::TableSetColumnIndex(1);
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (drawScriptCombo("##script", inoutRoute.scriptId))
	{
		// A new script has its own function list, so the old pick is dropped
		inoutRoute.functionName.clear();
		bChanged= true;
	}

	ImGui::TableSetColumnIndex(2);
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (drawFunctionCombo("##function", inoutRoute.scriptId, inoutRoute.functionName))
	{
		bChanged= true;
	}

	return bChanged;
}

bool GuiPanel_HttpTriggers::drawScriptCombo(const std::string& comboId, MikanScriptID& inoutScriptId)
{
	ScriptObjectSystemPtr scriptSystem= getScriptSystem(getOwnerAppStage());
	if (!scriptSystem)
		return false;

	ScriptComponentPtr current= scriptSystem->getTypedComponentById(inoutScriptId);
	const std::string preview= current ? current->getName() : locText("httpTriggers.noScript");

	bool bChanged= false;
	if (ImGui::BeginCombo(comboId.c_str(), preview.c_str()))
	{
		if (ImGui::Selectable(locText("httpTriggers.noScript"), !current))
		{
			if (inoutScriptId != INVALID_MIKAN_ID)
			{
				inoutScriptId= INVALID_MIKAN_ID;
				bChanged= true;
			}
		}

		// Pool order, the order the scripts load in
		for (ScriptDefinitionPtr definition : scriptSystem->getTypedDefinitionConst()->getAllDefinitions())
		{
			ScriptComponentPtr script= scriptSystem->getTypedComponentById(definition->getScriptId());
			if (!script)
				continue;

			const bool bSelected= script->getComponentId() == inoutScriptId;
			ImGui::PushID(static_cast<int>(script->getComponentId()));
			if (ImGui::Selectable(script->getName().c_str(), bSelected) && !bSelected)
			{
				inoutScriptId= script->getComponentId();
				bChanged= true;
			}
			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	return bChanged;
}

bool GuiPanel_HttpTriggers::drawFunctionCombo(const std::string& comboId, MikanScriptID scriptId,
											  std::string& inoutFunctionName)
{
	ScriptObjectSystemPtr scriptSystem= getScriptSystem(getOwnerAppStage());
	ScriptComponentPtr script= scriptSystem ? scriptSystem->getTypedComponentById(scriptId) : nullptr;

	std::vector<std::string> functionNames;
	if (script)
	{
		script->getHttpTriggerNames(functionNames);
	}

	// A stored name the script no longer has stays visible so the user sees
	// what the route was pointing at
	const std::string preview= inoutFunctionName.empty() ? locText("httpTriggers.noFunction") : inoutFunctionName;

	bool bChanged= false;
	ImGui::BeginDisabled(!script);
	if (ImGui::BeginCombo(comboId.c_str(), preview.c_str()))
	{
		if (ImGui::Selectable(locText("httpTriggers.noFunction"), inoutFunctionName.empty()))
		{
			if (!inoutFunctionName.empty())
			{
				inoutFunctionName.clear();
				bChanged= true;
			}
		}

		for (const std::string& functionName : functionNames)
		{
			const bool bSelected= functionName == inoutFunctionName;
			if (ImGui::Selectable(functionName.c_str(), bSelected) && !bSelected)
			{
				inoutFunctionName= functionName;
				bChanged= true;
			}
		}

		ImGui::EndCombo();
	}
	ImGui::EndDisabled();

	return bChanged;
}

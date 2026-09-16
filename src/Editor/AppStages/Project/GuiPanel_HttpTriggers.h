#pragma once

#include "MikanTypeFwd.h"
#include "Shared/GuiPanel.h"

#include <string>
#include <vector>

struct ScriptHttpRoute;

// The HTTP trigger server: its port and reach, and the project's route table.
// Each route binds /trigger/<route> to one HttpTrigger_ function of one
// script component, and can be fired from here without an external client.
class GuiPanel_HttpTriggers : public GuiPanel
{
public:
	GuiPanel_HttpTriggers(AppStage* ownerAppStage)
		: GuiPanel(ownerAppStage)
	{
	}

	bool init(class ProjectGuiPanelContext* context);
	virtual void onGui() override;

private:
	void drawServerSettings();
	void drawRouteTable();
	// The route, script, and function cells of one row. True when an edit
	// was committed into outRoute.
	bool drawRouteCells(size_t rowIndex, ScriptHttpRoute& inoutRoute);
	bool drawScriptCombo(const std::string& comboId, MikanScriptID& inoutScriptId);
	bool drawFunctionCombo(const std::string& comboId, MikanScriptID scriptId, std::string& inoutFunctionName);

	class ProjectGuiPanelContext* m_context= nullptr;
	// The row whose route text is being edited, and its uncommitted text
	int m_editingRow= -1;
	std::string m_editingRouteText;
};

#pragma once

#include "ProjectPipelineModel.h"
#include "Shared/GuiPanel.h"

#include <string>

// The chain feeding the outputs from the current scene, as a strip of boxes:
// the six stages on the top row, the selected stage's sub-stages on the
// second, and the members of a selected kind box on the third. Each box shows
// a state dot and one value. The rows keep a fixed height, so the panels
// docked below never move when the selection changes. A click selects the
// box's component in the outliner, a selection made anywhere else moves the
// highlight to the box that stands for it, and a double-click opens the box's
// tool when it has one.
class GuiPanel_Pipeline : public GuiPanel
{
public:
	GuiPanel_Pipeline(AppStage* ownerAppStage)
		: GuiPanel(ownerAppStage)
	{
	}

	bool init(class GuiPanel_ProjectOutliner* outliner);
	virtual void onGui() override;
	virtual void dispose() override;

	// Selects a box by its pipeline id as a click would. An empty id clears
	// the selection. False when the current chain has no such box.
	bool selectNodeById(const std::string& nodeId);
	const std::string& getSelectedNodeId() const { return m_selection.selectedId; }
	// Selects a box and opens its tool as a double-click would. False when the
	// current chain has no such box or the box has no tool.
	bool invokeNodeTool(const std::string& nodeId);

private:
	// Highlights the node and selects its component in the outliner
	void selectNode(const PipelineSnapshot& snapshot, int nodeIndex);
	// Runs the node's tool function on its tool component, outside the frame
	bool invokeTool(const PipelineNode& node);
	void applyPendingFollow(const PipelineSnapshot& snapshot);
	void onSelectedComponentChanged(int componentId);

	class GuiPanel_ProjectOutliner* m_outliner= nullptr;
	PipelineSelection m_selection;
	// The outliner raises its selection again on every tree rebuild, so only a
	// component id that differs from the last one seen is followed
	int m_lastSeenComponentId= -1;
	bool m_bHasPendingFollow= false;
	int m_pendingFollowComponentId= -1;
};

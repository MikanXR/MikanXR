#include "GuiPanel_Pipeline.h"
#include "AppStage.h"
#include "LocText.h"
#include "MikanComponent.h"
#include "MkGuiDrawUtils.h"
#include "Project/GuiPanel_ProjectOutliner.h"
#include "ProjectManager.h"
#include "ProjectPipelineGather.h"

#include "imgui.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
// Authored pixel sizes, scaled where they are used
constexpr float k_topHeight= 46.f;
constexpr float k_lineHeight= 28.f;
constexpr float k_rowGap= 6.f;
constexpr float k_boxGap= 10.f;
constexpr float k_arrowLength= 26.f;
constexpr float k_padX= 9.f;
constexpr float k_minTopWidth= 118.f;
constexpr float k_dotRadius= 4.5f;
constexpr float k_dotSlot= 16.f;
constexpr float k_textGap= 8.f;
constexpr float k_rounding= 4.f;
// The value's slot is a fixed width per box style, in font heights, so a
// value that changes never resizes its box. A longer value is cut short with
// an ellipsis and reads in full in the tooltip.
constexpr float k_topValueEms= 8.5f;
constexpr float k_lineValueEms= 6.f;
const ImU32 k_accentColor= IM_COL32(128, 166, 230, 255);
const ImU32 k_arrowColor= IM_COL32(93, 107, 136, 255);
const ImU32 k_runningColor= IM_COL32(96, 200, 120, 255);
const ImU32 k_degradedColor= IM_COL32(232, 178, 72, 255);
const ImU32 k_notConfiguredColor= IM_COL32(226, 96, 96, 255);

enum class eBoxStyle
{
	// The name over the dot and the value
	top,
	// One line: dot, name, value
	line,
};

struct BoxText
{
	std::string name;
	std::string value;
	PipelineStatus status;
};

std::string getNodeLabel(const PipelineNode& node)
{
	const char* nameKey= ProjectPipelineModel::getBoxNameKey(node.box);
	return nameKey[0] != '\0' ? std::string(locText(nameKey)) : node.name;
}

BoxText makeBoxText(const PipelineSnapshot& snapshot, int nodeIndex)
{
	BoxText text;
	text.name= getNodeLabel(snapshot.nodes[nodeIndex]);
	text.status= ProjectPipelineModel::computeStatus(snapshot, nodeIndex);
	text.value= formatPipelineText(text.status.value);
	return text;
}

// "<state>: <reason>", naming the node below that the state rolled up from
std::string makeReasonLine(const PipelineSnapshot& snapshot, int nodeIndex, const PipelineStatus& status)
{
	std::string line= locText(ProjectPipelineModel::getStateKey(status.state));
	const std::string reason= formatPipelineText(status.reason);
	if (reason.empty())
		return line;

	line+= ": ";
	if (status.sourceNodeIndex >= 0 && status.sourceNodeIndex != nodeIndex)
		line+= getNodeLabel(snapshot.nodes[status.sourceNodeIndex]) + ", ";
	return line + reason;
}

ImU32 getStateColor(ePipelineState state, float alpha)
{
	ImU32 color= ImGui::GetColorU32(ImGuiCol_TextDisabled, alpha);
	switch (state)
	{
	case ePipelineState::running:
		color= k_runningColor;
		break;
	case ePipelineState::degraded:
		color= k_degradedColor;
		break;
	case ePipelineState::notConfigured:
		color= k_notConfiguredColor;
		break;
	case ePipelineState::configured:
		color= ImGui::GetColorU32(ImGuiCol_Text, alpha);
		break;
	default:
		return color;
	}

	ImVec4 faded= ImGui::ColorConvertU32ToFloat4(color);
	faded.w*= alpha;
	return ImGui::ColorConvertFloat4ToU32(faded);
}

// A filled dot for a state backed by a live signal, a ring for one that is not
void drawStateDot(ImDrawList* drawList, const ImVec2& center, float radius, ePipelineState state, float alpha)
{
	const ImU32 color= getStateColor(state, alpha);
	if (state == ePipelineState::inactive || state == ePipelineState::configured)
		drawList->AddCircle(center, radius - 0.5f, color, 0, 1.5f);
	else
		drawList->AddCircleFilled(center, radius, color);
}

float getValueSlotWidth(eBoxStyle style)
{
	return (style == eBoxStyle::top ? k_topValueEms : k_lineValueEms) * ImGui::GetFontSize();
}

// The box's width from its name and its style alone, never from its value
float measureBox(const BoxText& text, eBoxStyle style)
{
	const float uiScale= MkGui::getUiScale();
	const float nameWidth= ImGui::CalcTextSize(text.name.c_str()).x;
	const float valueSlot= getValueSlotWidth(style);
	const float padding= 2.f * k_padX * uiScale;
	if (style == eBoxStyle::top)
		return std::max(k_minTopWidth * uiScale, std::max(nameWidth, k_dotSlot * uiScale + valueSlot) + padding);
	return k_dotSlot * uiScale + nameWidth + k_textGap * uiScale + valueSlot + padding;
}

// The text cut to a width, ending in an ellipsis when it does not fit
std::string fitText(const std::string& text, float maxWidth)
{
	if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth)
		return text;

	const char* ellipsis= "...";
	std::string cut= text;
	while (!cut.empty())
	{
		// Step back one whole UTF-8 character
		size_t last= cut.size() - 1;
		while (last > 0 && ((unsigned char)cut[last] & 0xC0) == 0x80)
			--last;
		cut.erase(last);
		if (ImGui::CalcTextSize((cut + ellipsis).c_str()).x <= maxWidth)
			break;
	}
	return cut + ellipsis;
}

void drawArrow(ImDrawList* drawList, const ImVec2& from, const ImVec2& to)
{
	const float uiScale= MkGui::getUiScale();
	const float headLength= 7.f * uiScale;
	const float headHalfWidth= 4.5f * uiScale;
	drawList->AddLine(from, ImVec2(to.x - headLength + 2.f, to.y), k_arrowColor, 2.f * uiScale);
	drawList->AddTriangleFilled(ImVec2(to.x, to.y), ImVec2(to.x - headLength, to.y - headHalfWidth),
								ImVec2(to.x - headLength, to.y + headHalfWidth), k_arrowColor);
}

// One box at a screen position. Returns true when it was clicked.
bool drawBox(const PipelineSnapshot& snapshot, int nodeIndex, const BoxText& text, eBoxStyle style,
			 const ImVec2& position, const ImVec2& size, bool bSelected, bool bOpen)
{
	const bool bHasTool= !snapshot.nodes[nodeIndex].toolFunctionId.empty();
	const float uiScale= MkGui::getUiScale();
	ImDrawList* drawList= ImGui::GetWindowDrawList();
	const ImVec2 boxMax(position.x + size.x, position.y + size.y);

	ImGui::SetCursorScreenPos(position);
	ImGui::PushID(snapshot.nodes[nodeIndex].id.c_str());
	const bool bClicked= ImGui::InvisibleButton("##box", size);
	const bool bHovered= ImGui::IsItemHovered();
	ImGui::PopID();

	// A box that is not part of the chain draws faded
	const float alpha= text.status.state == ePipelineState::inactive ? 0.5f : 1.f;
	const float rounding= k_rounding * uiScale;
	ImU32 fill= ImGui::GetColorU32(bHovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg, alpha);
	if (bSelected)
		fill= ImGui::GetColorU32(ImGuiCol_Header);
	drawList->AddRectFilled(position, boxMax, fill, rounding);
	if (bSelected)
		drawList->AddRect(position, boxMax, k_accentColor, rounding, 0, 2.f * uiScale);
	else if (bOpen)
		drawList->AddRect(position, boxMax, k_accentColor, rounding, 0, 1.f * uiScale);
	else
		drawList->AddRect(position, boxMax, ImGui::GetColorU32(ImGuiCol_Border, alpha), rounding);

	const ImU32 nameColor= ImGui::GetColorU32(ImGuiCol_Text, alpha);
	const ImU32 valueColor= ImGui::GetColorU32(ImGuiCol_TextDisabled, alpha);
	const float textHeight= ImGui::GetTextLineHeight();
	const float padX= k_padX * uiScale;
	const float dotRadius= k_dotRadius * uiScale;
	const float dotSlot= k_dotSlot * uiScale;
	if (style == eBoxStyle::top)
	{
		const float lineGap= 2.f * uiScale;
		const float nameY= position.y + (size.y - 2.f * textHeight - lineGap) * 0.5f;
		const float valueY= nameY + textHeight + lineGap;
		drawList->AddText(ImVec2(position.x + padX, nameY), nameColor, text.name.c_str());
		drawStateDot(drawList, ImVec2(position.x + padX + dotRadius, valueY + textHeight * 0.5f), dotRadius,
					 text.status.state, alpha);
		drawList->AddText(ImVec2(position.x + padX + dotSlot, valueY), valueColor,
						  fitText(text.value, getValueSlotWidth(style)).c_str());
	}
	else
	{
		const float textY= position.y + (size.y - textHeight) * 0.5f;
		float x= position.x + padX;
		drawStateDot(drawList, ImVec2(x + dotRadius, position.y + size.y * 0.5f), dotRadius, text.status.state, alpha);
		x+= dotSlot;
		drawList->AddText(ImVec2(x, textY), nameColor, text.name.c_str());
		x+= ImGui::CalcTextSize(text.name.c_str()).x + k_textGap * uiScale;
		if (!text.value.empty())
			drawList->AddText(ImVec2(x, textY), valueColor, fitText(text.value, boxMax.x - padX - x).c_str());
	}

	if (bHovered)
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.f);
		ImGui::TextUnformatted(text.name.c_str());
		if (!text.value.empty())
			ImGui::TextDisabled("%s", text.value.c_str());
		ImGui::PushStyleColor(ImGuiCol_Text, getStateColor(text.status.state, 1.f));
		ImGui::TextUnformatted(makeReasonLine(snapshot, nodeIndex, text.status).c_str());
		ImGui::PopStyleColor();
		if (bHasTool)
			ImGui::TextDisabled("%s", locText("pipeline.toolHint"));
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
	return bClicked;
}
} // namespace

bool GuiPanel_Pipeline::init(GuiPanel_ProjectOutliner* outliner)
{
	m_outliner= outliner;
	if (m_outliner != nullptr)
	{
		m_lastSeenComponentId= m_outliner->getSelectedComponentId();
		m_outliner->OnSelectedComponentChanged+= MakeDelegate(this, &GuiPanel_Pipeline::onSelectedComponentChanged);
	}

	return true;
}

void GuiPanel_Pipeline::dispose()
{
	if (m_outliner != nullptr)
	{
		m_outliner->OnSelectedComponentChanged-= MakeDelegate(this, &GuiPanel_Pipeline::onSelectedComponentChanged);
		m_outliner= nullptr;
	}

	GuiPanel::dispose();
}

void GuiPanel_Pipeline::onSelectedComponentChanged(int componentId)
{
	if (componentId == m_lastSeenComponentId)
		return;

	// Applied against the next frame's snapshot, since the selection can move
	// in the middle of an edit that changes the chain
	m_lastSeenComponentId= componentId;
	m_pendingFollowComponentId= componentId;
	m_bHasPendingFollow= true;
}

void GuiPanel_Pipeline::applyPendingFollow(const PipelineSnapshot& snapshot)
{
	if (!m_bHasPendingFollow)
		return;
	m_bHasPendingFollow= false;

	// The outliner echoing the strip's own click: the box already highlighted
	// stands for that component, and following would move it to the first box
	// that does (a click on Lens Calibration would jump to Video Source)
	const int selectedIndex= ProjectPipelineModel::findNodeById(snapshot, m_selection.selectedId);
	if (selectedIndex >= 0 && snapshot.nodes[selectedIndex].componentId == m_pendingFollowComponentId)
		return;

	ProjectPipelineModel::applyFollow(snapshot, m_pendingFollowComponentId, m_selection);
}

void GuiPanel_Pipeline::selectNode(const PipelineSnapshot& snapshot, int nodeIndex)
{
	ProjectPipelineModel::applyClick(snapshot, nodeIndex, m_selection);

	// A box that stands for no component (a kind box, Return Feed, SteamVR)
	// leaves the outliner where it is
	const int componentId= snapshot.nodes[nodeIndex].componentId;
	if (componentId >= 0 && m_outliner != nullptr)
	{
		GuiPanel_ProjectOutliner* outliner= m_outliner;
		addDeferredGuiEvent([outliner, componentId]() { outliner->selectComponent(componentId); });
	}
}

bool GuiPanel_Pipeline::invokeTool(const PipelineNode& node)
{
	if (node.toolFunctionId.empty() || node.toolComponentId < 0)
		return false;

	// Deferred like a property panel's function button, since a tool opens a
	// window or switches the app stage
	const std::string functionId= node.toolFunctionId;
	const int componentId= node.toolComponentId;
	addDeferredGuiEvent(
		[this, functionId, componentId]()
		{
			if (ProjectManagerPtr projectManager= m_ownerAppStage->getProjectManager())
			{
				if (MikanComponentPtr component= projectManager->getComponentById(componentId))
					component->invokeFunction(functionId);
			}
		});
	return true;
}

void GuiPanel_Pipeline::onGui()
{
	const PipelineSnapshot snapshot= gatherProjectPipeline(m_ownerAppStage->getProjectManager(),
														   m_ownerAppStage->getOwnerWindow()->getClientSourceManager());
	// A selection the chain no longer holds (a stencil deleted, the current
	// scene switched) is dropped before anything reads it
	ProjectPipelineModel::validateSelection(snapshot, m_selection);
	applyPendingFollow(snapshot);

	ImGui::BeginChild("##pipelineStrip", ImVec2(0.f, 0.f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

	const float uiScale= MkGui::getUiScale();
	const float topHeight= k_topHeight * uiScale;
	const float lineHeight= k_lineHeight * uiScale;
	const float rowGap= k_rowGap * uiScale;
	const float boxGap= k_boxGap * uiScale;
	const float arrowLength= k_arrowLength * uiScale;
	const float textHeight= ImGui::GetTextLineHeight();

	ImDrawList* drawList= ImGui::GetWindowDrawList();
	const ImVec2 origin= ImGui::GetCursorScreenPos();
	const int openStageIndex= ProjectPipelineModel::findNodeById(snapshot, m_selection.openStageId);
	const int openKindIndex= ProjectPipelineModel::findNodeById(snapshot, m_selection.openKindId);
	int clickedIndex= -1;
	bool bDoubleClicked= false;
	float contentRight= origin.x;

	auto box= [&](int nodeIndex, eBoxStyle style, const ImVec2& position, float height) -> float
	{
		const BoxText text= makeBoxText(snapshot, nodeIndex);
		const float width= measureBox(text, style);
		const bool bSelected= snapshot.nodes[nodeIndex].id == m_selection.selectedId;
		const bool bOpen= nodeIndex == openStageIndex || nodeIndex == openKindIndex;
		if (drawBox(snapshot, nodeIndex, text, style, position, ImVec2(width, height), bSelected, bOpen))
		{
			// A box reports the release of each click, so a double-click's
			// second release reads here with the click count it ended. A
			// keyboard press is never one.
			clickedIndex= nodeIndex;
			bDoubleClicked= ImGui::IsMouseReleased(ImGuiMouseButton_Left)
							&& ImGui::GetIO().MouseClickedLastCount[ImGuiMouseButton_Left] >= 2;
		}
		return width;
	};

	// A row of boxes that are a set rather than a flow, or a hint when the
	// row has nothing to show
	auto setRow= [&](const std::vector<int>& nodeIndices, float rowY, const char* emptyHintKey)
	{
		if (nodeIndices.empty())
		{
			if (emptyHintKey != nullptr)
			{
				drawList->AddText(ImVec2(origin.x, rowY + (lineHeight - textHeight) * 0.5f),
								  ImGui::GetColorU32(ImGuiCol_TextDisabled), locText(emptyHintKey));
			}
			return;
		}

		float x= origin.x;
		for (int nodeIndex : nodeIndices)
		{
			x+= box(nodeIndex, eBoxStyle::line, ImVec2(x, rowY), lineHeight) + boxGap;
		}
		contentRight= std::max(contentRight, x - boxGap);
	};

	// The top row: the six stages in order, joined by arrows
	{
		const float midY= origin.y + topHeight * 0.5f;
		float x= origin.x;
		bool bFirst= true;
		for (int nodeIndex : ProjectPipelineModel::getTopRow(snapshot))
		{
			if (!bFirst)
			{
				drawArrow(drawList, ImVec2(x, midY), ImVec2(x + arrowLength, midY));
				x+= arrowLength;
			}
			bFirst= false;
			x+= box(nodeIndex, eBoxStyle::top, ImVec2(x, origin.y), topHeight);
		}
		contentRight= std::max(contentRight, x);
	}

	// The second row: the open stage's sub-stages
	const float subRowY= origin.y + topHeight + rowGap;
	setRow(ProjectPipelineModel::getChildren(snapshot, openStageIndex), subRowY,
		   openStageIndex < 0 ? "pipeline.selectHint" : "pipeline.noMembers");

	// The third row: the open kind box's members
	const float memberRowY= subRowY + lineHeight + rowGap;
	setRow(ProjectPipelineModel::getChildren(snapshot, openKindIndex), memberRowY,
		   openKindIndex < 0 ? nullptr : "pipeline.noMembers");

	// The status line: what the selected box is doing and why
	const float statusY= memberRowY + lineHeight + rowGap;
	const int selectedIndex= ProjectPipelineModel::findNodeById(snapshot, m_selection.selectedId);
	if (selectedIndex >= 0)
	{
		const PipelineStatus status= ProjectPipelineModel::computeStatus(snapshot, selectedIndex);
		const std::string line=
			getNodeLabel(snapshot.nodes[selectedIndex]) + "  " + makeReasonLine(snapshot, selectedIndex, status);
		drawList->AddText(ImVec2(origin.x, statusY), getStateColor(status.state, 1.f), line.c_str());
		contentRight= std::max(contentRight, origin.x + ImGui::CalcTextSize(line.c_str()).x);
	}

	// Reserve what was drawn, so the child scrolls when the panel is small
	ImGui::SetCursorScreenPos(origin);
	ImGui::Dummy(ImVec2(contentRight - origin.x, statusY + textHeight - origin.y));

	ImGui::EndChild();

	if (clickedIndex >= 0)
	{
		selectNode(snapshot, clickedIndex);
		if (bDoubleClicked)
			invokeTool(snapshot.nodes[clickedIndex]);
	}
}

bool GuiPanel_Pipeline::selectNodeById(const std::string& nodeId)
{
	if (nodeId.empty())
	{
		m_selection= PipelineSelection();
		return true;
	}

	const PipelineSnapshot snapshot= gatherProjectPipeline(m_ownerAppStage->getProjectManager(),
														   m_ownerAppStage->getOwnerWindow()->getClientSourceManager());
	const int nodeIndex= ProjectPipelineModel::findNodeById(snapshot, nodeId);
	if (nodeIndex < 0)
		return false;

	selectNode(snapshot, nodeIndex);
	return true;
}

bool GuiPanel_Pipeline::invokeNodeTool(const std::string& nodeId)
{
	const PipelineSnapshot snapshot= gatherProjectPipeline(m_ownerAppStage->getProjectManager(),
														   m_ownerAppStage->getOwnerWindow()->getClientSourceManager());
	const int nodeIndex= ProjectPipelineModel::findNodeById(snapshot, nodeId);
	if (nodeIndex < 0 || snapshot.nodes[nodeIndex].toolFunctionId.empty())
		return false;

	selectNode(snapshot, nodeIndex);
	return invokeTool(snapshot.nodes[nodeIndex]);
}

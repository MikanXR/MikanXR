#include "MkCanvasScopedNode.h"
#include "MkCanvasWidgets.h"
#include "MkGuiDrawUtils.h"

#include "imgui.h"
#include "imgui_node_editor.h"

namespace ed= ax::NodeEditor;

static MkCanvasScopedNode* s_currentNode= nullptr;

MkCanvasScopedNode::MkCanvasScopedNode(int nodeId, const ImVec4& headerColor)
	: m_nodeId(MkCanvas::toCanvasId(nodeId))
	, m_headerColor(ImColor(headerColor))
{
	const float uiScale= MkGui::getUiScale();
	ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(8.f * uiScale, 4.f * uiScale, 8.f * uiScale, 8.f * uiScale));
	ed::BeginNode(m_nodeId);
	ImGui::PushID(m_nodeId);

	m_contentStartX= ImGui::GetCursorPosX();
	m_previousNode= s_currentNode;
	s_currentNode= this;
}

float MkCanvasScopedNode::getCurrentContentStartX()
{
	return s_currentNode != nullptr ? s_currentNode->m_contentStartX : 0.f;
}

MkCanvasScopedNode::~MkCanvasScopedNode()
{
	s_currentNode= m_previousNode;

	ed::EndNode();

	// The node rect is the last submitted item; paint the header band across
	// its full width now that the final size is known (blueprints-example
	// technique, minus the noise texture)
	if (m_bHasHeader && ImGui::IsItemVisible())
	{
		const ImVec2 nodeMin= ImGui::GetItemRectMin();
		const ImVec2 nodeMax= ImGui::GetItemRectMax();
		const float halfBorder= ed::GetStyle().NodeBorderWidth * 0.5f;
		const float rounding= ed::GetStyle().NodeRounding;

		ImDrawList* drawList= ed::GetNodeBackgroundDrawList(m_nodeId);
		const float uiScale= MkGui::getUiScale();
		const ImVec2 headerBandMin(nodeMin.x + halfBorder, nodeMin.y + halfBorder);
		const ImVec2 headerBandMax(nodeMax.x - halfBorder, m_headerMax.y + 2.f * uiScale);

		if (headerBandMax.y > headerBandMin.y)
		{
			drawList->AddRectFilled(headerBandMin, headerBandMax, m_headerColor, rounding, ImDrawFlags_RoundCornersTop);

			// Soft top-light gradient over the band, inset past the corner
			// rounding so the highlight stays within the rounded silhouette
			const int alpha= (int)(255 * ImGui::GetStyle().Alpha);
			const ImVec2 gradientMin(headerBandMin.x + rounding, headerBandMin.y);
			const ImVec2 gradientMax(headerBandMax.x - rounding, headerBandMax.y);
			if (gradientMax.x > gradientMin.x)
			{
				drawList->AddRectFilledMultiColor(gradientMin, gradientMax, IM_COL32(255, 255, 255, 45 * alpha / 255),
												  IM_COL32(255, 255, 255, 45 * alpha / 255), IM_COL32(255, 255, 255, 0),
												  IM_COL32(255, 255, 255, 0));
			}

			// Thin separator between the header band and the node body
			drawList->AddLine(ImVec2(headerBandMin.x, headerBandMax.y - 0.5f * uiScale),
							  ImVec2(headerBandMax.x, headerBandMax.y - 0.5f * uiScale),
							  IM_COL32(255, 255, 255, 32 * alpha / 255), 1.f * uiScale);
		}
	}

	ImGui::PopID();
	ed::PopStyleVar();
}

void MkCanvasScopedNode::beginHeader() { ImGui::BeginGroup(); }

void MkCanvasScopedNode::endHeader()
{
	ImGui::EndGroup();

	m_headerMin= ImGui::GetItemRectMin();
	m_headerMax= ImGui::GetItemRectMax();
	m_bHasHeader= true;

	// Breathing room between the header band and the first content row
	ImGui::Dummy(ImVec2(0.f, 4.f * MkGui::getUiScale()));
}

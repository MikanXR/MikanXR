#include "NodeLink.h"
#include "NodePin.h"
#include "NodeEditorState.h"
#include "Graphs/NodeGraph.h"
#include "Logger.h"

#include "imgui.h"
#include "imgui_node_editor.h"

// -- NodeConfig -----
configuru::Config NodeLinkConfig::writeToJSON()
{
	configuru::Config pt= CommonConfig::writeToJSON();

	pt["id"]= id;
	pt["start_pin_id"]= start_pin_id;
	pt["end_pin_id"]= end_pin_id;

	return pt;
}

void NodeLinkConfig::readFromJSON(const configuru::Config& pt)
{
	CommonConfig::readFromJSON(pt);

	id= pt.get_or<t_node_id>("id", -1);
	start_pin_id= pt.get_or<t_node_id>("start_pin_id", -1);
	end_pin_id= pt.get_or<t_node_id>("end_pin_id", -1);
}

//-- NodeLink -----
bool NodeLink::loadFromConfig(NodeLinkConfigConstPtr config)
{
	bool success= true;
	m_id= config->id;

	m_startPin= getOwnerGraph()->getPinById(config->start_pin_id);
	if (!m_startPin)
	{
		MIKAN_LOG_ERROR("NodeLink::loadFromConfig") << "Invalid start pin id: " << config->start_pin_id;
		success= false;
	}

	m_endPin= getOwnerGraph()->getPinById(config->end_pin_id);
	if (!m_endPin)
	{
		MIKAN_LOG_ERROR("NodeLink::loadFromConfig") << "Invalid end pin id: " << config->end_pin_id;
		success= false;
	}

	return success;
}

void NodeLink::saveToConfig(NodeLinkConfigPtr config) const
{
	config->id= m_id;
	config->start_pin_id= m_startPin ? m_startPin->getId() : -1;
	config->end_pin_id= m_endPin ? m_endPin->getId() : -1;
}

NodePinPtr NodeLink::getConnectedPin(NodePinPtr pin) const
{
	if (pin == m_startPin)
		return m_endPin;
	else if (pin == m_endPin)
		return m_startPin;
	else
		return NodePinPtr();
}

void NodeLink::editorRender(const NodeEditorState& editorState)
{
	// A link picked up by Ctrl+drag is drawn by the drag itself until it lands
	if (m_id == editorState.detachedLinkId)
		return;

	// Dim existing links while a new link is being dragged
	const float alpha= editorState.startedLinkPinId == -1 ? 1.f : 0.2f;

	ImVec4 linkColor= ImGui::ColorConvertU32ToFloat4(m_startPin->editorGetLinkStyleColor());
	linkColor.w*= alpha;

	ax::NodeEditor::Link(MkCanvas::toCanvasId(m_id), MkCanvas::toCanvasId(m_startPin->getId()),
						 MkCanvas::toCanvasId(m_endPin->getId()), linkColor, 2.f);
}
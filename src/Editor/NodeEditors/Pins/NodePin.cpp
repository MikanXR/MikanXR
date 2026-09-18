#include "NodePin.h"
#include "NodeEditorState.h"
#include "Nodes/Node.h"
#include "Graphs/NodeGraph.h"
#include "Pins/NodeLink.h"
#include "MkCanvasScopedPin.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiScopedStyleColor.h"
#include "StringUtils.h"
#include "Logger.h"

#include "imgui.h"

const float k_pin_alpha_default= 1.f;
const float k_pin_alpha_invalid= 0.2f;

// Authored against a 96 DPI display; reached through the scaled accessors below
static constexpr float k_editorPinIconSize= 16.f;
static constexpr float k_editorPinIconSpacing= 8.f;

float NodePin::getEditorPinIconSize() { return k_editorPinIconSize * MkGui::getUiScale(); }

float NodePin::getEditorPinIconSpacing() { return k_editorPinIconSpacing * MkGui::getUiScale(); }

// -- NodePinConfig -----
configuru::Config NodePinConfig::writeToJSON()
{
	configuru::Config pt= CommonConfig::writeToJSON();

	pt["class_name"]= className;
	pt["pin_name"]= pinName;
	pt["id"]= id;
	pt["direction"]= k_nodePinDirectionStrings[(int)direction];
	pt["owner_node_id"]= ownerNodeId;
	writeStdValueVector<t_node_link_id>(pt, "connected_links", connectedLinkIds);
	pt["has_default_value"]= hasDefaultValue;
	pt["is_dynamic"]= isDynamic;

	return pt;
}

void NodePinConfig::readFromJSON(const configuru::Config& pt)
{
	CommonConfig::readFromJSON(pt);

	className= pt.get_or<std::string>("class_name", "Node");
	pinName= pt.get_or<std::string>("pin_name", "");
	id= pt.get_or<t_node_id>("id", -1);
	const std::string directionString=
		pt.get_or<std::string>("direction", k_nodePinDirectionStrings[(int)eNodePinDirection::INPUT]);
	direction= StringUtils::FindEnumValue<eNodePinDirection>(directionString, k_nodePinDirectionStrings);
	ownerNodeId= pt.get_or<t_node_id>("owner_node_id", -1);
	readStdValueVector<t_node_link_id>(pt, "connected_links", connectedLinkIds);
	hasDefaultValue= pt.get_or<bool>("has_default_value", false);
	isDynamic= pt.get_or<bool>("is_dynamic", false);
}

// -- NodePin -----
NodePin::NodePin()
	: m_id(-1)
	, m_direction(eNodePinDirection::INVALID)
{
}

bool NodePin::loadFromConfig(NodeGraphPtr ownerGraph, NodePinConfigConstPtr config)
{
	bool bSuccess= true;

	m_id= config->id;
	m_direction= config->direction;
	m_name= config->pinName;
	m_bHasDefaultValue= config->hasDefaultValue;
	m_bIsDynamic= config->isDynamic;

	NodePtr ownerNode= ownerGraph->getNodeById(config->ownerNodeId);
	if (ownerNode)
	{
		setOwnerNode(ownerNode);
	}
	else
	{
		MIKAN_LOG_INFO("NodePin::loadFromConfig")
			<< "Missing owner node on pin id: " << m_id << ", owner node id: " << config->ownerNodeId;
		bSuccess= false;
	}

	for (t_node_link_id linkId : config->connectedLinkIds)
	{
		NodeLinkPtr link= ownerGraph->getLinkById(linkId);
		if (link)
		{
			m_connectedLinks.push_back(link);
		}
		else
		{
			MIKAN_LOG_INFO("NodePin::loadFromConfig")
				<< "Missing connected link id: " << linkId << ", on pin id: " << m_id;
			bSuccess= false;
		}
	}

	return bSuccess;
}

void NodePin::saveToConfig(NodePinConfigPtr config) const
{
	config->className= getClassName();
	config->id= m_id;
	config->direction= m_direction;
	config->pinName= m_name;
	config->ownerNodeId= m_ownerNode ? m_ownerNode->getId() : -1;
	config->hasDefaultValue= m_bHasDefaultValue;
	config->isDynamic= m_bIsDynamic;

	for (NodeLinkPtr linkPtr : m_connectedLinks)
	{
		config->connectedLinkIds.push_back(linkPtr->getId());
	}
}

float NodePin::editorComputeInputWidth() const
{
	// Default input width
	return 11.f * MkGui::getUiScale();
}

bool NodePin::canPinsBeConnected(NodePinPtr otherPinPtr) const
{
	if (!otherPinPtr)
		return false;

	// Are we trying to connect a pin back to itself?
	if (otherPinPtr.get() == this)
		return false;

	// Are pins not of the same type?
	if (this->getClassName() != otherPinPtr->getClassName())
		return false;

	// Pins sending/receiving the same size of data?
	if (this->getDataSize() != otherPinPtr->getDataSize())
		return false;

	// Is one pin an input and the other an output?
	if (this->getDirection() == otherPinPtr->getDirection())
		return false;

	if (!isOnSamePage(otherPinPtr))
		return false;

	return true;
}

bool NodePin::isOnSamePage(NodePinPtr otherPinPtr) const
{
	NodePtr otherNode= otherPinPtr ? otherPinPtr->getOwnerNode() : NodePtr();

	// Pins without an owner (factory default objects) sit on no page and pass
	return !m_ownerNode || !otherNode || m_ownerNode->getPageId() == otherNode->getPageId();
}

NodePinPtr NodePin::getConnectedSourcePin() const
{
	if (m_direction != eNodePinDirection::INPUT)
		return NodePinPtr();

	if (m_connectedLinks.size() == 0)
		return NodePinPtr();

	NodeLinkPtr link= m_connectedLinks[0];
	if (!link)
		return NodePinPtr();

	NodePinPtr startPin= link->getStartPin();
	NodePinPtr endPin= link->getEndPin();
	return this == startPin.get() ? endPin : startPin;
}

NodePinPtr NodePin::getConnectedTargetPin() const
{
	if (m_direction != eNodePinDirection::OUTPUT)
		return NodePinPtr();

	if (m_connectedLinks.size() == 0)
		return NodePinPtr();

	NodeLinkPtr link= m_connectedLinks[0];
	if (!link)
		return NodePinPtr();

	NodePinPtr startPin= link->getStartPin();
	NodePinPtr endPin= link->getEndPin();
	return this == startPin.get() ? endPin : startPin;
}

bool NodePin::connectLink(NodeLinkPtr linkPtr)
{
	// If this is an input pin, only allow one connection
	if (m_direction == eNodePinDirection::INPUT)
	{
		// Delete all existing links
		while (m_connectedLinks.size() > 0)
		{
			t_node_link_id existingLinkId= m_connectedLinks[0]->getId();

			if (!getOwnerNode()->getOwnerGraph()->deleteLinkById(existingLinkId))
			{
				MIKAN_LOG_ERROR("NodePin::connectLink")
					<< "Failed to delete pre-existing link id: " << existingLinkId << ", from node pin id: " << getId()
					<< ", of pin class: " << getClassName();
				break;
			}
		}
	}

	// Now we can add the new link
	m_connectedLinks.push_back(linkPtr);

	// Let the node know that a link was connected to this pin
	if (m_ownerNode)
	{
		m_ownerNode->onLinkConnected(linkPtr, shared_from_this());
	}

	return true;
}

bool NodePin::disconnectLink(NodeLinkPtr linkPtr)
{
	auto it= std::find(m_connectedLinks.begin(), m_connectedLinks.end(), linkPtr);
	if (it != m_connectedLinks.end())
	{
		// Now we can remove the new link
		m_connectedLinks.erase(it);

		// Let the node know that a link was disconnected from this pin
		if (m_ownerNode)
		{
			m_ownerNode->onLinkDisconnected(linkPtr, shared_from_this());
		}

		return true;
	}

	return false;
}

float NodePin::editorComputeNodeAlpha(const NodeEditorState& editorState) const
{
	if (editorState.startedLinkPinId == -1)
	{
		return k_pin_alpha_default;
	}
	else
	{
		NodePinPtr startPinPtr= m_ownerNode->getOwnerGraph()->getPinById(editorState.startedLinkPinId);

		if (editorState.startedLinkPinId == m_id || this->canPinsBeConnected(startPinPtr))
		{
			return k_pin_alpha_default;
		}
		else
		{
			return k_pin_alpha_invalid;
		}
	}
}

void NodePin::editorRenderInputPin(const NodeEditorState& editorState)
{
	const float alpha= editorComputeNodeAlpha(editorState);

	MkGuiScopedStyleColor styleColor;
	styleColor.push(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, alpha));

	ImVec4 pinColor= editorGetPinColor();
	pinColor.w*= alpha;

	{
		MkCanvasScopedPin pin(m_id, MkCanvasPinDirection::Input);
		const float iconSize= getEditorPinIconSize();
		MkCanvas::drawPinIcon(ImVec2(iconSize, iconSize), editorGetPinIcon(), hasAnyConnectedLinks(),
							  MkCanvasPinDirection::Input, pinColor);
		ImGui::SameLine();
		if (editorShowPinName())
		{
			ImGui::TextUnformatted(m_name.c_str());
		}
		editorRenderInputTextEntry(editorState);
	}
}

void NodePin::editorRenderOutputPin(const NodeEditorState& editorState, float prefixWidth)
{
	const float alpha= editorComputeNodeAlpha(editorState);

	MkGuiScopedStyleColor styleColor;
	styleColor.push(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, alpha));

	ImVec4 pinColor= editorGetPinColor();
	pinColor.w*= alpha;

	{
		MkCanvasScopedPin pin(m_id, MkCanvasPinDirection::Output);
		if (prefixWidth > 0.f)
		{
			ImGui::Dummy(ImVec2(prefixWidth, 1.0f));
			ImGui::SameLine();
		}
		if (editorShowPinName())
		{
			ImGui::TextUnformatted(m_name.c_str());
			ImGui::SameLine();
		}
		const float iconSize= getEditorPinIconSize();
		MkCanvas::drawPinIcon(ImVec2(iconSize, iconSize), editorGetPinIcon(), hasAnyConnectedLinks(),
							  MkCanvasPinDirection::Output, pinColor);
	}
}

MkCanvas::PinIcon NodePin::editorGetPinIcon() const { return MkCanvas::PinIcon::Circle; }

ImVec4 NodePin::editorGetPinColor() const { return ImVec4(252.f / 255.f, 200.f / 255.f, 35.f / 255.f, 1.f); }

ImU32 NodePin::editorGetLinkStyleColor() const { return IM_COL32(252, 200, 35, 255); }
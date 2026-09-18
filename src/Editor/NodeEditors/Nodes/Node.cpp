#include "Node.h"
#include "NodeEditorState.h"
#include "Graphs/NodeGraph.h"
#include "Graphs/NodeEvaluator.h"
#include "Pins/NodePin.h"
#include "Pins/FlowPin.h"
#include "MkCanvasScopedNode.h"
#include "MkGuiScopedGroup.h"
#include "MkGuiScopedStyleVar.h"
#include "Logger.h"
#include "StringUtils.h"

#include "imgui.h"

// -- NodeConfig -----
configuru::Config NodeConfig::writeToJSON()
{
	configuru::Config pt= CommonConfig::writeToJSON();

	pt["class_name"]= className;
	pt["id"]= id;
	writeStdValueVector<t_node_pin_id>(pt, "pins_in", pinIDsIn);
	writeStdValueVector<t_node_pin_id>(pt, "pins_out", pinIDsOut);
	writeStdArray<float, 2>(pt, "pos", pos);
	if (pageId != 0)
	{
		pt["page"]= pageId;
	}

	return pt;
}

void NodeConfig::readFromJSON(const configuru::Config& pt)
{
	CommonConfig::readFromJSON(pt);

	className= pt.get_or<std::string>("class_name", "Node");
	id= pt.get_or<t_node_id>("id", -1);
	readStdValueVector<t_node_pin_id>(pt, "pins_in", pinIDsIn);
	readStdValueVector<t_node_pin_id>(pt, "pins_out", pinIDsOut);
	readStdArray<float, 2>(pt, "pos", pos);
	pageId= pt.get_or<t_graph_page_id>("page", 0);
}

// -- Node -----
Node::Node()
	: m_id(-1)
	, m_nodePos(glm::vec2(0.f))
	, m_bIsPendingDeletion(false)
{
}

Node::~Node()= default;

bool Node::loadFromConfig(NodeConfigConstPtr nodeConfig)
{
	bool success= true;

	m_id= nodeConfig->id;
	m_nodePos= {nodeConfig->pos[0], nodeConfig->pos[1]};
	m_pageId= nodeConfig->pageId;

	for (t_node_pin_id pinId : nodeConfig->pinIDsIn)
	{
		NodePinPtr pin= m_ownerGraph->getPinById(pinId);
		if (pin)
		{
			m_pinsIn.push_back(pin);
		}
		else
		{
			MIKAN_LOG_WARNING("Node::loadFromConfig")
				<< "Failed to find create input pin: " << pinId << ", on node: " << getClassName();
			success= false;
		}
	}

	for (t_node_pin_id pinId : nodeConfig->pinIDsOut)
	{
		NodePinPtr pin= m_ownerGraph->getPinById(pinId);
		if (pin)
		{
			m_pinsOut.push_back(pin);
		}
		else
		{
			MIKAN_LOG_WARNING("Node::loadFromConfig")
				<< "Failed to find create output pin: " << pinId << ", on node: " << getClassName();
			success= false;
		}
	}

	return success;
}

void Node::saveToConfig(NodeConfigPtr nodeConfig) const
{
	nodeConfig->className= getClassName();
	nodeConfig->id= m_id;
	nodeConfig->pos= {m_nodePos.x, m_nodePos.y};
	nodeConfig->pageId= m_pageId;

	for (NodePinPtr pin : m_pinsIn)
	{
		nodeConfig->pinIDsIn.push_back(pin->getId());
	}

	for (NodePinPtr pin : m_pinsOut)
	{
		nodeConfig->pinIDsOut.push_back(pin->getId());
	}
}

void Node::setOwnerGraph(NodeGraphPtr ownerGraph) { m_ownerGraph= ownerGraph; }

ProjectManagerPtr Node::getOwnerProject() const { return getOwnerGraph()->getOwnerProject(); }

NodePinPtr Node::addPinByClassName(const std::string& className, const std::string& name, eNodePinDirection direction)
{
	NodePtr ownerNode= shared_from_this();
	NodeGraphPtr ownerGraph= ownerNode->getOwnerGraph();
	assert(ownerGraph);
	NodePinFactoryPtr pinFactory= ownerGraph->getPinFactory(className);
	assert(pinFactory);
	NodePinPtr pin= pinFactory->allocatePin();
	assert(pin);

	pin->setId(ownerNode->getOwnerGraph()->allocateId());
	pin->setOwnerNode(ownerNode);
	pin->setName(name);
	pin->setDirection(direction);
	if (direction == eNodePinDirection::OUTPUT)
		m_pinsOut.push_back(pin);
	else if (direction == eNodePinDirection::INPUT)
		m_pinsIn.push_back(pin);

	// Tell the graph about the new pin
	ownerGraph->addPin(pin);

	return pin;
}

bool Node::disconnectPin(NodePinPtr pinPtr)
{
	if (pinPtr->getDirection() == eNodePinDirection::INPUT)
	{
		auto it= std::find(m_pinsIn.begin(), m_pinsIn.end(), pinPtr);
		if (it != m_pinsIn.end())
		{
			m_pinsIn.erase(it);
			return true;
		}
	}
	else if (pinPtr->getDirection() == eNodePinDirection::OUTPUT)
	{
		auto it= std::find(m_pinsOut.begin(), m_pinsOut.end(), pinPtr);
		if (it != m_pinsOut.end())
		{
			m_pinsOut.erase(it);
			return true;
		}
	}

	return false;
}

void Node::disconnectAllPins()
{
	// Delete all the output pins associated with the node
	while (m_pinsOut.size() > 0)
	{
		const t_node_pin_id pinId= m_pinsOut[0]->getId();

		if (!m_ownerGraph->deletePinById(pinId))
		{
			MIKAN_LOG_ERROR("Node::disconnectAllPins")
				<< "Failed to delete output pin id: " << pinId << ", on node class: " << getClassName();
			break;
		}
	}

	// Delete all the input pins associated with the node
	while (m_pinsIn.size() > 0)
	{
		const t_node_pin_id pinId= m_pinsIn[0]->getId();

		if (!m_ownerGraph->deletePinById(pinId))
		{
			MIKAN_LOG_ERROR("Node::disconnectAllPins")
				<< "Failed to delete input pin id: " << pinId << ", on node class: " << getClassName();
			break;
		}
	}
}

bool Node::evaluateNode(NodeEvaluator& evaluator)
{
	evaluator.addError(
		NodeEvaluationError(eNodeEvaluationErrorCode::invalidNode, "Node missing evaluateNode implementation", this));

	return false;
}

bool Node::evaluateInputs(NodeEvaluator& evaluator)
{
	// Early out if input evaluation is disabled (already evaluated)
	if (evaluator.getIsInputEvaluationDisabled())
	{
		return true;
	}

	for (auto inputPin : m_pinsIn)
	{
		assert(inputPin->getDirection() == eNodePinDirection::INPUT);

		// Don't consider flow pins
		// (the control evaluation order, not value propagation)
		if (std::dynamic_pointer_cast<FlowPin>(inputPin))
			continue;

		// Get the output pin that this input pin feed by
		NodePinPtr outputSourcePin= inputPin->getConnectedSourcePin();
		if (!outputSourcePin)
		{
			if (inputPin->getHasDefaultValue())
			{
				// Use the default value the pin itself has
				continue;
			}
			else
			{
				evaluator.addError(NodeEvaluationError(
					eNodeEvaluationErrorCode::missingInput,
					StringUtils::stringify(inputPin->getName(), " missing input connection"), this, inputPin.get()));
				return false;
			}
		}

		// Recurse into node that owns the output and evaluate it's inputs ...
		// ... unless the source node has flow pins,
		// which means the source need should have already been evaluated
		NodePtr sourceNode= outputSourcePin->getOwnerNode();
		if (!sourceNode->hasAnyFlowPins())
		{
			// Recursively evaluate the inputs for the source node (if any)
			if (!sourceNode->evaluateInputs(evaluator))
				return false;

			// Then evaluate the node to update its output pins
			// but disable input evaluation to avoid re-evaluating
			// the source inputs we just evaluated recursively
			evaluator.setDisableInputEvaluation(true);
			bool bEvaluationSuccess= sourceNode->evaluateNode(evaluator);
			evaluator.setDisableInputEvaluation(false);
			if (!bEvaluationSuccess)
				return false;
		}

		// Update the input pin now that output it's connected to is updated
		inputPin->copyValueFromSourcePin();
	}

	return true;
}

bool Node::hasAnyConnectedPins() const
{
	for (const NodePinPtr& pin : m_pinsIn)
	{
		if (pin->hasAnyConnectedLinks())
			return true;
	}

	for (const NodePinPtr& pin : m_pinsOut)
	{
		if (pin->hasAnyConnectedLinks())
			return true;
	}

	return false;
}

FlowPinPtr Node::getOutputFlowPin() const { return FlowPinPtr(); }

void Node::editorRenderNode(const NodeEditorState& editorState)
{
	MkCanvasScopedNode scopedNode(m_id, editorGetHeaderColor());

	// Title
	editorRenderTitle(scopedNode);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));

	// Inputs
	editorRenderInputPins(editorState);

	// Outputs
	editorRenderOutputPins(editorState);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));
}

ImVec4 Node::editorGetHeaderColor() const { return ImVec4(85.f / 255.f, 85.f / 255.f, 85.f / 255.f, 1.f); }

void Node::editorComputeNodeDimensions(NodeDimensions& outDims) const
{
	// Measure the header string as rendered, icon prefix included
	const std::string titleString= editorGetComposedTitle();
	outDims.titleWidth= ImGui::CalcTextSize(titleString.c_str()).x;
	outDims.totalNodeWidth= outDims.titleWidth;
	outDims.inputColomnWidth= 0.0f;
	outDims.outputColomnWidth= 0.0f;

	for (auto& pin : m_pinsIn)
	{
		// Pins that hide their name contribute only their icon footprint
		const float nameWidth= pin->editorShowPinName()
								   ? ImGui::CalcTextSize(pin->getName().c_str()).x + NodePin::getEditorPinIconSpacing()
								   : 0.f;
		const float textWidth= NodePin::getEditorPinIconSize() + NodePin::getEditorPinIconSpacing() + nameWidth;
		const float inputWidth= pin->editorComputeInputWidth();

		outDims.inputColomnWidth= std::max(outDims.inputColomnWidth, std::max(textWidth, inputWidth));
	}

	for (auto& pin : m_pinsOut)
	{
		const float nameWidth= pin->editorShowPinName()
								   ? ImGui::CalcTextSize(pin->getName().c_str()).x + NodePin::getEditorPinIconSpacing()
								   : 0.f;

		outDims.outputColomnWidth= std::max(outDims.outputColomnWidth, nameWidth + NodePin::getEditorPinIconSize());
	}

	outDims.totalNodeWidth= std::max(outDims.totalNodeWidth, outDims.inputColomnWidth + outDims.outputColomnWidth);
}

std::string Node::editorGetComposedTitle() const
{
	const char* headerIcon= editorGetHeaderIcon();

	return headerIcon != nullptr ? std::string(headerIcon) + "  " + editorGetTitle() : editorGetTitle();
}

void Node::editorRenderTitle(MkCanvasScopedNode& scopedNode) const
{
	const std::string titleString= editorGetComposedTitle();

	scopedNode.beginHeader();
	{
		MkGuiScopedStyleVar styleVar;
		styleVar.push(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
		ImGui::TextUnformatted(titleString.c_str());
	}
	scopedNode.endHeader();
}

void Node::editorRenderInputPins(const NodeEditorState& editorState)
{
	if (m_pinsIn.size() > 0)
	{
		{
			MkGuiScopedGroup group;
			for (auto& pin : m_pinsIn)
			{
				pin->editorRenderInputPin(editorState);
			}
		}
		ImGui::SameLine();
	}
}

void Node::editorRenderOutputPins(const NodeEditorState& editorState) const
{
	if (m_pinsOut.size() == 0)
		return;

	NodeDimensions nodeDims= {};
	editorComputeNodeDimensions(nodeDims);

	// Right-align each output row against the node's content width, measured
	// from the actual cursor so rows starting after other content (a preview
	// image, the input column) pad by exactly the remaining distance
	const float rowRightEdgeX= MkCanvasScopedNode::getCurrentContentStartX() + nodeDims.totalNodeWidth;

	MkGuiScopedGroup group;
	for (auto& pin : m_pinsOut)
	{
		const float nameWidth= pin->editorShowPinName()
								   ? ImGui::CalcTextSize(pin->getName().c_str()).x + NodePin::getEditorPinIconSpacing()
								   : 0.f;
		const float prefixWidth= rowRightEdgeX - ImGui::GetCursorPosX() - nameWidth - NodePin::getEditorPinIconSize();

		pin->editorRenderOutputPin(editorState, std::max(prefixWidth, 0.f));
	}
}

// -- NodeFactory -----
NodeConfigPtr NodeFactory::allocateNodeConfig() const { return std::make_shared<NodeConfig>(); }

NodePtr NodeFactory::allocateNode() const { return std::make_shared<Node>(); }

NodePtr NodeFactory::createNode(const NodeEditorState& editorState) const
{
	NodePtr newNode= allocateNode();
	newNode->setId(editorState.nodeGraph->allocateId());
	newNode->setPageId(editorState.currentPageId);
	// The creation point is the node's first position; the editor window reads it from the node
	newNode->setNodePos(glm::vec2(editorState.hangPosGridSpace.x, editorState.hangPosGridSpace.y));

	// Assign graph to the node, which may in turn setup listener delegates to graph events
	newNode->setOwnerGraph(editorState.nodeGraph);

	return newNode;
}

void NodeFactory::autoConnectInputPin(const NodeEditorState& editorState, NodePinPtr inputPin) const
{
	assert(inputPin->getDirection() == eNodePinDirection::INPUT);

	// If spawned in an editor context from a dangling pin link
	// auto-connect the input pin to a compatible output pin
	if (editorState.startedLinkPinId != -1)
	{
		NodePinPtr outputPin= editorState.nodeGraph->getPinById(editorState.startedLinkPinId);

		if (inputPin->canPinsBeConnected(outputPin))
		{
			assert(outputPin->getDirection() == eNodePinDirection::OUTPUT);

			editorState.nodeGraph->createLink(inputPin->getId(), outputPin->getId());
		}
	}
}

void NodeFactory::autoConnectOutputPin(const NodeEditorState& editorState, NodePinPtr outputPin) const
{
	assert(outputPin->getDirection() == eNodePinDirection::OUTPUT);

	// If spawned in an editor context from a dangling pin link
	// auto-connect the output pin to a compatible input pin
	if (editorState.startedLinkPinId != -1)
	{
		NodePinPtr inputPin= editorState.nodeGraph->getPinById(editorState.startedLinkPinId);

		if (outputPin->canPinsBeConnected(inputPin))
		{
			assert(inputPin->getDirection() == eNodePinDirection::INPUT);

			editorState.nodeGraph->createLink(inputPin->getId(), outputPin->getId());
		}
	}
}
#include "ShapeSelectNode.h"
#include "IconsForkAwesome.h"
#include "LocText.h"
#include "Logger.h"
#include "NodeEditorState.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiStyleManager.h"

#include "CompositorComponent.h"
#include "MikanObject.h"
#include "QuadShapeComponent.h"
#include "BoxShapeComponent.h"
#include "ModelShapeComponent.h"
#include "QuadShapeSystem.h"
#include "BoxShapeSystem.h"
#include "ModelShapeSystem.h"

#include "Graphs/NodeEvaluator.h"

#include "Graphs/CompositorNodeGraph.h"
#include "Pins/ArrayPin.h"
#include "Properties/GraphShapeProperty.h"

#include "imgui.h"
#include "MkCanvasScopedNode.h"

// -- ShapeSelectNodeConfig -----
configuru::Config ShapeSelectNodeConfig::writeToJSON()
{
	configuru::Config pt= NodeConfig::writeToJSON();

	pt["enable_quad_shapes"]= bEnableQuadShapes;
	pt["enable_box_shapes"]= bEnableBoxShapes;
	pt["enable_model_shapes"]= bEnableModelShapes;

	return pt;
}

void ShapeSelectNodeConfig::readFromJSON(const configuru::Config& pt)
{
	NodeConfig::readFromJSON(pt);

	bEnableQuadShapes= pt.get_or<bool>("enable_quad_shapes", true);
	bEnableBoxShapes= pt.get_or<bool>("enable_box_shapes", true);
	bEnableModelShapes= pt.get_or<bool>("enable_model_shapes", true);
}

// -- ShapeSelectNode -----
bool ShapeSelectNode::loadFromConfig(NodeConfigConstPtr nodeConfig)
{
	if (Node::loadFromConfig(nodeConfig))
	{
		auto config= std::static_pointer_cast<const ShapeSelectNodeConfig>(nodeConfig);

		m_bEnableQuadShapes= config->bEnableQuadShapes;
		m_bEnableBoxShapes= config->bEnableBoxShapes;
		m_bEnableModelShapes= config->bEnableModelShapes;

		return true;
	}

	return false;
}

void ShapeSelectNode::saveToConfig(NodeConfigPtr nodeConfig) const
{
	auto config= std::static_pointer_cast<ShapeSelectNodeConfig>(nodeConfig);

	config->bEnableQuadShapes= m_bEnableQuadShapes;
	config->bEnableBoxShapes= m_bEnableBoxShapes;
	config->bEnableModelShapes= m_bEnableModelShapes;

	Node::saveToConfig(nodeConfig);
}

bool ShapeSelectNode::evaluateNode(NodeEvaluator& evaluator)
{
	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	CompositorComponentPtr compositorComponent= compositorGraph->getBoundCompositorComponent();
	if (!compositorComponent)
		return false;

	std::vector<GraphPropertyPtr> gathered;

	if (m_bEnableQuadShapes)
	{
		std::vector<QuadShapeComponentPtr> quadList;
		getObjectSystemOfType<QuadShapeSystem>()->getQuadShapeComponentList(quadList);

		for (QuadShapeComponentPtr shape : quadList)
		{
			// Only this compositor's scene draws into its composite
			if (!compositorComponent->ownsSceneObject(shape->getOwnerObject()))
				continue;

			auto prop= std::make_shared<GraphShapeProperty>();
			prop->setShapeComponent(shape);
			gathered.push_back(prop);
		}
	}

	if (m_bEnableBoxShapes)
	{
		std::vector<BoxShapeComponentPtr> boxList;
		getObjectSystemOfType<BoxShapeSystem>()->getBoxShapeComponentList(boxList);

		for (BoxShapeComponentPtr shape : boxList)
		{
			if (!compositorComponent->ownsSceneObject(shape->getOwnerObject()))
				continue;

			auto prop= std::make_shared<GraphShapeProperty>();
			prop->setShapeComponent(shape);
			gathered.push_back(prop);
		}
	}

	if (m_bEnableModelShapes)
	{
		std::vector<ModelShapeComponentPtr> modelList;
		getObjectSystemOfType<ModelShapeSystem>()->getModelShapeComponentList(modelList);

		for (ModelShapeComponentPtr shape : modelList)
		{
			if (!compositorComponent->ownsSceneObject(shape->getOwnerObject()))
				continue;

			auto prop= std::make_shared<GraphShapeProperty>();
			prop->setShapeComponent(shape);
			gathered.push_back(prop);
		}
	}

	auto outputPin= getFirstPinOfType<ArrayPin>(eNodePinDirection::OUTPUT);
	if (outputPin)
	{
		outputPin->setArray(gathered);
	}

	return true;
}

ImVec4 ShapeSelectNode::editorGetHeaderColor() const
{
	return ImVec4(80.f / 255.f, 150.f / 255.f, 130.f / 255.f, 225.f / 255.f);
}

void ShapeSelectNode::editorRenderNode(const NodeEditorState& editorState)
{
	MkCanvasScopedNode scopedNode(m_id, editorGetHeaderColor());

	editorRenderTitle(scopedNode);
	editorRenderOutputPins(editorState);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));
}

void ShapeSelectNode::editorRenderPropertySheet(const NodeEditorState& editorState)
{
	if (MkGui::drawPropertySheetHeader(editorState.styleManager->getStyle("node_editor_panel_header"),
									   locText("nodes.shapeSelectHeader")))
	{
		MkGuiStyleConstPtr propertyStyle= editorState.styleManager->getStyle("node_editor_property_value");

		MkGui::drawCheckBoxProperty(propertyStyle, "ShapeSelectNodeEnableQuad", locText("nodes.enableQuads"),
									m_bEnableQuadShapes);
		MkGui::drawCheckBoxProperty(propertyStyle, "ShapeSelectNodeEnableBox", locText("nodes.enableBoxes"),
									m_bEnableBoxShapes);
		MkGui::drawCheckBoxProperty(propertyStyle, "ShapeSelectNodeEnableModel", locText("nodes.enableModels"),
									m_bEnableModelShapes);
	}
}

// -- ShapeSelectNodeFactory -----
NodePtr ShapeSelectNodeFactory::createNode(const NodeEditorState& editorState) const
{
	auto node= std::static_pointer_cast<ShapeSelectNode>(NodeFactory::createNode(editorState));

	ArrayPinPtr outPin= node->addPin<ArrayPin>("shapes", eNodePinDirection::OUTPUT);
	outPin->setElementClassName(GraphShapeProperty::k_propertyClassName);

	autoConnectOutputPin(editorState, outPin);

	return node;
}

const char* ShapeSelectNode::editorGetHeaderIcon() const { return ICON_FK_LIST_UL; }

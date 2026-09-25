#include "StencilSelectNode.h"
#include "IconsForkAwesome.h"
#include "CameraComponent.h"
#include "CompositorComponent.h"
#include "MikanObject.h"
#include "LocText.h"
#include "Logger.h"
#include "NodeEditorState.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiStyleManager.h"

#include "QuadStencilComponent.h"
#include "BoxStencilComponent.h"
#include "ModelStencilComponent.h"
#include "QuadStencilSystem.h"
#include "BoxStencilSystem.h"
#include "ModelStencilSystem.h"

#include "Graphs/CompositorNodeGraph.h"
#include "Graphs/NodeEvaluator.h"

#include "Pins/ArrayPin.h"
#include "Properties/GraphStencilProperty.h"

#include "imgui.h"
#include "MkCanvasScopedNode.h"

// -- StencilSelectNodeConfig -----
configuru::Config StencilSelectNodeConfig::writeToJSON()
{
	configuru::Config pt= NodeConfig::writeToJSON();

	pt["enable_quad_stencils"]= bEnableQuadStencils;
	pt["enable_box_stencils"]= bEnableBoxStencils;
	pt["enable_model_stencils"]= bEnableModelStencils;

	return pt;
}

void StencilSelectNodeConfig::readFromJSON(const configuru::Config& pt)
{
	NodeConfig::readFromJSON(pt);

	bEnableQuadStencils= pt.get_or<bool>("enable_quad_stencils", true);
	bEnableBoxStencils= pt.get_or<bool>("enable_box_stencils", true);
	bEnableModelStencils= pt.get_or<bool>("enable_model_stencils", true);
}

// -- StencilSelectNode -----
bool StencilSelectNode::loadFromConfig(NodeConfigConstPtr nodeConfig)
{
	if (Node::loadFromConfig(nodeConfig))
	{
		auto config= std::static_pointer_cast<const StencilSelectNodeConfig>(nodeConfig);

		m_bEnableQuadStencils= config->bEnableQuadStencils;
		m_bEnableBoxStencils= config->bEnableBoxStencils;
		m_bEnableModelStencils= config->bEnableModelStencils;

		return true;
	}

	return false;
}

void StencilSelectNode::saveToConfig(NodeConfigPtr nodeConfig) const
{
	auto config= std::static_pointer_cast<StencilSelectNodeConfig>(nodeConfig);

	config->bEnableQuadStencils= m_bEnableQuadStencils;
	config->bEnableBoxStencils= m_bEnableBoxStencils;
	config->bEnableModelStencils= m_bEnableModelStencils;

	Node::saveToConfig(nodeConfig);
}

bool StencilSelectNode::evaluateNode(NodeEvaluator& evaluator)
{
	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	CompositorComponentPtr compositorComponent= compositorGraph->getBoundCompositorComponent();
	CameraComponentPtr cameraComponent= compositorGraph->getBoundCameraComponent();
	if (!compositorComponent || !cameraComponent)
		return false;

	// World space: the stencil systems cull against stencil world transforms
	glm::mat4 cameraXform;
	if (!cameraComponent->getWorldSpaceAperturePose(cameraXform))
		return false;

	const glm::vec3 cameraForward(cameraXform[2] * -1.f); // Camera forward is along negative z-axis
	const glm::vec3 cameraPosition(cameraXform[3]);

	std::vector<GraphPropertyPtr> gathered;

	if (m_bEnableQuadStencils)
	{
		std::vector<QuadStencilComponentPtr> quadList;
		getObjectSystemOfType<QuadStencilSystem>()->getRelevantQuadStencilList(nullptr, cameraPosition, cameraForward,
																			   quadList);

		for (QuadStencilComponentPtr stencil : quadList)
		{
			// Only this compositor's scene masks its composite
			if (!compositorComponent->ownsSceneObject(stencil->getOwnerObject()))
				continue;

			auto prop= std::make_shared<GraphStencilProperty>();
			prop->setStencilComponent(stencil);
			gathered.push_back(prop);
		}
	}

	if (m_bEnableBoxStencils)
	{
		std::vector<BoxStencilComponentPtr> boxList;
		getObjectSystemOfType<BoxStencilSystem>()->getRelevantBoxStencilList(nullptr, cameraPosition, cameraForward,
																			 boxList);

		for (BoxStencilComponentPtr stencil : boxList)
		{
			if (!compositorComponent->ownsSceneObject(stencil->getOwnerObject()))
				continue;

			auto prop= std::make_shared<GraphStencilProperty>();
			prop->setStencilComponent(stencil);
			gathered.push_back(prop);
		}
	}

	if (m_bEnableModelStencils)
	{
		std::vector<ModelStencilComponentPtr> modelList;
		getObjectSystemOfType<ModelStencilSystem>()->getRelevantModelStencilList(nullptr, cameraPosition, cameraForward,
																				 modelList);

		for (ModelStencilComponentPtr stencil : modelList)
		{
			if (!compositorComponent->ownsSceneObject(stencil->getOwnerObject()))
				continue;

			auto prop= std::make_shared<GraphStencilProperty>();
			prop->setStencilComponent(stencil);
			gathered.push_back(prop);
		}
	}

	// Assign the resulting array to the output pin
	auto outputPin= getFirstPinOfType<ArrayPin>(eNodePinDirection::OUTPUT);
	if (outputPin)
	{
		outputPin->setArray(gathered);
	}

	return true;
}

ImVec4 StencilSelectNode::editorGetHeaderColor() const
{
	return ImVec4(150.f / 255.f, 130.f / 255.f, 110.f / 255.f, 225.f / 255.f);
}

void StencilSelectNode::editorRenderNode(const NodeEditorState& editorState)
{
	MkCanvasScopedNode scopedNode(m_id, editorGetHeaderColor());

	// Title
	editorRenderTitle(scopedNode);

	// Outputs
	editorRenderOutputPins(editorState);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));
}

void StencilSelectNode::editorRenderPropertySheet(const NodeEditorState& editorState)
{
	if (MkGui::drawPropertySheetHeader(editorState.styleManager->getStyle("node_editor_panel_header"),
									   locText("nodes.stencilSelectHeader")))
	{
		MkGuiStyleConstPtr propertyStyle= editorState.styleManager->getStyle("node_editor_property_value");

		MkGui::drawCheckBoxProperty(propertyStyle, "StencilSelectNodeEnableQuad", locText("nodes.enableQuads"),
									m_bEnableQuadStencils);
		MkGui::drawCheckBoxProperty(propertyStyle, "StencilSelectNodeEnableBox", locText("nodes.enableBoxes"),
									m_bEnableBoxStencils);
		MkGui::drawCheckBoxProperty(propertyStyle, "StencilSelectNodeEnableModel", locText("nodes.enableModels"),
									m_bEnableModelStencils);
	}
}

// -- StencilSelectNodeFactory -----
NodePtr StencilSelectNodeFactory::createNode(const NodeEditorState& editorState) const
{
	auto node= std::static_pointer_cast<StencilSelectNode>(NodeFactory::createNode(editorState));

	ArrayPinPtr outPin= node->addPin<ArrayPin>("stencils", eNodePinDirection::OUTPUT);
	outPin->setElementClassName(GraphStencilProperty::k_propertyClassName);

	autoConnectOutputPin(editorState, outPin);

	return node;
}

const char* StencilSelectNode::editorGetHeaderIcon() const { return ICON_FK_LIST_UL; }

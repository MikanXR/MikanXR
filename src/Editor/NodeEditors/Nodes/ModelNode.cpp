#include "ModelNode.h"
#include "IconsForkAwesome.h"
#include "MkMaterial.h"
#include "IMkTexture.h"
#include "Logger.h"
#include "MkGuiDrawUtils.h"
#include "ModelAssetReference.h"
#include "NodeEditorState.h"
#include "Graphs/NodeGraph.h"
#include "Pins/NodePin.h"
#include "Pins/PropertyPin.h"
#include "Properties/GraphModelProperty.h"

#include "imgui.h"
#include "MkCanvasScopedNode.h"

// -- ModelNodeConfig -----
configuru::Config ModelNodeConfig::writeToJSON()
{
	configuru::Config pt= NodeConfig::writeToJSON();

	pt["model_property_id"]= modelPropertyId;

	return pt;
}

void ModelNodeConfig::readFromJSON(const configuru::Config& pt)
{
	NodeConfig::readFromJSON(pt);

	modelPropertyId= pt.get_or<t_graph_property_id>("model_property_id", -1);
}

// -- ModelNode -----
ModelNode::~ModelNode() { setOwnerGraph(NodeGraphPtr()); }

void ModelNode::setOwnerGraph(NodeGraphPtr newOwnerGraph)
{
	if (newOwnerGraph != m_ownerGraph)
	{
		if (m_ownerGraph)
		{
			m_ownerGraph->OnPropertyDeleted-= MakeDelegate(this, &ModelNode::onGraphPropertyDeleted);
			m_ownerGraph= nullptr;
		}

		if (newOwnerGraph)
		{
			newOwnerGraph->OnPropertyDeleted+= MakeDelegate(this, &ModelNode::onGraphPropertyDeleted);
			m_ownerGraph= newOwnerGraph;
		}
	}
}

bool ModelNode::loadFromConfig(NodeConfigConstPtr nodeConfig)
{
	if (Node::loadFromConfig(nodeConfig))
	{
		auto modelNodeConfig= std::static_pointer_cast<const ModelNodeConfig>(nodeConfig);
		t_graph_property_id propId= modelNodeConfig->modelPropertyId;

		auto materialProperty= getOwnerGraph()->getTypedPropertyById<GraphModelProperty>(propId);
		if (materialProperty)
		{
			setModelSource(materialProperty);
			return true;
		}
		else
		{
			MIKAN_LOG_WARNING("ModelNode::loadFromConfig")
				<< "Failed to find model property: " << propId << ", on model node";
		}
	}

	return false;
}

void ModelNode::saveToConfig(NodeConfigPtr nodeConfig) const
{
	auto materialNodeConfig= std::static_pointer_cast<ModelNodeConfig>(nodeConfig);
	materialNodeConfig->modelPropertyId= m_sourceProperty ? m_sourceProperty->getId() : -1;

	Node::saveToConfig(nodeConfig);
}

MikanRenderModelResourcePtr ModelNode::getModelResource() const
{
	return m_sourceProperty ? m_sourceProperty->getModelResource() : MikanRenderModelResourcePtr();
}

void ModelNode::setModelSource(GraphModelPropertyPtr inModelProperty)
{
	m_sourceProperty= inModelProperty;

	PropertyPinPtr outPin= getFirstPinOfType<PropertyPin>(eNodePinDirection::OUTPUT);
	if (outPin)
	{
		outPin->setValue(m_sourceProperty);
	}
}

bool ModelNode::evaluateNode(NodeEvaluator& evaluator)
{
	// Only update output pin in setMaterialSource

	return true;
}

ImVec4 ModelNode::editorGetHeaderColor() const
{
	return ImVec4(150.f / 255.f, 130.f / 255.f, 110.f / 255.f, 225.f / 255.f);
}

std::string ModelNode::editorGetTitle() const
{
	if (m_sourceProperty)
	{
		// The graph variable's name is what the editor lets you rename, so it
		// titles the node; the asset name is only a fallback for an unnamed one
		const std::string& propertyName= m_sourceProperty->getName();
		if (!propertyName.empty())
		{
			return propertyName;
		}

		auto assetRef= m_sourceProperty->getModelAssetReference();
		if (assetRef)
		{
			return assetRef->getShortName();
		}

		return propertyName;
	}
	else
	{
		return "Empty Model";
	}
}

void ModelNode::editorRenderNode(const NodeEditorState& editorState)
{
	MkCanvasScopedNode scopedNode(m_id, editorGetHeaderColor());

	// Title
	editorRenderTitle(scopedNode);

	// Texture
	ImGui::Dummy(ImVec2(1.0f, 0.5f));
	// A node with no model assigned still draws, titled "Empty Model"
	ModelAssetReferencePtr modelAssetRef=
		m_sourceProperty ? m_sourceProperty->getModelAssetReference() : ModelAssetReferencePtr();
	IMkTexturePtr textureResource= modelAssetRef ? modelAssetRef->getPreviewTexture() : IMkTexturePtr();
	MkGui::drawImage(textureResource, 100.f, 100.f);
	ImGui::SameLine();

	// Outputs
	editorRenderOutputPins(editorState);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));
}

void ModelNode::onGraphPropertyDeleted(t_graph_property_id id)
{
	if (m_sourceProperty && m_sourceProperty->getId() == id)
	{
		setModelSource(GraphModelPropertyPtr());
	}
}

// -- ModelNode Factory -----
NodePtr ModelNodeFactory::createNode(const NodeEditorState& editorState) const
{
	// Create the node and pins
	NodePtr node= NodeFactory::createNode(editorState);
	auto outputPin= node->addPin<PropertyPin>("model", eNodePinDirection::OUTPUT);
	outputPin->setPropertyClassName(GraphModelProperty::k_propertyClassName);
	outputPin->editorSetShowPinName(false);

	// If spawned in an editor context from a dangling pin link
	// auto-connect the output pin to a compatible input pin
	autoConnectOutputPin(editorState, outputPin);

	return node;
}

const char* ModelNode::editorGetHeaderIcon() const { return ICON_FK_CUBE; }

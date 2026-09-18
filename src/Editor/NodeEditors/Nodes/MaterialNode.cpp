#include "MaterialNode.h"
#include "IconsForkAwesome.h"
#include "MkMaterial.h"
#include "IMkTexture.h"
#include "Logger.h"
#include "MaterialAssetReference.h"
#include "MkGuiDrawUtils.h"
#include "NodeEditorState.h"
#include "Graphs/NodeGraph.h"
#include "Pins/NodePin.h"
#include "Pins/PropertyPin.h"
#include "Properties/GraphMaterialProperty.h"

#include "imgui.h"
#include "MkCanvasScopedNode.h"

// -- MaterialNodeConfig -----
configuru::Config MaterialNodeConfig::writeToJSON()
{
	configuru::Config pt= NodeConfig::writeToJSON();

	pt["material_property_id"]= materialPropertyId;

	return pt;
}

void MaterialNodeConfig::readFromJSON(const configuru::Config& pt)
{
	NodeConfig::readFromJSON(pt);

	materialPropertyId= pt.get_or<t_graph_property_id>("material_property_id", -1);
}

// -- MaterialNode -----
MaterialNode::~MaterialNode() { setOwnerGraph(NodeGraphPtr()); }

void MaterialNode::setOwnerGraph(NodeGraphPtr newOwnerGraph)
{
	if (newOwnerGraph != m_ownerGraph)
	{
		if (m_ownerGraph)
		{
			m_ownerGraph->OnPropertyDeleted-= MakeDelegate(this, &MaterialNode::onGraphPropertyDeleted);
			m_ownerGraph= nullptr;
		}

		if (newOwnerGraph)
		{
			newOwnerGraph->OnPropertyDeleted+= MakeDelegate(this, &MaterialNode::onGraphPropertyDeleted);
			m_ownerGraph= newOwnerGraph;
		}
	}
}

bool MaterialNode::loadFromConfig(NodeConfigConstPtr nodeConfig)
{
	if (Node::loadFromConfig(nodeConfig))
	{
		auto materialNodeConfig= std::static_pointer_cast<const MaterialNodeConfig>(nodeConfig);
		t_graph_property_id propId= materialNodeConfig->materialPropertyId;

		auto materialProperty= getOwnerGraph()->getTypedPropertyById<GraphMaterialProperty>(propId);
		if (materialProperty)
		{
			setMaterialSource(materialProperty);
			return true;
		}
		else
		{
			MIKAN_LOG_WARNING("MaterialNode::loadFromConfig")
				<< "Failed to find material property: " << propId << ", on material node";
		}
	}

	return false;
}

void MaterialNode::editorRenderPropertySheet(const NodeEditorState& editorState)
{
	if (m_sourceProperty)
	{
		m_sourceProperty->editorRenderPropertySheet(editorState);
	}
}

void MaterialNode::saveToConfig(NodeConfigPtr nodeConfig) const
{
	auto materialNodeConfig= std::static_pointer_cast<MaterialNodeConfig>(nodeConfig);
	materialNodeConfig->materialPropertyId= m_sourceProperty ? m_sourceProperty->getId() : -1;

	Node::saveToConfig(nodeConfig);
}

MkMaterialConstPtr MaterialNode::getMaterialResource() const
{
	return m_sourceProperty ? m_sourceProperty->getMaterialResource() : MkMaterialConstPtr();
}

void MaterialNode::setMaterialSource(GraphMaterialPropertyPtr inMaterialProperty)
{
	m_sourceProperty= inMaterialProperty;

	auto outPin= getFirstPinOfType<PropertyPin>(eNodePinDirection::OUTPUT);
	if (outPin)
	{
		outPin->setValue(m_sourceProperty);
	}
}

bool MaterialNode::evaluateNode(NodeEvaluator& evaluator)
{
	// Only update output pin in setMaterialSource

	return true;
}

ImVec4 MaterialNode::editorGetHeaderColor() const
{
	return ImVec4(150.f / 255.f, 130.f / 255.f, 110.f / 255.f, 225.f / 255.f);
}

std::string MaterialNode::editorGetTitle() const
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

		auto assetRef= m_sourceProperty->getMaterialAssetReference();
		if (assetRef)
		{
			return assetRef->getShortName();
		}

		return propertyName;
	}
	else
	{
		return "Empty Material";
	}
}

void MaterialNode::editorRenderNode(const NodeEditorState& editorState)
{
	MkCanvasScopedNode scopedNode(m_id, editorGetHeaderColor());

	// Title
	editorRenderTitle(scopedNode);

	// Texture
	ImGui::Dummy(ImVec2(1.0f, 0.5f));
	// A node with no material assigned still draws, titled "Empty Material"
	MaterialAssetReferencePtr materialAssetRef=
		m_sourceProperty ? m_sourceProperty->getMaterialAssetReference() : MaterialAssetReferencePtr();
	if (materialAssetRef)
	{
		auto previewTexture= materialAssetRef->getPreviewTexture();

		if (previewTexture && previewTexture->getGlTextureId() != 0)
		{
			MkGui::drawImage(previewTexture, 100.f, 100.f);
			ImGui::SameLine();
		}
	}

	// Outputs
	editorRenderOutputPins(editorState);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));
}

void MaterialNode::editorOnDoubleClicked(const NodeEditorState& editorState)
{
	if (m_sourceProperty)
	{
		m_sourceProperty->editorOpenSourceGraph();
	}
}

void MaterialNode::onGraphPropertyDeleted(t_graph_property_id id)
{
	if (m_sourceProperty && m_sourceProperty->getId() == id)
	{
		setMaterialSource(GraphMaterialPropertyPtr());
	}
}

// -- MaterialNode Factory -----
NodePtr MaterialNodeFactory::createNode(const NodeEditorState& editorState) const
{
	// Create the node and pins
	NodePtr node= NodeFactory::createNode(editorState);
	PropertyPinPtr outputPin= node->addPin<PropertyPin>("material", eNodePinDirection::OUTPUT);
	outputPin->setPropertyClassName(GraphMaterialProperty::k_propertyClassName);
	outputPin->editorSetShowPinName(false);
	// TODO: Add vertex definition attribute to the pin

	// If spawned in an editor context from a dangling pin link
	// auto-connect the output pin to a compatible input pin
	autoConnectOutputPin(editorState, outputPin);

	return node;
}

const char* MaterialNode::editorGetHeaderIcon() const { return ICON_FK_PAINT_BRUSH; }

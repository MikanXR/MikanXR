#include "TextureNode.h"
#include "IconsForkAwesome.h"
#include "IMkTexture.h"
#include "MkGuiDrawUtils.h"
#include "NodeEditorState.h"
#include "Graphs/NodeGraph.h"
#include "Logger.h"
#include "TextureAssetReference.h"
#include "Pins/NodePin.h"
#include "Pins/TexturePin.h"
#include "Properties/GraphTextureProperty.h"

#include "imgui.h"
#include "MkCanvasScopedNode.h"

// -- TextureNodeConfig -----
configuru::Config TextureNodeConfig::writeToJSON()
{
	configuru::Config pt= NodeConfig::writeToJSON();

	pt["texture_property_id"]= texturePropertyId;

	return pt;
}

void TextureNodeConfig::readFromJSON(const configuru::Config& pt)
{
	NodeConfig::readFromJSON(pt);

	texturePropertyId= pt.get_or<t_graph_property_id>("texture_property_id", -1);
}

// -- TextureNode -----
TextureNode::~TextureNode()
{
	// Force de-registration of graph property change delegates
	setOwnerGraph(NodeGraphPtr());
}

void TextureNode::setOwnerGraph(NodeGraphPtr newOwnerGraph)
{
	if (newOwnerGraph != m_ownerGraph)
	{
		if (m_ownerGraph)
		{
			m_ownerGraph->OnPropertyDeleted-= MakeDelegate(this, &TextureNode::onGraphPropertyDeleted);
			m_ownerGraph= nullptr;
		}

		if (newOwnerGraph)
		{
			newOwnerGraph->OnPropertyDeleted+= MakeDelegate(this, &TextureNode::onGraphPropertyDeleted);
			m_ownerGraph= newOwnerGraph;
		}
	}
}

bool TextureNode::loadFromConfig(NodeConfigConstPtr nodeConfig)
{
	if (Node::loadFromConfig(nodeConfig))
	{
		auto textureNodeConfig= std::static_pointer_cast<const TextureNodeConfig>(nodeConfig);
		t_graph_property_id propId= textureNodeConfig->texturePropertyId;

		auto textureProperty= getOwnerGraph()->getTypedPropertyById<GraphTextureProperty>(propId);
		if (textureProperty)
		{
			setTextureSource(textureProperty);
			return true;
		}
		else
		{
			MIKAN_LOG_WARNING("TextureNode::loadFromConfig")
				<< "Failed to find texture property: " << propId << ", on texture node";
		}
	}

	return false;
}

void TextureNode::saveToConfig(NodeConfigPtr nodeConfig) const
{
	auto textureNodeConfig= std::static_pointer_cast<TextureNodeConfig>(nodeConfig);
	textureNodeConfig->texturePropertyId= m_sourceProperty ? m_sourceProperty->getId() : -1;

	Node::saveToConfig(nodeConfig);
}

IMkTexturePtr TextureNode::getTextureResource() const
{
	return m_sourceProperty ? m_sourceProperty->getTextureResource() : IMkTexturePtr();
}

void TextureNode::setTextureSource(GraphTexturePropertyPtr inTextureProperty)
{
	m_sourceProperty= inTextureProperty;

	auto outPin= getFirstPinOfType<TexturePin>(eNodePinDirection::OUTPUT);
	if (outPin)
	{
		outPin->setValue(m_sourceProperty->getTextureResource());
	}
}

bool TextureNode::evaluateNode(NodeEvaluator& evaluator)
{
	// Only update output in in setTextureSource

	return true;
}

ImVec4 TextureNode::editorGetHeaderColor() const
{
	return ImVec4(150.f / 255.f, 130.f / 255.f, 110.f / 255.f, 225.f / 255.f);
}

std::string TextureNode::editorGetTitle() const
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

		auto assetRef= m_sourceProperty->getTextureAssetReference();
		if (assetRef)
		{
			return assetRef->getShortName();
		}

		return propertyName;
	}
	else
	{
		return "Empty Texture";
	}
}

void TextureNode::editorRenderNode(const NodeEditorState& editorState)
{
	MkCanvasScopedNode scopedNode(m_id, editorGetHeaderColor());

	// Title
	editorRenderTitle(scopedNode);

	// Texture
	ImGui::Dummy(ImVec2(1.0f, 0.5f));
	IMkTexturePtr textureResource= getTextureResource();
	MkGui::drawImage(textureResource, 100.f, 100.f);
	ImGui::SameLine();

	// Outputs
	editorRenderOutputPins(editorState);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));
}

void TextureNode::editorRenderPropertySheet(const NodeEditorState& editorState)
{
	if (m_sourceProperty)
	{
		m_sourceProperty->editorRenderPropertySheet(editorState);
	}
}

void TextureNode::onGraphPropertyDeleted(t_graph_property_id id)
{
	if (m_sourceProperty && m_sourceProperty->getId() == id)
	{
		setTextureSource(GraphTexturePropertyPtr());
	}
}

// -- TextureNode Factory -----
NodePtr TextureNodeFactory::createNode(const NodeEditorState& editorState) const
{
	// Create the node and pins
	NodePtr node= NodeFactory::createNode(editorState);
	auto outputPin= node->addPin<TexturePin>("texture", eNodePinDirection::OUTPUT);
	outputPin->editorSetShowPinName(false);

	// If spawned in an editor context from a dangling pin link
	// auto-connect the output pin to a compatible input pin
	autoConnectOutputPin(editorState, outputPin);

	return node;
}

const char* TextureNode::editorGetHeaderIcon() const { return ICON_FK_PICTURE_O; }

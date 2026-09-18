#include "IMkTexture.h"
#include "IconsForkAwesome.h"
#include "TextureSourceNode.h"
#include "Logger.h"
#include "MkGuiDrawUtils.h"
#include "NodeEditorState.h"
#include "TextureSourceComponent.h"

#include "Graphs/NodeEvaluator.h"
#include "Graphs/NodeGraph.h"

#include "Pins/NodePin.h"
#include "Pins/TexturePin.h"

#include "Properties/GraphTextureSourceProperty.h"

#include "imgui.h"
#include "MkCanvasScopedNode.h"

// -- TextureSourceNodeConfig -----
configuru::Config TextureSourceNodeConfig::writeToJSON()
{
	configuru::Config pt= NodeConfig::writeToJSON();

	pt["texture_source_property_id"]= textureSourcePropertyId;

	return pt;
}

void TextureSourceNodeConfig::readFromJSON(const configuru::Config& pt)
{
	NodeConfig::readFromJSON(pt);

	textureSourcePropertyId= pt.get_or<t_graph_property_id>("texture_source_property_id", -1);
}

// -- TextureSourceNode -----
TextureSourceNode::~TextureSourceNode() { setOwnerGraph(NodeGraphPtr()); }

void TextureSourceNode::setOwnerGraph(NodeGraphPtr newOwnerGraph)
{
	if (newOwnerGraph != m_ownerGraph)
	{
		if (m_ownerGraph)
		{
			m_ownerGraph->OnPropertyDeleted-= MakeDelegate(this, &TextureSourceNode::onGraphPropertyDeleted);
			m_ownerGraph= nullptr;
		}

		if (newOwnerGraph)
		{
			newOwnerGraph->OnPropertyDeleted+= MakeDelegate(this, &TextureSourceNode::onGraphPropertyDeleted);
			m_ownerGraph= newOwnerGraph;
		}
	}
}

bool TextureSourceNode::loadFromConfig(NodeConfigConstPtr nodeConfig)
{
	if (Node::loadFromConfig(nodeConfig))
	{
		auto textureSourceNodeConfig= std::static_pointer_cast<const TextureSourceNodeConfig>(nodeConfig);
		t_graph_property_id propId= textureSourceNodeConfig->textureSourcePropertyId;

		auto textureSourceProperty= getOwnerGraph()->getTypedPropertyById<GraphTextureSourceProperty>(propId);
		if (textureSourceProperty)
		{
			setTextureSourceProperty(textureSourceProperty);
			return true;
		}
		else
		{
			MIKAN_LOG_WARNING("TextureSourceNode::loadFromConfig")
				<< "Failed to find Stencil property: " << propId << ", on Stencil node";
		}
	}

	return false;
}

void TextureSourceNode::saveToConfig(NodeConfigPtr nodeConfig) const
{
	TextureSourceNodeConfigPtr textureSourceNodeConfig= std::static_pointer_cast<TextureSourceNodeConfig>(nodeConfig);
	textureSourceNodeConfig->textureSourcePropertyId= m_sourceProperty ? m_sourceProperty->getId() : -1;

	Node::saveToConfig(nodeConfig);
}

TextureSourceComponentPtr TextureSourceNode::getTextureSourceComponent() const
{
	return m_sourceProperty ? m_sourceProperty->getTextureSourceComponent() : TextureSourceComponentPtr();
}

void TextureSourceNode::setTextureSourceProperty(GraphTextureSourcePropertyPtr inTextureSourceProperty)
{
	m_sourceProperty= inTextureSourceProperty;
}

IMkTexturePtr TextureSourceNode::getTextureResource() const
{
	TextureSourceComponentPtr textureSourceComponent= getTextureSourceComponent();
	if (textureSourceComponent != nullptr)
	{
		// TODO: Not sure what to use for cameraId here (probably should come from NodeEvaluator context?)
		// For now just pass an invalid ID and let the component decide what to do with it
		MikanCameraID cameraId= INVALID_MIKAN_ID;
		return textureSourceComponent->getClientColorSourceTexture(cameraId, eTextureSourceColorType::colorRGBA);
	}

	return IMkTexturePtr();
}

bool TextureSourceNode::evaluateNode(NodeEvaluator& evaluator)
{
	TextureSourceComponentPtr textureSourceComponent= getTextureSourceComponent();

	// Since the editor can change the texture source texture can change out from under us
	// it's safest to just refresh the output texture pin every frame
	IMkTexturePtr textureResource= getTextureResource();
	auto outputPin= getFirstPinOfType<TexturePin>(eNodePinDirection::OUTPUT);
	if (outputPin)
	{
		outputPin->setValue(textureResource);
		outputPin->editorSetShowPinName(false);
	}

	if (!textureSourceComponent)
	{
		evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::missingInput,
											   StringUtils::stringify("Texture Source Component is not valid"), this));
	}
	else if (!textureResource || !textureResource->getIsValid())
	{
		evaluator.addError(NodeEvaluationError(
			eNodeEvaluationErrorCode::missingInput,
			StringUtils::stringify("TextureSource ", textureSourceComponent->getName(), " missing output"), this));
	}

	return true;
}

ImVec4 TextureSourceNode::editorGetHeaderColor() const
{
	return ImVec4(150.f / 255.f, 130.f / 255.f, 110.f / 255.f, 225.f / 255.f);
}

void TextureSourceNode::editorRenderNode(const NodeEditorState& textureSourcePropertyId)
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
	editorRenderOutputPins(textureSourcePropertyId);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));
}

std::string TextureSourceNode::editorGetTitle() const
{
	if (m_sourceProperty)
	{
		TextureSourceComponentPtr textureSourceComponent= m_sourceProperty->getTextureSourceComponent();
		if (textureSourceComponent)
		{
			return textureSourceComponent->getName();
		}
	}

	return "Stencil";
}

void TextureSourceNode::editorRenderPropertySheet(const NodeEditorState& textureSourcePropertyId)
{
	if (m_sourceProperty)
	{
		m_sourceProperty->editorRenderPropertySheet(textureSourcePropertyId);
	}
}

void TextureSourceNode::onGraphPropertyDeleted(t_graph_property_id id)
{
	if (m_sourceProperty && m_sourceProperty->getId() == id)
	{
		setTextureSourceProperty(GraphTextureSourcePropertyPtr());
	}
}

// -- TextureSourceNode Factory -----
NodePtr TextureSourceNodeFactory::createNode(const NodeEditorState& textureSourcePropertyId) const
{
	// Create the node and pins
	NodePtr node= NodeFactory::createNode(textureSourcePropertyId);
	auto outputPin= node->addPin<TexturePin>("Texture", eNodePinDirection::OUTPUT);
	outputPin->editorSetShowPinName(false);

	// If spawned in an editor context from a dangling pin link
	// auto-connect the output pin to a compatible input pin
	autoConnectOutputPin(textureSourcePropertyId, outputPin);

	return node;
}

const char* TextureSourceNode::editorGetHeaderIcon() const { return ICON_FK_PICTURE_O; }

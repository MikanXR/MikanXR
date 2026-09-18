#include "CameraComponent.h"
#include "CompositorComponent.h"
#include "DepthTextureSourceNode.h"
#include "MkScopedObjectBinding.h"
#include "IEditorWindow.h"
#include "IMkFrameBuffer.h"
#include "IMkGraphicsContext.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MikanShaderCache.h"
#include "MkStateStack.h"
#include "IMkTexture.h"
#include "MikanTextureCache.h"
#include "IMkTriangulatedMesh.h"
#include "LocText.h"
#include "Logger.h"
#include "NodeEditorState.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiStyleManager.h"
#include "StringUtils.h"
#include "ProjectManager.h"
#include "TextureSourceQueries.h"
#include "TextureSourceComponent.h"
#include "MikanCoreTypes.h"

#include "DataSources/TextureSourceListDataSource.h"

#include "Graphs/NodeEvaluator.h"
#include "Graphs/NodeGraph.h"
#include "Graphs/CompositorNodeGraph.h"

#include "Pins/NodePin.h"
#include "Pins/TexturePin.h"

#include "Properties/GraphTextureProperty.h"

#include "imgui.h"
#include "MkCanvasScopedNode.h"

// -- ClientTextureNodeConfig -----
configuru::Config DepthTextureSourceNodeConfig::writeToJSON()
{
	configuru::Config pt= NodeConfig::writeToJSON();

	pt["texture_source_depth_type"]= k_textureSourceDepthTypeStrings[(int)textureSourceColorType];
	pt["texture_source_id"]= textureVideoSourceId;
	pt["vertical_flip"]= bVerticalFlip;

	return pt;
}

void DepthTextureSourceNodeConfig::readFromJSON(const configuru::Config& pt)
{
	NodeConfig::readFromJSON(pt);

	const std::string clientTextureTypeString= pt.get_or<std::string>(
		"texture_source_depth_type", k_textureSourceDepthTypeStrings[(int)eTextureSourceDepthType::depthPackRGBA]);
	textureSourceColorType=
		StringUtils::FindEnumValue<eTextureSourceDepthType>(clientTextureTypeString, k_textureSourceDepthTypeStrings);
	bVerticalFlip= pt.get_or<bool>("vertical_flip", false);

	textureVideoSourceId= pt.get_or<int>("texture_source_id", INVALID_MIKAN_ID);
}

// -- ClientTextureNode -----
bool DepthTextureSourceNode::loadFromConfig(NodeConfigConstPtr nodeConfig)
{
	if (Node::loadFromConfig(nodeConfig))
	{
		auto clientTextureNodeConfig= std::static_pointer_cast<const DepthTextureSourceNodeConfig>(nodeConfig);

		m_clientTextureType= clientTextureNodeConfig->textureSourceColorType;
		m_bVerticalFlip= clientTextureNodeConfig->bVerticalFlip;

		// Get the client video source component corresponding to the saved video source id
		ProjectManagerPtr projectManager= getOwnerProject();
		if (projectManager)
		{
			m_textureSourceComponent= TextureSourceQueries::getTextureSourceById(
				projectManager, clientTextureNodeConfig->textureVideoSourceId);
		}

		return true;
	}

	return false;
}

void DepthTextureSourceNode::saveToConfig(NodeConfigPtr nodeConfig) const
{
	auto textureSourceNodeConfig= std::static_pointer_cast<DepthTextureSourceNodeConfig>(nodeConfig);
	TextureSourceComponentPtr textureSourceComponent= getTextureSourceComponent();

	textureSourceNodeConfig->textureSourceColorType= m_clientTextureType;
	textureSourceNodeConfig->bVerticalFlip= m_bVerticalFlip;
	textureSourceNodeConfig->textureVideoSourceId=
		textureSourceComponent ? textureSourceComponent->getTextureSourceId() : INVALID_MIKAN_ID;

	Node::saveToConfig(nodeConfig);
}

TextureSourceComponentPtr DepthTextureSourceNode::getTextureSourceComponent() const
{
	return m_textureSourceComponent.lock();
}

IMkTexturePtr DepthTextureSourceNode::getTextureResource() const
{
	return m_linearDepthFrameBuffer ? m_linearDepthFrameBuffer->getDepthTexture() : IMkTexturePtr();
}

bool DepthTextureSourceNode::evaluateNode(NodeEvaluator& evaluator)
{
	// Since the frame compositor can change the client source texture can change out from under us
	// it's safest to just refresh the output texture pin every frame
	auto outputPin= getFirstPinOfType<TexturePin>(eNodePinDirection::OUTPUT);

	// Update the linear depth texture from the client depth texture
	IMkTexturePtr clientDepthTexture= getDepthSourceTexture();
	if (clientDepthTexture)
	{
		updateLinearDepthFrameBuffer(evaluator, clientDepthTexture);
	}

	// Return the linear depth texture from the frame buffer
	IMkTexturePtr linearDepthTexture= getTextureResource();
	outputPin->setValue(linearDepthTexture);

	return true;
}

IMkTexturePtr DepthTextureSourceNode::getDepthSourceTexture() const
{
	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	CameraComponentPtr boundCameraComponent= compositorGraph->getBoundCameraComponent();
	CompositorComponentPtr boundCompositorComponent= compositorGraph->getBoundCompositorComponent();
	TextureSourceComponentPtr textureSourceComponent= getTextureSourceComponent();

	if (boundCameraComponent != nullptr && boundCompositorComponent != nullptr && textureSourceComponent != nullptr)
	{
		const int64_t pendingFrameIndex= boundCompositorComponent->getPendingCompositedFrameIndex();
		IMkTexturePtr clientTexture= textureSourceComponent->getClientDepthSourceTexture(
			boundCameraComponent->getCameraId(), m_clientTextureType, pendingFrameIndex);

		// If the client texture is not available, return a white texture
		// (white RGBA decodes to linearDepth ≈ 1.004, which is always behind videoZ ≤ 0.9998)
		if (clientTexture)
		{
			return clientTexture;
		}
		else
		{
			auto* textureCache= getOwnerGraph()->getOwnerWindow()->getGraphicsContext()->getTextureCache();

			if (m_clientTextureType == eTextureSourceDepthType::depthPackRGBA)
			{
				return textureCache->tryGetTextureByName(INTERNAL_TEXTURE_WHITE_RGBA);
			}
		}
	}

	return IMkTexturePtr();
}

void DepthTextureSourceNode::updateLinearDepthFrameBuffer(NodeEvaluator& evaluator, IMkTexturePtr clientTexture)
{
	assert(m_clientTextureType == eTextureSourceDepthType::depthPackRGBA);

	if (m_linearDepthFrameBuffer == nullptr)
	{
		m_linearDepthFrameBuffer= createMkFrameBuffer("DepthTextureSourceNode");
		m_linearDepthFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR_AND_DEPTH);
	}

	// Update render target size
	m_linearDepthFrameBuffer->setSize(clientTexture->getTextureWidth(), clientTexture->getTextureHeight());
	if (!m_linearDepthFrameBuffer->isValid())
	{
		m_linearDepthFrameBuffer->createResources();
	}

	IMkGraphicsContext* graphicsContext= evaluator.getCurrentGraphicsContext();
	MkScopedObjectBinding depthFramebufferBinding(graphicsContext->getMkStateStack().getCurrentState(),
												  "Depth Texture Framebuffer Scope", m_linearDepthFrameBuffer);
	if (depthFramebufferBinding)
	{
		IMkState* glState= depthFramebufferBinding.getMkState();
		MkMaterialConstPtr depthUnpackMaterial=
			graphicsContext->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_UNPACK_RGBA_DEPTH_TEXTURE);

		if (depthUnpackMaterial != nullptr)
		{
			m_depthMaterialInstance= createMkMaterialInstance(depthUnpackMaterial);
		}

		if (m_depthMaterialInstance)
		{
			evaluateDepthTexture(glState, clientTexture);
		}
	}
}

void DepthTextureSourceNode::evaluateDepthTexture(IMkState* glState, IMkTexturePtr depthTexture)
{
	assert(depthTexture);
	assert(m_depthMaterialInstance);

	MkMaterialConstPtr material= m_depthMaterialInstance->getMaterial();
	if (auto materialBinding= material->bindMaterial())
	{
		// Bind the depth texture
		m_depthMaterialInstance->setTextureBySemantic(eUniformSemantic::rgbaTexture, depthTexture);

		if (auto materialInstanceBinding= m_depthMaterialInstance->bindMaterialInstance(materialBinding))
		{
			auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());

			if (m_bVerticalFlip)
			{
				compositorGraph->getLayerVFlippedMesh()->drawElements();
			}
			else
			{
				compositorGraph->getLayerMesh()->drawElements();
			}
		}
	}
}

ImVec4 DepthTextureSourceNode::editorGetHeaderColor() const
{
	return ImVec4(150.f / 255.f, 130.f / 255.f, 110.f / 255.f, 225.f / 255.f);
}

std::string DepthTextureSourceNode::editorGetTitle() const
{
	if (!isDefaultNode())
	{
		TextureSourceComponentPtr textureSource= getTextureSourceComponent();
		std::string sourceId= textureSource ? textureSource->getName() : "<NONE>";

		return StringUtils::stringify("Depth Source", sourceId);
	}

	return "Depth Source";
}

void DepthTextureSourceNode::editorRenderNode(const NodeEditorState& editorState)
{
	MkCanvasScopedNode scopedNode(m_id, editorGetHeaderColor());

	// Title
	editorRenderTitle(scopedNode);

	// Texture Preview (color texture of the frame buffer)
	ImGui::Dummy(ImVec2(1.0f, 0.5f));
	IMkTexturePtr colorTexture=
		m_linearDepthFrameBuffer ? m_linearDepthFrameBuffer->getColorTexture() : IMkTexturePtr();
	MkGui::drawImage(colorTexture, 100.f, 100.f);
	ImGui::SameLine();

	// Outputs
	editorRenderOutputPins(editorState);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));
}

void DepthTextureSourceNode::editorRenderPropertySheet(const NodeEditorState& editorState)
{
	if (MkGui::drawPropertySheetHeader(editorState.styleManager->getStyle("node_editor_panel_header"),
									   locText("nodes.clientTextureHeader")))
	{
		MkGuiStyleConstPtr propertyStyle= editorState.styleManager->getStyle("node_editor_property_value");

		// Texture Type
		int iTextureType= (int)m_clientTextureType;
		if (MkGui::drawSimpleComboBoxProperty(propertyStyle, "textureSourceColorType", locText("nodes.type"),
											  "depthPackRGBA\0", iTextureType))
		{
			m_clientTextureType= (eTextureSourceDepthType)iTextureType;
		}

		// Texture Type
		ProjectManagerPtr projectManager= getOwnerProject();
		TextureSourceListDataSource dataSource(projectManager);
		if (dataSource.getEntryCount() > 0)
		{
			TextureSourceComponentPtr videoSourceComponent= getTextureSourceComponent();

			int selectedIndex= dataSource.getEntryIndex(videoSourceComponent);
			if (MkGui::drawComboBoxProperty(propertyStyle, "textureSourceIndex", locText("nodes.source"), &dataSource,
											selectedIndex))
			{
				m_textureSourceComponent= dataSource.getEntryAtIndex(selectedIndex);
			}
		}

		// Vertical Flip
		MkGui::drawCheckBoxProperty(propertyStyle, "drawDepthTextureVerticalFlip", locText("nodes.verticalFlip"),
									m_bVerticalFlip);
	}
}

// -- ClientTextureNode Factory -----
NodePtr DepthTextureSourceNodeFactory::createNode(const NodeEditorState& editorState) const
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
#include "CompositorComponent.h"
#include "IconsForkAwesome.h"
#include "IEditorWindow.h"
#include "IMkFrameBuffer.h"
#include "IMkGraphicsContext.h"
#include "IMkShaderCache.h"
#include "IMkState.h"
#include "IMkTexture.h"
#include "IMkTriangulatedMesh.h"
#include "LocText.h"
#include "Logger.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkScopedObjectBinding.h"
#include "MkStateStack.h"
#include "NodeEditorState.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiStyleManager.h"
#include "StringUtils.h"
#include "VideoTextureNode.h"
#include "VideoSourceComponent.h"

#include "Graphs/CompositorNodeGraph.h"
#include "Graphs/NodeEvaluator.h"
#include "Graphs/NodeGraph.h"

#include "Pins/NodePin.h"
#include "Pins/TexturePin.h"

#include "Properties/GraphTextureProperty.h"

#include "imgui.h"
#include "MkCanvasScopedNode.h"

// -- VideoTextureNodeConfig -----
configuru::Config VideoTextureNodeConfig::writeToJSON()
{
	configuru::Config pt= NodeConfig::writeToJSON();

	pt["video_texture_source"]= k_videoTextureStrings[(int)videoTextureSource];
	pt["transfer_function_override"]= (int)transferFunctionOverride;

	return pt;
}

void VideoTextureNodeConfig::readFromJSON(const configuru::Config& pt)
{
	NodeConfig::readFromJSON(pt);

	const std::string textureSourceString=
		pt.get_or<std::string>("video_texture_source", k_videoTextureStrings[(int)eVideoTextureSource::video_texture]);
	videoTextureSource= StringUtils::FindEnumValue<eVideoTextureSource>(textureSourceString, k_videoTextureStrings);

	transferFunctionOverride=
		(eVideoTransferFunction)pt.get_or<int>("transfer_function_override", (int)eVideoTransferFunction::INVALID);
}

// -- VideoTextureNode -----
bool VideoTextureNode::loadFromConfig(NodeConfigConstPtr nodeConfig)
{
	if (Node::loadFromConfig(nodeConfig))
	{
		auto videoTextureNodeConfig= std::static_pointer_cast<const VideoTextureNodeConfig>(nodeConfig);

		m_videoTextureSource= videoTextureNodeConfig->videoTextureSource;
		m_transferFunctionOverride= videoTextureNodeConfig->transferFunctionOverride;
		return true;
	}

	return false;
}

void VideoTextureNode::saveToConfig(NodeConfigPtr nodeConfig) const
{
	auto videoTextureNodeConfig= std::static_pointer_cast<VideoTextureNodeConfig>(nodeConfig);
	videoTextureNodeConfig->videoTextureSource= m_videoTextureSource;
	videoTextureNodeConfig->transferFunctionOverride= m_transferFunctionOverride;

	Node::saveToConfig(nodeConfig);
}

IMkTexturePtr VideoTextureNode::getTextureResource() const
{
	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	CompositorComponentPtr compositorComponent= compositorGraph->getBoundCompositorComponent();
	if (compositorComponent != nullptr)
	{
		return compositorComponent->getVideoSourceTexture(m_videoTextureSource);
	}

	return IMkTexturePtr();
}

IMkTexturePtr VideoTextureNode::getPreviewTextureResource() const
{
	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	CompositorComponentPtr compositorComponent= compositorGraph->getBoundCompositorComponent();
	if (compositorComponent != nullptr)
	{
		return compositorComponent->getVideoPreviewTexture(m_videoTextureSource);
	}

	return IMkTexturePtr();
}

bool VideoTextureNode::evaluateNode(NodeEvaluator& evaluator)
{
	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	CompositorComponentPtr compositorComponent= compositorGraph->getBoundCompositorComponent();

	// Since the frame compositor can change the video source texture can change out from under us
	// it's safest to just refresh the output texture pin every frame
	auto outputPin= getFirstPinOfType<TexturePin>(eNodePinDirection::OUTPUT);
	IMkTexturePtr textureResource= getTextureResource();

	// The raw video texture is in a non-linear (gamma/sRGB) color space. Convert it to linear
	// color space so downstream compositing math (multiply/blend) is performed correctly.
	IMkTexturePtr outputTexture= textureResource;
	if (m_videoTextureSource == eVideoTextureSource::video_texture && textureResource && textureResource->getIsValid())
	{
		IMkTexturePtr linearTexture= linearizeVideoTexture(evaluator, textureResource);
		if (linearTexture)
		{
			outputTexture= linearTexture;
		}
	}

	outputPin->setValue(outputTexture);
	outputPin->editorSetShowPinName(false);

	if (!compositorComponent->getIsRunning())
	{
		evaluator.addError(NodeEvaluationError(
			eNodeEvaluationErrorCode::missingInput,
			StringUtils::stringify("Compositor ", compositorComponent->getName(), " is not active"), this));
	}
	else if (!textureResource || !textureResource->getIsValid())
	{
		evaluator.addError(NodeEvaluationError(
			eNodeEvaluationErrorCode::missingInput,
			StringUtils::stringify("Compositor ", compositorComponent->getName(), " missing output"), this));
	}

	return true;
}

eVideoTransferFunction VideoTextureNode::getColorTransferFunction() const
{
	// Manual override takes precedence when set (anything other than INVALID = "Auto")
	if (m_transferFunctionOverride != eVideoTransferFunction::INVALID)
		return m_transferFunctionOverride;

	// Otherwise read the transfer function reported by the bound video source's colorimetry
	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	VideoSourceComponentPtr videoSource= compositorGraph->getBoundVideoSourceComponent();
	if (videoSource)
	{
		VideoColorimetry colorimetry;
		if (videoSource->getVideoColorimetry(colorimetry))
		{
			return colorimetry.transferFunction;
		}
	}

	// Unknown - the linearize shader falls back to an sRGB decode for INVALID
	return eVideoTransferFunction::INVALID;
}

IMkTexturePtr VideoTextureNode::linearizeVideoTexture(NodeEvaluator& evaluator, IMkTexturePtr sourceTexture)
{
	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	IEditorWindow* ownerWindow= compositorGraph->getOwnerWindow();
	if (ownerWindow == nullptr || !sourceTexture)
		return IMkTexturePtr();

	// Lazily fetch the linearize material instance
	if (!m_linearizeMaterialInstance)
	{
		MkMaterialConstPtr material=
			ownerWindow->getGraphicsContext()->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_PT_LINEARIZE_RGB);
		if (!material)
			return IMkTexturePtr();

		m_linearizeMaterialInstance= createMkMaterialInstance(material);
	}

	// Lazily create / resize the 16-bit linear render target to match the source texture
	const int frameWidth= (int)sourceTexture->getTextureWidth();
	const int frameHeight= (int)sourceTexture->getTextureHeight();
	if (!m_linearFrameBuffer)
	{
		m_linearFrameBuffer= createMkFrameBuffer("Video Texture Linearize Frame Buffer");
		m_linearFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
		m_linearFrameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::RGBA16);
	}
	m_linearFrameBuffer->setSize(frameWidth, frameHeight);
	if (!m_linearFrameBuffer->isValid())
	{
		if (!m_linearFrameBuffer->createResources())
			return IMkTexturePtr();
	}

	const eVideoTransferFunction transferFunction= getColorTransferFunction();

	// Render the source texture through the linearize material into the linear frame buffer
	IMkGraphicsContext* graphicsContext= evaluator.getCurrentGraphicsContext();
	MkScopedObjectBinding linearFramebufferBinding(graphicsContext->getMkStateStack().getCurrentState(),
												   "Video Texture Linearize Scope", m_linearFrameBuffer);
	if (linearFramebufferBinding)
	{
		linearFramebufferBinding.getMkState()->disableFlag(eMkStateFlagType::depthTest);

		MkMaterialConstPtr material= m_linearizeMaterialInstance->getMaterial();
		MkScopedMaterialBinding materialBinding= material->bindMaterial();
		if (materialBinding)
		{
			m_linearizeMaterialInstance->setTextureBySemantic(eUniformSemantic::rgbTexture, sourceTexture);
			m_linearizeMaterialInstance->setFloatBySemantic(eUniformSemantic::floatConstant0,
															(float)(int)transferFunction);

			MkScopedMaterialInstanceBinding materialInstanceBinding=
				m_linearizeMaterialInstance->bindMaterialInstance(materialBinding);
			if (materialInstanceBinding)
			{
				compositorGraph->getLayerMesh()->drawElements();
			}
		}
	}

	return m_linearFrameBuffer->getColorTexture();
}

ImVec4 VideoTextureNode::editorGetHeaderColor() const
{
	return ImVec4(150.f / 255.f, 130.f / 255.f, 110.f / 255.f, 225.f / 255.f);
}

void VideoTextureNode::editorRenderNode(const NodeEditorState& editorState)
{
	MkCanvasScopedNode scopedNode(m_id, editorGetHeaderColor());

	// Title
	editorRenderTitle(scopedNode);

	// Texture
	ImGui::Dummy(ImVec2(1.0f, 0.5f));
	IMkTexturePtr textureResource= getPreviewTextureResource();
	MkGui::drawImage(textureResource, 100.f, 100.f);
	ImGui::SameLine();

	// Outputs
	editorRenderOutputPins(editorState);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));
}

void VideoTextureNode::editorRenderPropertySheet(const NodeEditorState& editorState)
{
	if (MkGui::drawPropertySheetHeader(editorState.styleManager->getStyle("node_editor_panel_header"),
									   locText("nodes.videoTextureHeader")))
	{
		MkGuiStyleConstPtr propertyStyle= editorState.styleManager->getStyle("node_editor_property_value");

		const std::string videoSourceOptions=
			std::string(locText("nodes.videoSourceVideo")) + '\0' + locText("nodes.videoSourceDistortion") + '\0';

		// Texture Source
		int iTextureSource= (int)m_videoTextureSource;
		if (MkGui::drawSimpleComboBoxProperty(propertyStyle, "videoTextureNodeSource", locText("nodes.source"),
											  videoSourceOptions.c_str(), iTextureSource))
		{
			m_videoTextureSource= (eVideoTextureSource)iTextureSource;
		}

		// Transfer function override (Auto reads the value from the video source colorimetry)
		static const eVideoTransferFunction k_tfOptions[]= {
			eVideoTransferFunction::INVALID,   // Auto
			eVideoTransferFunction::Gamma_1_0, // Linear
			eVideoTransferFunction::Gamma_1_8, eVideoTransferFunction::Gamma_2_0, eVideoTransferFunction::Gamma_2_2,
			eVideoTransferFunction::Gamma_2_6, eVideoTransferFunction::Gamma_2_8, eVideoTransferFunction::BT709,
			eVideoTransferFunction::SRGB,
		};
		const std::string tfComboOptions= std::string(locText("nodes.auto")) + '\0' + locText("nodes.transferLinear")
										  + '\0' + locText("nodes.transferGamma18") + '\0'
										  + locText("nodes.transferGamma20") + '\0' + locText("nodes.transferGamma22")
										  + '\0' + locText("nodes.transferGamma26") + '\0'
										  + locText("nodes.transferGamma28") + '\0' + locText("nodes.transferBt709")
										  + '\0' + locText("nodes.transferSrgb") + '\0';
		const int k_tfOptionCount= sizeof(k_tfOptions) / sizeof(k_tfOptions[0]);

		int iTransferFunction= 0;
		for (int i= 0; i < k_tfOptionCount; ++i)
		{
			if (k_tfOptions[i] == m_transferFunctionOverride)
			{
				iTransferFunction= i;
				break;
			}
		}

		if (MkGui::drawSimpleComboBoxProperty(propertyStyle, "videoTextureNodeTransferFunction",
											  locText("nodes.transferFunc"), tfComboOptions.c_str(), iTransferFunction))
		{
			m_transferFunctionOverride= k_tfOptions[iTransferFunction];
		}
	}
}

// -- VideoTextureNode Factory -----
NodePtr VideoTextureNodeFactory::createNode(const NodeEditorState& editorState) const
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

const char* VideoTextureNode::editorGetHeaderIcon() const { return ICON_FK_VIDEO_CAMERA; }

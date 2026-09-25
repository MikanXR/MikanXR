#include "ColorTextureSourceNode.h"
#include "CompositorComponent.h"
#include "CameraComponent.h"
#include "MkScopedObjectBinding.h"
#include "IEditorWindow.h"
#include "IMkGraphicsContext.h"
#include "IMkFrameBuffer.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MikanShaderCache.h"
#include "MikanCoreTypes.h"
#include "MkStateStack.h"
#include "IMkTexture.h"
#include "MikanTextureCache.h"
#include "IMkTriangulatedMesh.h"
#include "LocText.h"
#include "Logger.h"
#include "NodeEditorState.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiStyleManager.h"
#include "ProjectManager.h"
#include "StringUtils.h"
#include "TextureSourceQueries.h"
#include "TextureSourceComponent.h"
#include "VideoSourceComponent.h"

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
configuru::Config ColorTextureSourceNodeConfig::writeToJSON()
{
	configuru::Config pt= NodeConfig::writeToJSON();

	pt["texture_source_color_type"]= k_textureSourceColorTypeStrings[(int)textureSourceColorType];
	pt["fallback_mode"]= k_colorTextureFallbackModeStrings[(int)fallbackMode];
	pt["resolve_alpha_mode"]= k_colorTextureResolveAlphaModeStrings[(int)resolveAlphaMode];
	pt["texture_source_id"]= textureSourceId;
	pt["vertical_flip"]= bVerticalFlip;

	return pt;
}

void ColorTextureSourceNodeConfig::readFromJSON(const configuru::Config& pt)
{
	NodeConfig::readFromJSON(pt);

	const std::string clientTextureTypeString= pt.get_or<std::string>(
		"texture_source_color_type", k_textureSourceColorTypeStrings[(int)eTextureSourceColorType::colorRGB]);
	textureSourceColorType=
		StringUtils::FindEnumValue<eTextureSourceColorType>(clientTextureTypeString, k_textureSourceColorTypeStrings);

	const std::string fallbackModeString= pt.get_or<std::string>(
		"fallback_mode", k_colorTextureFallbackModeStrings[(int)eColorTextureFallbackMode::autoByType]);
	fallbackMode=
		StringUtils::FindEnumValue<eColorTextureFallbackMode>(fallbackModeString, k_colorTextureFallbackModeStrings);
	// Legacy graphs (and any unrecognized value) fall back to the backward-compatible auto behavior
	if (fallbackMode == eColorTextureFallbackMode::INVALID)
	{
		fallbackMode= eColorTextureFallbackMode::autoByType;
	}

	const std::string resolveAlphaModeString= pt.get_or<std::string>(
		"resolve_alpha_mode", k_colorTextureResolveAlphaModeStrings[(int)eColorTextureResolveAlphaMode::none]);
	resolveAlphaMode= StringUtils::FindEnumValue<eColorTextureResolveAlphaMode>(resolveAlphaModeString,
																				k_colorTextureResolveAlphaModeStrings);
	// A graph saved before the option existed got a plain average, which is what none does
	if (resolveAlphaMode == eColorTextureResolveAlphaMode::INVALID)
	{
		resolveAlphaMode= eColorTextureResolveAlphaMode::none;
	}

	bVerticalFlip= pt.get_or<bool>("vertical_flip", false);

	textureSourceId= pt.get_or<int>("texture_source_id", INVALID_MIKAN_ID);
}

// -- ClientTextureNode -----
bool ColorTextureSourceNode::loadFromConfig(NodeConfigConstPtr nodeConfig)
{
	if (Node::loadFromConfig(nodeConfig))
	{
		auto textureSourceNodeConfig= std::static_pointer_cast<const ColorTextureSourceNodeConfig>(nodeConfig);

		m_clientTextureType= textureSourceNodeConfig->textureSourceColorType;
		m_fallbackMode= textureSourceNodeConfig->fallbackMode;
		m_resolveAlphaMode= textureSourceNodeConfig->resolveAlphaMode;
		m_bVerticalFlip= textureSourceNodeConfig->bVerticalFlip;

		// Get the client video source component corresponding to the saved video source id
		ProjectManagerPtr projectManager= getOwnerProject();
		if (projectManager)
		{
			m_textureSourceComponent=
				TextureSourceQueries::getTextureSourceById(projectManager, textureSourceNodeConfig->textureSourceId);
		}

		return true;
	}

	return false;
}

void ColorTextureSourceNode::saveToConfig(NodeConfigPtr nodeConfig) const
{
	auto textureSourceNodeConfig= std::static_pointer_cast<ColorTextureSourceNodeConfig>(nodeConfig);
	TextureSourceComponentPtr textureSourceComponent= getTextureSourceComponent();

	textureSourceNodeConfig->textureSourceColorType= m_clientTextureType;
	textureSourceNodeConfig->fallbackMode= m_fallbackMode;
	textureSourceNodeConfig->resolveAlphaMode= m_resolveAlphaMode;
	textureSourceNodeConfig->bVerticalFlip= m_bVerticalFlip;
	textureSourceNodeConfig->textureSourceId=
		textureSourceComponent ? textureSourceComponent->getTextureSourceId() : INVALID_MIKAN_ID;

	Node::saveToConfig(nodeConfig);
}

TextureSourceComponentPtr ColorTextureSourceNode::getTextureSourceComponent() const
{
	return m_textureSourceComponent.lock();
}

IMkTexturePtr ColorTextureSourceNode::getTextureResource() const
{
	return m_colorFrameBuffer && m_colorFrameBuffer->isValid() ? m_colorFrameBuffer->getColorTexture()
															   : getColorSourceTexture();
}

bool ColorTextureSourceNode::evaluateNode(NodeEvaluator& evaluator)
{
	// Since the frame compositor can change the client source texture can change out from under us
	// it's safest to just refresh the output texture pin every frame
	auto outputPin= getFirstPinOfType<TexturePin>(eNodePinDirection::OUTPUT);

	// Flip the Y axis and/or resolve a client texture rendered above the video resolution.
	// Both are the same fullscreen pass, so they cost one draw together.
	updateColorFrameBuffer(evaluator, getColorSourceTexture());

	// Render the output color texture to the output pin
	outputPin->setValue(getTextureResource());

	return true;
}

IMkTexturePtr ColorTextureSourceNode::getColorSourceTexture() const
{
	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	CameraComponentPtr boundCameraComponent= compositorGraph->getBoundCameraComponent();
	CompositorComponentPtr boundCompositorComponent= compositorGraph->getBoundCompositorComponent();
	TextureSourceComponentPtr textureSourceComponent= getTextureSourceComponent();

	if (boundCompositorComponent && boundCameraComponent && textureSourceComponent)
	{
		const int64_t pendingFrameIndex= boundCompositorComponent->getPendingCompositedFrameIndex();
		IMkTexturePtr clientTexture= textureSourceComponent->getClientColorSourceTexture(
			boundCameraComponent->getCameraId(), m_clientTextureType, pendingFrameIndex);

		// If the client texture is not available, return a black texture
		if (clientTexture)
		{
			return clientTexture;
		}
		else
		{
			auto* textureCache= getOwnerGraph()->getOwnerWindow()->getGraphicsContext()->getTextureCache();

			// An explicit per-node fallback override wins over the color-type default. This is how
			// the graph author picks the identity texture appropriate for the downstream material
			// (e.g. opaque black for an inverted-alpha layer, white for a multiply layer), which
			// can't be inferred here.
			switch (m_fallbackMode)
			{
			case eColorTextureFallbackMode::transparentBlack:
				return textureCache->tryGetTextureByName(INTERNAL_TEXTURE_BLACK_RGBA_TRANSPARENT);
			case eColorTextureFallbackMode::opaqueBlack:
				return textureCache->tryGetTextureByName(INTERNAL_TEXTURE_BLACK_RGBA);
			case eColorTextureFallbackMode::opaqueWhite:
				return textureCache->tryGetTextureByName(INTERNAL_TEXTURE_WHITE_RGBA);
			case eColorTextureFallbackMode::autoByType:
			default:
				break;
			}

			// Auto: derive the identity from how this color type is normally composited.
			switch (m_clientTextureType)
			{
			case eTextureSourceColorType::colorRGBA:
				return textureCache->tryGetTextureByName(INTERNAL_TEXTURE_BLACK_RGBA);
			// Shadow buffers composite multiplicatively, so the "no data" identity is white
			// (white * background == background, i.e. no shadow) rather than transparent black.
			case eTextureSourceColorType::shadowRGBA:
				return textureCache->tryGetTextureByName(INTERNAL_TEXTURE_WHITE_RGBA);
			case eTextureSourceColorType::shadowRGB:
				return textureCache->tryGetTextureByName(INTERNAL_TEXTURE_WHITE_RGB);
			case eTextureSourceColorType::colorRGB:
			default:
				return textureCache->tryGetTextureByName(INTERNAL_TEXTURE_BLACK_RGB);
			}
		}
	}

	return IMkTexturePtr();
}

bool ColorTextureSourceNode::getResolveTargetSize(int& outWidth, int& outHeight) const
{
	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	VideoSourceComponentPtr videoSource= compositorGraph->getBoundVideoSourceComponent();

	return videoSource && videoSource->getVideoPixelDimensions(outWidth, outHeight);
}

void ColorTextureSourceNode::updateColorFrameBuffer(NodeEvaluator& evaluator, IMkTexturePtr clientTexture)
{
	IMkGraphicsContext* graphicsContext= evaluator.getCurrentGraphicsContext();

	const bool bIsRGBAVariant= m_clientTextureType == eTextureSourceColorType::colorRGBA
							   || m_clientTextureType == eTextureSourceColorType::shadowRGBA;

	assert(m_clientTextureType == eTextureSourceColorType::colorRGBA
		   || m_clientTextureType == eTextureSourceColorType::colorRGB
		   || m_clientTextureType == eTextureSourceColorType::shadowRGBA
		   || m_clientTextureType == eTextureSourceColorType::shadowRGB);

	// Decide what this frame's pass has to do, and at what size, before touching any resources.
	// A texture already at or below the size the composite consumes it at needs no resolve:
	// magnifying it is what the layer quad's bilinear filter is for.
	int targetWidth= 0, targetHeight= 0;
	bool bWantsResolve= false;
	if (clientTexture && getResolveTargetSize(targetWidth, targetHeight) && targetWidth > 0 && targetHeight > 0)
	{
		bWantsResolve= (int)clientTexture->getTextureWidth() > targetWidth
					   || (int)clientTexture->getTextureHeight() > targetHeight;
	}

	const bool bWantsPass= clientTexture != nullptr && (m_bVerticalFlip || bWantsResolve);
	if (!bWantsPass)
	{
		// Nothing to do, so release the framebuffer and let the source texture pass straight through
		if (m_colorFrameBuffer != nullptr)
		{
			m_colorFrameBuffer->disposeResources();
			m_colorFrameBuffer= nullptr;
			m_colorMaterialInstance= nullptr;
		}
		m_bIsResolving= false;
		return;
	}

	// A client color buffer can be half-float, so an 8-bit target would clamp away the
	// HDR range it was published with
	const bool bIsFloatSource= clientTexture->getTextureFormat() == MK_RGBA16F;
	const IMkFrameBuffer::eColorFormat colorFormat=
		bIsFloatSource ? IMkFrameBuffer::eColorFormat::RGBA16F
					   : (bIsRGBAVariant ? IMkFrameBuffer::eColorFormat::RGBA : IMkFrameBuffer::eColorFormat::RGB);

	if (m_colorFrameBuffer == nullptr)
	{
		m_colorFrameBuffer= createMkFrameBuffer("ColorTextureSourceNode");
		m_colorFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	}
	// setColorFormat invalidates the frame buffer when the format actually changes
	m_colorFrameBuffer->setColorFormat(colorFormat);

	// Resolving renders at the size the composite consumes, flipping alone keeps the source size
	if (bWantsResolve)
	{
		m_colorFrameBuffer->setSize(targetWidth, targetHeight);
	}
	else
	{
		m_colorFrameBuffer->setSize(clientTexture->getTextureWidth(), clientTexture->getTextureHeight());
	}

	// The two jobs use different materials, so a switch between them needs a new instance
	// even when the frame buffer itself is still valid
	if (m_bIsResolving != bWantsResolve)
	{
		m_bIsResolving= bWantsResolve;
		m_colorMaterialInstance= nullptr;
	}

	if (!m_colorFrameBuffer->isValid())
	{
		m_colorFrameBuffer->createResources();
		m_colorMaterialInstance= nullptr;
	}

	if (m_colorMaterialInstance == nullptr)
	{
		std::string colorMaterialName;
		if (bWantsResolve)
		{
			colorMaterialName= bIsRGBAVariant ? INTERNAL_MATERIAL_PT_RESOLVE_DOWNSAMPLE_RGBA
											  : INTERNAL_MATERIAL_PT_RESOLVE_DOWNSAMPLE_RGB;
		}
		else
		{
			colorMaterialName= bIsRGBAVariant ? INTERNAL_MATERIAL_PT_FULLSCREEN_RGBA_TEXTURE
											  : INTERNAL_MATERIAL_PT_FULLSCREEN_RGB_TEXTURE;
		}

		MkMaterialConstPtr colorMaterial= graphicsContext->getShaderCache()->getMaterialByName(colorMaterialName);
		if (colorMaterial != nullptr)
		{
			m_colorMaterialInstance= createMkMaterialInstance(colorMaterial);
		}
		else
		{
			MIKAN_LOG_ERROR("updateColorFrameBuffer") << "Failed to get color material " << colorMaterialName;
		}
	}

	if (m_colorMaterialInstance)
	{
		MkScopedObjectBinding colorFramebufferBinding(graphicsContext->getMkStateStack().getCurrentState(),
													  "Color Texture Framebuffer Scope", m_colorFrameBuffer);
		if (colorFramebufferBinding)
		{
			IMkState* glState= colorFramebufferBinding.getMkState();

			evaluateColorTexture(glState, clientTexture);
		}
	}
}

void ColorTextureSourceNode::evaluateColorTexture(IMkState* glState, IMkTexturePtr colorTexture)
{
	assert(colorTexture);
	assert(m_colorMaterialInstance);

	MkMaterialConstPtr material= m_colorMaterialInstance->getMaterial();
	if (auto materialBinding= material->bindMaterial())
	{
		// Bind the color texture
		const bool bIsRGBAVariant= m_clientTextureType == eTextureSourceColorType::colorRGBA
								   || m_clientTextureType == eTextureSourceColorType::shadowRGBA;
		if (bIsRGBAVariant)
		{
			m_colorMaterialInstance->setTextureBySemantic(eUniformSemantic::rgbaTexture, colorTexture);
		}
		else
		{
			m_colorMaterialInstance->setTextureBySemantic(eUniformSemantic::rgbTexture, colorTexture);
		}

		if (m_bIsResolving)
		{
			const float sourceWidth= (float)colorTexture->getTextureWidth();
			const float sourceHeight= (float)colorTexture->getTextureHeight();

			// Source texels one destination pixel covers, which is what sizes the filter kernel
			m_colorMaterialInstance->setVec2BySemantic(eUniformSemantic::screenSize,
													   glm::vec2(sourceWidth, sourceHeight));
			m_colorMaterialInstance->setFloatBySemantic(eUniformSemantic::floatConstant0,
														sourceWidth / (float)m_colorFrameBuffer->getWidth());
			m_colorMaterialInstance->setFloatBySemantic(eUniformSemantic::floatConstant1,
														sourceHeight / (float)m_colorFrameBuffer->getHeight());

			if (bIsRGBAVariant)
			{
				m_colorMaterialInstance->setFloatBySemantic(eUniformSemantic::floatConstant2,
															(float)(int)m_resolveAlphaMode);
			}
		}

		// Draw the color texture
		if (auto materialInstanceBinding= m_colorMaterialInstance->bindMaterialInstance(materialBinding))
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

ImVec4 ColorTextureSourceNode::editorGetHeaderColor() const
{
	return ImVec4(150.f / 255.f, 130.f / 255.f, 110.f / 255.f, 225.f / 255.f);
}

std::string ColorTextureSourceNode::editorGetTitle() const
{
	if (!isDefaultNode())
	{
		TextureSourceComponentPtr textureSource= getTextureSourceComponent();
		std::string sourceId= textureSource ? textureSource->getName() : "<None>";

		return StringUtils::stringify("Color Source ", sourceId);
	}

	return "Color Source";
}

void ColorTextureSourceNode::editorRenderNode(const NodeEditorState& editorState)
{
	MkCanvasScopedNode scopedNode(m_id, editorGetHeaderColor());

	// Title
	editorRenderTitle(scopedNode);

	// Texture Preview
	ImGui::Dummy(ImVec2(1.0f, 0.5f));
	IMkTexturePtr textureResource= getTextureResource();
	MkGui::drawImage(textureResource, 100.f, 100.f);
	ImGui::SameLine();

	// Outputs
	editorRenderOutputPins(editorState);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));
}

void ColorTextureSourceNode::editorRenderPropertySheet(const NodeEditorState& editorState)
{
	if (MkGui::drawPropertySheetHeader(editorState.styleManager->getStyle("node_editor_panel_header"),
									   locText("nodes.clientTextureHeader")))
	{
		MkGuiStyleConstPtr propertyStyle= editorState.styleManager->getStyle("node_editor_property_value");

		// Texture Type
		int iTextureType= (int)m_clientTextureType;
		if (MkGui::drawSimpleComboBoxProperty(propertyStyle, "textureSourceColorType", locText("nodes.type"),
											  "colorRGB\0colorRGBA\0shadowRGB\0shadowRGBA\0", iTextureType))
		{
			m_clientTextureType= (eTextureSourceColorType)iTextureType;
		}

		// No-client fallback texture (identity when no client renderer is attached)
		const std::string fallbackModeItems=
			std::string(locText("nodes.auto")) + '\0' + locText("nodes.fallbackTransparentBlack") + '\0'
			+ locText("nodes.fallbackOpaqueBlack") + '\0' + locText("nodes.fallbackOpaqueWhite") + '\0';
		int iFallbackMode= (int)m_fallbackMode;
		if (MkGui::drawSimpleComboBoxProperty(propertyStyle, "colorTextureFallbackMode",
											  locText("nodes.noClientFallback"), fallbackModeItems.c_str(),
											  iFallbackMode))
		{
			m_fallbackMode= (eColorTextureFallbackMode)iFallbackMode;
		}

		// What the alpha channel means when the client texture is filtered down to the
		// video resolution (see eColorTextureResolveAlphaMode)
		const std::string resolveAlphaModeItems= std::string(locText("nodes.resolveAlphaNone")) + '\0'
												 + locText("nodes.resolveAlphaStraight") + '\0'
												 + locText("nodes.resolveAlphaInverted") + '\0';
		int iResolveAlphaMode= (int)m_resolveAlphaMode;
		if (MkGui::drawSimpleComboBoxProperty(propertyStyle, "colorTextureResolveAlphaMode",
											  locText("nodes.resolveAlphaMode"), resolveAlphaModeItems.c_str(),
											  iResolveAlphaMode))
		{
			m_resolveAlphaMode= (eColorTextureResolveAlphaMode)iResolveAlphaMode;
		}

		// Texture Type
		ProjectManagerPtr projectManager= getOwnerProject();
		TextureSourceListDataSource dataSource(projectManager);
		if (dataSource.getEntryCount() > 0)
		{
			TextureSourceComponentPtr TextureSourceComponent= getTextureSourceComponent();

			int selectedIndex= dataSource.getEntryIndex(TextureSourceComponent);
			if (MkGui::drawComboBoxProperty(propertyStyle, "textureSourceIndex", locText("nodes.source"), &dataSource,
											selectedIndex))
			{
				m_textureSourceComponent= dataSource.getEntryAtIndex(selectedIndex);
			}
		}

		// Vertical Flip
		MkGui::drawCheckBoxProperty(propertyStyle, "drawColorTextureVerticalFlip", locText("nodes.verticalFlip"),
									m_bVerticalFlip);
	}
}

// -- ClientTextureNode Factory -----
NodePtr ColorTextureSourceNodeFactory::createNode(const NodeEditorState& editorState) const
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
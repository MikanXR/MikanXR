#include "DepthOfFieldNode.h"
#include "CameraComponent.h"
#include "CameraMath.h"
#include "DepthOfFieldReference.h"
#include "IconsForkAwesome.h"
#include "IMkFrameBuffer.h"
#include "IMkGraphicsContext.h"
#include "IMkShaderCache.h"
#include "IMkState.h"
#include "IMkTexture.h"
#include "IMkTriangulatedMesh.h"
#include "LocText.h"
#include "MikanVideoSourceTypes.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiStyleManager.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkScopedObjectBinding.h"
#include "MkStateStack.h"
#include "NodeEditorState.h"
#include "StringUtils.h"

#include "Graphs/CompositorNodeGraph.h"
#include "Graphs/NodeEvaluator.h"

#include "Pins/TexturePin.h"

#include "imgui.h"
#include "MkCanvasScopedNode.h"

#include <algorithm>

// -- DepthOfFieldNodeConfig -----
configuru::Config DepthOfFieldNodeConfig::writeToJSON()
{
	configuru::Config pt= NodeConfig::writeToJSON();

	pt["max_radius"]= (int64_t)maxRadius;
	pt["alpha_mode"]= k_colorTextureResolveAlphaModeStrings[(int)alphaMode];

	return pt;
}

void DepthOfFieldNodeConfig::readFromJSON(const configuru::Config& pt)
{
	NodeConfig::readFromJSON(pt);

	// Clamped here so a hand-edited file cannot ask for more taps than the dilation loops over
	maxRadius=
		std::clamp(pt.get_or<int>("max_radius", 16), DepthOfFieldNode::k_minRadius, DepthOfFieldNode::k_maxRadius);

	const std::string alphaModeString= pt.get_or<std::string>(
		"alpha_mode", k_colorTextureResolveAlphaModeStrings[(int)eColorTextureResolveAlphaMode::straight]);
	alphaMode= StringUtils::FindEnumValue<eColorTextureResolveAlphaMode>(alphaModeString,
																		 k_colorTextureResolveAlphaModeStrings);
	if (alphaMode == eColorTextureResolveAlphaMode::INVALID)
	{
		alphaMode= eColorTextureResolveAlphaMode::straight;
	}
}

// -- DepthOfFieldNode -----
DepthOfFieldNode::DepthOfFieldNode()
	: Node()
{
	// Created without resources: the size comes from the input color at evaluation
	m_cocFrameBuffer= createMkFrameBuffer("DepthOfField CoC");
	m_cocFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_cocFrameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::RGBA16F);

	m_dilateHFrameBuffer= createMkFrameBuffer("DepthOfField Dilate H");
	m_dilateHFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_dilateHFrameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::RGBA16F);

	m_dilateVFrameBuffer= createMkFrameBuffer("DepthOfField Dilate V");
	m_dilateVFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_dilateVFrameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::RGBA16F);

	// Linear like the client color the layer materials read
	m_gatherFrameBuffer= createMkFrameBuffer("DepthOfField Output");
	m_gatherFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_gatherFrameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::RGBA16F);
}

DepthOfFieldNode::~DepthOfFieldNode()
{
	m_cocFrameBuffer= nullptr;
	m_dilateHFrameBuffer= nullptr;
	m_dilateVFrameBuffer= nullptr;
	m_gatherFrameBuffer= nullptr;
	m_cocMaterialInstance= nullptr;
	m_dilateHMaterialInstance= nullptr;
	m_dilateVMaterialInstance= nullptr;
	m_gatherMaterialInstance= nullptr;

	m_colorTexturePin= nullptr;
	m_depthTexturePin= nullptr;
	m_outTexturePin= nullptr;

	setOwnerGraph(NodeGraphPtr());
}

bool DepthOfFieldNode::loadFromConfig(NodeConfigConstPtr nodeConfig)
{
	if (Node::loadFromConfig(nodeConfig))
	{
		auto config= std::static_pointer_cast<const DepthOfFieldNodeConfig>(nodeConfig);

		m_maxRadius= config->maxRadius;
		m_alphaMode= config->alphaMode;

		return true;
	}

	return false;
}

void DepthOfFieldNode::saveToConfig(NodeConfigPtr nodeConfig) const
{
	auto config= std::static_pointer_cast<DepthOfFieldNodeConfig>(nodeConfig);

	config->maxRadius= m_maxRadius;
	config->alphaMode= m_alphaMode;

	Node::saveToConfig(nodeConfig);
}

void DepthOfFieldNode::onGraphLoaded(bool success)
{
	if (!success)
		return;

	// Two texture inputs, so they are told apart by name rather than by type
	for (NodePinPtr pin : m_pinsIn)
	{
		TexturePinPtr texturePin= std::dynamic_pointer_cast<TexturePin>(pin);
		if (!texturePin)
			continue;

		if (texturePin->getName() == "colorTexture")
			setColorTexturePin(texturePin);
		else if (texturePin->getName() == "depthTexture")
			setDepthTexturePin(texturePin);
	}

	setOutputTexturePin(getFirstPinOfType<TexturePin>(eNodePinDirection::OUTPUT));
}

void DepthOfFieldNode::setOwnerGraph(NodeGraphPtr newOwnerGraph)
{
	if (newOwnerGraph != m_ownerGraph)
	{
		if (m_ownerGraph)
		{
			m_ownerGraph->OnGraphLoaded-= MakeDelegate(this, &DepthOfFieldNode::onGraphLoaded);
			m_ownerGraph= nullptr;
		}

		if (newOwnerGraph)
		{
			newOwnerGraph->OnGraphLoaded+= MakeDelegate(this, &DepthOfFieldNode::onGraphLoaded);
			m_ownerGraph= newOwnerGraph;
		}
	}
}

bool DepthOfFieldNode::computeLensParams(float textureWidth, glm::vec4& outLensParams) const
{
	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	CameraComponentPtr cameraComponent= compositorGraph ? compositorGraph->getBoundCameraComponent() : nullptr;
	if (!cameraComponent)
		return false;

	CameraDefinitionPtr cameraDefinition= cameraComponent->getCameraDefinition();
	const float apertureDiameterMM= cameraDefinition->getApertureDiameterMM();
	if (apertureDiameterMM <= 0.f)
		return false;

	MikanVideoSourceIntrinsics intrinsics;
	if (!cameraComponent->getApertureIntrinsics(intrinsics)
		|| intrinsics.intrinsics_type != MikanIntrinsicsType::MONO_CAMERA_INTRINSICS)
		return false;

	const MikanMonoIntrinsics& monoIntrinsics= intrinsics.getMonoIntrinsics();
	if (monoIntrinsics.pixel_width <= 0.0)
		return false;

	float focalLengthX, focalLengthY, principalPointX, principalPointY, skew;
	extractCameraIntrinsicMatrixParameters(monoIntrinsics.undistorted_camera_matrix, focalLengthX, focalLengthY,
										   principalPointX, principalPointY, skew);

	// The intrinsics are in video pixels, and the blur is measured in the color's
	const float focalLengthPixels= focalLengthX * textureWidth / (float)monoIntrinsics.pixel_width;

	outLensParams= DepthOfFieldReference::computeLensParams((float)monoIntrinsics.znear, (float)monoIntrinsics.zfar,
															focalLengthPixels, apertureDiameterMM * 0.001f,
															cameraDefinition->getFocusDistance());
	return true;
}

bool DepthOfFieldNode::ensureResources(NodeEvaluator& evaluator, int width, int height)
{
	IMkGraphicsContext* graphicsContext= evaluator.getCurrentGraphicsContext();

	if (!m_cocMaterialInstance || !m_dilateHMaterialInstance || !m_dilateVMaterialInstance || !m_gatherMaterialInstance)
	{
		IMkShaderCache* shaderCache= graphicsContext->getShaderCache();
		MkMaterialConstPtr coc= shaderCache->getMaterialByName(INTERNAL_MATERIAL_PT_DEPTH_OF_FIELD_COC);
		MkMaterialConstPtr dilateH= shaderCache->getMaterialByName(INTERNAL_MATERIAL_PT_DEPTH_MINMAX_H);
		MkMaterialConstPtr dilateV= shaderCache->getMaterialByName(INTERNAL_MATERIAL_PT_DEPTH_MINMAX_V);
		MkMaterialConstPtr gather= shaderCache->getMaterialByName(INTERNAL_MATERIAL_PT_DEPTH_OF_FIELD_GATHER);
		if (!coc || !dilateH || !dilateV || !gather)
		{
			evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::evaluationError,
												   "Missing depth of field materials", this));
			return false;
		}

		m_cocMaterialInstance= createMkMaterialInstance(coc);
		m_dilateHMaterialInstance= createMkMaterialInstance(dilateH);
		m_dilateVMaterialInstance= createMkMaterialInstance(dilateV);
		m_gatherMaterialInstance= createMkMaterialInstance(gather);
	}

	// setSize is a no-op at the same size and invalidates otherwise
	IMkFrameBufferPtr frameBuffers[]= {m_cocFrameBuffer, m_dilateHFrameBuffer, m_dilateVFrameBuffer,
									   m_gatherFrameBuffer};
	for (IMkFrameBufferPtr frameBuffer : frameBuffers)
	{
		frameBuffer->setSize(width, height);
		if (!frameBuffer->isValid() && !frameBuffer->createResources())
		{
			evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::evaluationError,
												   "Unable to create depth of field frame buffer", this));
			return false;
		}
	}

	return true;
}

bool DepthOfFieldNode::renderCoCPass(NodeEvaluator& evaluator, IMkTexturePtr depth, const glm::vec4& lensParams)
{
	IMkGraphicsContext* graphicsContext= evaluator.getCurrentGraphicsContext();
	MkScopedObjectBinding frameBufferBinding(graphicsContext->getMkStateStack().getCurrentState(),
											 "Depth Of Field CoC Scope", m_cocFrameBuffer);
	if (!frameBufferBinding)
		return false;

	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	MkMaterialConstPtr material= m_cocMaterialInstance->getMaterial();
	if (auto materialBinding= material->bindMaterial())
	{
		m_cocMaterialInstance->setTextureBySemantic(eUniformSemantic::depthTexture, depth);
		m_cocMaterialInstance->setVec4ByUniformName("lensParams", lensParams);
		m_cocMaterialInstance->setFloatBySemantic(eUniformSemantic::floatConstant0, (float)m_maxRadius);

		if (auto materialInstanceBinding= m_cocMaterialInstance->bindMaterialInstance(materialBinding))
		{
			compositorGraph->getLayerMesh()->drawElements();
			return true;
		}
	}

	return false;
}

bool DepthOfFieldNode::renderDilatePass(NodeEvaluator& evaluator, IMkFrameBufferPtr target,
										MkMaterialInstancePtr materialInstance, eUniformSemantic inputSemantic,
										IMkTexturePtr input, const glm::vec2& size)
{
	IMkGraphicsContext* graphicsContext= evaluator.getCurrentGraphicsContext();
	MkScopedObjectBinding frameBufferBinding(graphicsContext->getMkStateStack().getCurrentState(),
											 "Depth Of Field Dilate Scope", target);
	if (!frameBufferBinding)
		return false;

	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	MkMaterialConstPtr material= materialInstance->getMaterial();
	if (auto materialBinding= material->bindMaterial())
	{
		// The depth min/max passes, whose max over the window is the largest blur that reaches
		materialInstance->setTextureBySemantic(inputSemantic, input);
		materialInstance->setVec2BySemantic(eUniformSemantic::screenSize, size);
		materialInstance->setFloatBySemantic(eUniformSemantic::floatConstant0, (float)m_maxRadius);

		if (auto materialInstanceBinding= materialInstance->bindMaterialInstance(materialBinding))
		{
			compositorGraph->getLayerMesh()->drawElements();
			return true;
		}
	}

	return false;
}

bool DepthOfFieldNode::renderGatherPass(NodeEvaluator& evaluator, IMkTexturePtr color, IMkTexturePtr depth,
										IMkTexturePtr reach, const glm::vec4& lensParams, const glm::vec2& size)
{
	IMkGraphicsContext* graphicsContext= evaluator.getCurrentGraphicsContext();
	MkScopedObjectBinding frameBufferBinding(graphicsContext->getMkStateStack().getCurrentState(),
											 "Depth Of Field Gather Scope", m_gatherFrameBuffer);
	if (!frameBufferBinding)
		return false;

	// The output replaces the color outright, so nothing a caller set may blend it
	IMkState* mkState= frameBufferBinding.getMkState();
	mkState->disableFlag(eMkStateFlagType::blend);
	mkState->disableFlag(eMkStateFlagType::depthTest);
	mkState->disableFlag(eMkStateFlagType::stencilTest);

	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	MkMaterialConstPtr material= m_gatherMaterialInstance->getMaterial();
	if (auto materialBinding= material->bindMaterial())
	{
		m_gatherMaterialInstance->setTextureBySemantic(eUniformSemantic::rgbaTexture, color);
		m_gatherMaterialInstance->setTextureBySemantic(eUniformSemantic::depthTexture, depth);
		m_gatherMaterialInstance->setTextureBySemantic(eUniformSemantic::textureParam, reach);
		m_gatherMaterialInstance->setVec4ByUniformName("lensParams", lensParams);
		m_gatherMaterialInstance->setVec2BySemantic(eUniformSemantic::screenSize, size);
		m_gatherMaterialInstance->setFloatBySemantic(eUniformSemantic::floatConstant0, (float)m_maxRadius);
		m_gatherMaterialInstance->setFloatBySemantic(eUniformSemantic::floatConstant1, (float)(int)m_alphaMode);

		if (auto materialInstanceBinding= m_gatherMaterialInstance->bindMaterialInstance(materialBinding))
		{
			compositorGraph->getLayerMesh()->drawElements();
			return true;
		}
	}

	return false;
}

bool DepthOfFieldNode::evaluateNode(NodeEvaluator& evaluator)
{
	if (!evaluateInputs(evaluator))
	{
		m_outTexturePin->setValue(IMkTexturePtr());
		return false;
	}

	IMkTexturePtr color= m_colorTexturePin ? m_colorTexturePin->getValue() : IMkTexturePtr();
	IMkTexturePtr depth= m_depthTexturePin ? m_depthTexturePin->getValue() : IMkTexturePtr();
	if (!color)
	{
		m_outTexturePin->setValue(IMkTexturePtr());
		evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::missingInput, "No color texture", this));
		return false;
	}

	// Without depth the layer keeps compositing, sharp, rather than stopping the chain
	if (!depth)
	{
		m_outTexturePin->setValue(color);
		evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::missingInput, "No depth texture", this,
											   nullptr, eNodeEvaluationSeverity::warning));
		return true;
	}

	// Everything runs at the color's resolution; the depth is fetched by normalized
	// coordinate so it may be any size
	const int width= color->getTextureWidth();
	const int height= color->getTextureHeight();

	// No aperture is a lens with everything in focus
	glm::vec4 lensParams;
	if (!computeLensParams((float)width, lensParams))
	{
		m_outTexturePin->setValue(color);
		return true;
	}

	if (!ensureResources(evaluator, width, height))
	{
		m_outTexturePin->setValue(IMkTexturePtr());
		return false;
	}

	const glm::vec2 size((float)width, (float)height);
	bool bSuccess= renderCoCPass(evaluator, depth, lensParams);
	bSuccess= bSuccess
			  && renderDilatePass(evaluator, m_dilateHFrameBuffer, m_dilateHMaterialInstance,
								  eUniformSemantic::depthTexture, m_cocFrameBuffer->getColorTexture(), size);
	bSuccess= bSuccess
			  && renderDilatePass(evaluator, m_dilateVFrameBuffer, m_dilateVMaterialInstance,
								  eUniformSemantic::textureParam, m_dilateHFrameBuffer->getColorTexture(), size);
	bSuccess= bSuccess
			  && renderGatherPass(evaluator, color, depth, m_dilateVFrameBuffer->getColorTexture(), lensParams, size);
	if (!bSuccess)
	{
		m_outTexturePin->setValue(IMkTexturePtr());
		evaluator.addError(
			NodeEvaluationError(eNodeEvaluationErrorCode::evaluationError, "Depth of field pass failed", this));
		return false;
	}

	// Set every evaluation: a resize recreates the color texture object
	m_outTexturePin->setValue(m_gatherFrameBuffer->getColorTexture());
	return true;
}

void DepthOfFieldNode::editorRenderNode(const NodeEditorState& editorState)
{
	MkCanvasScopedNode scopedNode(m_id, editorGetHeaderColor());

	editorRenderTitle(scopedNode);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));

	editorRenderInputPins(editorState);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));
	IMkTexturePtr previewTexture= m_outTexturePin ? m_outTexturePin->getValue() : IMkTexturePtr();
	MkGui::drawImage(previewTexture, 100.f, 100.f);
	ImGui::SameLine();

	editorRenderOutputPins(editorState);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));
}

void DepthOfFieldNode::editorRenderPropertySheet(const NodeEditorState& editorState)
{
	if (MkGui::drawPropertySheetHeader(editorState.styleManager->getStyle("node_editor_panel_header"),
									   locText("nodes.depthOfFieldHeader")))
	{
		MkGuiStyleConstPtr propertyStyle= editorState.styleManager->getStyle("node_editor_property_value");

		// The int widget has no range, so the shaders' bound is applied after it
		MkGui::drawIntProperty(propertyStyle, "DepthOfFieldMaxRadius", locText("nodes.maxBlurRadius"), m_maxRadius);
		m_maxRadius= std::clamp(m_maxRadius, k_minRadius, k_maxRadius);

		// What the color's alpha channel means, and the convention the output is written in
		// (see eColorTextureResolveAlphaMode)
		const std::string alphaModeItems= std::string(locText("nodes.resolveAlphaNone")) + '\0'
										  + locText("nodes.resolveAlphaStraight") + '\0'
										  + locText("nodes.resolveAlphaInverted") + '\0';
		int iAlphaMode= (int)m_alphaMode;
		if (MkGui::drawSimpleComboBoxProperty(propertyStyle, "DepthOfFieldAlphaMode", locText("nodes.colorAlphaMode"),
											  alphaModeItems.c_str(), iAlphaMode))
		{
			m_alphaMode= (eColorTextureResolveAlphaMode)iAlphaMode;
		}

		// The lens lives on the camera, so show what this node is blurring with
		auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
		CameraComponentPtr cameraComponent= compositorGraph ? compositorGraph->getBoundCameraComponent() : nullptr;
		if (cameraComponent)
		{
			CameraDefinitionPtr cameraDefinition= cameraComponent->getCameraDefinition();
			ImGui::TextWrapped(locText("nodes.depthOfFieldLensSummaryFmt"), cameraDefinition->getFocusDistance(),
							   cameraDefinition->getApertureDiameterMM());
		}
		else
		{
			ImGui::TextWrapped("%s", locText("nodes.depthOfFieldNoCamera"));
		}
	}
}

const char* DepthOfFieldNode::editorGetHeaderIcon() const { return ICON_FK_CAMERA; }

// -- DepthOfFieldNode Factory -----
NodePtr DepthOfFieldNodeFactory::createNode(const NodeEditorState& editorState) const
{
	auto node= std::static_pointer_cast<DepthOfFieldNode>(NodeFactory::createNode(editorState));

	TexturePinPtr colorPin= node->addPin<TexturePin>("colorTexture", eNodePinDirection::INPUT);
	node->setColorTexturePin(colorPin);
	TexturePinPtr depthPin= node->addPin<TexturePin>("depthTexture", eNodePinDirection::INPUT);
	node->setDepthTexturePin(depthPin);

	TexturePinPtr outputPin= node->addPin<TexturePin>("texture", eNodePinDirection::OUTPUT);
	outputPin->editorSetShowPinName(false);
	node->setOutputTexturePin(outputPin);

	autoConnectInputPin(editorState, colorPin);
	autoConnectOutputPin(editorState, outputPin);

	return node;
}

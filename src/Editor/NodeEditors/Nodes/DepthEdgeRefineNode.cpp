#include "DepthEdgeRefineNode.h"
#include "IconsForkAwesome.h"
#include "IMkFrameBuffer.h"
#include "IMkGraphicsContext.h"
#include "IMkShaderCache.h"
#include "IMkState.h"
#include "IMkTexture.h"
#include "IMkTriangulatedMesh.h"
#include "LocText.h"
#include "Logger.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiStyleManager.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkScopedObjectBinding.h"
#include "MkStateModifiers.h"
#include "MkStateStack.h"
#include "NodeEditorState.h"

#include "Graphs/CompositorNodeGraph.h"
#include "Graphs/NodeEvaluator.h"

#include "Pins/TexturePin.h"

#include "imgui.h"
#include "MkCanvasScopedNode.h"

#include <algorithm>

// -- DepthEdgeRefineNodeConfig -----
configuru::Config DepthEdgeRefineNodeConfig::writeToJSON()
{
	configuru::Config pt= NodeConfig::writeToJSON();

	pt["band_radius"]= (int64_t)bandRadius;
	pt["color_sigma"]= (double)colorSigma;
	pt["depth_jump"]= (double)depthJump;
	depthPreviewSettings.writeToJSON(pt);

	return pt;
}

void DepthEdgeRefineNodeConfig::readFromJSON(const configuru::Config& pt)
{
	NodeConfig::readFromJSON(pt);

	// Clamped here so a hand-edited file cannot ask for more taps than the shader loops over
	bandRadius= std::clamp(pt.get_or<int>("band_radius", 16), DepthEdgeRefineNode::k_minBandRadius,
						   DepthEdgeRefineNode::k_maxBandRadius);
	colorSigma= pt.get_or<float>("color_sigma", 0.2f);
	depthJump= pt.get_or<float>("depth_jump", 0.05f);
	depthPreviewSettings.readFromJSON(pt);
}

// -- DepthEdgeRefineNode -----
DepthEdgeRefineNode::DepthEdgeRefineNode()
	: Node()
{
	// Created without resources: the size comes from the input mask at evaluation
	m_minMaxHFrameBuffer= createMkFrameBuffer("DepthEdgeRefine MinMax H");
	m_minMaxHFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_minMaxHFrameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::RGBA16F);

	m_minMaxVFrameBuffer= createMkFrameBuffer("DepthEdgeRefine MinMax V");
	m_minMaxVFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_minMaxVFrameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::RGBA16F);

	// The refined mask is the depth attachment, the same texture type the input mask is
	m_refinedFrameBuffer= createMkFrameBuffer("DepthEdgeRefine Output");
	m_refinedFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR_AND_DEPTH);
	m_refinedFrameBuffer->setClearColor(glm::vec4(1.f, 1.f, 1.f, 1.f));
}

DepthEdgeRefineNode::~DepthEdgeRefineNode()
{
	m_minMaxHFrameBuffer= nullptr;
	m_minMaxVFrameBuffer= nullptr;
	m_refinedFrameBuffer= nullptr;
	m_minMaxHMaterialInstance= nullptr;
	m_minMaxVMaterialInstance= nullptr;
	m_refineMaterialInstance= nullptr;

	m_depthTexturePin= nullptr;
	m_videoTexturePin= nullptr;
	m_outTexturePin= nullptr;

	setOwnerGraph(NodeGraphPtr());
}

bool DepthEdgeRefineNode::loadFromConfig(NodeConfigConstPtr nodeConfig)
{
	if (Node::loadFromConfig(nodeConfig))
	{
		auto config= std::static_pointer_cast<const DepthEdgeRefineNodeConfig>(nodeConfig);

		m_bandRadius= config->bandRadius;
		m_colorSigma= config->colorSigma;
		m_depthJump= config->depthJump;
		m_depthPreviewSettings= config->depthPreviewSettings;

		return true;
	}

	return false;
}

void DepthEdgeRefineNode::saveToConfig(NodeConfigPtr nodeConfig) const
{
	auto config= std::static_pointer_cast<DepthEdgeRefineNodeConfig>(nodeConfig);

	config->bandRadius= m_bandRadius;
	config->colorSigma= m_colorSigma;
	config->depthJump= m_depthJump;
	config->depthPreviewSettings= m_depthPreviewSettings;

	Node::saveToConfig(nodeConfig);
}

void DepthEdgeRefineNode::onGraphLoaded(bool success)
{
	if (!success)
		return;

	// Two texture inputs, so they are told apart by name rather than by type
	for (NodePinPtr pin : m_pinsIn)
	{
		TexturePinPtr texturePin= std::dynamic_pointer_cast<TexturePin>(pin);
		if (!texturePin)
			continue;

		if (texturePin->getName() == "depthTexture")
			setDepthTexturePin(texturePin);
		else if (texturePin->getName() == "videoTexture")
			setVideoTexturePin(texturePin);
	}

	setOutputTexturePin(getFirstPinOfType<TexturePin>(eNodePinDirection::OUTPUT));
}

void DepthEdgeRefineNode::setOwnerGraph(NodeGraphPtr newOwnerGraph)
{
	if (newOwnerGraph != m_ownerGraph)
	{
		if (m_ownerGraph)
		{
			m_ownerGraph->OnGraphLoaded-= MakeDelegate(this, &DepthEdgeRefineNode::onGraphLoaded);
			m_ownerGraph= nullptr;
		}

		if (newOwnerGraph)
		{
			newOwnerGraph->OnGraphLoaded+= MakeDelegate(this, &DepthEdgeRefineNode::onGraphLoaded);
			m_ownerGraph= newOwnerGraph;
		}
	}
}

bool DepthEdgeRefineNode::ensureResources(NodeEvaluator& evaluator, int width, int height)
{
	IMkGraphicsContext* graphicsContext= evaluator.getCurrentGraphicsContext();

	if (!m_minMaxHMaterialInstance || !m_minMaxVMaterialInstance || !m_refineMaterialInstance)
	{
		IMkShaderCache* shaderCache= graphicsContext->getShaderCache();
		MkMaterialConstPtr minMaxH= shaderCache->getMaterialByName(INTERNAL_MATERIAL_PT_DEPTH_MINMAX_H);
		MkMaterialConstPtr minMaxV= shaderCache->getMaterialByName(INTERNAL_MATERIAL_PT_DEPTH_MINMAX_V);
		MkMaterialConstPtr refine= shaderCache->getMaterialByName(INTERNAL_MATERIAL_PT_DEPTH_EDGE_REFINE);
		if (!minMaxH || !minMaxV || !refine)
		{
			evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::evaluationError,
												   "Missing depth edge refine materials", this));
			return false;
		}

		m_minMaxHMaterialInstance= createMkMaterialInstance(minMaxH);
		m_minMaxVMaterialInstance= createMkMaterialInstance(minMaxV);
		m_refineMaterialInstance= createMkMaterialInstance(refine);
	}

	// setSize is a no-op at the same size and invalidates otherwise
	IMkFrameBufferPtr frameBuffers[]= {m_minMaxHFrameBuffer, m_minMaxVFrameBuffer, m_refinedFrameBuffer};
	for (IMkFrameBufferPtr frameBuffer : frameBuffers)
	{
		frameBuffer->setSize(width, height);
		if (!frameBuffer->isValid() && !frameBuffer->createResources())
		{
			evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::evaluationError,
												   "Unable to create depth edge refine frame buffer", this));
			return false;
		}
	}

	return true;
}

bool DepthEdgeRefineNode::renderMinMaxPass(NodeEvaluator& evaluator, IMkFrameBufferPtr target,
										   MkMaterialInstancePtr materialInstance, eUniformSemantic inputSemantic,
										   IMkTexturePtr input, const glm::vec2& size)
{
	IMkGraphicsContext* graphicsContext= evaluator.getCurrentGraphicsContext();
	MkScopedObjectBinding frameBufferBinding(graphicsContext->getMkStateStack().getCurrentState(),
											 "Depth Edge Refine MinMax Scope", target);
	if (!frameBufferBinding)
		return false;

	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	MkMaterialConstPtr material= materialInstance->getMaterial();
	if (auto materialBinding= material->bindMaterial())
	{
		materialInstance->setTextureBySemantic(inputSemantic, input);
		materialInstance->setVec2BySemantic(eUniformSemantic::screenSize, size);
		materialInstance->setFloatBySemantic(eUniformSemantic::floatConstant0, (float)m_bandRadius);

		if (auto materialInstanceBinding= materialInstance->bindMaterialInstance(materialBinding))
		{
			compositorGraph->getLayerMesh()->drawElements();
			return true;
		}
	}

	return false;
}

bool DepthEdgeRefineNode::renderRefinePass(NodeEvaluator& evaluator, IMkTexturePtr depth, IMkTexturePtr video,
										   IMkTexturePtr windowMinMax, const glm::vec2& size)
{
	IMkGraphicsContext* graphicsContext= evaluator.getCurrentGraphicsContext();
	MkScopedObjectBinding frameBufferBinding(graphicsContext->getMkStateStack().getCurrentState(),
											 "Depth Edge Refine Output Scope", m_refinedFrameBuffer);
	if (!frameBufferBinding)
		return false;

	// The result goes out through gl_FragDepth, so the depth write has to be on and
	// nothing a caller set may blend or stencil it away. The buffer clears to the far
	// plane, so a refined value of exactly 1.0 failing the LESS test leaves the same answer.
	IMkState* mkState= frameBufferBinding.getMkState();
	mkState->enableFlag(eMkStateFlagType::depthTest);
	mkState->disableFlag(eMkStateFlagType::blend);
	mkState->disableFlag(eMkStateFlagType::stencilTest);
	mkStateSetDepthMask(mkState, true);

	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	MkMaterialConstPtr material= m_refineMaterialInstance->getMaterial();
	if (auto materialBinding= material->bindMaterial())
	{
		m_refineMaterialInstance->setTextureBySemantic(eUniformSemantic::depthTexture, depth);
		m_refineMaterialInstance->setTextureBySemantic(eUniformSemantic::rgbaTexture, video);
		m_refineMaterialInstance->setTextureBySemantic(eUniformSemantic::textureParam, windowMinMax);
		m_refineMaterialInstance->setVec2BySemantic(eUniformSemantic::screenSize, size);
		m_refineMaterialInstance->setFloatBySemantic(eUniformSemantic::floatConstant0, (float)m_bandRadius);
		m_refineMaterialInstance->setFloatBySemantic(eUniformSemantic::floatConstant1, m_colorSigma);
		m_refineMaterialInstance->setFloatBySemantic(eUniformSemantic::floatConstant2, m_depthJump);

		if (auto materialInstanceBinding= m_refineMaterialInstance->bindMaterialInstance(materialBinding))
		{
			compositorGraph->getLayerMesh()->drawElements();
			return true;
		}
	}

	return false;
}

bool DepthEdgeRefineNode::evaluateNode(NodeEvaluator& evaluator)
{
	if (!evaluateInputs(evaluator))
	{
		m_outTexturePin->setValue(IMkTexturePtr());
		return false;
	}

	IMkTexturePtr depth= m_depthTexturePin ? m_depthTexturePin->getValue() : IMkTexturePtr();
	IMkTexturePtr video= m_videoTexturePin ? m_videoTexturePin->getValue() : IMkTexturePtr();
	if (!depth || !video)
	{
		m_outTexturePin->setValue(IMkTexturePtr());
		evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::missingInput,
											   depth ? "No video texture" : "No depth texture", this));
		return false;
	}

	// Everything runs at the mask's resolution; the video is sampled by normalized
	// coordinate so it may be any size
	const int width= depth->getTextureWidth();
	const int height= depth->getTextureHeight();
	if (!ensureResources(evaluator, width, height))
	{
		m_outTexturePin->setValue(IMkTexturePtr());
		return false;
	}

	const glm::vec2 size((float)width, (float)height);
	bool bSuccess= renderMinMaxPass(evaluator, m_minMaxHFrameBuffer, m_minMaxHMaterialInstance,
									eUniformSemantic::depthTexture, depth, size);
	bSuccess= bSuccess
			  && renderMinMaxPass(evaluator, m_minMaxVFrameBuffer, m_minMaxVMaterialInstance,
								  eUniformSemantic::textureParam, m_minMaxHFrameBuffer->getColorTexture(), size);
	bSuccess= bSuccess && renderRefinePass(evaluator, depth, video, m_minMaxVFrameBuffer->getColorTexture(), size);
	if (!bSuccess)
	{
		m_outTexturePin->setValue(IMkTexturePtr());
		evaluator.addError(
			NodeEvaluationError(eNodeEvaluationErrorCode::evaluationError, "Depth edge refine pass failed", this));
		return false;
	}

	// Colorize for the node body only; the pin carries the untouched refined depth
	m_depthPreviewRenderer.render(evaluator, m_refinedFrameBuffer->getColorTexture(), m_depthPreviewSettings);

	// Set every evaluation: a resize recreates the depth texture object
	m_outTexturePin->setValue(m_refinedFrameBuffer->getDepthTexture());
	return true;
}

void DepthEdgeRefineNode::editorRenderNode(const NodeEditorState& editorState)
{
	MkCanvasScopedNode scopedNode(m_id, editorGetHeaderColor());

	editorRenderTitle(scopedNode);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));

	editorRenderInputPins(editorState);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));
	IMkTexturePtr previewTexture= m_depthPreviewRenderer.getPreviewTexture();
	if (previewTexture == nullptr)
	{
		previewTexture= m_refinedFrameBuffer ? m_refinedFrameBuffer->getColorTexture() : IMkTexturePtr();
	}
	MkGui::drawImage(previewTexture, 100.f, 100.f);
	ImGui::SameLine();

	editorRenderOutputPins(editorState);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));
}

void DepthEdgeRefineNode::editorRenderPropertySheet(const NodeEditorState& editorState)
{
	if (MkGui::drawPropertySheetHeader(editorState.styleManager->getStyle("node_editor_panel_header"),
									   locText("nodes.depthEdgeRefineHeader")))
	{
		MkGuiStyleConstPtr propertyStyle= editorState.styleManager->getStyle("node_editor_property_value");

		// The int widget has no range, so the shader's bound is applied after it
		MkGui::drawIntProperty(propertyStyle, "DepthEdgeRefineBandRadius", locText("nodes.bandRadius"), m_bandRadius);
		m_bandRadius= std::clamp(m_bandRadius, k_minBandRadius, k_maxBandRadius);
		MkGui::drawFloatSliderProperty(propertyStyle, "DepthEdgeRefineColorSigma", locText("nodes.colorSigma"),
									   m_colorSigma, 0.01f, 1.f, 0.01f, 1.f);
		MkGui::drawFloatSliderProperty(propertyStyle, "DepthEdgeRefineDepthJump", locText("nodes.depthJump"),
									   m_depthJump, 0.005f, 0.5f, 0.005f, 0.5f);

		drawDepthPreviewProperties(propertyStyle, m_depthPreviewSettings);
	}
}

const char* DepthEdgeRefineNode::editorGetHeaderIcon() const { return ICON_FK_CROP; }

// -- DepthEdgeRefineNode Factory -----
NodePtr DepthEdgeRefineNodeFactory::createNode(const NodeEditorState& editorState) const
{
	auto node= std::static_pointer_cast<DepthEdgeRefineNode>(NodeFactory::createNode(editorState));

	// No default values: a missing link is an evaluation error rather than a silent pass-through
	TexturePinPtr depthPin= node->addPin<TexturePin>("depthTexture", eNodePinDirection::INPUT);
	node->setDepthTexturePin(depthPin);
	TexturePinPtr videoPin= node->addPin<TexturePin>("videoTexture", eNodePinDirection::INPUT);
	node->setVideoTexturePin(videoPin);

	TexturePinPtr outputPin= node->addPin<TexturePin>("texture", eNodePinDirection::OUTPUT);
	outputPin->editorSetShowPinName(false);
	node->setOutputTexturePin(outputPin);

	autoConnectInputPin(editorState, depthPin);
	autoConnectOutputPin(editorState, outputPin);

	return node;
}

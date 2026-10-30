#include "MatteDepthRefineNode.h"
#include "CompositorComponent.h"
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

// -- MatteDepthRefineNodeConfig -----
configuru::Config MatteDepthRefineNodeConfig::writeToJSON()
{
	configuru::Config pt= NodeConfig::writeToJSON();

	pt["band_radius"]= (int64_t)bandRadius;
	depthPreviewSettings.writeToJSON(pt);

	return pt;
}

void MatteDepthRefineNodeConfig::readFromJSON(const configuru::Config& pt)
{
	NodeConfig::readFromJSON(pt);

	// Clamped here so a hand-edited file cannot ask for more taps than the shader loops over
	bandRadius= std::clamp(pt.get_or<int>("band_radius", 64), MatteDepthRefineNode::k_minBandRadius,
						   MatteDepthRefineNode::k_maxBandRadius);
	depthPreviewSettings.readFromJSON(pt);
}

// -- MatteDepthRefineNode -----
MatteDepthRefineNode::MatteDepthRefineNode()
	: Node()
{
	// Created without resources: the size comes from the input mask at evaluation
	m_minMaxHFrameBuffer= createMkFrameBuffer("MatteDepthRefine MinMax H");
	m_minMaxHFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_minMaxHFrameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::RGBA16F);

	m_minMaxVFrameBuffer= createMkFrameBuffer("MatteDepthRefine MinMax V");
	m_minMaxVFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_minMaxVFrameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::RGBA16F);

	// The refined mask is the depth attachment, the same texture type the input mask is
	m_refinedFrameBuffer= createMkFrameBuffer("MatteDepthRefine Output");
	m_refinedFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR_AND_DEPTH);
	m_refinedFrameBuffer->setClearColor(glm::vec4(1.f, 1.f, 1.f, 1.f));
}

MatteDepthRefineNode::~MatteDepthRefineNode()
{
	m_minMaxHFrameBuffer= nullptr;
	m_minMaxVFrameBuffer= nullptr;
	m_refinedFrameBuffer= nullptr;
	m_minMaxHMaterialInstance= nullptr;
	m_minMaxVMaterialInstance= nullptr;
	m_refineMaterialInstance= nullptr;

	m_depthTexturePin= nullptr;
	m_outTexturePin= nullptr;

	setOwnerGraph(NodeGraphPtr());
}

bool MatteDepthRefineNode::loadFromConfig(NodeConfigConstPtr nodeConfig)
{
	if (Node::loadFromConfig(nodeConfig))
	{
		auto config= std::static_pointer_cast<const MatteDepthRefineNodeConfig>(nodeConfig);

		m_bandRadius= config->bandRadius;
		m_depthPreviewSettings= config->depthPreviewSettings;

		return true;
	}

	return false;
}

void MatteDepthRefineNode::saveToConfig(NodeConfigPtr nodeConfig) const
{
	auto config= std::static_pointer_cast<MatteDepthRefineNodeConfig>(nodeConfig);

	config->bandRadius= m_bandRadius;
	config->depthPreviewSettings= m_depthPreviewSettings;

	Node::saveToConfig(nodeConfig);
}

void MatteDepthRefineNode::onGraphLoaded(bool success)
{
	if (!success)
		return;

	setDepthTexturePin(getFirstPinOfType<TexturePin>(eNodePinDirection::INPUT));
	setOutputTexturePin(getFirstPinOfType<TexturePin>(eNodePinDirection::OUTPUT));
}

void MatteDepthRefineNode::setOwnerGraph(NodeGraphPtr newOwnerGraph)
{
	if (newOwnerGraph != m_ownerGraph)
	{
		if (m_ownerGraph)
		{
			m_ownerGraph->OnGraphLoaded-= MakeDelegate(this, &MatteDepthRefineNode::onGraphLoaded);
			m_ownerGraph= nullptr;
		}

		if (newOwnerGraph)
		{
			newOwnerGraph->OnGraphLoaded+= MakeDelegate(this, &MatteDepthRefineNode::onGraphLoaded);
			m_ownerGraph= newOwnerGraph;
		}
	}
}

bool MatteDepthRefineNode::ensureResources(NodeEvaluator& evaluator, int width, int height)
{
	IMkGraphicsContext* graphicsContext= evaluator.getCurrentGraphicsContext();

	if (!m_minMaxHMaterialInstance || !m_minMaxVMaterialInstance || !m_refineMaterialInstance)
	{
		IMkShaderCache* shaderCache= graphicsContext->getShaderCache();
		MkMaterialConstPtr minMaxH= shaderCache->getMaterialByName(INTERNAL_MATERIAL_PT_DEPTH_MINMAX_H);
		MkMaterialConstPtr minMaxV= shaderCache->getMaterialByName(INTERNAL_MATERIAL_PT_DEPTH_MINMAX_V);
		MkMaterialConstPtr refine= shaderCache->getMaterialByName(INTERNAL_MATERIAL_PT_MATTE_DEPTH_REFINE);
		if (!minMaxH || !minMaxV || !refine)
		{
			evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::evaluationError,
												   "Missing matte depth refine materials", this));
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
												   "Unable to create matte depth refine frame buffer", this));
			return false;
		}
	}

	return true;
}

bool MatteDepthRefineNode::renderMinMaxPass(NodeEvaluator& evaluator, IMkFrameBufferPtr target,
											MkMaterialInstancePtr materialInstance, eUniformSemantic inputSemantic,
											IMkTexturePtr input, const glm::vec2& size)
{
	IMkGraphicsContext* graphicsContext= evaluator.getCurrentGraphicsContext();
	MkScopedObjectBinding frameBufferBinding(graphicsContext->getMkStateStack().getCurrentState(),
											 "Matte Depth Refine MinMax Scope", target);
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

bool MatteDepthRefineNode::renderRefinePass(NodeEvaluator& evaluator, IMkTexturePtr depth, IMkTexturePtr matte,
											IMkTexturePtr windowMinMax, const glm::vec2& size)
{
	IMkGraphicsContext* graphicsContext= evaluator.getCurrentGraphicsContext();
	MkScopedObjectBinding frameBufferBinding(graphicsContext->getMkStateStack().getCurrentState(),
											 "Matte Depth Refine Output Scope", m_refinedFrameBuffer);
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
		m_refineMaterialInstance->setTextureBySemantic(eUniformSemantic::alphaTexture, matte);
		m_refineMaterialInstance->setTextureBySemantic(eUniformSemantic::textureParam, windowMinMax);
		m_refineMaterialInstance->setVec2BySemantic(eUniformSemantic::screenSize, size);

		if (auto materialInstanceBinding= m_refineMaterialInstance->bindMaterialInstance(materialBinding))
		{
			compositorGraph->getLayerMesh()->drawElements();
			return true;
		}
	}

	return false;
}

bool MatteDepthRefineNode::evaluateNode(NodeEvaluator& evaluator)
{
	if (!evaluateInputs(evaluator))
	{
		m_outTexturePin->setValue(IMkTexturePtr());
		return false;
	}

	IMkTexturePtr depth= m_depthTexturePin ? m_depthTexturePin->getValue() : IMkTexturePtr();
	if (!depth)
	{
		m_outTexturePin->setValue(IMkTexturePtr());
		evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::missingInput, "No depth texture", this));
		return false;
	}

	// The matte belongs to the frame being composited; without one the mask passes through
	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	CompositorComponentPtr compositorComponent= compositorGraph->getBoundCompositorComponent();
	IMkTexturePtr matte=
		compositorComponent
			? compositorComponent->getOcclusionMatteTexture(compositorComponent->getPendingCompositedFrameIndex())
			: IMkTexturePtr();
	if (!matte)
	{
		m_bHadMatte= false;
		m_outTexturePin->setValue(depth);
		evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::missingInput,
											   "No occlusion matte for this frame", this, nullptr,
											   eNodeEvaluationSeverity::warning));
		return true;
	}
	m_bHadMatte= true;

	// Everything runs at the mask's resolution; the matte is sampled by normalized
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
	bSuccess= bSuccess && renderRefinePass(evaluator, depth, matte, m_minMaxVFrameBuffer->getColorTexture(), size);
	if (!bSuccess)
	{
		m_outTexturePin->setValue(IMkTexturePtr());
		evaluator.addError(
			NodeEvaluationError(eNodeEvaluationErrorCode::evaluationError, "Matte depth refine pass failed", this));
		return false;
	}

	// Colorize for the node body only; the pin carries the untouched refined depth
	m_depthPreviewRenderer.render(evaluator, m_refinedFrameBuffer->getColorTexture(), m_depthPreviewSettings);

	// Set every evaluation: a resize recreates the depth texture object
	m_outTexturePin->setValue(m_refinedFrameBuffer->getDepthTexture());
	return true;
}

void MatteDepthRefineNode::editorRenderNode(const NodeEditorState& editorState)
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

void MatteDepthRefineNode::editorRenderPropertySheet(const NodeEditorState& editorState)
{
	if (MkGui::drawPropertySheetHeader(editorState.styleManager->getStyle("node_editor_panel_header"),
									   locText("nodes.matteDepthRefineHeader")))
	{
		MkGuiStyleConstPtr propertyStyle= editorState.styleManager->getStyle("node_editor_property_value");

		// The int widget has no range, so the shader's bound is applied after it
		MkGui::drawIntProperty(propertyStyle, "MatteDepthRefineBandRadius", locText("nodes.bandRadius"), m_bandRadius);
		m_bandRadius= std::clamp(m_bandRadius, k_minBandRadius, k_maxBandRadius);

		ImGui::TextUnformatted(
			locText(m_bHadMatte ? "nodes.matteDepthRefineHasMatte" : "nodes.matteDepthRefineNoMatte"));

		drawDepthPreviewProperties(propertyStyle, m_depthPreviewSettings);
	}
}

const char* MatteDepthRefineNode::editorGetHeaderIcon() const { return ICON_FK_CROP; }

// -- MatteDepthRefineNode Factory -----
NodePtr MatteDepthRefineNodeFactory::createNode(const NodeEditorState& editorState) const
{
	auto node= std::static_pointer_cast<MatteDepthRefineNode>(NodeFactory::createNode(editorState));

	// No default value: a missing link is an evaluation error rather than a silent pass-through
	TexturePinPtr depthPin= node->addPin<TexturePin>("depthTexture", eNodePinDirection::INPUT);
	node->setDepthTexturePin(depthPin);

	TexturePinPtr outputPin= node->addPin<TexturePin>("texture", eNodePinDirection::OUTPUT);
	outputPin->editorSetShowPinName(false);
	node->setOutputTexturePin(outputPin);

	autoConnectInputPin(editorState, depthPin);
	autoConnectOutputPin(editorState, outputPin);

	return node;
}

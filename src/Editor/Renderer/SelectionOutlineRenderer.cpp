#include "SelectionOutlineRenderer.h"
#include "Colors.h"
#include "IMkCamera.h"
#include "IMkFrameBuffer.h"
#include "IMkGraphicsContext.h"
#include "IMkSceneRenderable.h"
#include "IMkShaderCache.h"
#include "IMkState.h"
#include "IMkTriangulatedMesh.h"
#include "Logger.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkScopedObjectBinding.h"
#include "MkScopedState.h"
#include "MkStateModifiers.h"
#include "MkStateStack.h"

#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float4.hpp"

namespace
{
constexpr float k_outlineRadiusPx= 2.f;
// Matches the transform gizmo's occluded pass, so occluded outlines read the same way
constexpr float k_outlineOccludedAlpha= 0.35f;

// A mask code is (state * 2 + visible): the visible part of a target outranks its occluded
// part, and every part of a higher state outranks a lower one
float getMaskCode(eSelectionOutlineState state, bool bVisible) { return (float)((int)state * 2 + (bVisible ? 1 : 0)); }
} // namespace

SelectionOutlineRenderer::SelectionOutlineRenderer(IMkGraphicsContext* graphicsContext)
	: m_graphicsContext(graphicsContext)
{
}

void SelectionOutlineRenderer::render(IMkCameraConstPtr camera, const glm::i32vec4& viewportRect,
									  const std::vector<SelectionOutlineTarget>& targets)
{
	if (targets.empty() || !camera || viewportRect.z <= 0 || viewportRect.w <= 0)
		return;

	if (!ensureResources(viewportRect.z, viewportRect.w))
		return;

	MkStateStack& stateStack= m_graphicsContext->getMkStateStack();

	renderMask(stateStack, camera, viewportRect, targets);
	renderOutline(stateStack);
}

bool SelectionOutlineRenderer::ensureResources(int width, int height)
{
	IMkShaderCache* shaderCache= m_graphicsContext->getShaderCache();

	if (!m_maskMaterialInstance)
	{
		MkMaterialConstPtr maskMaterial= shaderCache->getMaterialByName(INTERNAL_MATERIAL_P_SELECTION_MASK);
		if (!maskMaterial)
			return false;

		m_maskMaterialInstance= std::make_shared<MkMaterialInstance>(maskMaterial);
	}

	if (!m_outlineQuad)
	{
		MkMaterialConstPtr outlineMaterial= shaderCache->getMaterialByName(INTERNAL_MATERIAL_PT_SELECTION_OUTLINE);
		if (!outlineMaterial)
			return false;

		m_outlineQuad= createFullscreenQuadMesh(m_graphicsContext, outlineMaterial, false);

		MkMaterialInstancePtr outlineInstance= m_outlineQuad->getMaterialInstance();
		outlineInstance->setFloatBySemantic(eUniformSemantic::floatConstant0, k_outlineRadiusPx);
		outlineInstance->setFloatBySemantic(eUniformSemantic::floatConstant1, k_outlineOccludedAlpha);
		outlineInstance->setVec4ByUniformName("hoverColor", glm::vec4(Colors::LightGray, 1.f));
		outlineInstance->setVec4ByUniformName("selectedColor", glm::vec4(Colors::Yellow, 1.f));
		outlineInstance->setVec4ByUniformName("gizmoColor", glm::vec4(Colors::GreenYellow, 1.f));
	}

	if (!m_maskFrameBuffer)
	{
		// The COLOR type's depth/stencil format matches the default frame buffer's,
		// which the viewport depth copy needs
		m_maskFrameBuffer= createMkFrameBuffer("selection_outline_mask");
		m_maskFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
		m_maskFrameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::R8);
		m_maskFrameBuffer->setClearColor(glm::vec4(0.f));
	}

	m_maskFrameBuffer->setSize(width, height);
	if (!m_maskFrameBuffer->isValid() && !m_maskFrameBuffer->createResources())
	{
		MIKAN_LOG_ERROR("SelectionOutlineRenderer::ensureResources")
			<< "Failed to create the outline mask frame buffer";
		return false;
	}

	return true;
}

void SelectionOutlineRenderer::renderMask(MkStateStack& stateStack, IMkCameraConstPtr camera,
										  const glm::i32vec4& viewportRect,
										  const std::vector<SelectionOutlineTarget>& targets)
{
	// Occlusion is judged against everything the viewport has drawn so far. A failed copy
	// (the copy logs its GL error) means the frame buffer's depth format will never match,
	// so it is not retried.
	bool bCopiedDepth= false;
	if (!m_bDepthCopyFailed)
	{
		bCopiedDepth= m_maskFrameBuffer->copyDepthFromBoundFrameBuffer(viewportRect.x, viewportRect.y);
		m_bDepthCopyFailed= !bCopiedDepth;
	}

	MkScopedObjectBinding maskBinding(stateStack.getCurrentState(), "SelectionOutline mask", m_maskFrameBuffer);
	if (!maskBinding)
		return;

	MkScopedState maskScope= stateStack.createScopedState("SelectionOutline mask passes");
	IMkState* maskState= maskScope.getStackState();

	// Without the viewport depth, a far plane depth makes every target read as visible
	if (!bCopiedDepth)
	{
		mkStateClearBuffer(maskState, eMkClearFlags::depth);
	}

	// MAX keeps the highest code per pixel whatever order the targets draw in, and the
	// copied depth is only compared against, never written
	maskState->disableFlag(eMkStateFlagType::cullFace);
	maskState->enableFlag(eMkStateFlagType::blend);
	mkStateSetBlendEquation(maskState, eMkBlendEquation::MAX);
	mkStateSetDepthMask(maskState, false);

	// Every target's whole silhouette as occluded, then its visible part over that
	{
		MkScopedState occludedScope= stateStack.createScopedState("SelectionOutline occluded pass");
		occludedScope.getStackState()->disableFlag(eMkStateFlagType::depthTest);

		renderMaskTargets(camera, targets, false);
	}

	{
		MkScopedState visibleScope= stateStack.createScopedState("SelectionOutline visible pass");
		visibleScope.getStackState()->enableFlag(eMkStateFlagType::depthTest);

		renderMaskTargets(camera, targets, true);
	}
}

void SelectionOutlineRenderer::renderMaskTargets(IMkCameraConstPtr camera,
												 const std::vector<SelectionOutlineTarget>& targets, bool bVisiblePass)
{
	MkMaterialConstPtr maskMaterial= m_maskMaterialInstance->getMaterial();
	if (auto materialBinding= maskMaterial->bindMaterial())
	{
		const glm::mat4 vpMatrix= camera->getViewProjectionMatrix();

		for (const SelectionOutlineTarget& target : targets)
		{
			// The mask shader only reads the position, which every mesh layout leads with,
			// so the renderable draws through its own vertex array
			m_maskMaterialInstance->setMat4BySemantic(eUniformSemantic::modelViewProjectionMatrix,
													  vpMatrix * target.renderable->getModelMatrix());
			m_maskMaterialInstance->setFloatBySemantic(eUniformSemantic::floatConstant0,
													   getMaskCode(target.state, bVisiblePass));

			if (auto materialInstanceBinding= m_maskMaterialInstance->bindMaterialInstance(materialBinding))
			{
				target.renderable->render();
			}
		}
	}
}

void SelectionOutlineRenderer::renderOutline(MkStateStack& stateStack)
{
	MkScopedState outlineScope= stateStack.createScopedState("SelectionOutline composite");
	IMkState* outlineState= outlineScope.getStackState();

	outlineState->disableFlag(eMkStateFlagType::depthTest);
	outlineState->disableFlag(eMkStateFlagType::cullFace);
	outlineState->enableFlag(eMkStateFlagType::blend);
	mkStateSetBlendEquation(outlineState, eMkBlendEquation::ADD);
	mkStateSetBlendFunc(outlineState, eMkBlendFunction::SRC_ALPHA, eMkBlendFunction::ONE_MINUS_SRC_ALPHA);

	MkMaterialInstancePtr outlineInstance= m_outlineQuad->getMaterialInstance();
	MkMaterialConstPtr outlineMaterial= outlineInstance->getMaterial();
	if (auto materialBinding= outlineMaterial->bindMaterial())
	{
		outlineInstance->setTextureBySemantic(eUniformSemantic::rgbTexture, m_maskFrameBuffer->getColorTexture());
		outlineInstance->setVec2BySemantic(
			eUniformSemantic::screenSize,
			glm::vec2((float)m_maskFrameBuffer->getWidth(), (float)m_maskFrameBuffer->getHeight()));

		if (auto materialInstanceBinding= outlineInstance->bindMaterialInstance(materialBinding))
		{
			m_outlineQuad->drawElements();
		}
	}
}

#pragma once

#include "MikanRendererFwd.h"

#include <vector>

#include "glm/ext/vector_int4_sized.hpp"

class IMkGraphicsContext;
class MkStateStack;

// The values are the outline priority: where outlines of two states meet, the higher one wins
enum class eSelectionOutlineState : int
{
	hovered= 1,
	selected= 2,
	transformGizmoBound= 3,
};

struct SelectionOutlineTarget
{
	IMkSceneRenderableConstPtr renderable;
	eSelectionOutlineState state;
};

// Outlines meshes in screen space, so a dense mesh keeps its texture readable while selected.
// The targets are drawn into a mask, then the mask's border is drawn over the viewport, full
// strength where the target is visible and faded where the scene already drawn occludes it.
class SelectionOutlineRenderer
{
public:
	SelectionOutlineRenderer(IMkGraphicsContext* graphicsContext);

	// Draws into the bound frame buffer over viewportRect (x, y, width, height, GL bottom-left
	// origin). The frame buffer's depth must already hold the scene that occludes the targets.
	void render(IMkCameraConstPtr camera, const glm::i32vec4& viewportRect,
				const std::vector<SelectionOutlineTarget>& targets);

private:
	bool ensureResources(int width, int height);
	void renderMask(MkStateStack& stateStack, IMkCameraConstPtr camera, const glm::i32vec4& viewportRect,
					const std::vector<SelectionOutlineTarget>& targets);
	void renderMaskTargets(IMkCameraConstPtr camera, const std::vector<SelectionOutlineTarget>& targets,
						   bool bVisiblePass);
	void renderOutline(MkStateStack& stateStack);

	IMkGraphicsContext* m_graphicsContext;
	IMkFrameBufferPtr m_maskFrameBuffer;
	MkMaterialInstancePtr m_maskMaterialInstance;
	IMkTriangulatedMeshPtr m_outlineQuad;
	bool m_bDepthCopyFailed= false;
};

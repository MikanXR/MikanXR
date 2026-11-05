#pragma once

#include "CommonConfigFwd.h"
#include "ComponentFwd.h"
#include "MikanRendererFwd.h"
#include "ObjectSystemFwd.h"
#include "SceneFwd.h"
#include "SelectionOutlineRenderer.h"
#include "Shared/GuiPanel.h"

#include <memory>

#include "glm/ext/vector_float2.hpp"

// The composite of the current scene's display compositor, drawn into an offscreen
// frame buffer at the composited frame's resolution and letterboxed into the panel.
// Ctrl+left click on the image focuses the compositor camera on the stencil under it.
class GuiPanel_CompositorOutput : public GuiPanel
{
public:
	GuiPanel_CompositorOutput(AppStage* ownerAppStage)
		: GuiPanel(ownerAppStage)
	{
	}

	bool init(IMkGraphicsContext* graphicsContext);
	virtual void onGui() override;
	virtual void dispose() override;

private:
	void bindSceneComponent(SceneComponentPtr sceneComponent);
	void rebindCompositorFromScene();
	void onSceneActivated(SceneComponentPtr newScene);
	void onSceneDefinitionChanged(CommonConfigPtr configPtr, const ConfigPropertyChangeSet& changedPropertySet);
	void onSceneComponentDisposed(MikanObjectSystemPtr objectSystem, MikanComponentConstPtr component);
	void onCompositorComponentDisposed(MikanObjectSystemPtr objectSystem, MikanComponentConstPtr component);

	void updateViewCamera();
	bool ensureFrameBuffer(int width, int height);
	void renderFrameBuffer();
	// Sets the bound camera's focus distance from the stencil under a pixel of the presented image
	void pickFocusAtImagePixel(const glm::vec2& imagePixel, const glm::vec2& imageSize);

	IMkGraphicsContext* m_graphicsContext= nullptr;

	SceneObjectSystemWeakPtr m_sceneSystem;
	CompositorObjectSystemWeakPtr m_compositorSystem;
	EditorObjectSystemWeakPtr m_editorSystem;
	BoxStencilSystemWeakPtr m_boxStencilSystem;
	QuadStencilSystemWeakPtr m_quadStencilSystem;
	ModelStencilSystemWeakPtr m_modelStencilSystem;

	SceneComponentWeakPtr m_sceneComponent;
	CompositorComponentWeakPtr m_compositorComponent;
	MikanCameraPtr m_viewCamera; // scene rendering camera
	IMkScenePtr m_mkScene;
	std::unique_ptr<SelectionOutlineRenderer> m_selectionOutlineRenderer;
	IMkFrameBufferPtr m_frameBuffer;
	IMkTriangulatedMeshPtr m_compositedFrameQuad; // fullscreen quad for compositor texture
	IMkTriangulatedMeshPtr m_backgroundQuad;      // fullscreen quad for the animated test card
};

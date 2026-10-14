#pragma once

//-- includes -----
#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "SceneFwd.h"
#include "EditorWindow.h"
#include "MikanRendererFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "SelectionOutlineRenderer.h"

#include <memory>

//-- definitions -----
class CompositorOutputEditorWindow : public EditorWindow
{
public:
	CompositorOutputEditorWindow(class App* ownerApp);

	bool bindSceneComponent(SceneComponentPtr sceneComponent);

	// -- IEditorWindow --
	virtual bool startup() override;
	virtual void update(float deltaSeconds) override;
	virtual void render() override;
	virtual void shutdown() override;

	virtual bool getIsRenderingStage() const override { return false; }
	virtual IMkViewportPtr getRenderingViewport() const override { return nullptr; }

	// -- IMkWindowEventListener
	virtual bool onWindowEvent(const class MkWindowEvent& event) override;

private:
	void rebindCompositorFromScene();
	void onSceneActivated(SceneComponentPtr newScene);
	void onSceneDefinitionChanged(CommonConfigPtr configPtr, const ConfigPropertyChangeSet& changedPropertySet);
	void onSceneComponentDisposed(MikanObjectSystemPtr objectSystem, MikanComponentConstPtr component);
	void onCompositorComponentDisposed(MikanObjectSystemPtr objectSystem, MikanComponentConstPtr component);

	BoxStencilSystemWeakPtr m_boxStencilSystem;
	QuadStencilSystemWeakPtr m_quadStencilSystem;
	ModelStencilSystemWeakPtr m_modelStencilSystem;

	SceneComponentWeakPtr m_sceneComponent;
	CompositorComponentWeakPtr m_compositorComponent;
	MikanCameraPtr m_viewCamera; // scene rendering camera
	IMkScenePtr m_mkScene;
	std::unique_ptr<SelectionOutlineRenderer> m_selectionOutlineRenderer;
	IMkTriangulatedMeshPtr m_compositedFrameQuad; // fullscreen quad for compositor texture
	IMkTriangulatedMeshPtr m_backgroundQuad;      // fullscreen quad for scrolling background
	float m_shaderTime= 0.f;
};

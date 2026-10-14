//-- includes -----
#include "CompositorOutputEditorWindow.h"

#include "App.h"
#include "BoxStencilSystem.h"
#include "CameraComponent.h"
#include "ClientSourceManager.h"
#include "CompositorComponent.h"
#include "CompositorObjectSystem.h"
#include "EditorObjectSystem.h"
#include "EventBus.h"
#include "IMkLineRenderer.h"
#include "IMkGraphicsContext.h"
#include "IMkShaderCode.h"
#include "IMkShaderCache.h"
#include "IMkState.h"
#include "IMkTexture.h"
#include "IMkTextureCache.h"
#include "IMkTriangulatedMesh.h"
#include "InputManager.h"
#include "LocalizationManager.h"
#include "LocText.h"
#include "MainWindow.h"
#include "MikanCamera.h"
#include "MikanModelResourceManager.h"
#include "MikanObject.h"
#include "ObjectSystemRenderQueries.h"
#include "MikanServer.h"
#include "MikanTextRenderer.h"
#include "ModelStencilSystem.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkScene.h"
#include "MkScopedState.h"
#include "MkStateModifiers.h"
#include "MkStateStack.h"
#include "MkGuiContext.h"
#include "MkGuiScopedUpdate.h"
#include "MkWindowEvent.h"
#include "OpenCVManager.h"
#include "PathUtils.h"
#include "ProjectManager.h"
#include "QuadStencilSystem.h"
#include "SceneComponent.h"
#include "SceneObjectSystem.h"
#include "TextStyle.h"
#include "VideoSourceComponent.h"

#include "IMkFontManager.h"

#include <easy/profiler.h>

//-- constants -----
static const int k_compositor_output_window_width= 1280;
static const int k_compositor_output_window_height= 720;

//-- public methods -----
CompositorOutputEditorWindow::CompositorOutputEditorWindow(App* ownerApp)
	: EditorWindow(ownerApp)
{
	shareGraphicsContextWithMainWindow();
}

bool CompositorOutputEditorWindow::startup()
{
	EASY_FUNCTION();

	bool success= true;

	if (success
		&& !startupWindow(locText("nodeEditor.compositorOutputWindowTitle"), k_compositor_output_window_width,
						  k_compositor_output_window_height))
	{
		success= false;
	}

	if (success && !startupGuiContext("compositor_output"))
	{
		success= false;
	}

	if (success && !startupStyleManager())
	{
		success= false;
	}

	if (success && !startupTextureCache())
	{
		success= false;
	}

	if (success && !startupModelResourceManager())
	{
		success= false;
	}

	// Build the compositor frame fullscreen quad (uses built-in RGB material)
	if (success)
	{
		m_compositedFrameQuad= createFullscreenQuadMesh(m_graphicsContext.get(), true, false);
	}

	// Build the "no composited frame" mesh
	if (success)
	{
		MkMaterialConstPtr backgroundMaterial=
			m_graphicsContext->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_PT_PM5544_TEST_CARD);

		m_backgroundQuad= createFullscreenQuadMesh(m_graphicsContext.get(), backgroundMaterial, false);
	}

	// Create a standalone stationary camera for scene rendering
	if (success)
	{
		m_viewCamera= std::make_shared<MikanCamera>();
		m_viewCamera->setName("compositor output camera");
		m_viewCamera->setCameraMovementMode(eCameraMovementMode::stationary);

		// Create a mikan scene for 3d rendering
		m_mkScene= std::make_shared<MkScene>();
		m_selectionOutlineRenderer= std::make_unique<SelectionOutlineRenderer>(m_graphicsContext.get());
	}

	// Listen for scene activation and disposal
	auto sceneSystem= getProjectManager()->getSystemOfType<SceneObjectSystem>();
	if (sceneSystem)
	{
		sceneSystem->OnSceneActivated+= MakeDelegate(this, &CompositorOutputEditorWindow::onSceneActivated);
		sceneSystem->OnComponentDisposed+= MakeDelegate(this, &CompositorOutputEditorWindow::onSceneComponentDisposed);
	}
	auto compositorSystem= getProjectManager()->getSystemOfType<CompositorObjectSystem>();
	if (compositorSystem)
	{
		compositorSystem->OnComponentDisposed+=
			MakeDelegate(this, &CompositorOutputEditorWindow::onCompositorComponentDisposed);
	}

	m_boxStencilSystem= getProjectManager()->getSystemOfType<BoxStencilSystem>();
	m_quadStencilSystem= getProjectManager()->getSystemOfType<QuadStencilSystem>();
	m_modelStencilSystem= getProjectManager()->getSystemOfType<ModelStencilSystem>();

	return success;
}

bool CompositorOutputEditorWindow::bindSceneComponent(SceneComponentPtr sceneComponent)
{
	// Unregister from old scene
	SceneComponentPtr oldScene= m_sceneComponent.lock();
	if (oldScene)
	{
		oldScene->getSceneComponentDefinition()->OnPropertyChanged-=
			MakeDelegate(this, &CompositorOutputEditorWindow::onSceneDefinitionChanged);
	}

	m_sceneComponent= sceneComponent;

	if (sceneComponent)
	{
		// Listen for display compositor changes on the new scene's definition
		sceneComponent->getSceneComponentDefinition()->OnPropertyChanged+=
			MakeDelegate(this, &CompositorOutputEditorWindow::onSceneDefinitionChanged);

		rebindCompositorFromScene();
	}

	return true;
}

void CompositorOutputEditorWindow::rebindCompositorFromScene()
{
	SceneComponentPtr sceneComponent= m_sceneComponent.lock();
	if (!sceneComponent)
		return;

	MikanCompositorID compositorId= sceneComponent->getSceneComponentDefinition()->getDisplayCompositorId();

	CompositorComponentPtr compositor;
	if (compositorId != INVALID_MIKAN_ID)
	{
		auto compositorSystem= getProjectManager()->getSystemOfType<CompositorObjectSystem>();
		if (compositorSystem)
			compositor= compositorSystem->getCompositorById(compositorId);
	}

	m_compositorComponent= compositor;

	// Apply video source camera intrinsics to the view camera
	if (compositor && m_viewCamera)
	{
		VideoSourceComponentPtr videoSource= compositor->getVideoSourceComponent();
		if (videoSource)
		{
			MikanVideoSourceIntrinsics cameraIntrinsics;
			videoSource->getCameraIntrinsics(cameraIntrinsics);
			m_viewCamera->applyMonoCameraIntrinsics(&cameraIntrinsics);
		}
	}

	// Update window title
	std::string compositorName= compositor ? compositor->getName() : locText("nodeEditor.noCompositor");
	setTitle(locFormat("nodeEditor.compositorOutputTitleFmt", compositorName.c_str()));
}

void CompositorOutputEditorWindow::onSceneActivated(SceneComponentPtr newScene) { bindSceneComponent(newScene); }

void CompositorOutputEditorWindow::onSceneDefinitionChanged(CommonConfigPtr configPtr,
															const ConfigPropertyChangeSet& changedPropertySet)
{
	if (changedPropertySet.hasPropertyName(SceneComponentDefinition::k_displayCompositorIdPropertyId))
	{
		rebindCompositorFromScene();
	}
}

void CompositorOutputEditorWindow::onSceneComponentDisposed(MikanObjectSystemPtr objectSystem,
															MikanComponentConstPtr component)
{
	SceneComponentPtr sceneComponent= m_sceneComponent.lock();
	if (sceneComponent && component->getComponentId() == sceneComponent->getComponentId())
	{
		m_mkWindowContext->requestClose();
	}
}

void CompositorOutputEditorWindow::onCompositorComponentDisposed(MikanObjectSystemPtr objectSystem,
																 MikanComponentConstPtr component)
{
	if (m_compositorComponent.lock() && component->getComponentId() == m_compositorComponent.lock()->getComponentId())
	{
		m_mkWindowContext->requestClose();
	}
}

void CompositorOutputEditorWindow::update(float deltaSeconds)
{
	EASY_FUNCTION();

	m_shaderTime+= deltaSeconds;

	// Push ImGui update scope (handles events, builds draw lists)
	MkGuiScopedUpdate scopedCtx(*m_guiContext);

	// Process SDL events
	m_mkWindowContext->handleEvents(this);

	// Update compositor camera transform each frame
	CompositorComponentPtr compositor= m_compositorComponent.lock();
	if (compositor)
	{
		CameraComponentPtr cameraComponent= compositor->getCameraComponent();
		if (cameraComponent && m_viewCamera)
		{
			// World space: this view camera renders scene renderables, whose model
			// matrices are world transforms
			glm::mat4 cameraXform;
			if (cameraComponent->getWorldSpaceAperturePose(cameraXform))
			{
				m_viewCamera->setCameraTransform(cameraXform);
			}
		}
	}
}

void CompositorOutputEditorWindow::render()
{
	EASY_FUNCTION();

	IMkGraphicsContext* gfx= m_graphicsContext.get();
	MkStateStack& stateStack= gfx->getMkStateStack();

	// Clear the window
	gfx->renderBegin();

	{
		MkScopedState scopedState= stateStack.createScopedState("CompositorOutput renderScene");
		IMkState* glState= scopedState.getStackState();

		mkStateSetViewport(glState, 0, 0, (int)m_mkWindowContext->getWidth(), (int)m_mkWindowContext->getHeight());

		CompositorComponentPtr compositor= m_compositorComponent.lock();
		IMkTextureConstPtr frameTexture= compositor ? compositor->getCompositedFrameTexture() : nullptr;

		// --- Layer 1: Compositor frame or no-video background ---
		if (frameTexture && m_compositedFrameQuad)
		{
			// Render compositor output frame fullscreen
			MkMaterialInstancePtr materialInstance= m_compositedFrameQuad->getMaterialInstance();
			MkMaterialConstPtr material= materialInstance->getMaterial();

			if (auto materialBinding= material->bindMaterial())
			{
				materialInstance->setTextureBySemantic(eUniformSemantic::rgbTexture, frameTexture);

				if (auto materialInstanceBinding= materialInstance->bindMaterialInstance(materialBinding))
				{
					MkScopedState frameScopedState= stateStack.createScopedState("CompositorFrame");
					frameScopedState.getStackState()->disableFlag(eMkStateFlagType::depthTest);

					m_compositedFrameQuad->drawElements();
				}
			}
		}
		else if (m_backgroundQuad)
		{
			MkMaterialInstancePtr materialInstance= m_backgroundQuad->getMaterialInstance();
			MkMaterialConstPtr material= materialInstance->getMaterial();

			if (auto materialBinding= material->bindMaterial())
			{
				// TODO: "Time" and "ScreenSize" are uniforms that all materials
				//  should have available by default in the graphics context
				const double currentTimeSeconds= getOwnerApp()->getSecondsSinceAppStart();
				const float shaderTime= (float)fmodf(currentTimeSeconds, 1000.0);
				const float screenWidth= m_mkWindowContext->getWidth();
				const float screenHeight= m_mkWindowContext->getHeight();
				const glm::vec2 screenSize(screenWidth, screenHeight);

				materialInstance->setVec2BySemantic(eUniformSemantic::screenSize, screenSize);
				materialInstance->setFloatBySemantic(eUniformSemantic::floatConstant0, shaderTime);

				if (auto materialInstanceBinding= materialInstance->bindMaterialInstance(materialBinding))
				{
					MkScopedState bgScopedState= stateStack.createScopedState("MainTargetDepthRender");
					IMkState* bgState= bgScopedState.getStackState();

					bgState->disableFlag(eMkStateFlagType::depthTest);
					bgState->disableFlag(eMkStateFlagType::cullFace);
					bgState->enableFlag(eMkStateFlagType::texture2d);
					bgState->enableFlag(eMkStateFlagType::blend);

					mkStateSetBlendEquation(bgState, eMkBlendEquation::ADD);
					mkStateSetBlendFunc(bgState, eMkBlendFunction::SRC_ALPHA, eMkBlendFunction::ONE_MINUS_SRC_ALPHA);

					m_backgroundQuad->drawElements();
				}
			}
		}

		// --- Layer 2: Editor scene overlay from compositor camera perspective ---
		if (compositor && m_viewCamera)
		{
			MainWindow* mainWindow= getMainWindow();
			ProjectManagerPtr projectManager= mainWindow->getProjectManager();
			auto sceneSystem= projectManager->getSystemOfType<SceneObjectSystem>();
			auto editorSystem= projectManager->getSystemOfType<EditorObjectSystem>();
			if (sceneSystem && editorSystem)
			{
				SceneComponentConstPtr currentScene= sceneSystem->getCurrentScene();
				const EditorSettings& editorConfig= editorSystem->getEditorSettings();

				if (currentScene)
				{
					// Clear the scene of any previously rendered instances. Done even while the
					// overlay is disabled, so toggling it off does not leave the scene holding
					// renderables belonging to since-deleted actors.
					m_mkScene->removeAllInstances();

					// This window shows what the shot looks like, so the editor's authoring
					// overlay stays out of it unless "Debug Render in Compositor" is explicitly
					// enabled. The per-object "Render <X>" flags still apply on top of it.
					if (editorConfig.bDebugRenderInCompositor)
					{
						// Add scene actors to the MkScene for rendering, minus the scenes
						// the project viewport is not drawing either
						if (editorConfig.bDebugRenderBoxStencils)
						{
							addAllRenderablesToMkScene(m_boxStencilSystem.lock(), m_mkScene, isSceneGeometryRendered);
						}

						if (editorConfig.bDebugRenderModelStencils)
						{
							addAllRenderablesToMkScene(m_modelStencilSystem.lock(), m_mkScene, isSceneGeometryRendered);
						}

						if (editorConfig.bDebugRenderQuadStencils)
						{
							addAllRenderablesToMkScene(m_quadStencilSystem.lock(), m_mkScene, isSceneGeometryRendered);
						}

						// Clear the depth buffer so the overlay scene draws over the composited frame
						mkStateClearBuffer(stateStack.getCurrentState(), eMkClearFlags::depth);

						// Render the 3d scene
						m_mkScene->render(m_viewCamera, stateStack);

						// Outline the selection over it. Model stencils are the only outlined
						// objects this overlay draws.
						ModelStencilSystemPtr modelStencilSystem= m_modelStencilSystem.lock();
						if (editorConfig.bDebugRenderModelStencils && modelStencilSystem)
						{
							std::vector<SelectionOutlineTarget> outlineTargets;
							editorSystem->getSelectionOutlineTargets(
								[&modelStencilSystem](MikanObjectConstPtr object)
								{
									return object->getOwnerSystem() == modelStencilSystem
										   && isSceneGeometryRendered(object);
								},
								outlineTargets);

							m_selectionOutlineRenderer->render(m_viewCamera,
															   glm::i32vec4(0, 0, (int)m_mkWindowContext->getWidth(),
																			(int)m_mkWindowContext->getHeight()),
															   outlineTargets);
						}
					}
				}
			}
		}
	}

	// --- Layer 3: HUD text ---
	{
		MkScopedState scopedState= stateStack.createScopedState("CompositorOutput renderUI");
		IMkState* glState= scopedState.getStackState();
		mkStateSetViewport(glState, 0, 0, (int)m_mkWindowContext->getWidth(), (int)m_mkWindowContext->getHeight());

		CompositorComponentPtr compositor= m_compositorComponent.lock();

		// Upper left: compositor name | video source name
		{
			TextStyle style= getDefaultTextStyle();
			style.horizontalAlignment= eHorizontalTextAlignment::Left;
			style.verticalAlignment= eVerticalTextAlignment::Top;

			std::string compositorName= compositor ? compositor->getName() : locText("nodeEditor.noCompositor");
			std::string videoSourceName;
			if (compositor)
			{
				VideoSourceComponentPtr vs= compositor->getVideoSourceComponent();
				if (vs)
					videoSourceName= vs->getName();
			}

			drawTextAtScreenPosition(gfx, style, glm::vec2(1.f, 1.f), L"%hs | %hs", compositorName.c_str(),
									 videoSourceName.c_str());
		}

		// Lower right: FPS
		{
			TextStyle style= getDefaultTextStyle();
			style.horizontalAlignment= eHorizontalTextAlignment::Right;
			style.verticalAlignment= eVerticalTextAlignment::Bottom;

			drawTextAtScreenPosition(
				gfx, style, glm::vec2(m_mkWindowContext->getWidth() - 1.f, m_mkWindowContext->getHeight() - 1.f),
				L"%.1ffps", App::getInstance()->getFPS());
		}

		// Submit ImGui draw data
		m_guiContext->submitDrawData();
	}

	// Render any 2D line segments emitted by the AppStage renderUI phase
	gfx->getLineRenderer()->render(true);

	// Render any glyphs emitted by the AppStage renderUI phase
	gfx->getTextRenderer()->render();

	// Finalize rendering
	gfx->renderEnd();

	// Present the rendered frame
	presentFrame();
}

void CompositorOutputEditorWindow::shutdown()
{
	// Unregister scene delegates before clearing the reference
	SceneComponentPtr sceneComponent= m_sceneComponent.lock();
	if (sceneComponent)
	{
		sceneComponent->getSceneComponentDefinition()->OnPropertyChanged-=
			MakeDelegate(this, &CompositorOutputEditorWindow::onSceneDefinitionChanged);
	}

	// Stop listening for scene activation and disposal
	auto sceneSystem= getProjectManager()->getSystemOfType<SceneObjectSystem>();
	if (sceneSystem)
	{
		sceneSystem->OnSceneActivated-= MakeDelegate(this, &CompositorOutputEditorWindow::onSceneActivated);
		sceneSystem->OnComponentDisposed-= MakeDelegate(this, &CompositorOutputEditorWindow::onSceneComponentDisposed);
	}
	auto compositorSystem= getProjectManager()->getSystemOfType<CompositorObjectSystem>();
	if (compositorSystem)
	{
		compositorSystem->OnComponentDisposed-=
			MakeDelegate(this, &CompositorOutputEditorWindow::onCompositorComponentDisposed);
	}

	m_sceneComponent.reset();
	m_compositorComponent.reset();

	m_compositedFrameQuad= nullptr;
	m_backgroundQuad= nullptr;
	m_viewCamera= nullptr;
	m_mkScene= nullptr;
	m_selectionOutlineRenderer= nullptr;

	shutdownModelResourceManager();
	shutdownTextureCache();
	shutdownStyleManager();
	shutdownGuiContext();
	shutdownWindow();
}

// -- IMkWindowEventListener
bool CompositorOutputEditorWindow::onWindowEvent(const MkWindowEvent& event)
{
	return m_guiContext->onWindowEvent(event);
}

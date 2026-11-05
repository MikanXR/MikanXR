#include "GuiPanel_CompositorOutput.h"
#include "App.h"
#include "AppStage.h"
#include "BoxStencilSystem.h"
#include "CameraComponent.h"
#include "CompositorComponent.h"
#include "CompositorObjectSystem.h"
#include "EditorObjectSystem.h"
#include "IMkFrameBuffer.h"
#include "IMkGraphicsContext.h"
#include "IMkShaderCache.h"
#include "IMkState.h"
#include "IMkTexture.h"
#include "IMkTriangulatedMesh.h"
#include "LocText.h"
#include "Logger.h"
#include "MikanCamera.h"
#include "MikanObject.h"
#include "ModelStencilSystem.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkScene.h"
#include "MkScopedObjectBinding.h"
#include "MkScopedState.h"
#include "MkStateModifiers.h"
#include "MkStateStack.h"
#include "ObjectSystemRenderQueries.h"
#include "QuadStencilSystem.h"
#include "SceneComponent.h"
#include "SceneObjectSystem.h"
#include "VideoSourceComponent.h"

#include "imgui.h"

#include <easy/profiler.h>

#include <cmath>
#include <string>
#include <vector>

namespace
{
// The test card's resolution while no composited frame is available
const int k_testCardWidth= 1280;
const int k_testCardHeight= 720;
const glm::vec4 k_compositorOutputClearColor(0.f, 0.f, 0.f, 1.f);
// Inset of the HUD text from the image's top-left corner, in pixels
const float k_hudInset= 4.f;
} // namespace

bool GuiPanel_CompositorOutput::init(IMkGraphicsContext* graphicsContext)
{
	EASY_FUNCTION();

	m_graphicsContext= graphicsContext;
	if (m_graphicsContext == nullptr)
		return false;

	// Build the compositor frame fullscreen quad (uses built-in RGB material)
	m_compositedFrameQuad= createFullscreenQuadMesh(m_graphicsContext, true, false);

	// Build the "no composited frame" mesh
	MkMaterialConstPtr backgroundMaterial=
		m_graphicsContext->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_PT_PM5544_TEST_CARD);
	m_backgroundQuad= createFullscreenQuadMesh(m_graphicsContext, backgroundMaterial, false);

	// Create a standalone stationary camera for scene rendering
	m_viewCamera= std::make_shared<MikanCamera>();
	m_viewCamera->setName("compositor output camera");
	m_viewCamera->setCameraMovementMode(eCameraMovementMode::stationary);

	// Create a mikan scene for 3d rendering
	m_mkScene= std::make_shared<MkScene>();
	m_selectionOutlineRenderer= std::make_unique<SelectionOutlineRenderer>(m_graphicsContext);

	m_editorSystem= m_ownerAppStage->getObjectSystemOfType<EditorObjectSystem>();
	m_boxStencilSystem= m_ownerAppStage->getObjectSystemOfType<BoxStencilSystem>();
	m_quadStencilSystem= m_ownerAppStage->getObjectSystemOfType<QuadStencilSystem>();
	m_modelStencilSystem= m_ownerAppStage->getObjectSystemOfType<ModelStencilSystem>();

	// Listen for scene activation and disposal
	SceneObjectSystemPtr sceneSystem= m_ownerAppStage->getObjectSystemOfType<SceneObjectSystem>();
	m_sceneSystem= sceneSystem;
	if (sceneSystem)
	{
		sceneSystem->OnSceneActivated+= MakeDelegate(this, &GuiPanel_CompositorOutput::onSceneActivated);
		sceneSystem->OnComponentDisposed+= MakeDelegate(this, &GuiPanel_CompositorOutput::onSceneComponentDisposed);
	}

	CompositorObjectSystemPtr compositorSystem= m_ownerAppStage->getObjectSystemOfType<CompositorObjectSystem>();
	m_compositorSystem= compositorSystem;
	if (compositorSystem)
	{
		compositorSystem->OnComponentDisposed+=
			MakeDelegate(this, &GuiPanel_CompositorOutput::onCompositorComponentDisposed);
	}

	// Follow the current scene from here on
	if (sceneSystem)
	{
		bindSceneComponent(sceneSystem->getCurrentScene());
	}

	return true;
}

void GuiPanel_CompositorOutput::dispose()
{
	// Unregister scene delegates before clearing the reference
	SceneComponentPtr sceneComponent= m_sceneComponent.lock();
	if (sceneComponent)
	{
		sceneComponent->getSceneComponentDefinition()->OnPropertyChanged-=
			MakeDelegate(this, &GuiPanel_CompositorOutput::onSceneDefinitionChanged);
	}

	// Stop listening for scene activation and disposal
	if (SceneObjectSystemPtr sceneSystem= m_sceneSystem.lock())
	{
		sceneSystem->OnSceneActivated-= MakeDelegate(this, &GuiPanel_CompositorOutput::onSceneActivated);
		sceneSystem->OnComponentDisposed-= MakeDelegate(this, &GuiPanel_CompositorOutput::onSceneComponentDisposed);
	}
	if (CompositorObjectSystemPtr compositorSystem= m_compositorSystem.lock())
	{
		compositorSystem->OnComponentDisposed-=
			MakeDelegate(this, &GuiPanel_CompositorOutput::onCompositorComponentDisposed);
	}

	m_sceneSystem.reset();
	m_compositorSystem.reset();
	m_sceneComponent.reset();
	m_compositorComponent.reset();

	if (m_frameBuffer)
	{
		m_frameBuffer->disposeResources();
		m_frameBuffer= nullptr;
	}
	m_compositedFrameQuad= nullptr;
	m_backgroundQuad= nullptr;
	m_viewCamera= nullptr;
	m_mkScene= nullptr;
	m_selectionOutlineRenderer= nullptr;

	GuiPanel::dispose();
}

void GuiPanel_CompositorOutput::bindSceneComponent(SceneComponentPtr sceneComponent)
{
	// Unregister from old scene
	SceneComponentPtr oldScene= m_sceneComponent.lock();
	if (oldScene)
	{
		oldScene->getSceneComponentDefinition()->OnPropertyChanged-=
			MakeDelegate(this, &GuiPanel_CompositorOutput::onSceneDefinitionChanged);
	}

	m_sceneComponent= sceneComponent;
	m_compositorComponent.reset();

	if (sceneComponent)
	{
		// Listen for display compositor changes on the new scene's definition
		sceneComponent->getSceneComponentDefinition()->OnPropertyChanged+=
			MakeDelegate(this, &GuiPanel_CompositorOutput::onSceneDefinitionChanged);

		rebindCompositorFromScene();
	}
}

void GuiPanel_CompositorOutput::rebindCompositorFromScene()
{
	SceneComponentPtr sceneComponent= m_sceneComponent.lock();
	if (!sceneComponent)
		return;

	MikanCompositorID compositorId= sceneComponent->getSceneComponentDefinition()->getDisplayCompositorId();

	CompositorComponentPtr compositor;
	if (compositorId != INVALID_MIKAN_ID)
	{
		if (CompositorObjectSystemPtr compositorSystem= m_compositorSystem.lock())
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
}

void GuiPanel_CompositorOutput::onSceneActivated(SceneComponentPtr newScene) { bindSceneComponent(newScene); }

void GuiPanel_CompositorOutput::onSceneDefinitionChanged(CommonConfigPtr configPtr,
														 const ConfigPropertyChangeSet& changedPropertySet)
{
	if (changedPropertySet.hasPropertyName(SceneComponentDefinition::k_displayCompositorIdPropertyId))
	{
		rebindCompositorFromScene();
	}
}

void GuiPanel_CompositorOutput::onSceneComponentDisposed(MikanObjectSystemPtr objectSystem,
														 MikanComponentConstPtr component)
{
	SceneComponentPtr sceneComponent= m_sceneComponent.lock();
	if (sceneComponent && component->getComponentId() == sceneComponent->getComponentId())
	{
		sceneComponent->getSceneComponentDefinition()->OnPropertyChanged-=
			MakeDelegate(this, &GuiPanel_CompositorOutput::onSceneDefinitionChanged);
		m_sceneComponent.reset();
		m_compositorComponent.reset();
	}
}

void GuiPanel_CompositorOutput::onCompositorComponentDisposed(MikanObjectSystemPtr objectSystem,
															  MikanComponentConstPtr component)
{
	CompositorComponentPtr compositor= m_compositorComponent.lock();
	if (compositor && component->getComponentId() == compositor->getComponentId())
	{
		m_compositorComponent.reset();
	}
}

void GuiPanel_CompositorOutput::onGui()
{
	EASY_FUNCTION();

	if (m_graphicsContext == nullptr)
		return;

	updateViewCamera();

	// The frame buffer matches the composited frame, so resizing the panel never
	// changes what the compositor outputs
	CompositorComponentPtr compositor= m_compositorComponent.lock();
	IMkTextureConstPtr frameTexture= compositor ? compositor->getCompositedFrameTexture() : nullptr;
	int frameWidth= k_testCardWidth;
	int frameHeight= k_testCardHeight;
	if (frameTexture && frameTexture->getTextureWidth() > 0 && frameTexture->getTextureHeight() > 0)
	{
		frameWidth= (int)frameTexture->getTextureWidth();
		frameHeight= (int)frameTexture->getTextureHeight();
	}

	if (!ensureFrameBuffer(frameWidth, frameHeight))
		return;

	renderFrameBuffer();

	// Letterbox the frame into the panel, centered
	const ImVec2 available= ImGui::GetContentRegionAvail();
	if (available.x < 1.f || available.y < 1.f)
		return;

	const float frameAspect= (float)frameWidth / (float)frameHeight;
	ImVec2 imageSize(available.x, available.x / frameAspect);
	if (imageSize.y > available.y)
	{
		imageSize= ImVec2(available.y * frameAspect, available.y);
	}

	const ImVec2 cursorPos= ImGui::GetCursorPos();
	ImGui::SetCursorPos(ImVec2(cursorPos.x + std::floor((available.x - imageSize.x) * 0.5f),
							   cursorPos.y + std::floor((available.y - imageSize.y) * 0.5f)));

	IMkTexturePtr colorTexture= m_frameBuffer->getColorTexture();
	if (!colorTexture || colorTexture->getGlTextureId() == 0)
		return;

	// The image widget alpha blends, and the overlay's blended geometry lowers the
	// frame buffer's alpha, so an opaque backing keeps the panel from showing through
	const ImVec2 imageMin= ImGui::GetCursorScreenPos();
	ImGui::GetWindowDrawList()->AddRectFilled(imageMin, ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y),
											  IM_COL32_BLACK);

	// The framebuffer texture is bottom-up, so flip V for the top-down image widget
	ImGui::Image((void*)(intptr_t)colorTexture->getGlTextureId(), imageSize, ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));

	// Ctrl+click focuses the camera's lens on the surface under the cursor
	const ImGuiIO& io= ImGui::GetIO();
	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.KeyCtrl)
	{
		pickFocusAtImagePixel(glm::vec2(io.MousePos.x - imageMin.x, io.MousePos.y - imageMin.y),
							  glm::vec2(imageSize.x, imageSize.y));
	}

	// Upper left: compositor name | video source name
	{
		std::string compositorName= compositor ? compositor->getName() : locText("nodeEditor.noCompositor");
		std::string videoSourceName;
		if (compositor)
		{
			VideoSourceComponentPtr vs= compositor->getVideoSourceComponent();
			if (vs)
				videoSourceName= vs->getName();
		}

		const std::string hudText= compositorName + " | " + videoSourceName;
		ImGui::GetWindowDrawList()->AddText(ImVec2(imageMin.x + k_hudInset, imageMin.y + k_hudInset),
											ImGui::GetColorU32(ImGuiCol_Text), hudText.c_str());
	}
}

void GuiPanel_CompositorOutput::updateViewCamera()
{
	CompositorComponentPtr compositor= m_compositorComponent.lock();
	if (!compositor || !m_viewCamera)
		return;

	CameraComponentPtr cameraComponent= compositor->getCameraComponent();
	if (!cameraComponent)
		return;

	// World space: this view camera renders scene renderables, whose model
	// matrices are world transforms. They are drawn over the composited frame,
	// so from the pose that frame was composited with, the camera's own until
	// one has been.
	glm::mat4 cameraXform;
	if (compositor->getCompositedFrameWorldAperturePose(cameraXform)
		|| cameraComponent->getWorldSpaceAperturePose(cameraXform))
	{
		m_viewCamera->setCameraTransform(cameraXform);
	}
}

bool GuiPanel_CompositorOutput::ensureFrameBuffer(int width, int height)
{
	if (m_frameBuffer && m_frameBuffer->isValid() && m_frameBuffer->getWidth() == width
		&& m_frameBuffer->getHeight() == height)
	{
		return true;
	}

	if (m_frameBuffer)
	{
		m_frameBuffer->disposeResources();
	}

	// The COLOR type's depth/stencil format matches the selection outline mask's,
	// which the outline pass's depth copy needs
	m_frameBuffer= createMkFrameBuffer("compositor_output");
	m_frameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_frameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::RGBA);
	m_frameBuffer->setSize(width, height);
	m_frameBuffer->setClearColor(k_compositorOutputClearColor);

	if (!m_frameBuffer->createResources())
	{
		MIKAN_LOG_ERROR("GuiPanel_CompositorOutput::ensureFrameBuffer")
			<< "Failed to create the " << width << "x" << height << " compositor output frame buffer";
		m_frameBuffer= nullptr;
		return false;
	}

	return true;
}

void GuiPanel_CompositorOutput::renderFrameBuffer()
{
	EASY_FUNCTION();

	MkStateStack& stateStack= m_graphicsContext->getMkStateStack();
	MkScopedState scopedState= stateStack.createScopedState("CompositorOutput renderScene");
	IMkState* glState= scopedState.getStackState();

	// Binding sets the viewport to the frame buffer and clears it to the clear color
	MkScopedObjectBinding frameBufferBinding(glState, "CompositorOutput", m_frameBuffer);
	if (!frameBufferBinding)
		return;

	const int frameWidth= m_frameBuffer->getWidth();
	const int frameHeight= m_frameBuffer->getHeight();

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
			const double currentTimeSeconds= App::getInstance()->getSecondsSinceAppStart();
			const float shaderTime= (float)fmodf(currentTimeSeconds, 1000.0);
			const glm::vec2 screenSize((float)frameWidth, (float)frameHeight);

			materialInstance->setVec2BySemantic(eUniformSemantic::screenSize, screenSize);
			materialInstance->setFloatBySemantic(eUniformSemantic::floatConstant0, shaderTime);

			if (auto materialInstanceBinding= materialInstance->bindMaterialInstance(materialBinding))
			{
				MkScopedState bgScopedState= stateStack.createScopedState("CompositorOutput testCard");
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
	SceneComponentPtr sceneComponent= m_sceneComponent.lock();
	EditorObjectSystemPtr editorSystem= m_editorSystem.lock();
	if (compositor && sceneComponent && editorSystem && m_viewCamera)
	{
		const EditorSettings& editorConfig= editorSystem->getEditorSettings();

		// Clear the scene of any previously rendered instances. Done even while the
		// overlay is disabled, so toggling it off does not leave the scene holding
		// renderables belonging to since-deleted actors.
		m_mkScene->removeAllInstances();

		// This panel shows what the shot looks like, so the editor's authoring
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
					{ return object->getOwnerSystem() == modelStencilSystem && isSceneGeometryRendered(object); },
					outlineTargets);

				m_selectionOutlineRenderer->render(m_viewCamera, glm::i32vec4(0, 0, frameWidth, frameHeight),
												   outlineTargets);
			}
		}
	}
}

void GuiPanel_CompositorOutput::pickFocusAtImagePixel(const glm::vec2& imagePixel, const glm::vec2& imageSize)
{
	CompositorComponentPtr compositor= m_compositorComponent.lock();
	CameraComponentPtr cameraComponent= compositor ? compositor->getCameraComponent() : nullptr;
	if (!cameraComponent)
		return;

	int videoWidth= 0;
	int videoHeight= 0;
	if (!cameraComponent->getAperturePixelDimensions(videoWidth, videoHeight) || imageSize.x <= 0.f
		|| imageSize.y <= 0.f)
		return;

	// The composite fills the presented image
	const glm::vec2 videoPixel(imagePixel.x * (float)videoWidth / imageSize.x,
							   imagePixel.y * (float)videoHeight / imageSize.y);

	// Only stencils that shape this composite stand in for the surfaces it shows
	auto ownsObject= [compositor](MikanObjectConstPtr object) { return compositor->ownsSceneObject(object); };

	float focusDistance= 0.f;
	if (cameraComponent->computeFocusDistanceAtVideoPixel(videoPixel, ownsObject, focusDistance))
	{
		cameraComponent->getCameraDefinition()->setFocusDistance(focusDistance);
	}
	else
	{
		MIKAN_LOG_INFO("GuiPanel_CompositorOutput::pickFocusAtImagePixel")
			<< "No stencil under (" << videoPixel.x << ", " << videoPixel.y << "), focus unchanged";
	}
}

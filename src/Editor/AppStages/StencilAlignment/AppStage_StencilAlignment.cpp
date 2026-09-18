//-- includes -----
#include "StencilAlignment/AppStage_StencilAlignment.h"
#include "StencilAlignment/GuiPanel_StencilAlignment.h"
#include "App.h"
#include "CameraComponent.h"
#include "Colors.h"
#include "CalibrationRenderHelpers.h"
#include "MikanCamera.h"
#include "IMkFrameBuffer.h"
#include "IMkGraphicsContext.h"
#include "MikanLineRenderer.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MikanModelResourceManager.h"
#include "MikanRenderModelResource.h"
#include "MkScopedObjectBinding.h"
#include "IMkStaticMeshInstance.h"
#include "MkScene.h"
#include "MkStateModifiers.h"
#include "MkStateStack.h"
#include "MikanTextRenderer.h"
#include "IMkLineRenderer.h"
#include "IMkTextRenderer.h"
#include "IMkTriangulatedMesh.h"
#include "MikanViewport.h"
#include "IMkWireframeMesh.h"
#include "InputManager.h"
#include "MainWindow.h"
#include "MathTypeConversion.h"
#include "MathUtility.h"
#include "MeshColliderComponent.h"
#include "ModelStencilComponent.h"
#include "ProjectConfig.h"
#include "StencilUtils.h"
#include "StencilComponent.h"
#include "StringUtils.h"
#include "TextStyle.h"
#include "VideoFrameDistortionView.h"
#include "VRObjectSystem.h"
#include "VRDeviceComponent.h"
#include "VideoSourceComponent.h"

#include "glm/gtc/quaternion.hpp"

#include "MkGuiScopedWindow.h"

#include "imgui.h"
#include "MkGuiDrawUtils.h"

//-- statics ----
const char* AppStage_StencilAlignment::APP_STAGE_NAME= "StencilAlignment";

//-- public methods -----
AppStage_StencilAlignment::AppStage_StencilAlignment(IEditorWindow* ownerWindow)
	: AppStage(ownerWindow, AppStage_StencilAlignment::APP_STAGE_NAME)
	, m_videoSourceComponent()
	, m_stencilAligner(nullptr)
	, m_monoDistortionView(nullptr)
	, m_scene(std::make_shared<MkScene>())
	, m_mkCamera(nullptr)
	, m_frameBuffer(createMkFrameBuffer())
	, m_fullscreenRGBQuad(createFullscreenQuadMesh(ownerWindow->getGraphicsContext().get(), false))
{
}

AppStage_StencilAlignment::~AppStage_StencilAlignment() {}

void AppStage_StencilAlignment::enter()
{
	AppStage::enter();

	assert(m_cameraComponent);
	m_videoSourceComponent= m_cameraComponent->getVideoSourceComponent();
	assert(m_videoSourceComponent);

	// Listen for mouse ray events
	MikanViewportPtr viewport= getFirstViewport();
	viewport->OnMouseRayChanged+= MakeDelegate(this, &AppStage_StencilAlignment::onMouseRayChanged);
	viewport->OnMouseRayButtonUp+= MakeDelegate(this, &AppStage_StencilAlignment::onMouseRayButtonUp);

	// Create a new camera to view the scene
	m_mkCamera= viewport->getCurrentMikanCamera();
	m_mkCamera->setCameraMovementMode(eCameraMovementMode::stationary);

	// Center the orbit camera on the stencil model
	updateVRCamera();

	// Make sure the camera doing the 3d rendering has the same
	// fov and aspect ration as the real camera
	MikanVideoSourceIntrinsics cameraIntrinsics;
	m_videoSourceComponent->getCameraIntrinsics(cameraIntrinsics);
	m_mkCamera->applyMonoCameraIntrinsics(&cameraIntrinsics);

	// Create a frame buffer to render the scene into using the resolution and fov from the camera intrinsics
	const MikanMonoIntrinsics& monoIntrinsics= cameraIntrinsics.getMonoIntrinsics();
	m_frameBuffer->setName("StencilAlignment");
	m_frameBuffer->setSize(monoIntrinsics.pixel_width, monoIntrinsics.pixel_height);
	m_frameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_frameBuffer->createResources();

	// Add the stencil's wireframe meshes to the scene
	if (m_targetStencilComponent)
	{
		for (IMkStaticMeshInstancePtr meshInstance : m_targetStencilComponent->getWireframeMeshes())
		{
			m_scene->addInstance(meshInstance);
		}
	}

	// Create the distortion view (acts as stream ownership token)
	m_monoDistortionView= new VideoFrameDistortionView(m_videoSourceComponent, eVideoFrameProcessorMode::CALIBRATION);
	m_monoDistortionView->setVideoDisplayMode(eVideoDisplayMode::mode_undistored);

	// Register as a stream consumer — update() drives the retry loop
	m_videoSourceComponent->startVideoStream(m_monoDistortionView);
	eStencilAlignmentMenuState newState= eStencilAlignmentMenuState::pendingVideoStart;

	// Create app stage GUI panels
	// (Auto cleaned up on app state exit)
	{
		m_calibrationPanel= addGuiPanel<GuiPanel_StencilAlignment>();
		m_calibrationPanel->OnOkEvent= [this]() { onOkEvent(); };
		m_calibrationPanel->OnRedoEvent= [this]() { onRedoEvent(); };
		m_calibrationPanel->OnCancelEvent= [this]() { onCancelEvent(); };
	}

	setMenuState(newState);
}

void AppStage_StencilAlignment::exit()
{
	setMenuState(eStencilAlignmentMenuState::inactive);

	// Stop listening to mouse ray events
	MikanViewportPtr viewport= getFirstViewport();
	viewport->OnMouseRayChanged-= MakeDelegate(this, &AppStage_StencilAlignment::onMouseRayChanged);
	viewport->OnMouseRayButtonUp-= MakeDelegate(this, &AppStage_StencilAlignment::onMouseRayButtonUp);

	// Forget about the stencil we were aligning
	m_targetStencilComponent= nullptr;

	// Forget about the camera
	m_mkCamera= nullptr;

	// Forget about the stencil model we added
	m_scene->removeAllInstances();

	// Free the aligner
	if (m_stencilAligner != nullptr)
	{
		delete m_stencilAligner;
		m_stencilAligner= nullptr;
	}

	if (m_videoSourceComponent)
	{
		// Release stream ownership then free the view
		if (m_monoDistortionView != nullptr)
		{
			m_videoSourceComponent->stopVideoStream(m_monoDistortionView);
			delete m_monoDistortionView;
			m_monoDistortionView= nullptr;
		}
		m_videoSourceComponent= nullptr;
	}

	AppStage::exit();
}

void AppStage_StencilAlignment::setupStencilAligner()
{
	// Create a aligner to calibrate the stencil
	// (m_monoDistortionView is already created in enter())
	m_stencilAligner= new StencilAligner(m_cameraComponent, m_monoDistortionView, m_targetStencilComponent);
}

void AppStage_StencilAlignment::updateXRCamera()
{
	// Update the transform of the camera so that vr models align over the tracking puck.
	// World space: the stencil wireframe meshes in m_scene carry world transforms.
	glm::mat4 cameraPose;
	if (m_cameraComponent->getWorldSpaceAperturePose(cameraPose))
	{
		m_mkCamera->setCameraTransform(cameraPose);
	}
}

void AppStage_StencilAlignment::updateVRCamera()
{
	if (!m_targetStencilComponent)
		return;

	bool bValidBoundingSphere= false;

	m_boundingSphereCenter= glm::vec3();
	m_boundingSphereRadius= 1.f;

	for (auto colliderComponent : m_targetStencilComponent->getColliderComponents())
	{
		glm::vec3 colliderCenter;
		float colliderRadius;
		if (colliderComponent->getBoundingSphere(colliderCenter, colliderRadius))
		{
			if (bValidBoundingSphere)
			{
				glm_sphere_union(m_boundingSphereCenter, m_boundingSphereRadius, colliderCenter, colliderRadius,
								 m_boundingSphereCenter, m_boundingSphereRadius);
			}
			else
			{
				m_boundingSphereCenter= colliderCenter;
				m_boundingSphereRadius= colliderRadius;
				bValidBoundingSphere= true;
			}
		}
	}

	if (bValidBoundingSphere)
	{
		m_mkCamera->setOrbitTargetPosition(m_boundingSphereCenter);
		m_mkCamera->setOrbitLocation(0.f, 0.f, m_boundingSphereRadius * 5.0f);
	}
}

void AppStage_StencilAlignment::update(float deltaSeconds)
{
	AppStage::update(deltaSeconds);

	switch (m_calibrationPanel->getMenuState())
	{
	case eStencilAlignmentMenuState::pendingVideoStart:
	{
		// Wait until the distortion view has a valid frame size (stream started + first frame size known)
		if (m_monoDistortionView->isReceivingFrames())
		{
			setupStencilAligner();
			setMenuState(eStencilAlignmentMenuState::verifyInitialCameraSetup);
		}
		else if (m_videoSourceComponent->getVideoStreamingStatus() == eVideoStreamingStatus::failed)
		{
			setMenuState(eStencilAlignmentMenuState::failedVideoStartStreamRequest);
		}
	}
	break;
	case eStencilAlignmentMenuState::verifyInitialCameraSetup:
	case eStencilAlignmentMenuState::verifyPointsCapture:
	case eStencilAlignmentMenuState::captureOriginPixel:
	case eStencilAlignmentMenuState::captureXAxisPixel:
	case eStencilAlignmentMenuState::captureYAxisPixel:
	case eStencilAlignmentMenuState::captureZAxisPixel:
	{
		m_monoDistortionView->readAndProcessVideoFrame();
		updateXRCamera();
	}
	break;
	case eStencilAlignmentMenuState::captureOriginVertex:
	case eStencilAlignmentMenuState::captureXAxisVertex:
	case eStencilAlignmentMenuState::captureYAxisVertex:
	case eStencilAlignmentMenuState::captureZAxisVertex:
	{
	}
	break;
	case eStencilAlignmentMenuState::testCalibration:
	{
		m_monoDistortionView->readAndProcessVideoFrame();
	}
	break;
	}
}

void AppStage_StencilAlignment::render(IMkViewportPtr targetViewport)
{
	IMkGraphicsContext* graphicsContext= getGraphicsContext();

	// Render the scene into the frame buffer
	if (m_frameBuffer->isValid())
	{
		MkScopedObjectBinding colorFramebufferBinding(graphicsContext->getMkStateStack().getCurrentState(),
													  "Color Framebuffer Scope", m_frameBuffer);

		if (colorFramebufferBinding)
		{
			switch (m_calibrationPanel->getMenuState())
			{
			case eStencilAlignmentMenuState::verifyInitialCameraSetup:
			case eStencilAlignmentMenuState::verifyPointsCapture:
			case eStencilAlignmentMenuState::captureOriginPixel:
			case eStencilAlignmentMenuState::captureXAxisPixel:
			case eStencilAlignmentMenuState::captureYAxisPixel:
			case eStencilAlignmentMenuState::captureZAxisPixel:
			{
				m_monoDistortionView->renderSelectedVideoBuffers();
				m_stencilAligner->renderPixelSamples();
			}
			break;
			case eStencilAlignmentMenuState::captureOriginVertex:
			case eStencilAlignmentMenuState::captureXAxisVertex:
			case eStencilAlignmentMenuState::captureYAxisVertex:
			case eStencilAlignmentMenuState::captureZAxisVertex:
			{
				renderStencilScene();
				m_stencilAligner->renderVertexSamples();
			}
			break;
			case eStencilAlignmentMenuState::testCalibration:
			{
				m_monoDistortionView->renderSelectedVideoBuffers();
				m_stencilAligner->renderVertexSamples();
				renderStencilScene();
			}
			break;
			}

			// Render any lines and text that were added to the scene by the calibrator in the frame buffer's viewport
			graphicsContext->getLineRenderer()->render(true);
			graphicsContext->getTextRenderer()->render();
		}
	}

	// Render the frame buffer to the screen
	if (m_frameBuffer->isValid())
	{
		MkMaterialInstancePtr materialInstance= m_fullscreenRGBQuad->getMaterialInstance();
		MkMaterialConstPtr material= materialInstance->getMaterial();

		if (auto materialBinding= material->bindMaterial())
		{
			auto colorTexture= m_frameBuffer->getColorTexture();

			// Bind the color texture
			materialInstance->setTextureBySemantic(eUniformSemantic::rgbTexture, colorTexture);

			// Draw the color texture
			if (auto materialInstanceBinding= materialInstance->bindMaterialInstance(materialBinding))
			{
				m_fullscreenRGBQuad->drawElements();
			}
		}
	}
}

void AppStage_StencilAlignment::renderStencilScene()
{
	IMkGraphicsContext* graphicsContext= getGraphicsContext();

	// Clear the depth buffer so the scene draws over the video frame
	mkStateClearBuffer(graphicsContext->getMkStateStack().getCurrentState(), eMkClearFlags::depth);

	m_scene->render(m_mkCamera, graphicsContext->getMkStateStack());

	if (m_targetStencilComponent)
	{
		// Draw the stencil's local axes
		glm::mat4 stencilXform= m_targetStencilComponent->getWorldTransform();
		drawTransformedAxes(graphicsContext, stencilXform, m_boundingSphereRadius * 1.1f, true);

		if (m_hoverResult.hitValid)
		{
			// Draw collision normal
			drawSegment(graphicsContext, glm::mat4(1.f), m_hoverResult.hitLocation,
						m_hoverResult.hitLocation + m_hoverResult.hitNormal * 0.01f, Colors::Green);

			// Draw the closest vertex to the collision point
			drawSegment(graphicsContext, glm::mat4(1.f), m_hoverResult.closestVertexWorld,
						m_hoverResult.closestVertexWorld + m_hoverResult.hitNormal * 0.01f, Colors::Yellow);
		}
	}
}

void AppStage_StencilAlignment::onGui()
{
	AppStage::onGui();

	const float panelWidth= 415.f * MkGui::getUiScale();
	const float displayWidth= m_ownerWindow->getWidth();
	const float displayHeight= m_ownerWindow->getHeight();
	ImGui::SetNextWindowPos(ImVec2(displayWidth - panelWidth, 0.f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelWidth, displayHeight), ImGuiCond_Always);

	constexpr ImGuiWindowFlags k_flags=
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

	MkGuiScopedWindow panel("##StencilAlignment", nullptr, k_flags);
	if (!panel)
		return;

	for (IGuiPanel* guiPanel : m_guiPanels)
		guiPanel->onGui();
}

void AppStage_StencilAlignment::setMenuState(eStencilAlignmentMenuState newState)
{
	eStencilAlignmentMenuState oldState= m_calibrationPanel->getMenuState();

	if (oldState != newState)
	{
		switch (newState)
		{
		case eStencilAlignmentMenuState::captureOriginVertex:
		case eStencilAlignmentMenuState::captureXAxisVertex:
		case eStencilAlignmentMenuState::captureYAxisVertex:
		case eStencilAlignmentMenuState::captureZAxisVertex:
			m_mkCamera->setCameraMovementMode(eCameraMovementMode::orbit);
			break;
		default:
			m_mkCamera->setCameraMovementMode(eCameraMovementMode::stationary);
		}

		// Update menu state on the data models
		m_calibrationPanel->setMenuState(newState);
	}
}

// Viewpoint Events
void AppStage_StencilAlignment::onMouseRayChanged(const glm::vec3& rayOrigin, const glm::vec3& rayDir)
{
	eStencilAlignmentMenuState menuState= m_calibrationPanel->getMenuState();
	if (menuState == eStencilAlignmentMenuState::captureOriginVertex
		|| menuState == eStencilAlignmentMenuState::captureXAxisVertex
		|| menuState == eStencilAlignmentMenuState::captureYAxisVertex
		|| menuState == eStencilAlignmentMenuState::captureZAxisVertex)
	{
		m_hoverResult= ColliderRaycastHitResult();

		if (m_targetStencilComponent)
		{
			glm::vec3 closestWorldspacePoint;
			float closestDistance= std::numeric_limits<float>::max();

			for (auto colliderComponent : m_targetStencilComponent->getColliderComponents())
			{
				ColliderRaycastHitRequest request= {rayOrigin, rayDir};
				ColliderRaycastHitResult result;

				if (colliderComponent->computeRayIntersection(request, result) && result.hitDistance < closestDistance)
				{
					m_hoverResult= result;
					closestDistance= result.hitDistance;
				}
			}

			// Update the color of the wireframe meshes based on the hover result
			glm::vec3 newColor= m_hoverResult.hitValid ? Colors::LightGray : Colors::DarkGray;
			for (IMkStaticMeshInstancePtr meshInstance : m_targetStencilComponent->getWireframeMeshes())
			{
				meshInstance->getMaterialInstance()->setVec4BySemantic(eUniformSemantic::diffuseColorRGBA,
																	   glm::vec4(newColor, 1.f));
			}
		}
	}
}

void AppStage_StencilAlignment::onMouseRayButtonUp(const glm::vec3& rayOrigin, const glm::vec3& rayDir, int button)
{
	if (button != MkMouseButton::LEFT)
		return;

	eStencilAlignmentMenuState menuState= m_calibrationPanel->getMenuState();
	if (menuState >= eStencilAlignmentMenuState::captureOriginPixel
		&& menuState <= eStencilAlignmentMenuState::captureZAxisVertex)
	{
		bool bValidSample= false;

		switch (menuState)
		{
		case eStencilAlignmentMenuState::captureOriginPixel:
		case eStencilAlignmentMenuState::captureXAxisPixel:
		case eStencilAlignmentMenuState::captureYAxisPixel:
		case eStencilAlignmentMenuState::captureZAxisPixel:
		{
			MikanViewportPtr viewport= getFirstViewport();

			// Get the cursor position in the window viewport
			glm::vec2 viewportPixel;
			if (viewport->getCursorViewportPixelPos(viewportPixel))
			{
				// Remap from window viewport size to the frame buffer size
				glm::i32vec2 windowViewportSize= viewport->getViewportSize();
				glm::vec2 frameBufferPixel=
					remapPointIntoTarget((float)windowViewportSize.x, (float)windowViewportSize.y, 0.f, 0.f,
										 m_frameBuffer->getWidth(), m_frameBuffer->getHeight(), viewportPixel);

				// Record the pixel location sample
				m_stencilAligner->samplePixel(frameBufferPixel);
				bValidSample= true;
			}
		}
		break;
		case eStencilAlignmentMenuState::captureOriginVertex:
		case eStencilAlignmentMenuState::captureXAxisVertex:
		case eStencilAlignmentMenuState::captureYAxisVertex:
		case eStencilAlignmentMenuState::captureZAxisVertex:
			if (m_hoverResult.closestVertexValid)
			{
				m_stencilAligner->sampleVertex(m_hoverResult.closestVertexLocal);
				bValidSample= true;
			}
			break;
		}

		if (bValidSample)
		{
			eStencilAlignmentMenuState newMenuState= (eStencilAlignmentMenuState)((int)menuState + 1);

			setMenuState(newMenuState);
		}
	}
}

// Calibration Model UI Events
void AppStage_StencilAlignment::onOkEvent()
{
	switch (m_calibrationPanel->getMenuState())
	{
	case eStencilAlignmentMenuState::verifyInitialCameraSetup:
	{
		// Clear out all of the calibration data we recorded
		m_stencilAligner->resetCalibrationState();

		setMenuState(eStencilAlignmentMenuState::captureOriginPixel);
	}
	break;
	case eStencilAlignmentMenuState::verifyPointsCapture:
	{

		glm::mat4 newStencilTransform;
		if (m_stencilAligner->computeStencilTransform(newStencilTransform))
		{
			m_targetStencilComponent->setWorldTransform(newStencilTransform);
		}

		setMenuState(eStencilAlignmentMenuState::testCalibration);
	}
	break;
	case eStencilAlignmentMenuState::testCalibration:
	case eStencilAlignmentMenuState::failedVideoStartStreamRequest:
	{
		m_ownerWindow->popAppState();
	}
	break;
	}
}

void AppStage_StencilAlignment::onRedoEvent()
{
	// Clear out all of the calibration data we recorded
	m_stencilAligner->resetCalibrationState();

	// Return to the capture state
	setMenuState(eStencilAlignmentMenuState::verifyInitialCameraSetup);
}

void AppStage_StencilAlignment::onCancelEvent() { m_ownerWindow->popAppState(); }
//-- includes -----
#include "PointCloudAlignment/AppStage_PointCloudAlignment.h"
#include "PointCloudAlignment/GuiPanel_PointCloudAlignment.h"
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
#include "ModelPointCloudAligner.h"
#include "NaturalFeatureCloudBuilder.h"
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
const char* AppStage_PointCloudAlignment::APP_STAGE_NAME= "PointCloudAlignment";

//-- public methods -----
AppStage_PointCloudAlignment::AppStage_PointCloudAlignment(IEditorWindow* ownerWindow)
	: AppStage(ownerWindow, AppStage_PointCloudAlignment::APP_STAGE_NAME)
	, m_videoSourceComponent()
	, m_cloudBuilder(nullptr)
	, m_aligner(nullptr)
	, m_monoDistortionView(nullptr)
	, m_scene(std::make_shared<MkScene>())
	, m_mkCamera(nullptr)
	, m_frameBuffer(createMkFrameBuffer())
	, m_fullscreenRGBQuad(createFullscreenQuadMesh(ownerWindow->getGraphicsContext().get(), false))
{
}

AppStage_PointCloudAlignment::~AppStage_PointCloudAlignment() {}

void AppStage_PointCloudAlignment::enter()
{
	AppStage::enter();

	assert(m_cameraComponent);
	m_videoSourceComponent= m_cameraComponent->getVideoSourceComponent();
	assert(m_videoSourceComponent);

	// Capture the stencil's current placement as a coarse alignment hypothesis
	if (m_targetStencilComponent)
		m_initialGuess= m_targetStencilComponent->getWorldTransform();

	// Listen for mouse ray events (used for optional ROI corner picking)
	MikanViewportPtr viewport= getFirstViewport();
	viewport->OnMouseRayButtonUp+= MakeDelegate(this, &AppStage_PointCloudAlignment::onMouseRayButtonUp);

	// Create a new camera to view the scene
	m_mkCamera= viewport->getCurrentMikanCamera();
	m_mkCamera->setCameraMovementMode(eCameraMovementMode::stationary);

	// Center the orbit camera on the stencil model
	updateVRCamera();

	// Make sure the camera doing the 3d rendering has the same
	// fov and aspect ratio as the real camera
	MikanVideoSourceIntrinsics cameraIntrinsics;
	m_videoSourceComponent->getCameraIntrinsics(cameraIntrinsics);
	m_mkCamera->applyMonoCameraIntrinsics(&cameraIntrinsics);

	// Create a frame buffer to render the scene into using the resolution and fov from the camera intrinsics
	const MikanMonoIntrinsics& monoIntrinsics= cameraIntrinsics.getMonoIntrinsics();
	m_frameBuffer->setName("PointCloudAlignment");
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

	// Create app stage GUI panels (auto cleaned up on app state exit)
	{
		m_calibrationPanel= addGuiPanel<GuiPanel_PointCloudAlignment>();
		m_calibrationPanel->OnOkEvent= [this]() { onOkEvent(); };
		m_calibrationPanel->OnRedoEvent= [this]() { onRedoEvent(); };
		m_calibrationPanel->OnCancelEvent= [this]() { onCancelEvent(); };
		m_calibrationPanel->OnBeginRoiEvent= [this]() { onBeginRoiEvent(); };
		m_calibrationPanel->OnSkipRoiEvent= [this]() { onSkipRoiEvent(); };
		m_calibrationPanel->OnStartCaptureEvent= [this]() { onStartCaptureEvent(); };
		m_calibrationPanel->OnStopCaptureEvent= [this]() { onStopCaptureEvent(); };
		m_calibrationPanel->OnRunAlignmentEvent= [this]() { onRunAlignmentEvent(); };
	}

	setMenuState(ePointCloudAlignmentMenuState::pendingVideoStart);
}

void AppStage_PointCloudAlignment::exit()
{
	setMenuState(ePointCloudAlignmentMenuState::inactive);

	// Stop listening to mouse ray events
	MikanViewportPtr viewport= getFirstViewport();
	viewport->OnMouseRayButtonUp-= MakeDelegate(this, &AppStage_PointCloudAlignment::onMouseRayButtonUp);

	// Forget about the stencil we were aligning
	m_targetStencilComponent= nullptr;

	// Forget about the camera
	m_mkCamera= nullptr;

	// Forget about the stencil model we added
	m_scene->removeAllInstances();

	// Free the tools
	if (m_aligner != nullptr)
	{
		delete m_aligner;
		m_aligner= nullptr;
	}
	if (m_cloudBuilder != nullptr)
	{
		delete m_cloudBuilder;
		m_cloudBuilder= nullptr;
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

void AppStage_PointCloudAlignment::setupTools()
{
	// (m_monoDistortionView is already created in enter())
	if (m_cloudBuilder == nullptr)
		m_cloudBuilder= new NaturalFeatureCloudBuilder(m_cameraComponent, m_monoDistortionView);
	if (m_aligner == nullptr)
		m_aligner= new ModelPointCloudAligner(m_targetStencilComponent);
}

void AppStage_PointCloudAlignment::updateXRCamera()
{
	// Update the transform of the camera so that vr models align over the tracking puck.
	// World space: the stencil this stage aligns carries a world transform.
	glm::mat4 cameraPose;
	if (m_cameraComponent->getWorldSpaceAperturePose(cameraPose))
	{
		m_mkCamera->setCameraTransform(cameraPose);
	}
}

void AppStage_PointCloudAlignment::updateVRCamera()
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

void AppStage_PointCloudAlignment::runAlignment()
{
	m_alignmentPending= false;

	if (m_cloudBuilder == nullptr || m_aligner == nullptr || m_targetStencilComponent == nullptr)
	{
		setMenuState(ePointCloudAlignmentMenuState::verifyAlignment);
		return;
	}

	IcpParams params;
	params.estimateUniformScale= true; // absorb 3D-print scale error

	IcpResult result;
	if (m_aligner->align(m_cloudBuilder->getCloudPoints(), m_initialGuess, params, result))
	{
		m_targetStencilComponent->setWorldTransform(result.modelWorldTransform);
		m_lastIcpResult= result;
	}
	else
	{
		m_lastIcpResult= IcpResult();
	}

	m_calibrationPanel->setAlignmentResult(m_lastIcpResult);
	setMenuState(ePointCloudAlignmentMenuState::verifyAlignment);
}

void AppStage_PointCloudAlignment::update(float deltaSeconds)
{
	AppStage::update(deltaSeconds);

	switch (m_calibrationPanel->getMenuState())
	{
	case ePointCloudAlignmentMenuState::pendingVideoStart:
	{
		// Wait until the distortion view has a valid frame size (stream started + first frame size known)
		if (m_monoDistortionView->isReceivingFrames())
		{
			setupTools();
			setMenuState(ePointCloudAlignmentMenuState::verifyInitialCameraSetup);
		}
		else if (m_videoSourceComponent->getVideoStreamingStatus() == eVideoStreamingStatus::failed)
		{
			setMenuState(ePointCloudAlignmentMenuState::failedVideoStartStreamRequest);
		}
	}
	break;
	case ePointCloudAlignmentMenuState::verifyInitialCameraSetup:
	case ePointCloudAlignmentMenuState::paintRegionOfInterest:
	{
		m_monoDistortionView->readAndProcessVideoFrame();
		updateXRCamera();
	}
	break;
	case ePointCloudAlignmentMenuState::captureFeatureCloud:
	{
		m_monoDistortionView->readAndProcessVideoFrame();
		updateXRCamera();
		if (m_cloudBuilder != nullptr)
		{
			m_cloudBuilder->processCurrentFrame();
			m_calibrationPanel->setCaptureStats(m_cloudBuilder->getStats());
		}
	}
	break;
	case ePointCloudAlignmentMenuState::reviewCloud:
	{
		m_monoDistortionView->readAndProcessVideoFrame();
		updateXRCamera();
	}
	break;
	case ePointCloudAlignmentMenuState::runAutoAlignment:
	{
		if (m_alignmentPending)
			runAlignment();
	}
	break;
	case ePointCloudAlignmentMenuState::verifyAlignment:
	{
		m_monoDistortionView->readAndProcessVideoFrame();
		updateXRCamera();
	}
	break;
	}
}

void AppStage_PointCloudAlignment::render(IMkViewportPtr targetViewport)
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
			case ePointCloudAlignmentMenuState::verifyInitialCameraSetup:
			{
				m_monoDistortionView->renderSelectedVideoBuffers();
			}
			break;
			case ePointCloudAlignmentMenuState::paintRegionOfInterest:
			{
				m_monoDistortionView->renderSelectedVideoBuffers();

				// Draw the ROI corners / rectangle being defined
				const float fbWidth= (float)m_frameBuffer->getWidth();
				const float fbHeight= (float)m_frameBuffer->getHeight();
				if (m_roiClickCount >= 1)
				{
					glm::vec3 p0(m_roiCorners[0].x, m_roiCorners[0].y, 0.5f);
					drawPointList2d(graphicsContext, fbWidth, fbHeight, &p0, 1, Colors::Yellow, 3.f);
				}
				if (m_roiClickCount >= 2)
				{
					const glm::vec2& a= m_roiCorners[0];
					const glm::vec2& b= m_roiCorners[1];
					glm::vec3 c0(a.x, a.y, 0.5f), c1(b.x, a.y, 0.5f), c2(b.x, b.y, 0.5f), c3(a.x, b.y, 0.5f);
					drawSegment2d(graphicsContext, fbWidth, fbHeight, c0, c1, Colors::Yellow, Colors::Yellow);
					drawSegment2d(graphicsContext, fbWidth, fbHeight, c1, c2, Colors::Yellow, Colors::Yellow);
					drawSegment2d(graphicsContext, fbWidth, fbHeight, c2, c3, Colors::Yellow, Colors::Yellow);
					drawSegment2d(graphicsContext, fbWidth, fbHeight, c3, c0, Colors::Yellow, Colors::Yellow);
				}
			}
			break;
			case ePointCloudAlignmentMenuState::captureFeatureCloud:
			{
				m_monoDistortionView->renderSelectedVideoBuffers();
				if (m_cloudBuilder != nullptr)
				{
					m_cloudBuilder->renderTrackedFeatures2d();
					m_cloudBuilder->renderCloudPoints3d();
				}
			}
			break;
			case ePointCloudAlignmentMenuState::reviewCloud:
			{
				m_monoDistortionView->renderSelectedVideoBuffers();
				if (m_cloudBuilder != nullptr)
					m_cloudBuilder->renderCloudPoints3d();
			}
			break;
			case ePointCloudAlignmentMenuState::verifyAlignment:
			{
				m_monoDistortionView->renderSelectedVideoBuffers();
				renderStencilScene();
				if (m_aligner != nullptr)
				{
					m_aligner->renderSegmentedCloud(graphicsContext);
					m_aligner->renderResult(graphicsContext);
				}
			}
			break;
			default:
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

void AppStage_PointCloudAlignment::renderStencilScene()
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
	}
}

void AppStage_PointCloudAlignment::onGui()
{
	AppStage::onGui();

	const float panelWidth= 415.f * MkGui::getUiScale();
	const float displayWidth= m_ownerWindow->getWidth();
	const float displayHeight= m_ownerWindow->getHeight();
	ImGui::SetNextWindowPos(ImVec2(displayWidth - panelWidth, 0.f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelWidth, displayHeight), ImGuiCond_Always);

	constexpr ImGuiWindowFlags k_flags=
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

	MkGuiScopedWindow panel("##PointCloudAlignment", nullptr, k_flags);
	if (!panel)
		return;

	for (IGuiPanel* guiPanel : m_guiPanels)
		guiPanel->onGui();
}

void AppStage_PointCloudAlignment::setMenuState(ePointCloudAlignmentMenuState newState)
{
	ePointCloudAlignmentMenuState oldState= m_calibrationPanel->getMenuState();

	if (oldState != newState)
	{
		// Keep the 3D preview camera stationary (aligned to the physical camera) throughout the automatic flow
		m_mkCamera->setCameraMovementMode(eCameraMovementMode::stationary);

		// Update menu state on the data model
		m_calibrationPanel->setMenuState(newState);
	}
}

// Viewport Events
void AppStage_PointCloudAlignment::onMouseRayButtonUp(const glm::vec3& rayOrigin, const glm::vec3& rayDir, int button)
{
	if (button != MkMouseButton::LEFT)
		return;

	if (m_calibrationPanel->getMenuState() != ePointCloudAlignmentMenuState::paintRegionOfInterest)
		return;

	MikanViewportPtr viewport= getFirstViewport();

	// Get the cursor position in the window viewport and remap to frame-buffer pixel space
	glm::vec2 viewportPixel;
	if (!viewport->getCursorViewportPixelPos(viewportPixel))
		return;

	glm::i32vec2 windowViewportSize= viewport->getViewportSize();
	glm::vec2 frameBufferPixel=
		remapPointIntoTarget((float)windowViewportSize.x, (float)windowViewportSize.y, 0.f, 0.f,
							 (float)m_frameBuffer->getWidth(), (float)m_frameBuffer->getHeight(), viewportPixel);

	if (m_roiClickCount < 2)
	{
		m_roiCorners[m_roiClickCount]= frameBufferPixel;
		m_roiClickCount++;
	}

	if (m_roiClickCount == 2 && m_cloudBuilder != nullptr)
	{
		m_cloudBuilder->setScreenRegionOfInterest(m_roiCorners[0], m_roiCorners[1]);
		setMenuState(ePointCloudAlignmentMenuState::captureFeatureCloud);
	}
}

// UI Events
void AppStage_PointCloudAlignment::onBeginRoiEvent()
{
	m_roiClickCount= 0;
	if (m_cloudBuilder != nullptr)
		m_cloudBuilder->clearScreenRegionOfInterest();
	setMenuState(ePointCloudAlignmentMenuState::paintRegionOfInterest);
}

void AppStage_PointCloudAlignment::onSkipRoiEvent()
{
	if (m_cloudBuilder != nullptr)
	{
		m_cloudBuilder->clearScreenRegionOfInterest();
		m_cloudBuilder->resetCaptureState();
	}
	setMenuState(ePointCloudAlignmentMenuState::captureFeatureCloud);
}

void AppStage_PointCloudAlignment::onStartCaptureEvent()
{
	setMenuState(ePointCloudAlignmentMenuState::captureFeatureCloud);
}

void AppStage_PointCloudAlignment::onStopCaptureEvent() { setMenuState(ePointCloudAlignmentMenuState::reviewCloud); }

void AppStage_PointCloudAlignment::onRunAlignmentEvent()
{
	m_alignmentPending= true;
	setMenuState(ePointCloudAlignmentMenuState::runAutoAlignment);
}

void AppStage_PointCloudAlignment::onOkEvent()
{
	// Accept the current alignment
	m_ownerWindow->popAppState();
}

void AppStage_PointCloudAlignment::onRedoEvent()
{
	// Discard the captured cloud and start capture over
	if (m_cloudBuilder != nullptr)
		m_cloudBuilder->resetCaptureState();
	m_roiClickCount= 0;
	setMenuState(ePointCloudAlignmentMenuState::verifyInitialCameraSetup);
}

void AppStage_PointCloudAlignment::onCancelEvent() { m_ownerWindow->popAppState(); }

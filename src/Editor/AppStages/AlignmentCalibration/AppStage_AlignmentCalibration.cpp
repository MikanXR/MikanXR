// Derived From example 11-1 of "Learning OpenCV: Computer Vision with the OpenCV Library" by Gary Bradski

//-- includes -----
#include "AlignmentCalibration/AppStage_AlignmentCalibration.h"
#include "AlignmentCalibration/GuiPanel_AlignmentCalibration.h"
#include "AlignmentCalibration/GuiPanel_AlignmentCameraSettings.h"
#include "ModalMessageBox/ModalDialog_MessageBox.h"
#include "MkGuiScopedWindow.h"
#include "CalibrationPatternFinder.h"
#include "CameraComponent.h"
#include "Colors.h"
#include "MkScene.h"
#include "IMkGraphicsContext.h"
#include "IMkFrameBuffer.h"
#include "IMkTriangulatedMesh.h"
#include "IMkLineRenderer.h"
#include "IMkTextRenderer.h"
#include "MathTypeConversion.h"
#include "MathUtility.h"
#include "MathGLM.h"
#include "MikanCamera.h"
#include "MikanLineRenderer.h"
#include "MikanTextRenderer.h"
#include "MikanViewport.h"
#include "MkStateModifiers.h"
#include "MainWindow.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkScopedObjectBinding.h"
#include "MkStateStack.h"
#include "MonoLensTrackerPoseCalibrator.h"
#include "StageComponent.h"
#include "TextStyle.h"
#include "TrackingMountObjectSystem.h"
#include "VideoSourceComponent.h"
#include "VideoFrameDistortionView.h"
#include "VRObjectSystem.h"
#include "VRDeviceComponent.h"
#include "VRTrackingVolumeComponent.h"

#include "glm/gtc/quaternion.hpp"
#include "glm/ext/vector_float4.hpp"

#include "imgui.h"
#include "MkGuiDrawUtils.h"
#include "LocText.h"

//-- statics ----
const char* AppStage_AlignmentCalibration::APP_STAGE_NAME= "AlignmentCalibration";

//-- constants -----
static const char* k_calibration_pattern_names[]= {
	"Chessboard",
	"Circle Grid",
};

//-- public methods -----
AppStage_AlignmentCalibration::AppStage_AlignmentCalibration(IEditorWindow* ownerWindow)
	: AppStage(ownerWindow, AppStage_AlignmentCalibration::APP_STAGE_NAME)
	, m_targetCameraComponent()
	, m_videoSourceComponent()
	, m_trackerPoseCalibrator(nullptr)
	, m_monoDistortionView(nullptr)
	, m_scene(std::make_shared<MkScene>())
	, m_mkCalibrationView(nullptr)
	, m_mkStageView(nullptr)
	, m_mkXRView(nullptr)
	, m_frameBuffer(createMkFrameBuffer())
	, m_fullscreenRGBQuad(createFullscreenQuadMesh(ownerWindow->getGraphicsContext().get(), false))
{
}

AppStage_AlignmentCalibration::~AppStage_AlignmentCalibration() {}

bool AppStage_AlignmentCalibration::tryEnterAlignmentCalibration(AppStage* fromAppStage,
																 CameraComponentPtr forCameraComponent)
{
	IEditorWindow* ownerWindow= fromAppStage->getOwnerWindow();

	VideoSourceComponentPtr videoSourceComponent= forCameraComponent->getVideoSourceComponent();
	if (!videoSourceComponent)
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("alignmentCalibration.noVideoSourceMsg"));
		return false;
	}

	if (!videoSourceComponent->areCameraIntrinsicsValid())
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("alignmentCalibration.invalidIntrinsicsMsg"));
		return false;
	}

	VRDevicePoseViewPtr cameraPuckPose_VRSystemSpace=
		forCameraComponent->makeTrackingMountPoseView(eVRDevicePoseSpace::VRTrackingSystemPose);
	if (!cameraPuckPose_VRSystemSpace)
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("alignmentCalibration.noTrackingMountMsg"));
		return false;
	}

	VRTrackingVolumeComponentConstPtr trackingVolumeComponent= forCameraComponent->getVRTrackingVolumeComponent();
	if (!trackingVolumeComponent)
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("alignmentCalibration.noTrackingVolumeMsg"));
		return false;
	}

	VRDevicePoseViewPtr matPuckPose_VRSystemSpace=
		trackingVolumeComponent->makeChArUcoTrackingMountPoseView(eVRDevicePoseSpace::VRTrackingSystemPose);
	if (!matPuckPose_VRSystemSpace)
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("alignmentCalibration.noCharucoMountMsg"));
		return false;
	}

	auto* alignmentCalibration= ownerWindow->pushAppStageOfType<AppStage_AlignmentCalibration>();
	alignmentCalibration->setTargetCameraComponent(forCameraComponent);
	alignmentCalibration->setVideoSourceComponent(videoSourceComponent);
	alignmentCalibration->setCameraPuckPose(cameraPuckPose_VRSystemSpace);
	alignmentCalibration->setMatPuckPose(matPuckPose_VRSystemSpace);

	return true;
}

void AppStage_AlignmentCalibration::setBypassCalibrationFlag(bool flag)
{
	m_bypassCalibrationFlag= flag;
	if (m_calibrationPanel != nullptr)
		m_calibrationPanel->setBypassCalibrationFlag(flag);
}

void AppStage_AlignmentCalibration::setTargetCameraComponent(CameraComponentPtr cameraComponent)
{
	m_targetCameraComponent= cameraComponent;
}

void AppStage_AlignmentCalibration::setVideoSourceComponent(VideoSourceComponentPtr videoSourceComponent)
{
	m_videoSourceComponent= videoSourceComponent;
}

void AppStage_AlignmentCalibration::setCameraPuckPose(VRDevicePoseViewPtr cameraPuckPose)
{
	m_cameraPuckPose_VRSystemSpace= cameraPuckPose;
}

void AppStage_AlignmentCalibration::setMatPuckPose(VRDevicePoseViewPtr matPuckPose)
{
	m_matPuckPose_VRSystemSpace= matPuckPose;
}

// -- AppStage -- //
void AppStage_AlignmentCalibration::enter()
{
	AppStage::enter();
	assert(m_targetCameraComponent != nullptr);
	assert(m_videoSourceComponent);
	assert(m_videoSourceComponent->areCameraIntrinsicsValid());
	assert(m_cameraPuckPose_VRSystemSpace);
	assert(m_matPuckPose_VRSystemSpace);

	// Get the camera intrinsics from the video source
	// so we can apply them to the calibration and XR views
	MikanVideoSourceIntrinsics cameraIntrinsics;
	m_videoSourceComponent->getCameraIntrinsics(cameraIntrinsics);

	// Consider the default camera associated with the viewport to be the stage view
	m_mkStageView= getFirstViewport()->getCurrentMikanCamera();
	m_mkStageView->setCameraMovementMode(eCameraMovementMode::fly);

	// Create a separate view for the calibration process that matches the real camera's intrinsics and is locked to the
	// camera puck pose
	m_mkCalibrationView= getFirstViewport()->addMikanCamera();
	m_mkCalibrationView->setCameraMovementMode(eCameraMovementMode::stationary);
	m_mkCalibrationView->setCameraTransform(glm::mat4(1.f));
	m_mkCalibrationView->applyMonoCameraIntrinsics(&cameraIntrinsics);

	// Create a separate view from the aperature PoV to show mixed reality compositing calibration
	m_mkXRView= getFirstViewport()->addMikanCamera();
	m_mkXRView->setCameraMovementMode(eCameraMovementMode::stationary);
	m_mkXRView->applyMonoCameraIntrinsics(&cameraIntrinsics);

	// Create a frame buffer to render the scene into using the resolution and fov from the camera intrinsics
	const MikanMonoIntrinsics& monoIntrinsics= cameraIntrinsics.getMonoIntrinsics();
	m_frameBuffer->setName("AlignmentCalibration");
	m_frameBuffer->setSize(monoIntrinsics.pixel_width, monoIntrinsics.pixel_height);
	m_frameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_frameBuffer->createResources();
	m_frameBuffer->setClearColor(glm::vec4(Colors::CornflowerBlue, 1.f));

	// Create the distortion view (acts as stream ownership token)
	m_monoDistortionView= new VideoFrameDistortionView(m_videoSourceComponent, eVideoFrameProcessorMode::CALIBRATION);
	m_monoDistortionView->setVideoDisplayMode(eVideoDisplayMode::mode_undistored);

	// Register as a stream consumer — update() drives the retry loop
	m_videoSourceComponent->startVideoStream(m_monoDistortionView);
	eAlignmentCalibrationMenuState newState= eAlignmentCalibrationMenuState::pendingVideoStart;

	// Create GUI panels
	// (Auto cleaned up on app state exit)
	{
		m_calibrationPanel= addGuiPanel<GuiPanel_AlignmentCalibration>();
		m_calibrationPanel->setBypassCalibrationFlag(m_bypassCalibrationFlag);
		m_calibrationPanel->OnBeginEvent= [this]() { onBeginEvent(); };
		m_calibrationPanel->OnRestartEvent= [this]() { onRestartEvent(); };
		m_calibrationPanel->OnCancelEvent= [this]() { onCancelEvent(); };
		m_calibrationPanel->OnReturnEvent= [this]() { onReturnEvent(); };
		m_calibrationPanel->OnChessboardStabilityChangedEvent= [this](bool bIsStable)
		{ onChessboardStabilityChangedEvent(bIsStable); };

		m_cameraSettingsPanel= addGuiPanel<GuiPanel_AlignmentCameraSettings>();
		m_cameraSettingsPanel->setCameraDefinition(m_targetCameraComponent->getCameraDefinition());
		m_cameraSettingsPanel->OnViewpointModeChanged= [this](eAlignmentCalibrationViewpointMode mode)
		{ onViewportModeChanged(mode); };
		m_cameraSettingsPanel->OnVRFrameDelayChanged= [this](int delay) { onVRFrameDelayChanged(delay); };
		m_cameraSettingsPanel->setViewpointMode(m_bypassCalibrationFlag
													? eAlignmentCalibrationViewpointMode::xrView
													: eAlignmentCalibrationViewpointMode::calibration);
		onViewportModeChanged(m_cameraSettingsPanel->getViewpointMode());
	}

	setMenuState(newState);
}

void AppStage_AlignmentCalibration::setupTrackerPoseCalibrator()
{
	// Create a calibrator to do the actual pattern recording and calibration
	// (m_monoDistortionView is already created in enter())
	m_trackerPoseCalibrator= new MonoLensTrackerPoseCalibrator(m_targetCameraComponent, m_cameraPuckPose_VRSystemSpace,
															   m_matPuckPose_VRSystemSpace, m_monoDistortionView,
															   DESIRED_CAPTURE_BOARD_COUNT);
}

void AppStage_AlignmentCalibration::exit()
{
	setMenuState(eAlignmentCalibrationMenuState::inactive);

	m_mkCalibrationView= nullptr;
	m_mkXRView= nullptr;
	m_mkStageView= nullptr;

	// Free the calibrator
	if (m_trackerPoseCalibrator != nullptr)
	{
		delete m_trackerPoseCalibrator;
		m_trackerPoseCalibrator= nullptr;
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

void AppStage_AlignmentCalibration::updateXRViewTransform()
{
	glm::mat4 cameraPuckPose_VRSystemSpace;
	if (m_cameraPuckPose_VRSystemSpace->getPose(m_targetCameraComponent, cameraPuckPose_VRSystemSpace))
	{
		bool bValidOffset= false;

		glm::mat4 cameraPuckToApertureXform;
		if (m_calibrationPanel->getMenuState() == eAlignmentCalibrationMenuState::testCalibration)
		{
			// Use the calibrated aperture offset
			bValidOffset= m_targetCameraComponent->getApertureOffsetXform(cameraPuckToApertureXform);
		}
		else
		{
			// Use the calibrator's most recent estimate of the aperture offset
			bValidOffset= m_trackerPoseCalibrator->getLastCameraPuckToApertureXform(cameraPuckToApertureXform);
		}

		if (bValidOffset)
		{
			// Update the camera transform so that it reflects
			// the current aperture pose in VRSystemSpace
			m_mkXRView->setCameraTransform(
				glm_composite_xform(cameraPuckToApertureXform, cameraPuckPose_VRSystemSpace));
		}
	}
}

void AppStage_AlignmentCalibration::update(float deltaSeconds)
{
	AppStage::update(deltaSeconds);

	switch (m_calibrationPanel->getMenuState())
	{
	case eAlignmentCalibrationMenuState::pendingVideoStart:
	{
		if (m_monoDistortionView->isReceivingFrames())
		{
			setupTrackerPoseCalibrator();
			setMenuState(m_bypassCalibrationFlag ? eAlignmentCalibrationMenuState::testCalibration
												 : eAlignmentCalibrationMenuState::verifySetup);
		}
		else if (m_videoSourceComponent->getVideoStreamingStatus() == eVideoStreamingStatus::failed)
		{
			setMenuState(eAlignmentCalibrationMenuState::failedVideoStartStreamRequest);
		}
	}
	break;
	case eAlignmentCalibrationMenuState::verifySetup:
	{
		// Update the video frame buffers to preview the calibration mat
		m_monoDistortionView->readAndProcessVideoFrame();

		// Look for a calibration pattern so that we can preview if it's in frame
		if (m_trackerPoseCalibrator->computeCalibrationSample())
		{
			// Apply the latest calibration state to the XRView
			updateXRViewTransform();
		}

		// See if we can compute a camera to puck transform this frame
		m_calibrationPanel->setCurrentChessboardValid(m_trackerPoseCalibrator->hasValidCameraPuckToApertureXform());

		// Update the time that the chessboard has been stable for
		m_calibrationPanel->updateChessboardStabilityTimer(deltaSeconds);
	}
	break;
	case eAlignmentCalibrationMenuState::capture:
	{
		// Update the video frame buffers
		m_monoDistortionView->readAndProcessVideoFrame();

		// Update the chess board capture state
		if (m_trackerPoseCalibrator->computeCalibrationSample())
		{
			m_trackerPoseCalibrator->recordCalibrationSample();

			// Apply the latest calibration state to the XRView
			updateXRViewTransform();

			// Update the calibration fraction on the UI Model
			m_calibrationPanel->setCalibrationFraction(m_trackerPoseCalibrator->getCalibrationProgress());
		}

		// See if we have gotten all the samples we require
		if (m_trackerPoseCalibrator->hasFinishedSampling())
		{
			MikanQuatd rotationOffset;
			MikanVector3d translationOffset;
			if (m_trackerPoseCalibrator->computeAverageCameraPuckToApertureOffset(rotationOffset, translationOffset))
			{
				// Store the calibrated camera offset on the video source settings
				m_targetCameraComponent->getCameraDefinition()->setAperturePoseOffset(rotationOffset,
																					  translationOffset);

				// Go to the test calibration state
				m_cameraSettingsPanel->setViewpointMode(eAlignmentCalibrationViewpointMode::xrView);
				setMenuState(eAlignmentCalibrationMenuState::testCalibration);
			}
		}
	}
	break;
	case eAlignmentCalibrationMenuState::testCalibration:
	{
		// Update the video frame buffers using the existing distortion calibration
		m_monoDistortionView->readAndProcessVideoFrame();

		// Apply the latest calibration state to the XRView
		updateXRViewTransform();
	}
	break;
	}
}

void AppStage_AlignmentCalibration::render(IMkViewportPtr targetViewport)
{
	AppStage::render(targetViewport);

	// Render the stageView into the frame buffer
	if (m_frameBuffer->isValid())
	{
		MkScopedObjectBinding colorFramebufferBinding(
			m_ownerWindow->getGraphicsContext()->getMkStateStack().getCurrentState(), "Color Framebuffer Scope",
			m_frameBuffer);

		if (colorFramebufferBinding)
		{
			switch (m_calibrationPanel->getMenuState())
			{
			case eAlignmentCalibrationMenuState::verifySetup:
			{
				switch (m_cameraSettingsPanel->getViewpointMode())
				{
				case eAlignmentCalibrationViewpointMode::calibration:
					m_monoDistortionView->renderSelectedVideoBuffers();
					m_trackerPoseCalibrator->renderApertureSpaceCalibrationState();
					break;
				case eAlignmentCalibrationViewpointMode::stageView:
					m_trackerPoseCalibrator->renderVRSpaceCalibrationState();
					renderVRDevices(m_mkStageView);
					break;
				case eAlignmentCalibrationViewpointMode::xrView:
					m_monoDistortionView->renderSelectedVideoBuffers();
					m_trackerPoseCalibrator->renderVRSpaceCalibrationState();
					renderVRDevices(m_mkXRView);
					break;
				}
			}
			break;
			case eAlignmentCalibrationMenuState::capture:
			{
				m_monoDistortionView->renderSelectedVideoBuffers();
				m_trackerPoseCalibrator->renderApertureSpaceCalibrationState();
			}
			break;
			case eAlignmentCalibrationMenuState::testCalibration:
			{
				switch (m_cameraSettingsPanel->getViewpointMode())
				{
				case eAlignmentCalibrationViewpointMode::stageView:
					renderVRDevices(m_mkStageView);
					renderVRStageViewDebug();
					break;
				case eAlignmentCalibrationViewpointMode::xrView:
					m_monoDistortionView->renderSelectedVideoBuffers();
					renderVRDevices(m_mkXRView);
					break;
				}
			}
			break;
			}
		}

		// Clear the depth buffer before drawing the scene
		IMkState* mkState= m_ownerWindow->getGraphicsContext()->getMkStateStack().getCurrentState();
		mkStateClearBuffer(mkState, eMkClearFlags::depth);

		// Render any lines and text that were added to the stageView by the calibrator in the frame buffer's viewport
		m_ownerWindow->getGraphicsContext()->getLineRenderer()->render(true);
		m_ownerWindow->getGraphicsContext()->getTextRenderer()->render();
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

void AppStage_AlignmentCalibration::renderVRDevices(IMkCameraConstPtr camera)
{
	IMkGraphicsContext* graphicsContext= getGraphicsContext();
	MkScene* stageView= m_scene.get();
	TextStyle style= getDefaultTextStyle();

	// Rebuild list of renderables
	stageView->removeAllInstances();

	// Add all renderable VR objects
	addAllVRDevicesToMkScene(getObjectSystemOfType<VRObjectSystem>(), m_scene);

	// Clear the depth buffer so the stageView draws over the video frame
	mkStateClearBuffer(graphicsContext->getMkStateStack().getCurrentState(), eMkClearFlags::depth);

	// Render the stageView
	stageView->render(camera, graphicsContext->getMkStateStack());
}

void AppStage_AlignmentCalibration::renderVRStageViewDebug()
{
	IMkGraphicsContext* graphicsContext= getGraphicsContext();
	TextStyle style= getDefaultTextStyle();

	// Draw the camera puck transform
	glm::mat4 cameraPuckXform_VRSpace;
	if (m_cameraPuckPose_VRSystemSpace->getPose(m_targetCameraComponent, cameraPuckXform_VRSpace))
	{
		drawTransformedAxes(graphicsContext, cameraPuckXform_VRSpace, 0.1f);
		drawTextAtWorldPosition(graphicsContext, style, glm_mat4_get_position(cameraPuckXform_VRSpace),
								L"Camera Puck (VR Space)");
	}

	// Draw the mat puck transform
	glm::mat4 matPuckXform_VRSpace;
	if (m_matPuckPose_VRSystemSpace->getPose(m_targetCameraComponent, matPuckXform_VRSpace))
	{
		drawTransformedAxes(graphicsContext, matPuckXform_VRSpace, 0.1f);
		drawTextAtWorldPosition(graphicsContext, style, glm_mat4_get_position(matPuckXform_VRSpace),
								L"Mat Puck (VR Space)");
	}

	// Draw the most recently derived camera transform derived from the mat puck
	const glm::mat4 apertureXform_VRSpace= m_mkXRView->getCameraTransformFromViewMatrix();
	const float hfov_radians= degrees_to_radians(m_mkXRView->getHorizontalFOVDegrees());
	const float vfov_radians= degrees_to_radians(m_mkXRView->getVerticalFOVDegrees());
	const float zNear= fmaxf(m_mkXRView->getZNear(), 0.1f);
	const float zFar= fminf(m_mkXRView->getZFar(), 2.0f);
	drawTransformedFrustum(graphicsContext, apertureXform_VRSpace, hfov_radians, vfov_radians, zNear, zFar,
						   Colors::Yellow);
	drawTransformedAxes(graphicsContext, apertureXform_VRSpace, 0.1f);
	drawTextAtWorldPosition(graphicsContext, style, glm_mat4_get_position(apertureXform_VRSpace),
							L"Aperture (VR Space)");

	// Draw the origin axes for reference
	drawTextAtWorldPosition(graphicsContext, style, glm::vec3(1.f, 0.f, 0.f), L"X");
	drawTextAtWorldPosition(graphicsContext, style, glm::vec3(0.f, 1.f, 0.f), L"Y");
	drawTextAtWorldPosition(graphicsContext, style, glm::vec3(0.f, 0.f, 1.f), L"Z");
}

void AppStage_AlignmentCalibration::setMenuState(eAlignmentCalibrationMenuState newState)
{
	if (m_calibrationPanel->getMenuState() != newState)
	{
		eAlignmentCalibrationMenuState oldState= m_calibrationPanel->getMenuState();

		// Update menu state on the panels
		m_calibrationPanel->setMenuState(newState);
		m_cameraSettingsPanel->setMenuState(newState);

		// Broadcast the menu state change to the remote control manager
		{
			std::vector<std::string> parameters;
			parameters.push_back(k_alignmentCalibrationMenuStateStrings[(int)oldState]);
			parameters.push_back(k_alignmentCalibrationMenuStateStrings[(int)newState]);

			sendRemoteControlEvent("menu_state_changed", parameters);
		}
	}
}

// Calibration Model UI Events
void AppStage_AlignmentCalibration::onBeginEvent() { tryBeginCapture(); }

bool AppStage_AlignmentCalibration::tryBeginCapture()
{
	if (m_calibrationPanel->getMenuState() == eAlignmentCalibrationMenuState::verifySetup)
	{
		// Clear out all of the calibration data we recorded
		m_trackerPoseCalibrator->resetCalibrationState();

		// Reset all calibration state on the calibration UI model
		m_calibrationPanel->setCalibrationFraction(0.f);

		// Go back to the camera viewpoint (in case we are in VR view)
		m_cameraSettingsPanel->setViewpointMode(eAlignmentCalibrationViewpointMode::calibration);

		// Advance to the capture state
		setMenuState(eAlignmentCalibrationMenuState::capture);

		return true;
	}

	return false;
}

void AppStage_AlignmentCalibration::onRestartEvent() { tryRestartCapture(); }

bool AppStage_AlignmentCalibration::tryRestartCapture()
{
	if (m_calibrationPanel->getMenuState() != eAlignmentCalibrationMenuState::verifySetup)
	{
		// Clear out all of the calibration data we recorded
		m_trackerPoseCalibrator->resetCalibrationState();

		// Reset all calibration state on the calibration UI model
		m_calibrationPanel->setCalibrationFraction(0.f);

		// Go back to the camera viewpoint (in case we are in VR view)
		m_cameraSettingsPanel->setViewpointMode(eAlignmentCalibrationViewpointMode::calibration);
		onViewportModeChanged(m_cameraSettingsPanel->getViewpointMode());

		// Return to the capture state
		setMenuState(eAlignmentCalibrationMenuState::verifySetup);

		return true;
	}

	return false;
}

void AppStage_AlignmentCalibration::onCancelEvent() { m_ownerWindow->popAppState(); }

void AppStage_AlignmentCalibration::onReturnEvent() { m_ownerWindow->popAppState(); }

void AppStage_AlignmentCalibration::onChessboardStabilityChangedEvent(bool bIsStable)
{
	std::vector<std::string> parameters;
	parameters.push_back(bIsStable ? "true" : "false");

	sendRemoteControlEvent("chessboard_stability_changed", parameters);
}

// Camera Settings Model UI Events
void AppStage_AlignmentCalibration::onViewportModeChanged(eAlignmentCalibrationViewpointMode newViewMode)
{
	switch (newViewMode)
	{
	case eAlignmentCalibrationViewpointMode::calibration:
	{
		getFirstViewport()->setCurrentCamera(m_mkCalibrationView);
	}
	break;
	case eAlignmentCalibrationViewpointMode::stageView:
	{
		getFirstViewport()->setCurrentCamera(m_mkStageView);
	}
	break;
	case eAlignmentCalibrationViewpointMode::xrView:
	{
		getFirstViewport()->setCurrentCamera(m_mkXRView);
	}
	break;
	default:
		break;
	}
}

void AppStage_AlignmentCalibration::onVRFrameDelayChanged(int newVRFrameDelay)
{
	m_targetCameraComponent->getCameraDefinition()->setTrackingFrameDelay(newVRFrameDelay);
}

// Remote Control
bool AppStage_AlignmentCalibration::handleRemoteControlCommand(const std::string& command,
															   const std::vector<std::string>& parameters,
															   std::vector<std::string>& outResults)
{
	if (command == "get_state")
	{
		return handleGetStateCommand(outResults);
	}
	else if (command == "get_chessboard_stability")
	{
		return handleGetChessboardStabilityCommand(outResults);
	}
	else if (command == "begin")
	{
		return handleBeginCommand(outResults);
	}
	else if (command == "restart")
	{
		return handleRestartCommand(outResults);
	}

	return AppStage::handleRemoteControlCommand(command, parameters, outResults);
}

bool AppStage_AlignmentCalibration::handleGetStateCommand(std::vector<std::string>& outResults)
{
	const eAlignmentCalibrationMenuState menuState= m_calibrationPanel->getMenuState();
	const std::string& stateName= k_alignmentCalibrationMenuStateStrings[(int)menuState];

	outResults.push_back(stateName);

	return true;
}

bool AppStage_AlignmentCalibration::handleGetChessboardStabilityCommand(std::vector<std::string>& outResults)
{
	const bool bIsStable= m_calibrationPanel->getCurrentChessboardStable();
	outResults.push_back(bIsStable ? IRemoteControllable::k_true : IRemoteControllable::k_false);

	return true;
}

bool AppStage_AlignmentCalibration::handleBeginCommand(std::vector<std::string>& outResults)
{
	if (tryBeginCapture())
	{
		outResults.push_back(IRemoteControllable::k_success);
	}
	else
	{
		outResults.push_back(IRemoteControllable::k_failure);
	}

	return true;
}

bool AppStage_AlignmentCalibration::handleRestartCommand(std::vector<std::string>& outResults)
{
	if (tryRestartCapture())
	{
		outResults.push_back(IRemoteControllable::k_success);
	}
	else
	{
		outResults.push_back(IRemoteControllable::k_failure);
	}

	return true;
}

void AppStage_AlignmentCalibration::onGui()
{
	AppStage::onGui();

	const float panelWidth= 415.f * MkGui::getUiScale();
	const float displayWidth= m_ownerWindow->getWidth();

	ImGui::SetNextWindowPos(ImVec2(displayWidth - panelWidth, 0.f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelWidth, 0), ImGuiCond_Always);
	constexpr ImGuiWindowFlags k_flags=
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
	MkGuiScopedWindow panel("##AlignmentCalibration", nullptr, k_flags);
	if (!panel)
		return;

	for (IGuiPanel* guiPanel : m_guiPanels)
		guiPanel->onGui();
}
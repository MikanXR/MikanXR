//-- includes -----
#include "VRTrackingRecenter/AppStage_VRTrackingRecenter.h"
#include "VRTrackingRecenter/GuiPanel_VRTrackingRecenter.h"
#include "ArucoMarkerPoseSampler.h"
#include "VRDevicePoseSampler.h"
#include "VRDevicePoseView.h"
#include "App.h"
#include "CalibrationPatternFinder.h"
#include "CameraComponent.h"
#include "CameraObjectSystem.h"
#include "MikanCamera.h"
#include "IMkFrameBuffer.h"
#include "IMkGraphicsContext.h"
#include "MikanLineRenderer.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkScene.h"
#include "MkStateModifiers.h"
#include "MkScopedObjectBinding.h"
#include "MkStateStack.h"
#include "IMkTriangulatedMesh.h"
#include "IMkLineRenderer.h"
#include "MikanTextRenderer.h"
#include "MikanViewport.h"
#include "MainWindow.h"
#include "MathTypeConversion.h"
#include "MathUtility.h"
#include "MathGLM.h"
#include "ModalMessageBox/ModalDialog_MessageBox.h"
#include "TextStyle.h"
#include "VideoFrameDistortionView.h"
#include "VRObjectSystem.h"
#include "VRTrackingVolumeSystem.h"
#include "VideoSourceComponent.h"

#include "glm/gtc/quaternion.hpp"

#include "MkGuiScopedWindow.h"

#include "imgui.h"
#include "MkGuiDrawUtils.h"
#include "LocText.h"

//-- statics ----
const char* AppStage_VRTrackingRecenter::APP_STAGE_NAME= "VRTrackingRecenter";

//-- public methods -----
AppStage_VRTrackingRecenter::AppStage_VRTrackingRecenter(IEditorWindow* ownerWindow)
	: AppStage(ownerWindow, AppStage_VRTrackingRecenter::APP_STAGE_NAME)
	, m_videoSourceComponent()
	, m_markerPoseSampler(nullptr)
	, m_monoDistortionView(nullptr)
	, m_mkCamera(nullptr)
	, m_frameBuffer(createMkFrameBuffer())
	, m_fullscreenRGBQuad(createFullscreenQuadMesh(ownerWindow->getGraphicsContext().get(), false))
{
}

AppStage_VRTrackingRecenter::~AppStage_VRTrackingRecenter() {}

bool AppStage_VRTrackingRecenter::tryEnterAlignmentCalibration(class AppStage* fromAppStage,
															   CameraComponentPtr withCameraComponent,
															   VRTrackingVolumeComponentPtr forTrackingVolume)
{
	assert(fromAppStage);
	assert(withCameraComponent);
	assert(forTrackingVolume);

	VideoSourceComponentPtr videoSourceComponent= withCameraComponent->getVideoSourceComponent();
	if (!videoSourceComponent)
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("vrTrackingRecenter.noVideoSourceMsg"));
		return false;
	}

	if (!withCameraComponent->areApertureIntrinsicsValid())
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("vrTrackingRecenter.invalidIntrinsicsMsg"));

		return false;
	}

	if (!withCameraComponent->hasValidTrackingMountPoseView())
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("vrTrackingRecenter.invalidTrackingMountMsg"));
		return false;
	}

	if (!withCameraComponent->hasValidApertureOffsetXform())
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("vrTrackingRecenter.invalidApertureOffsetMsg"));
		return false;
	}

	IEditorWindow* ownerWindow= fromAppStage->getOwnerWindow();
	auto* vrTrackingRecenterStage= ownerWindow->pushAppStageOfType<AppStage_VRTrackingRecenter>();
	vrTrackingRecenterStage->setSourceCamera(withCameraComponent);
	vrTrackingRecenterStage->setTargetVRTrackingVolume(forTrackingVolume);

	return true;
}

void AppStage_VRTrackingRecenter::setSourceCamera(CameraComponentPtr cameraComponent)
{
	// Get the current video source based on the config
	m_cameraComponent= cameraComponent;
	m_videoSourceComponent= m_cameraComponent->getVideoSourceComponent();
}

void AppStage_VRTrackingRecenter::enter()
{
	AppStage::enter();

	// Fetch the new camera associated with the viewport
	m_mkCamera= getFirstViewport()->getCurrentMikanCamera();
	m_mkCamera->setCameraMovementMode(eCameraMovementMode::stationary);

	// Make sure the camera doing the 3d rendering has the same
	// fov and aspect ration as the real camera
	MikanVideoSourceIntrinsics cameraIntrinsics;
	m_cameraComponent->getApertureIntrinsics(cameraIntrinsics);
	m_mkCamera->applyMonoCameraIntrinsics(&cameraIntrinsics);

	// Create a frame buffer to render the scene into using the resolution and fov from the camera intrinsics
	const MikanMonoIntrinsics& monoIntrinsics= cameraIntrinsics.getMonoIntrinsics();
	m_frameBuffer->setName("VRTrackingRecenter");
	m_frameBuffer->setSize(monoIntrinsics.pixel_width, monoIntrinsics.pixel_height);
	m_frameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_frameBuffer->createResources();

	// Create the distortion view (acts as stream ownership token)
	m_monoDistortionView= new VideoFrameDistortionView(m_videoSourceComponent, eVideoFrameProcessorMode::CALIBRATION);
	m_monoDistortionView->setVideoDisplayMode(eVideoDisplayMode::mode_undistored);

	// Register as a stream consumer — update() drives the retry loop
	m_videoSourceComponent->startVideoStream(m_monoDistortionView);
	eVRTrackingRecenterMenuState newState= eVRTrackingRecenterMenuState::pendingVideoStart;

	// Create app stage GUI panels
	// (Auto cleaned up on app state exit)
	{
		m_calibrationPanel= addGuiPanel<GuiPanel_VRTrackingRecenter>();
		m_calibrationPanel->OnBeginEvent= [this]() { onBeginEvent(); };
		m_calibrationPanel->OnRestartEvent= [this]() { onRestartEvent(); };
		m_calibrationPanel->OnCancelEvent= [this]() { onCancelEvent(); };
		m_calibrationPanel->OnReturnEvent= [this]() { onReturnEvent(); };
		m_calibrationPanel->OnMarkerStabilityChangedEvent= [this](bool b) { onMarkerStabilityChangedEvent(b); };
	}

	setMenuState(newState);
}

void AppStage_VRTrackingRecenter::exit()
{
	setMenuState(eVRTrackingRecenterMenuState::inactive);

	m_mkCamera= nullptr;

	// Free the calibrators
	if (m_markerPoseSampler != nullptr)
	{
		delete m_markerPoseSampler;
		m_markerPoseSampler= nullptr;
	}

	if (m_puckSampler != nullptr)
	{
		delete m_puckSampler;
		m_puckSampler= nullptr;
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

void AppStage_VRTrackingRecenter::setupMarkerPoseSampler()
{
	// Create a sampler to do the actual marker pose recording
	// (m_monoDistortionView is already created in enter())
	m_markerPoseSampler=
		new ArucoMarkerPoseSampler(m_cameraComponent, m_monoDistortionView, DESIRED_MARKER_SAMPLE_COUNT);

	// Create a sampler to record the camera puck pose in VR tracking space
	VRDevicePoseViewPtr puckPoseView=
		m_cameraComponent->makeTrackingMountPoseView(eVRDevicePoseSpace::VRTrackingSystemPose);
	m_puckSampler= new VRDevicePoseSampler(puckPoseView, m_cameraComponent, DESIRED_MARKER_SAMPLE_COUNT);
}

void AppStage_VRTrackingRecenter::updateCameraPose()
{
	switch (m_calibrationPanel->getMenuState())
	{
	case eVRTrackingRecenterMenuState::verifySetup:
	case eVRTrackingRecenterMenuState::capture:
	{
		// All debug rendering during calibration is camera relative
		// so zero out the camera transform
		m_mkCamera->setCameraTransform(glm::mat4(1.f));
	}
	break;
	case eVRTrackingRecenterMenuState::testCalibration:
	{
		// Use the re-centered scene space for the camera.
		// Stage space, deliberately: this stage draws the stage origin at identity to
		// check the recenter, so its viewpoint has to be in that same frame.
		glm::mat4 cameraPose;
		if (m_cameraComponent->getStageSpaceAperturePose(cameraPose))
		{
			m_mkCamera->setCameraTransform(cameraPose);
		}
	}
	break;
	}
}

void AppStage_VRTrackingRecenter::update(float deltaSeconds)
{
	updateCameraPose();

	switch (m_calibrationPanel->getMenuState())
	{
	case eVRTrackingRecenterMenuState::pendingVideoStart:
	{
		if (m_monoDistortionView->isReceivingFrames())
		{
			setupMarkerPoseSampler();
			setMenuState(eVRTrackingRecenterMenuState::verifySetup);
		}
		else if (m_videoSourceComponent->getVideoStreamingStatus() == eVideoStreamingStatus::failed)
		{
			setMenuState(eVRTrackingRecenterMenuState::failedVideoStartStreamRequest);
		}
	}
	break;
	case eVRTrackingRecenterMenuState::verifySetup:
	{
		// Update the video frame buffers to preview the calibration mat
		m_monoDistortionView->readAndProcessVideoFrame();

		// Look for a marker pose so that we can preview if it's in frame
		m_markerPoseSampler->computeApertureRelativeMarkerXform();

		// See if we can compute a valid marker pose
		m_calibrationPanel->setCurrentMarkerValid(m_markerPoseSampler->hasValidApertureRelativeMarkerXform());

		// Update the time that the chessboard has been stable for
		m_calibrationPanel->updateMarkerStabilityTimer(deltaSeconds);
	}
	break;
	case eVRTrackingRecenterMenuState::capture:
	{
		// Update the video frame buffers
		m_monoDistortionView->readAndProcessVideoFrame();

		// Sample marker and puck together each frame
		if (m_markerPoseSampler->computeApertureRelativeMarkerXform() && m_puckSampler->computeVRDeviceXform())
		{
			m_markerPoseSampler->sampleLastApertureRelativeMarkerXform();
			m_puckSampler->sampleLastVRDeviceXform();

			// Update the calibration fraction on the UI Model
			m_calibrationPanel->setCalibrationFraction(m_markerPoseSampler->getCalibrationProgress());
		}

		// See if we have gotten all the samples we require
		if (m_markerPoseSampler->hasFinishedSampling())
		{
			MikanQuatd puckRot;
			MikanVector3d puckPos;
			MikanQuatd markerRot;
			MikanVector3d markerPos;
			glm::mat4 apertureOffsetXform;
			if (m_puckSampler->computeCalibratedDevicePose(puckRot, puckPos)
				&& m_markerPoseSampler->computeCalibratedMarkerPose(markerRot, markerPos)
				&& m_cameraComponent->getApertureOffsetXform(apertureOffsetXform))
			{
				// Compute the puck pose in VRSpace
				glm::dmat4 avgPuckXform_VRSpace=
					glm_mat4_from_pose(MikanQuatd_to_glm_dquat(puckRot), MikanVector3d_to_glm_dvec3(puckPos));

				// Compute the aperture pose in VR space by applying the aperture offset to the puck pose
				glm::dmat4 avgAperturePose_VRSpace= glm_composite_xform(apertureOffsetXform, avgPuckXform_VRSpace);

				// Compute the marker pose in aperture space
				glm::dmat4 avgApertureToMarker=
					glm_mat4_from_pose(MikanQuatd_to_glm_dquat(markerRot), MikanVector3d_to_glm_dvec3(markerPos));

				// The conversion from stage space to VR tracking space is given
				// by the marker pose in VR space since the marker is at the origin of stage space
				glm::dmat4 stageSpaceToVRSpace= glm_composite_xform(avgApertureToMarker, avgAperturePose_VRSpace);
				// Thus the conversion from VR tracking space to stage space is the inverse of that
				glm::mat4 vrSpaceToStageSpace= glm::inverse(glm::mat4(stageSpaceToVRSpace));

				// Publish the new VR device pose offset to the target tracking volume
				m_targetTrackingVolume->setVRSpaceToStageSpace(vrSpaceToStageSpace);

				setMenuState(eVRTrackingRecenterMenuState::testCalibration);
			}
		}
	}
	break;
	case eVRTrackingRecenterMenuState::testCalibration:
	{
		// Update the video frame buffers using the existing distortion calibration
		m_monoDistortionView->readAndProcessVideoFrame();
	}
	break;
	}
}

void AppStage_VRTrackingRecenter::render(IMkViewportPtr targetViewport)
{
	IMkGraphicsContext* graphicsContext= getGraphicsContext();

	// Distortion view won't be valid if we haven't started the video stream yet
	if (!m_monoDistortionView)
		return;

	// Render the scene into the frame buffer
	if (m_frameBuffer->isValid())
	{
		MkScopedObjectBinding colorFramebufferBinding(graphicsContext->getMkStateStack().getCurrentState(),
													  "Color Framebuffer Scope", m_frameBuffer);

		if (colorFramebufferBinding)
		{
			// Draw the camera view no matter the calibration state
			m_monoDistortionView->renderSelectedVideoBuffers();

			switch (m_calibrationPanel->getMenuState())
			{
			case eVRTrackingRecenterMenuState::verifySetup:
			case eVRTrackingRecenterMenuState::capture:
			{
				// draw the camera relative calibration state when calibrating
				m_markerPoseSampler->renderApertureSpaceCalibrationState();
			}
			break;
			case eVRTrackingRecenterMenuState::testCalibration:
			{
				// Draw the origin after calibrating
				glm::mat4 origin(1.f);
				drawTransformedAxes(graphicsContext, origin, 0.1f);

				TextStyle style= getDefaultTextStyle();
				drawTextAtWorldPosition(graphicsContext, style, glm_mat4_get_position(origin), L"Origin");
			}
			break;
			}
		}

		// Clear the depth buffer before drawing the scene
		IMkState* mkState= graphicsContext->getMkStateStack().getCurrentState();
		mkStateClearBuffer(mkState, eMkClearFlags::depth);

		// Render any lines and text that were added to the scene by the calibrator in the frame buffer's viewport
		graphicsContext->getLineRenderer()->render(true);
		graphicsContext->getTextRenderer()->render();
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

void AppStage_VRTrackingRecenter::onGui()
{
	AppStage::onGui();

	const float panelWidth= 415.f * MkGui::getUiScale();
	const float displayWidth= m_ownerWindow->getWidth();
	ImGui::SetNextWindowPos(ImVec2(displayWidth - panelWidth, 0.f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelWidth, 0), ImGuiCond_Always);

	constexpr ImGuiWindowFlags k_flags=
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

	MkGuiScopedWindow panel("##VRTrackingRecenter", nullptr, k_flags);
	if (!panel)
		return;

	for (IGuiPanel* guiPanel : m_guiPanels)
		guiPanel->onGui();
}

void AppStage_VRTrackingRecenter::setMenuState(eVRTrackingRecenterMenuState newState)
{
	if (m_calibrationPanel->getMenuState() != newState)
	{
		eVRTrackingRecenterMenuState oldState= m_calibrationPanel->getMenuState();

		// Update menu state on the data models
		m_calibrationPanel->setMenuState(newState);

		// Broadcast the menu state change to the remote control manager
		{
			std::vector<std::string> parameters;
			parameters.push_back(k_VRTrackingRecenterMenuStateStrings[(int)oldState]);
			parameters.push_back(k_VRTrackingRecenterMenuStateStrings[(int)newState]);

			sendRemoteControlEvent("menu_state_changed", parameters);
		}
	}
}

// Calibration Model UI Events
void AppStage_VRTrackingRecenter::onBeginEvent() { tryBeginCapture(); }

bool AppStage_VRTrackingRecenter::tryBeginCapture()
{
	if (m_calibrationPanel->getMenuState() == eVRTrackingRecenterMenuState::verifySetup)
	{
		// Clear out all of the calibration data we recorded
		m_markerPoseSampler->resetCalibrationState();
		m_puckSampler->resetCalibrationState();

		// Reset all calibration state on the calibration UI model
		m_calibrationPanel->setCalibrationFraction(0.f);

		// Advance to the capture state
		setMenuState(eVRTrackingRecenterMenuState::capture);

		return true;
	}

	return false;
}

void AppStage_VRTrackingRecenter::onRestartEvent() { tryRestartCapture(); }

bool AppStage_VRTrackingRecenter::tryRestartCapture()
{
	if (m_calibrationPanel->getMenuState() != eVRTrackingRecenterMenuState::verifySetup)
	{
		// Clear out all of the calibration data we recorded
		m_markerPoseSampler->resetCalibrationState();
		m_puckSampler->resetCalibrationState();

		// Reset all calibration state on the calibration UI model
		m_calibrationPanel->setCalibrationFraction(0.f);

		// Return to the capture state
		setMenuState(eVRTrackingRecenterMenuState::verifySetup);

		return true;
	}

	return false;
}

void AppStage_VRTrackingRecenter::onCancelEvent() { m_ownerWindow->popAppState(); }

void AppStage_VRTrackingRecenter::onReturnEvent() { m_ownerWindow->popAppState(); }

void AppStage_VRTrackingRecenter::onMarkerStabilityChangedEvent(bool bIsStable)
{
	std::vector<std::string> parameters;
	parameters.push_back(bIsStable ? IRemoteControllable::k_true : IRemoteControllable::k_false);

	sendRemoteControlEvent("marker_stability_changed", parameters);
}

// Remote Control
bool AppStage_VRTrackingRecenter::handleRemoteControlCommand(const std::string& command,
															 const std::vector<std::string>& parameters,
															 std::vector<std::string>& outResults)
{
	if (!IRemoteControllable::handleRemoteControlCommand(command, parameters, outResults))
	{
		if (command == "get_state")
		{
			return handleGetStateCommand(outResults);
		}
		else if (command == "get_marker_stability")
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
	}

	return false;
}

bool AppStage_VRTrackingRecenter::handleGetStateCommand(std::vector<std::string>& outResults)
{
	const eVRTrackingRecenterMenuState menuState= m_calibrationPanel->getMenuState();
	const std::string& stateName= k_VRTrackingRecenterMenuStateStrings[(int)menuState];

	outResults.push_back(stateName);

	return true;
}

bool AppStage_VRTrackingRecenter::handleGetChessboardStabilityCommand(std::vector<std::string>& outResults)
{
	const bool bIsStable= m_calibrationPanel->getCurrentMarkerStable();
	outResults.push_back(bIsStable ? IRemoteControllable::k_true : IRemoteControllable::k_false);

	return true;
}

bool AppStage_VRTrackingRecenter::handleBeginCommand(std::vector<std::string>& outResults)
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

bool AppStage_VRTrackingRecenter::handleRestartCommand(std::vector<std::string>& outResults)
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
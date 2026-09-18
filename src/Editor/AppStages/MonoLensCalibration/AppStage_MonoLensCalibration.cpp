// Derived From example 11-1 of "Learning OpenCV: Computer Vision with the OpenCV Library" by Gary Bradski

//-- includes -----
#include "MonoLensCalibration/AppStage_MonoLensCalibration.h"
#include "MonoLensCalibration/GuiPanel_MonoLensCalibration.h"
#include "MonoLensCalibration/GuiPanel_MonoCameraSettings.h"
#include "imgui.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiScopedWindow.h"
#include "InputManager.h"
#include "MainWindow.h"
#include "MarkerObjectSystem.h"
#include "MonoLensDistortionCalibrator.h"
#include "CalibrationPatternFinder.h"
#include "VideoSourceComponent.h"
#include "VideoFrameDistortionView.h"

//-- statics ----
const char* AppStage_MonoLensCalibration::APP_STAGE_NAME= "MonoCalibration";

const std::string g_monoLensCalibrationMenuStateStrings[(int)eMonoLensCalibrationMenuState::COUNT]= {
	"inactive",          "capture",           "processingCalibration",
	"testCalibration",   "failedCalibration", "failedVideoStartStreamRequest",
	"pendingVideoStart",
};

//-- public methods -----
AppStage_MonoLensCalibration::AppStage_MonoLensCalibration(IEditorWindow* ownerWindow)
	: AppStage(ownerWindow, AppStage_MonoLensCalibration::APP_STAGE_NAME)
	, m_videoSourceComponent()
	, m_monoLensCalibrator(nullptr)
	, m_monoDistortionView(nullptr)
{
}

AppStage_MonoLensCalibration::~AppStage_MonoLensCalibration() {}

void AppStage_MonoLensCalibration::setBypassCalibrationFlag(bool flag)
{
	m_bypassCalibrationFlag= flag;
	if (m_calibrationPanel != nullptr)
		m_calibrationPanel->setBypassCalibrationFlag(flag);
}

void AppStage_MonoLensCalibration::setVideoSourceComponent(VideoSourceComponentPtr videoSourceComponent)
{
	m_videoSourceComponent= videoSourceComponent;
}

void AppStage_MonoLensCalibration::enter()
{
	AppStage::enter();

	// Bind to space bar to capture frames
	// (Auto cleared on AppStage exit)
	getOwnerWindow()->getInputManager()->fetchOrAddKeyBindings(MkKey::SPACE)->OnKeyPressed+=
		MakeDelegate(this, &AppStage_MonoLensCalibration::onCaptureKeyPressed);

	// Create the distortion view eagerly — it is the stream ownership token
	m_monoDistortionView= new VideoFrameDistortionView(m_videoSourceComponent, eVideoFrameProcessorMode::CALIBRATION);

	// Register as a stream consumer — VideoSourceComponent::update() drives the retry loop
	m_videoSourceComponent->startVideoStream(m_monoDistortionView);

	eMonoLensCalibrationMenuState newState= eMonoLensCalibrationMenuState::pendingVideoStart;

	// Create GUI panels
	// (Auto cleaned up on app state exit)
	{
		m_calibrationPanel= addGuiPanel<GuiPanel_MonoLensCalibration>();
		m_calibrationPanel->setBypassCalibrationFlag(m_bypassCalibrationFlag);
		m_calibrationPanel->OnCancelEvent= [this]() { onCancelEvent(); };
		m_calibrationPanel->OnRestartEvent= [this]() { onRestartEvent(); };
		m_calibrationPanel->OnReturnEvent= [this]() { onReturnEvent(); };
		m_calibrationPanel->OnImagePointStabilityChangedEvent= [this](bool bIsStable)
		{ onImagePointStabilityChangedEvent(bIsStable); };

		m_cameraSettingsPanel= addGuiPanel<GuiPanel_MonoCameraSettings>();
		m_cameraSettingsPanel->OnVideoDisplayModeChanged= [this](eVideoDisplayMode mode)
		{ onVideoDisplayModeChanged(mode); };
	}

	setMenuState(newState);
}

void AppStage_MonoLensCalibration::exit()
{
	setMenuState(eMonoLensCalibrationMenuState::inactive);

	// Free the calibrator
	if (m_monoLensCalibrator != nullptr)
	{
		delete m_monoLensCalibrator;
		m_monoLensCalibrator= nullptr;
	}

	// Stop the stream and free the distortion view buffers
	if (m_monoDistortionView != nullptr)
	{
		m_videoSourceComponent->stopVideoStream(m_monoDistortionView);
		delete m_monoDistortionView;
		m_monoDistortionView= nullptr;
	}

	m_videoSourceComponent= nullptr;

	AppStage::exit();
}

void AppStage_MonoLensCalibration::update(float deltaSeconds)
{
	AppStage::update(deltaSeconds);

	// Update the calibration state machine
	switch (m_calibrationPanel->getMenuState())
	{
	case eMonoLensCalibrationMenuState::inactive:
	{
	}
	break;

	case eMonoLensCalibrationMenuState::pendingVideoStart:
	{
		if (m_monoDistortionView->isReceivingFrames())
		{
			// Process the first video frame to determine the resolution and initialize the calibrator
			m_monoDistortionView->readAndProcessVideoFrame();

			// If we have a valid frame size, we can initialize the calibrator
			if (m_monoDistortionView->getFrameWidth() > 0 && m_monoDistortionView->getFrameHeight() > 0)
			{
				// State transition is handled inside setupMonoLensCalibrator()
				setupMonoLensCalibrator();
			}
		}
		else if (m_videoSourceComponent->getVideoStreamingStatus() == eVideoStreamingStatus::failed)
		{
			setMenuState(eMonoLensCalibrationMenuState::failedVideoStartStreamRequest);
		}
	}
	break;

	case eMonoLensCalibrationMenuState::capture:
	{
		assert(m_monoLensCalibrator != nullptr);

		// Update calibration progress UI data binding
		m_calibrationPanel->setCalibrationFraction(m_monoLensCalibrator->computeCalibrationProgress());

		// Update image points valid flag UI data binding
		m_calibrationPanel->setCurrentImagePointsValid(m_monoLensCalibrator->areCurrentImagePointsValid());

		// Update the image point stability timer / flag
		m_calibrationPanel->updateImagePointStabilityTimer(deltaSeconds);

		// Update the video frame buffers
		m_monoDistortionView->readAndProcessVideoFrame();

		// Update the chess board capture state
		assert(m_monoDistortionView->isGrayscaleUndistortDisabled());
		m_monoLensCalibrator->findNewCalibrationPattern(BOARD_NEW_LOCATION_PIXEL_DIST);

		// See if we have gotten all the samples we require
		if (m_monoLensCalibrator->hasSampledAllCalibrationPatterns())
		{
			// Kick off the async task (very expensive)
			m_monoLensCalibrator->computeCameraCalibration();
			setMenuState(eMonoLensCalibrationMenuState::processingCalibration);
		}
	}
	break;

	case eMonoLensCalibrationMenuState::processingCalibration:
	{
		MikanMonoIntrinsics new_mono_intrinsics;

		if (m_monoLensCalibrator->getIsCameraCalibrationComplete())
		{
			if (m_monoLensCalibrator->getCameraCalibration(&new_mono_intrinsics))
			{
				// Update the camera intrinsics for this camera
				MikanVideoSourceIntrinsics cameraIntrinsics;
				cameraIntrinsics.makeMonoIntrinsics()= new_mono_intrinsics;
				m_videoSourceComponent->setCameraIntrinsics(cameraIntrinsics);

				// Rebuild the distortion map to reflect the updated calibration
				m_monoDistortionView->applyMonoCameraIntrinsics(&new_mono_intrinsics);
				m_monoDistortionView->setGrayscaleUndistortDisabled(false);
				m_monoDistortionView->setVideoDisplayMode(eVideoDisplayMode::mode_bgr);

				// Switch back to the color video feed
				m_cameraSettingsPanel->setVideoDisplayMode(eVideoDisplayMode::mode_undistored);

				// Go to the test calibration state
				setMenuState(eMonoLensCalibrationMenuState::testCalibration);
			}
			else
			{
				setMenuState(eMonoLensCalibrationMenuState::failedCalibration);
			}
		}
	}
	break;

	case eMonoLensCalibrationMenuState::testCalibration:
	{
		// Update reprojection error in the UI data binding
		m_calibrationPanel->setReprojectionError(m_monoLensCalibrator->getReprojectionError());

		// Update the video frame buffers using the existing distortion calibration
		m_monoDistortionView->readAndProcessVideoFrame();
	}
	break;

	case eMonoLensCalibrationMenuState::failedCalibration:
	{
	}
	break;

	case eMonoLensCalibrationMenuState::failedVideoStartStreamRequest:
	{
	}
	break;

	default:
		assert(0 && "unreachable");
	}
}

void AppStage_MonoLensCalibration::render(IMkViewportPtr targetViewport)
{
	eMonoLensCalibrationMenuState menuState= m_calibrationPanel->getMenuState();

	if (menuState == eMonoLensCalibrationMenuState::capture)
	{
		m_monoDistortionView->renderSelectedVideoBuffers();
		m_monoLensCalibrator->renderCalibrationState();
	}
	else if (menuState == eMonoLensCalibrationMenuState::testCalibration)
	{
		m_monoDistortionView->renderSelectedVideoBuffers();
	}
}

void AppStage_MonoLensCalibration::onCaptureKeyPressed() { tryCapture(); }

bool AppStage_MonoLensCalibration::tryCapture()
{
	eMonoLensCalibrationMenuState menuState= m_calibrationPanel->getMenuState();

	if (menuState == eMonoLensCalibrationMenuState::capture)
	{
		// Update the chess board capture state
		if (m_monoLensCalibrator != nullptr)
		{
			return m_monoLensCalibrator->captureLastFoundCalibrationPattern();
		}
	}

	return false;
}

void AppStage_MonoLensCalibration::setupMonoLensCalibrator()
{
	m_monoLensCalibrator= new MonoLensDistortionCalibrator(getSystemOfType<MarkerObjectSystem>(), m_monoDistortionView,
														   DESIRED_CAPTURE_BOARD_COUNT);

	if (m_bypassCalibrationFlag)
	{
		m_monoDistortionView->setGrayscaleUndistortDisabled(false);
		setMenuState(eMonoLensCalibrationMenuState::testCalibration);
	}
	else
	{
		m_monoDistortionView->setGrayscaleUndistortDisabled(true);
		setMenuState(eMonoLensCalibrationMenuState::capture);
	}
}

void AppStage_MonoLensCalibration::setMenuState(eMonoLensCalibrationMenuState newState)
{
	if (m_calibrationPanel->getMenuState() != newState)
	{
		eMonoLensCalibrationMenuState oldState= m_calibrationPanel->getMenuState();

		// Update menu state on the calibration panel
		m_calibrationPanel->setMenuState(newState);

		// Broadcast the menu state change to the remote control manager
		{
			std::vector<std::string> parameters;
			parameters.push_back(g_monoLensCalibrationMenuStateStrings[(int)oldState]);
			parameters.push_back(g_monoLensCalibrationMenuStateStrings[(int)newState]);

			sendRemoteControlEvent("menu_state_changed", parameters);
		}
	}
}

// Calibration Model UI Events
void AppStage_MonoLensCalibration::onRestartEvent()
{
	// Clear out data model calibration state
	m_calibrationPanel->resetCalibrationState();

	// Clear out all of the calibration data we recorded
	m_monoLensCalibrator->resetCalibrationState();

	// Reset the distortion map back to the camera intrinsics we started with
	m_monoLensCalibrator->resetDistortionView();

	// Turn back off grayscale undistortion mode
	m_monoDistortionView->setGrayscaleUndistortDisabled(false);

	// Go back to the capture state
	setMenuState(eMonoLensCalibrationMenuState::capture);
}

void AppStage_MonoLensCalibration::onReturnEvent() { m_ownerWindow->popAppState(); }

void AppStage_MonoLensCalibration::onCancelEvent() { m_ownerWindow->popAppState(); }

void AppStage_MonoLensCalibration::onImagePointStabilityChangedEvent(bool bIsStable)
{
	std::vector<std::string> parameters;
	parameters.push_back(bIsStable ? "true" : "false");

	sendRemoteControlEvent("image_point_stability_changed", parameters);
}

// Camera Settings Model UI Events
void AppStage_MonoLensCalibration::onVideoDisplayModeChanged(eVideoDisplayMode newDisplayMode)
{
	m_monoDistortionView->setVideoDisplayMode(newDisplayMode);
}

bool AppStage_MonoLensCalibration::handleRemoteControlCommand(const std::string& command,
															  const std::vector<std::string>& parameters,
															  std::vector<std::string>& outResults)
{
	if (!IRemoteControllable::handleRemoteControlCommand(command, parameters, outResults))
	{
		if (command == "get_state")
		{
			return handleGetStateCommand(outResults);
		}
		else if (command == "get_image_point_stability")
		{
			return handleGetImagePointStabilityCommand(outResults);
		}
		else if (command == "get_samples_needed")
		{
			return handleGetSamplesNeededCommand(outResults);
		}
		else if (command == "capture")
		{
			return handleCaptureCommand(outResults);
		}
	}

	return false;
}

bool AppStage_MonoLensCalibration::handleGetStateCommand(std::vector<std::string>& outResults)
{
	const eMonoLensCalibrationMenuState menuState= m_calibrationPanel->getMenuState();
	const std::string& stateName= g_monoLensCalibrationMenuStateStrings[(int)menuState];

	outResults.push_back(stateName);

	return true;
}

bool AppStage_MonoLensCalibration::handleGetImagePointStabilityCommand(std::vector<std::string>& outResults)
{
	const bool bIsStable= m_monoLensCalibrator->getIsCameraCalibrationComplete();
	outResults.push_back(bIsStable ? IRemoteControllable::k_true : IRemoteControllable::k_false);

	return true;
}

bool AppStage_MonoLensCalibration::handleGetSamplesNeededCommand(std::vector<std::string>& outResults)
{
	const int samplesNeeded= m_monoLensCalibrator->getDesiredPatternCount();
	outResults.push_back(std::to_string(samplesNeeded));

	return true;
}

bool AppStage_MonoLensCalibration::handleCaptureCommand(std::vector<std::string>& outResults)
{
	if (tryCapture())
	{
		outResults.push_back(IRemoteControllable::k_success);
	}
	else
	{
		outResults.push_back(IRemoteControllable::k_failure);
	}

	float calibrationFraction= m_monoLensCalibrator->computeCalibrationProgress();
	outResults.push_back(std::to_string(calibrationFraction));

	return true;
}

void AppStage_MonoLensCalibration::onGui()
{
	AppStage::onGui();

	const float panelWidth= 415.f * MkGui::getUiScale();
	const float displayWidth= m_ownerWindow->getWidth();
	const float displayHeight= m_ownerWindow->getHeight();

	ImGui::SetNextWindowPos(ImVec2(displayWidth - panelWidth, 0.f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelWidth, displayHeight), ImGuiCond_Always);
	constexpr ImGuiWindowFlags k_flags=
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
	MkGuiScopedWindow panel("##MonoLensCalibration", nullptr, k_flags);
	if (!panel)
		return;

	for (IGuiPanel* guiPanel : m_guiPanels)
		guiPanel->onGui();
}
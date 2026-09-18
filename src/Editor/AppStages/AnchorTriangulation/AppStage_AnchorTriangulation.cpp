// Derived From example 11-1 of "Learning OpenCV: Computer Vision with the OpenCV Library" by Gary Bradski

//-- includes -----
#include "AnchorTriangulation/AppStage_AnchorTriangulation.h"
#include "AnchorTriangulation/GuiPanel_AnchorTriangulation.h"
#include "imgui.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiScopedWindow.h"
#include "App.h"
#include "CameraObjectSystem.h"
#include "Colors.h"
#include "IEditorWindow.h"
#include "IMkGraphicsContext.h"
#include "IMkLineRenderer.h"
#include "MikanCamera.h"
#include "MikanLineRenderer.h"
#include "MikanTextRenderer.h"
#include "MikanViewport.h"
#include "AnchorComponent.h"
#include "AnchorObjectSystem.h"
#include "AnchorTriangulator.h"
#include "CameraComponent.h"
#include "InputManager.h"
#include "MathTypeConversion.h"
#include "MathUtility.h"
#include "ProjectConfig.h"
#include "StringUtils.h"
#include "StageObjectSystem.h"
#include "TextStyle.h"
#include "VideoSourceComponent.h"
#include "VideoFrameDistortionView.h"

#include "glm/gtc/quaternion.hpp"

//-- statics ----
const char* AppStage_AnchorTriangulation::APP_STAGE_NAME= "AnchorTriangulation";

//-- public methods -----
AppStage_AnchorTriangulation::AppStage_AnchorTriangulation(IEditorWindow* ownerWindow)
	: AppStage(ownerWindow, AppStage_AnchorTriangulation::APP_STAGE_NAME)
	, m_currentSceneCameraComponent()
	, m_anchorTriangulator(nullptr)
	, m_monoDistortionView(nullptr)
	, m_mkCamera(nullptr)
{
	m_targetAnchor.anchorId= INVALID_MIKAN_ID;
	m_targetAnchor.anchorName= "";
	m_targetAnchor.worldTransform= GlmTransform();
}

AppStage_AnchorTriangulation::~AppStage_AnchorTriangulation() {}

void AppStage_AnchorTriangulation::setBypassCalibrationFlag(bool flag)
{
	m_bypassCalibrationFlag= flag;
	if (m_calibrationPanel != nullptr)
		m_calibrationPanel->setBypassCalibrationFlag(flag);
}

void AppStage_AnchorTriangulation::setSourceCamera(CameraComponentPtr cameraComponent)
{
	m_currentSceneCameraComponent= cameraComponent;
	m_videoSourceComponent= m_currentSceneCameraComponent->getVideoSourceComponent();
}

void AppStage_AnchorTriangulation::enter()
{
	AppStage::enter();

	// Create a new camera to view the scene
	m_mkCamera= getFirstViewport()->getCurrentMikanCamera();
	m_mkCamera->setCameraMovementMode(eCameraMovementMode::stationary);

	// Make sure the camera doing the 3d rendering has the same
	// fov and aspect ration as the real camera
	MikanVideoSourceIntrinsics cameraIntrinsics;
	m_currentSceneCameraComponent->getApertureIntrinsics(cameraIntrinsics);
	m_mkCamera->applyMonoCameraIntrinsics(&cameraIntrinsics);

	// Create the distortion view eagerly — it is the stream ownership token
	m_monoDistortionView= new VideoFrameDistortionView(m_videoSourceComponent, eVideoFrameProcessorMode::CALIBRATION);
	m_monoDistortionView->setVideoDisplayMode(eVideoDisplayMode::mode_undistored);

	// Register as a stream consumer — VideoSourceComponent::update() drives the retry loop
	m_videoSourceComponent->startVideoStream(m_monoDistortionView);

	eAnchorTriangulationMenuState newState= eAnchorTriangulationMenuState::pendingVideoStartStreamRequest;

	// Create GUI panels
	// (Auto cleaned up on app state exit)
	{
		m_calibrationPanel= addGuiPanel<GuiPanel_AnchorTriangulation>();
		m_calibrationPanel->setBypassCalibrationFlag(m_bypassCalibrationFlag);
		m_calibrationPanel->OnOkEvent= [this]() { onOkEvent(); };
		m_calibrationPanel->OnRedoEvent= [this]() { onRedoEvent(); };
		m_calibrationPanel->OnCancelEvent= [this]() { onCancelEvent(); };
	}

	// Bind to space bar to capture frames
	// (Auto cleared on AppStage exit)
	{
		EventBindingSet* bindingSet= getOwnerWindow()->getInputManager()->getCurrentEventBindingSet();

		bindingSet->OnMouseButtonReleasedEvent+= MakeDelegate(this, &AppStage_AnchorTriangulation::onMouseButtonUp);
	}

	setMenuState(newState);
}

void AppStage_AnchorTriangulation::setupDistortionView()
{
	// Create a calibrator to do the actual triangulation
	m_anchorTriangulator= new AnchorTriangulator(m_currentSceneCameraComponent, m_monoDistortionView);
}

void AppStage_AnchorTriangulation::exit()
{
	setMenuState(eAnchorTriangulationMenuState::inactive);

	m_currentSceneCameraComponent= nullptr;
	m_mkCamera= nullptr;

	// Free the calibrator
	if (m_anchorTriangulator != nullptr)
	{
		delete m_anchorTriangulator;
		m_anchorTriangulator= nullptr;
	}

	// Stop the stream and free the distortion view buffers
	if (m_monoDistortionView != nullptr)
	{
		if (m_videoSourceComponent)
			m_videoSourceComponent->stopVideoStream(m_monoDistortionView);
		delete m_monoDistortionView;
		m_monoDistortionView= nullptr;
	}

	m_videoSourceComponent= nullptr;

	AppStage::exit();
}

void AppStage_AnchorTriangulation::updateCameraTransform()
{
	// Update the transform of the camera so that vr models align over the tracking puck.
	// World space: the triangulated anchor is stored with setWorldTransform.
	glm::mat4 cameraPose;
	if (m_currentSceneCameraComponent->getWorldSpaceAperturePose(cameraPose))
	{
		m_mkCamera->setCameraTransform(cameraPose);
	}
}

void AppStage_AnchorTriangulation::update(float deltaSeconds)
{
	AppStage::update(deltaSeconds);

	updateCameraTransform();

	eAnchorTriangulationMenuState calibrationState= m_calibrationPanel->getMenuState();

	if (calibrationState == eAnchorTriangulationMenuState::pendingVideoStartStreamRequest)
	{
		if (m_monoDistortionView->isReceivingFrames())
		{
			setupDistortionView();
			if (m_bypassCalibrationFlag)
				setMenuState(eAnchorTriangulationMenuState::testCalibration);
			else
				setMenuState(eAnchorTriangulationMenuState::verifyInitialCameraSetup);
		}
		else if (m_videoSourceComponent->getVideoStreamingStatus() == eVideoStreamingStatus::failed)
		{
			setMenuState(eAnchorTriangulationMenuState::failedVideoStartStreamRequest);
		}
		return;
	}

	// Update the video frame buffers to preview the calibration mat
	if (m_monoDistortionView->isReceivingFrames())
	{
		m_monoDistortionView->readAndProcessVideoFrame();

		// Update triangulation during triangulation states
		if (calibrationState == eAnchorTriangulationMenuState::captureOrigin2
			|| calibrationState == eAnchorTriangulationMenuState::captureXAxis2
			|| calibrationState == eAnchorTriangulationMenuState::captureYAxis2)
		{
			m_anchorTriangulator->computeCurrentTriangulation();
		}
	}
}

void AppStage_AnchorTriangulation::render(IMkViewportPtr targetViewport)
{
	switch (m_calibrationPanel->getMenuState())
	{
	case eAnchorTriangulationMenuState::verifyInitialCameraSetup:
	case eAnchorTriangulationMenuState::captureOrigin1:
	case eAnchorTriangulationMenuState::captureXAxis1:
	case eAnchorTriangulationMenuState::captureYAxis1:
	case eAnchorTriangulationMenuState::verifyInitialPointCapture:
	{
		m_monoDistortionView->renderSelectedVideoBuffers();
		m_anchorTriangulator->renderInitialPoint2dSegements();
	}
	break;
	case eAnchorTriangulationMenuState::moveCamera:
	{
		m_monoDistortionView->renderSelectedVideoBuffers();
		m_anchorTriangulator->renderInitialPoint3dRays();
	}
	break;
	case eAnchorTriangulationMenuState::captureOrigin2:
	case eAnchorTriangulationMenuState::captureXAxis2:
	case eAnchorTriangulationMenuState::captureYAxis2:
	{
		m_monoDistortionView->renderSelectedVideoBuffers();
		m_anchorTriangulator->renderCurrentPointTriangulation();
	}
	break;
	case eAnchorTriangulationMenuState::verifyTriangulatedPoints:
	{
		m_monoDistortionView->renderSelectedVideoBuffers();
		m_anchorTriangulator->renderAllTriangulatedPoints(false);
	}
	break;
	case eAnchorTriangulationMenuState::testCalibration:
	{
		m_monoDistortionView->renderSelectedVideoBuffers();
		m_anchorTriangulator->renderAllTriangulatedPoints(false);
		m_anchorTriangulator->renderAnchorTransform();
	}
	break;
	}

	// Render any pending lines with depth testing disabled
	getGraphicsContext()->getLineRenderer()->render(true);
}

void AppStage_AnchorTriangulation::setMenuState(eAnchorTriangulationMenuState newState)
{
	if (m_calibrationPanel->getMenuState() != newState)
	{
		// Update menu state on the data models
		m_calibrationPanel->setMenuState(newState);
	}
}

// Input Events
void AppStage_AnchorTriangulation::onMouseButtonUp(int button)
{
	eAnchorTriangulationMenuState menuState= m_calibrationPanel->getMenuState();

	if (menuState == eAnchorTriangulationMenuState::captureOrigin1
		|| menuState == eAnchorTriangulationMenuState::captureXAxis1
		|| menuState == eAnchorTriangulationMenuState::captureYAxis1
		|| menuState == eAnchorTriangulationMenuState::captureOrigin2
		|| menuState == eAnchorTriangulationMenuState::captureXAxis2
		|| menuState == eAnchorTriangulationMenuState::captureYAxis2)
	{
		if (button == MkMouseButton::LEFT)
		{
			m_anchorTriangulator->sampleMouseScreenPosition();
		}

		switch (menuState)
		{
		case eAnchorTriangulationMenuState::captureOrigin1:
			setMenuState(eAnchorTriangulationMenuState::captureXAxis1);
			break;
		case eAnchorTriangulationMenuState::captureXAxis1:
			setMenuState(eAnchorTriangulationMenuState::captureYAxis1);
			break;
		case eAnchorTriangulationMenuState::captureYAxis1:
			setMenuState(eAnchorTriangulationMenuState::verifyInitialPointCapture);
			break;
		case eAnchorTriangulationMenuState::captureOrigin2:
			setMenuState(eAnchorTriangulationMenuState::captureXAxis2);
			break;
		case eAnchorTriangulationMenuState::captureXAxis2:
			setMenuState(eAnchorTriangulationMenuState::captureYAxis2);
			break;
		case eAnchorTriangulationMenuState::captureYAxis2:
			setMenuState(eAnchorTriangulationMenuState::verifyTriangulatedPoints);
			break;
		}
	}
}

// Calibration Model UI Events
void AppStage_AnchorTriangulation::onOkEvent()
{
	switch (m_calibrationPanel->getMenuState())
	{
	case eAnchorTriangulationMenuState::verifyInitialCameraSetup:
	{
		// Clear out all of the calibration data we recorded
		m_anchorTriangulator->resetCalibrationState();

		// Record the initial camera post
		m_anchorTriangulator->sampleCameraPose();

		// Reset the capture point count on the UI model
		m_calibrationPanel->setCapturedPointCount(0);

		setMenuState(eAnchorTriangulationMenuState::captureOrigin1);
	}
	break;
	case eAnchorTriangulationMenuState::verifyInitialPointCapture:
	{
		setMenuState(eAnchorTriangulationMenuState::moveCamera);
	}
	break;
	case eAnchorTriangulationMenuState::moveCamera:
	{
		// Reset all calibration state on the calibration UI model
		m_calibrationPanel->setCapturedPointCount(0);

		setMenuState(eAnchorTriangulationMenuState::captureOrigin2);
	}
	break;
	case eAnchorTriangulationMenuState::verifyTriangulatedPoints:
	{
		m_anchorTriangulator->computeAnchorTransform(m_targetAnchor);

		if (m_targetAnchor.anchorId == INVALID_MIKAN_ID)
		{
			getSystemOfType<AnchorObjectSystem>()->addNewObjectByTypedDefinition(
				[this](AnchorDefinitionPtr anchorDefinition)
				{
					anchorDefinition->setComponentName(m_targetAnchor.anchorName);
					// Newly created anchor has no parent, so relative transform is world transform
					anchorDefinition->setRelativeTransform(m_targetAnchor.worldTransform);
					return true;
				});
		}
		else
		{
			AnchorComponentPtr anchorComponent=
				getSystemOfType<AnchorObjectSystem>()->getSpatialAnchorById(m_targetAnchor.anchorId);

			anchorComponent->setWorldTransform(m_targetAnchor.worldTransform.getMat4());
		}

		setMenuState(eAnchorTriangulationMenuState::testCalibration);
	}
	break;
	case eAnchorTriangulationMenuState::testCalibration:
	case eAnchorTriangulationMenuState::failedVideoStartStreamRequest:
	{
		m_ownerWindow->popAppState();
	}
	break;
	}
}

void AppStage_AnchorTriangulation::onRedoEvent()
{
	// Clear out all of the calibration data we recorded
	m_anchorTriangulator->resetCalibrationState();

	// Reset the capture point count on the UI model
	m_calibrationPanel->setCapturedPointCount(0);

	// Return to the capture state
	switch (m_calibrationPanel->getMenuState())
	{
	case eAnchorTriangulationMenuState::captureOrigin1:
	case eAnchorTriangulationMenuState::captureXAxis1:
	case eAnchorTriangulationMenuState::captureYAxis1:
	case eAnchorTriangulationMenuState::verifyInitialPointCapture:
	case eAnchorTriangulationMenuState::testCalibration:
		setMenuState(eAnchorTriangulationMenuState::verifyInitialCameraSetup);
		break;
	case eAnchorTriangulationMenuState::captureOrigin2:
	case eAnchorTriangulationMenuState::captureXAxis2:
	case eAnchorTriangulationMenuState::captureYAxis2:
	case eAnchorTriangulationMenuState::verifyTriangulatedPoints:
		setMenuState(eAnchorTriangulationMenuState::captureOrigin2);
		break;
	}
}

void AppStage_AnchorTriangulation::onCancelEvent() { m_ownerWindow->popAppState(); }

void AppStage_AnchorTriangulation::onGui()
{
	AppStage::onGui();

	const float panelWidth= 415.f * MkGui::getUiScale();
	const float displayWidth= m_ownerWindow->getWidth();
	const float displayHeight= m_ownerWindow->getHeight();

	ImGui::SetNextWindowPos(ImVec2(displayWidth - panelWidth, 0.f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelWidth, displayHeight), ImGuiCond_Always);
	constexpr ImGuiWindowFlags k_flags=
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
	MkGuiScopedWindow panel("##AnchorTriangulation", nullptr, k_flags);
	if (!panel)
		return;

	for (IGuiPanel* guiPanel : m_guiPanels)
		guiPanel->onGui();
}
#include "LightFixtureCalibration/AppStage_LightFixtureCalibration.h"
#include "LightFixtureCalibration/GuiPanel_LightFixtureCalibration.h"

#include "imgui.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiScopedWindow.h"
#include "CameraComponent.h"
#include "Colors.h"
#include "DMXFixtureComponent.h"
#include "IEditorWindow.h"
#include "IMkGraphicsContext.h"
#include "IMkLineRenderer.h"
#include "InputManager.h"
#include "LightFixtureTriangulator.h"
#include "MikanCamera.h"
#include "MikanViewport.h"
#include "RGBSpotLightComponent.h"
#include "RGBSpotLightSystem.h"
#include "RGBPixelGridComponent.h"
#include "RGBPixelGridSystem.h"
#include "TransformComponent.h"
#include "VideoFrameDistortionView.h"
#include "VideoSourceComponent.h"

#include "glm/gtc/matrix_transform.hpp"

const char* AppStage_LightFixtureCalibration::APP_STAGE_NAME= "LightFixtureCalibration";

AppStage_LightFixtureCalibration::AppStage_LightFixtureCalibration(IEditorWindow* ownerWindow)
	: AppStage(ownerWindow, APP_STAGE_NAME)
{
}

AppStage_LightFixtureCalibration::~AppStage_LightFixtureCalibration() {}

void AppStage_LightFixtureCalibration::setTargetFixture(DMXFixtureComponentPtr targetFixture)
{
	m_targetFixture= targetFixture;
}

void AppStage_LightFixtureCalibration::setSourceCamera(CameraComponentPtr cameraComponent)
{
	m_currentSceneCameraComponent= cameraComponent;
	m_videoSourceComponent= m_currentSceneCameraComponent->getVideoSourceComponent();
}

void AppStage_LightFixtureCalibration::enter()
{
	AppStage::enter();

	m_mkCamera= getFirstViewport()->getCurrentMikanCamera();
	m_mkCamera->setCameraMovementMode(eCameraMovementMode::stationary);

	MikanVideoSourceIntrinsics cameraIntrinsics;
	m_currentSceneCameraComponent->getApertureIntrinsics(cameraIntrinsics);
	m_mkCamera->applyMonoCameraIntrinsics(&cameraIntrinsics);

	// Flash fixture to white so it's visible in the video feed
	flashFixtureWhite();

	// Create the distortion view eagerly — it is the stream ownership token
	m_monoDistortionView= new VideoFrameDistortionView(m_videoSourceComponent, eVideoFrameProcessorMode::CALIBRATION);
	m_monoDistortionView->setVideoDisplayMode(eVideoDisplayMode::mode_undistored);

	// Register as a stream consumer — VideoSourceComponent::update() drives the retry loop
	m_videoSourceComponent->startVideoStream(m_monoDistortionView);

	eLightFixtureCalibrationMenuState newState= eLightFixtureCalibrationMenuState::pendingVideoStartStreamRequest;

	// Create GUI panel
	m_calibrationPanel= addGuiPanel<GuiPanel_LightFixtureCalibration>();
	m_calibrationPanel->setFixtureName(m_targetFixture->getName());
	m_calibrationPanel->OnOkEvent= [this]() { onOkEvent(); };
	m_calibrationPanel->OnRedoEvent= [this]() { onRedoEvent(); };
	m_calibrationPanel->OnCancelEvent= [this]() { onCancelEvent(); };

	// Bind mouse input
	EventBindingSet* bindingSet= getOwnerWindow()->getInputManager()->getCurrentEventBindingSet();
	bindingSet->OnMouseButtonReleasedEvent+= MakeDelegate(this, &AppStage_LightFixtureCalibration::onMouseButtonUp);

	setMenuState(newState);
}

void AppStage_LightFixtureCalibration::setupDistortionView()
{
	m_triangulator= new LightFixtureTriangulator(m_currentSceneCameraComponent, m_monoDistortionView);
}

void AppStage_LightFixtureCalibration::exit()
{
	setMenuState(eLightFixtureCalibrationMenuState::inactive);

	// Restore the fixture's previous color
	restoreFixtureColor();

	m_currentSceneCameraComponent= nullptr;
	m_mkCamera= nullptr;

	if (m_triangulator != nullptr)
	{
		delete m_triangulator;
		m_triangulator= nullptr;
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

void AppStage_LightFixtureCalibration::updateCameraTransform()
{
	// World space: the calibrated fixture is stored with setWorldTransform
	glm::mat4 cameraPose;
	if (m_currentSceneCameraComponent->getWorldSpaceAperturePose(cameraPose))
	{
		m_mkCamera->setCameraTransform(cameraPose);
	}
}

void AppStage_LightFixtureCalibration::update(float deltaSeconds)
{
	AppStage::update(deltaSeconds);

	updateCameraTransform();

	eLightFixtureCalibrationMenuState calibrationState= m_calibrationPanel->getMenuState();

	if (calibrationState == eLightFixtureCalibrationMenuState::pendingVideoStartStreamRequest)
	{
		if (m_monoDistortionView->isReceivingFrames())
		{
			setupDistortionView();
			setMenuState(eLightFixtureCalibrationMenuState::verifyInitialCameraSetup);
		}
		else if (m_videoSourceComponent->getVideoStreamingStatus() == eVideoStreamingStatus::failed)
		{
			setMenuState(eLightFixtureCalibrationMenuState::failedVideoStartStreamRequest);
		}
		return;
	}

	if (m_monoDistortionView->isReceivingFrames())
	{
		m_monoDistortionView->readAndProcessVideoFrame();

		// Run live triangulation while user is choosing camera position 2
		if (calibrationState == eLightFixtureCalibrationMenuState::capturePosition2)
		{
			m_triangulator->computeCurrentTriangulation();
		}
	}
}

void AppStage_LightFixtureCalibration::render(IMkViewportPtr targetViewport)
{
	switch (m_calibrationPanel->getMenuState())
	{
	case eLightFixtureCalibrationMenuState::verifyInitialCameraSetup:
	case eLightFixtureCalibrationMenuState::capturePosition1:
		m_monoDistortionView->renderSelectedVideoBuffers();
		break;

	case eLightFixtureCalibrationMenuState::moveCamera:
		m_monoDistortionView->renderSelectedVideoBuffers();
		m_triangulator->renderInitialPoint2d();
		break;

	case eLightFixtureCalibrationMenuState::capturePosition2:
		m_monoDistortionView->renderSelectedVideoBuffers();
		m_triangulator->renderCurrentTriangulation();
		break;

	case eLightFixtureCalibrationMenuState::verifyTriangulatedPosition:
	case eLightFixtureCalibrationMenuState::calibrationComplete:
		m_monoDistortionView->renderSelectedVideoBuffers();
		m_triangulator->renderTriangulatedPosition();
		break;

	default:
		break;
	}

	// Render any pending lines with depth testing disabled
	getGraphicsContext()->getLineRenderer()->render(true);
}

void AppStage_LightFixtureCalibration::setMenuState(eLightFixtureCalibrationMenuState newState)
{
	if (m_calibrationPanel->getMenuState() != newState)
	{
		m_calibrationPanel->setMenuState(newState);
	}
}

void AppStage_LightFixtureCalibration::onMouseButtonUp(int button)
{
	eLightFixtureCalibrationMenuState menuState= m_calibrationPanel->getMenuState();

	if (button == MkMouseButton::LEFT)
	{
		if (menuState == eLightFixtureCalibrationMenuState::capturePosition1)
		{
			// Save camera 1 pose, then save 2D pixel click
			m_triangulator->sampleCameraPose();
			m_triangulator->sampleMouseScreenPosition();
			setMenuState(eLightFixtureCalibrationMenuState::moveCamera);
		}
		else if (menuState == eLightFixtureCalibrationMenuState::capturePosition2)
		{
			// Commit the current live triangulated point
			m_triangulator->sampleMouseScreenPosition();
			setMenuState(eLightFixtureCalibrationMenuState::verifyTriangulatedPosition);
		}
	}
}

void AppStage_LightFixtureCalibration::onOkEvent()
{
	switch (m_calibrationPanel->getMenuState())
	{
	case eLightFixtureCalibrationMenuState::verifyInitialCameraSetup:
	{
		m_triangulator->resetCalibrationState();
		setMenuState(eLightFixtureCalibrationMenuState::capturePosition1);
	}
	break;

	case eLightFixtureCalibrationMenuState::moveCamera:
	{
		setMenuState(eLightFixtureCalibrationMenuState::capturePosition2);
	}
	break;

	case eLightFixtureCalibrationMenuState::verifyTriangulatedPosition:
	{
		glm::vec3 worldPosition;
		if (m_triangulator->getTriangulatedPosition(worldPosition))
		{
			// Write position to the fixture's world transform (preserve rotation)
			if (m_targetFixture)
			{
				glm::mat4 xform= m_targetFixture->getWorldTransform();
				xform[3]= glm::vec4(worldPosition, 1.f);
				m_targetFixture->setWorldTransform(xform);
			}
		}
		setMenuState(eLightFixtureCalibrationMenuState::calibrationComplete);
	}
	break;

	case eLightFixtureCalibrationMenuState::calibrationComplete:
	case eLightFixtureCalibrationMenuState::failedVideoStartStreamRequest:
	{
		m_ownerWindow->popAppState();
	}
	break;

	default:
		break;
	}
}

void AppStage_LightFixtureCalibration::onRedoEvent()
{
	m_triangulator->resetCalibrationState();
	setMenuState(eLightFixtureCalibrationMenuState::verifyInitialCameraSetup);
}

void AppStage_LightFixtureCalibration::onCancelEvent() { m_ownerWindow->popAppState(); }

void AppStage_LightFixtureCalibration::onGui()
{
	AppStage::onGui();

	const float panelWidth= 415.f * MkGui::getUiScale();
	const float displayWidth= m_ownerWindow->getWidth();
	const float displayHeight= m_ownerWindow->getHeight();

	ImGui::SetNextWindowPos(ImVec2(displayWidth - panelWidth, 0.f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelWidth, displayHeight), ImGuiCond_Always);
	constexpr ImGuiWindowFlags k_flags=
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
	MkGuiScopedWindow panel("##LightFixtureCalibration", nullptr, k_flags);
	if (!panel)
		return;

	for (IGuiPanel* guiPanel : m_guiPanels)
		guiPanel->onGui();
}

void AppStage_LightFixtureCalibration::flashFixtureWhite()
{
	if (!m_targetFixture)
		return;

	if (auto spotLight= std::dynamic_pointer_cast<RGBSpotLightComponent>(m_targetFixture))
	{
		m_savedRed= spotLight->getRed();
		m_savedGreen= spotLight->getGreen();
		m_savedBlue= spotLight->getBlue();
		m_colorSaved= true;
		spotLight->setRGB(255, 255, 255);
	}
	else if (auto pixelGrid= std::dynamic_pointer_cast<RGBPixelGridComponent>(m_targetFixture))
	{
		// Save first pixel as representative color; fill all white
		const auto& pixelData= pixelGrid->getPixelData();
		if (pixelData.size() >= 3)
		{
			m_savedRed= pixelData[0];
			m_savedGreen= pixelData[1];
			m_savedBlue= pixelData[2];
			m_colorSaved= true;
		}
		pixelGrid->fillPixels(255, 255, 255);
	}
}

void AppStage_LightFixtureCalibration::restoreFixtureColor()
{
	if (!m_colorSaved)
		return;

	if (!m_targetFixture)
		return;

	if (auto spotLight= std::dynamic_pointer_cast<RGBSpotLightComponent>(m_targetFixture))
	{
		spotLight->setRGB(m_savedRed, m_savedGreen, m_savedBlue);
	}
	else if (auto pixelGrid= std::dynamic_pointer_cast<RGBPixelGridComponent>(m_targetFixture))
	{
		pixelGrid->fillPixels(m_savedRed, m_savedGreen, m_savedBlue);
	}

	m_colorSaved= false;
}

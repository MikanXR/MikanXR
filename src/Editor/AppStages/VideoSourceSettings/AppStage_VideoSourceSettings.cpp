//-- inludes -----
#include "VideoSourceSettings/AppStage_VideoSourceSettings.h"
#include "ARKitVideoSourceComponent.h"
#include "FileVideoSourceComponent.h"
#include "Shared/GuiPanel_ARKitVideoSourceComponent.h"
#include "Shared/GuiPanel_FileVideoSourceComponent.h"
#include "Shared/GuiPanel_USBVideoSourceComponent.h"
#include "Shared/GuiPanel_NetworkVideoSourceComponent.h"
#include "Shared/VideoSourceStatusGui.h"
#include "MonoLensCalibration/AppStage_MonoLensCalibration.h"
#include "MainMenu/AppStage_MainMenu.h"
#include "App.h"
#include "LocText.h"
#include "MikanTextRenderer.h"
#include "MainWindow.h"
#include "MkGuiScopedWindow.h"
#include "MulticastDelegate.h"
#include "NetworkVideoSourceComponent.h"
#include "ProjectConfig.h"
#include "TextStyle.h"
#include "VideoSourceComponent.h"
#include "VideoFrameDistortionView.h"
#include "VideoSourceSettings/VideoSourceRecorder.h"
#include "USBVideoSourceComponent.h"

#include "imgui.h"
#include "MkGuiDrawUtils.h"

namespace
{
const char* poseTrackSkipReasonKey(ePoseTrackSkipReason reason)
{
	switch (reason)
	{
	case ePoseTrackSkipReason::noCamera:
		return "videoSourceSettings.recordingNoCamera";
	case ePoseTrackSkipReason::uncalibrated:
		return "videoSourceSettings.recordingUncalibrated";
	case ePoseTrackSkipReason::stereo:
		return "videoSourceSettings.recordingStereo";
	default:
		return nullptr;
	}
}

const char* recorderErrorKey(eVideoRecorderError error)
{
	switch (error)
	{
	case eVideoRecorderError::noProject:
		return "videoSourceSettings.recordingNoProject";
	case eVideoRecorderError::sourceNotStreaming:
		return "videoSourceSettings.recordingSourceNotStreaming";
	case eVideoRecorderError::writerOpenFailed:
		return "videoSourceSettings.recordingWriterFailed";
	case eVideoRecorderError::writeFailed:
		return "videoSourceSettings.recordingWriteFailed";
	default:
		return nullptr;
	}
}

const char* recorderStateName(eVideoRecorderState state)
{
	switch (state)
	{
	case eVideoRecorderState::recording:
		return "recording";
	case eVideoRecorderState::finishing:
		return "finishing";
	default:
		return "idle";
	}
}
} // namespace

//-- statics ----__
const char* AppStage_VideoSourceSettings::APP_STAGE_NAME= "VideoSourceSettings";

//-- public methods -----
AppStage_VideoSourceSettings::AppStage_VideoSourceSettings(IEditorWindow* ownerWindow)
	: AppStage(ownerWindow, AppStage_VideoSourceSettings::APP_STAGE_NAME)
{
}

AppStage_VideoSourceSettings::~AppStage_VideoSourceSettings()
{
	assert(m_videoBufferView == nullptr);
	assert(m_recorder == nullptr);
}

void AppStage_VideoSourceSettings::enter()
{
	AppStage::enter();

	VideoSourceComponentPtr videoSourceComponent= m_videoSourceComponent.lock();

	// Create app stage GUI panels
	// (Auto cleaned up on app state exit)
	{
		auto* usbPanel= addGuiPanel<GuiPanel_USBVideoSourceComponent>();
		usbPanel->init();
		if (auto usbVideoSourceComponent= std::dynamic_pointer_cast<USBVideoSourceComponent>(videoSourceComponent))
		{
			usbPanel->setComponent(usbVideoSourceComponent);
		}

		auto* networkPanel= addGuiPanel<GuiPanel_NetworkVideoSourceComponent>();
		networkPanel->init();
		if (auto networkVideoSourceComponent=
				std::dynamic_pointer_cast<NetworkVideoSourceComponent>(videoSourceComponent))
		{
			networkPanel->setComponent(networkVideoSourceComponent);
		}

		auto* arkitPanel= addGuiPanel<GuiPanel_ARKitVideoSourceComponent>();
		arkitPanel->init();
		if (auto arkitVideoSourceComponent= std::dynamic_pointer_cast<ARKitVideoSourceComponent>(videoSourceComponent))
		{
			arkitPanel->setComponent(arkitVideoSourceComponent);
		}

		auto* filePanel= addGuiPanel<GuiPanel_FileVideoSourceComponent>();
		filePanel->init();
		if (auto fileVideoSourceComponent= std::dynamic_pointer_cast<FileVideoSourceComponent>(videoSourceComponent))
		{
			filePanel->setComponent(fileVideoSourceComponent);
		}
	}

	if (videoSourceComponent)
	{
		// The preview view. It reads the source's intrinsics when its buffers are
		// first sized, so it is created before the stream starts.
		m_videoBufferView=
			std::make_shared<VideoFrameDistortionView>(videoSourceComponent, eVideoFrameProcessorMode::COMPOSITOR);

		// Register as a stream consumer (VideoSourceComponent::update() drives the retry loop)
		videoSourceComponent->startVideoStream(m_videoBufferView.get());

		m_recorder= std::make_unique<VideoSourceRecorder>(getOwnerWindow(), videoSourceComponent);
	}
}

void AppStage_VideoSourceSettings::exit()
{
	VideoSourceComponentPtr videoSourceComponent= m_videoSourceComponent.lock();

	// A take in progress is closed out before the stage loses its tick
	if (m_recorder)
	{
		m_recorder->finishNow();
		m_recorder.reset();
	}

	if (videoSourceComponent && m_videoBufferView)
	{
		videoSourceComponent->stopVideoStream(m_videoBufferView.get());
		m_videoBufferView= nullptr;
	}

	AppStage::exit();
}

void AppStage_VideoSourceSettings::pause()
{
	VideoSourceComponentPtr videoSourceComponent= m_videoSourceComponent.lock();

	// A paused stage gets no update(), so the take cannot finish on its own
	if (m_recorder)
	{
		m_recorder->finishNow();
	}

	if (videoSourceComponent && m_videoBufferView)
	{
		videoSourceComponent->stopVideoStream(m_videoBufferView.get());
	}

	AppStage::pause();
}

void AppStage_VideoSourceSettings::resume()
{
	VideoSourceComponentPtr videoSourceComponent= m_videoSourceComponent.lock();

	if (videoSourceComponent && m_videoBufferView)
	{
		videoSourceComponent->startVideoStream(m_videoBufferView.get());
	}

	AppStage::resume();
}

void AppStage_VideoSourceSettings::update(float deltaSeconds)
{
	AppStage::update(deltaSeconds);

	// Get the latest video frame
	if (m_videoBufferView != nullptr)
	{
		m_videoBufferView->readAndProcessVideoFrame();
	}

	if (m_recorder)
	{
		m_recorder->update();
	}
}

void AppStage_VideoSourceSettings::onGui()
{
	AppStage::onGui();

	const float panelWidth= 415.f * MkGui::getUiScale();
	const float displayWidth= m_ownerWindow->getWidth();
	const float displayHeight= m_ownerWindow->getHeight();
	ImGui::SetNextWindowPos(ImVec2(displayWidth - panelWidth, 0.f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelWidth, displayHeight), ImGuiCond_Always);

	constexpr ImGuiWindowFlags k_flags=
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

	MkGuiScopedWindow panel("##VideoSourceSettings", nullptr, k_flags);
	if (!panel)
		return;

	if (ImGui::Button(locLabel("videoSourceSettings.return")))
		onReturnEvent();
	ImGui::Separator();

	drawRecordingGui();

	// One warning for whichever source type this stage was entered on, above the
	// panels, since only the matching one has a component bound
	VideoSourceStatusGui::drawIntrinsicsWarning(m_videoSourceComponent.lock());

	for (IGuiPanel* guiPanel : m_guiPanels)
		guiPanel->onGui();
}

bool AppStage_VideoSourceSettings::takeMarkerArmed()
{
	const bool bMarker= m_bMarkerArmed;
	m_bMarkerArmed= false;
	return bMarker;
}

// Record, Stop, Capture Image, the marker toggle, and a status line. The
// recorder is main-thread state and onGui runs before update() in the same
// tick, so the buttons act directly.
void AppStage_VideoSourceSettings::drawRecordingGui()
{
	if (!m_recorder)
		return;

	const VideoRecorderStatus& status= m_recorder->getStatus();
	const bool bIdle= (status.state == eVideoRecorderState::idle);

	if (status.state == eVideoRecorderState::recording)
	{
		if (ImGui::Button(locLabel("videoSourceSettings.stop")))
			m_recorder->stopRecording();
	}
	else
	{
		ImGui::BeginDisabled(!bIdle);
		if (ImGui::Button(locLabel("videoSourceSettings.record")))
			m_recorder->startRecording(takeMarkerArmed(), m_videoBufferView ? m_videoBufferView->getFPS() : 0.f);
		ImGui::EndDisabled();
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(!bIdle);
	if (ImGui::Button(locLabel("videoSourceSettings.captureImage")))
		m_recorder->captureImage(takeMarkerArmed());
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::Checkbox(locLabel("videoSourceSettings.markerReference"), &m_bMarkerArmed);

	switch (status.state)
	{
	case eVideoRecorderState::recording:
	{
		const int elapsed= static_cast<int>(status.elapsedSeconds);
		ImGui::Text(locText("videoSourceSettings.recordingStatusFmt"), elapsed / 60, elapsed % 60,
					static_cast<int>(status.framesWritten), static_cast<int>(status.framesDropped));
		break;
	}
	case eVideoRecorderState::finishing:
		ImGui::TextUnformatted(locText("videoSourceSettings.recordingFinishing"));
		break;
	default:
		if (status.lastError != eVideoRecorderError::none)
		{
			ImGui::TextUnformatted(locText(recorderErrorKey(status.lastError)));
		}
		else if (!status.lastStoredPath.empty())
		{
			ImGui::Text(locText("videoSourceSettings.recordingSavedFmt"), status.lastStoredPath.c_str(),
						status.backendName.c_str());
		}
		else
		{
			ImGui::TextUnformatted(locText("videoSourceSettings.recordingIdle"));
		}
		break;
	}

	if (!bIdle || !status.lastStoredPath.empty())
	{
		if (const char* skipKey= poseTrackSkipReasonKey(status.poseTrackSkipReason))
		{
			ImGui::TextWrapped("%s", locText(skipKey));
		}
	}

	ImGui::Separator();
}

void AppStage_VideoSourceSettings::render(IMkViewportPtr targetViewport)
{
	if (m_videoBufferView != nullptr)
	{
		m_videoBufferView->renderSelectedVideoBuffers();
	}

	// Always draw the FPS in the lower right
	TextStyle style= getDefaultTextStyle();
	style.horizontalAlignment= eHorizontalTextAlignment::Left;
	style.verticalAlignment= eVerticalTextAlignment::Bottom;
	drawTextAtScreenPosition(
		getGraphicsContext(), style, glm::vec2(0.f, m_ownerWindow->getHeight() - 1), L"%hs",
		locFormat("videoSourceSettings.cameraFpsFmt", m_videoBufferView ? m_videoBufferView->getFPS() : 0.f).c_str());
}

void AppStage_VideoSourceSettings::onReturnEvent() { getOwnerWindow()->popAppState(); }

// Remote Control
bool AppStage_VideoSourceSettings::handleRemoteControlCommand(const std::string& command,
															  const std::vector<std::string>& parameters,
															  std::vector<std::string>& outResults)
{
	if (command == "get_video_source_component_id")
	{
		return handleGetVideoSourceComponentId(outResults);
	}
	else if (command == "return")
	{
		return handleReturnRequest(outResults);
	}
	else if (command == "record_start")
	{
		return handleRecordStart(parameters, outResults);
	}
	else if (command == "record_stop")
	{
		return handleRecordStop(outResults);
	}
	else if (command == "capture_image")
	{
		return handleCaptureImage(parameters, outResults);
	}
	else if (command == "get_recording_state")
	{
		return handleGetRecordingState(outResults);
	}

	return AppStage::handleRemoteControlCommand(command, parameters, outResults);
}

bool AppStage_VideoSourceSettings::handleRecordStart(const std::vector<std::string>& parameters,
													 std::vector<std::string>& outResults)
{
	const bool bMarker= !parameters.empty() && parameters[0] == "marker";
	const bool bStarted=
		m_recorder && m_recorder->startRecording(bMarker, m_videoBufferView ? m_videoBufferView->getFPS() : 0.f);
	outResults.push_back(bStarted ? IRemoteControllable::k_success : IRemoteControllable::k_failure);
	return true;
}

bool AppStage_VideoSourceSettings::handleRecordStop(std::vector<std::string>& outResults)
{
	const bool bStopped= m_recorder && m_recorder->stopRecording();
	outResults.push_back(bStopped ? IRemoteControllable::k_success : IRemoteControllable::k_failure);
	return true;
}

bool AppStage_VideoSourceSettings::handleCaptureImage(const std::vector<std::string>& parameters,
													  std::vector<std::string>& outResults)
{
	const bool bMarker= !parameters.empty() && parameters[0] == "marker";
	const bool bCapturing= m_recorder && m_recorder->captureImage(bMarker);
	outResults.push_back(bCapturing ? IRemoteControllable::k_success : IRemoteControllable::k_failure);
	return true;
}

bool AppStage_VideoSourceSettings::handleGetRecordingState(std::vector<std::string>& outResults)
{
	if (!m_recorder)
	{
		outResults.push_back(IRemoteControllable::k_failure);
		return true;
	}

	const VideoRecorderStatus& status= m_recorder->getStatus();
	outResults.push_back(std::string("state ") + recorderStateName(status.state));
	outResults.push_back("frames " + std::to_string(status.framesWritten));
	outResults.push_back("dropped " + std::to_string(status.framesDropped));
	outResults.push_back("last " + (status.lastStoredPath.empty() ? std::string("-") : status.lastStoredPath));
	outResults.push_back(std::string("sidecar ") + (status.bWritesPoseTrack ? "yes" : "no"));
	const char* skipKey= poseTrackSkipReasonKey(status.poseTrackSkipReason);
	outResults.push_back(std::string("sidecar_skip ") + (skipKey ? skipKey : "-"));
	outResults.push_back(std::string("backend ") + (status.backendName.empty() ? "-" : status.backendName));
	const char* errorKey= recorderErrorKey(status.lastError);
	outResults.push_back(std::string("error ") + (errorKey ? errorKey : "-"));
	return true;
}

bool AppStage_VideoSourceSettings::handleGetVideoSourceComponentId(std::vector<std::string>& outResults)
{
	VideoSourceComponentPtr videoSource= m_videoSourceComponent.lock();
	MikanVideoSourceID videoSourceId= videoSource ? videoSource->getComponentId() : INVALID_MIKAN_ID;

	outResults.push_back(std::to_string(videoSourceId));

	return true;
}

bool AppStage_VideoSourceSettings::handleReturnRequest(std::vector<std::string>& outResults)
{
	getOwnerWindow()->popAppState();
	outResults.push_back(IRemoteControllable::k_success);
	return true;
}
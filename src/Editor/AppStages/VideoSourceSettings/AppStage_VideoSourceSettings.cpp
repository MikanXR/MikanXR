//-- inludes -----
#include "VideoSourceSettings/AppStage_VideoSourceSettings.h"
#include "ARKitVideoSourceComponent.h"
#include "FileVideoSourceComponent.h"
#include "Shared/GuiPanel_ARKitVideoSourceComponent.h"
#include "Shared/GuiPanel_FileVideoSourceComponent.h"
#include "Shared/GuiPanel_USBVideoSourceComponent.h"
#include "Shared/GuiPanel_NetworkVideoSourceComponent.h"
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
#include "USBVideoSourceComponent.h"

#include "imgui.h"

//-- statics ----__
const char* AppStage_VideoSourceSettings::APP_STAGE_NAME= "VideoSourceSettings";

//-- public methods -----
AppStage_VideoSourceSettings::AppStage_VideoSourceSettings(IEditorWindow* ownerWindow)
	: AppStage(ownerWindow, AppStage_VideoSourceSettings::APP_STAGE_NAME)
{
}

AppStage_VideoSourceSettings::~AppStage_VideoSourceSettings() { assert(m_videoBufferView == nullptr); }

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
		// Create the video buffer view eagerly — it registers itself for OnFrameSizeChanged
		m_videoBufferView=
			std::make_shared<VideoFrameDistortionView>(videoSourceComponent, eVideoFrameProcessorMode::COMPOSITOR);

		// Register as a stream consumer (VideoSourceComponent::update() drives the retry loop)
		videoSourceComponent->startVideoStream(m_videoBufferView.get());
	}
}

void AppStage_VideoSourceSettings::exit()
{
	VideoSourceComponentPtr videoSourceComponent= m_videoSourceComponent.lock();

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
}

void AppStage_VideoSourceSettings::onGui()
{
	AppStage::onGui();

	constexpr float k_panelWidth= 415.f;
	const float displayWidth= m_ownerWindow->getWidth();
	const float displayHeight= m_ownerWindow->getHeight();
	ImGui::SetNextWindowPos(ImVec2(displayWidth - k_panelWidth, 0.f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(k_panelWidth, displayHeight), ImGuiCond_Always);

	constexpr ImGuiWindowFlags k_flags=
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

	MkGuiScopedWindow panel("##VideoSourceSettings", nullptr, k_flags);
	if (!panel)
		return;

	if (ImGui::Button(locLabel("videoSourceSettings.return")))
		onReturnEvent();
	ImGui::Separator();

	for (IGuiPanel* guiPanel : m_guiPanels)
		guiPanel->onGui();
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

	return AppStage::handleRemoteControlCommand(command, parameters, outResults);
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
//-- inludes -----
#include "App.h"
#include "ClientTextureSourceComponent.h"
#include "CameraComponent.h"
#include "CameraObjectSystem.h"
#include "CameraRequestHandler.h"
#include "CompositorObjectSystem.h"
#include "IMkShaderCache.h"
#include "IMkTriangulatedMesh.h"
#include "IMkGraphicsContext.h"
#include "TextureSourceSettings/AppStage_TextureSourceSettings.h"
#include "Shared/GuiPanel_ClientTextureSourceComponent.h"
#include "Shared/GuiPanel_SpoutTextureSourceComponent.h"
#include "LocText.h"
#include "MainMenu/AppStage_MainMenu.h"
#include "MikanTextRenderer.h"
#include "MikanServer.h"
#include "MainWindow.h"
#include "MkGuiScopedWindow.h"
#include "MkMaterialInstance.h"
#include "MulticastDelegate.h"
#include "SpoutTextureSourceComponent.h"
#include "ProjectConfig.h"
#include "TextureSourceComponent.h"

#include "imgui.h"
#include "MkGuiDrawUtils.h"

//-- statics ----__
const char* AppStage_TextureSourceSettings::APP_STAGE_NAME= "TextureSourceSettings";

//-- public methods -----
AppStage_TextureSourceSettings::AppStage_TextureSourceSettings(IEditorWindow* ownerWindow)
	: AppStage(ownerWindow, AppStage_TextureSourceSettings::APP_STAGE_NAME)
{
}

void AppStage_TextureSourceSettings::enter()
{
	AppStage::enter();

	TextureSourceComponentPtr textureSourceComponent= m_textureSourceComponent.lock();

	// Cache the camera component for use in rendering the video frame in the app stage
	m_cameraComponent= getObjectSystemOfType<CameraObjectSystem>()->getCameraById(m_cameraId);

	// Pause all compositor components while in the texture source settings stage
	getObjectSystemOfType<CompositorObjectSystem>()->setAllCompositorsPaused(true);

	// Create app stage GUI panels
	// (Auto cleaned up on app state exit)
	{
		m_clientTextureSourceComponentPanel= addGuiPanel<GuiPanel_ClientTextureSourceComponent>();
		m_clientTextureSourceComponentPanel->init();
		if (auto clientTextureSourceComponent=
				std::dynamic_pointer_cast<ClientTextureSourceComponent>(textureSourceComponent))
		{
			m_clientTextureSourceComponentPanel->setComponent(clientTextureSourceComponent);
		}

		auto* spoutPanel= addGuiPanel<GuiPanel_SpoutTextureSourceComponent>();
		spoutPanel->init();
		if (auto spoutTextureSourceComponent=
				std::dynamic_pointer_cast<SpoutTextureSourceComponent>(textureSourceComponent))
		{
			spoutPanel->setComponent(spoutTextureSourceComponent);
		}
	}

	// Create a meshes used to render the video frame
	IMkGraphicsContext* graphicsContext= m_ownerWindow->getGraphicsContext().get();
	m_fullscreenRGBQuad= createFullscreenQuadMesh(graphicsContext, true, false);
	m_fullscreenRGBAQuad= createFullscreenQuadMesh(graphicsContext, true, true);
	m_fullscreenDepthUnpackQuad= createFullscreenQuadMesh(
		graphicsContext,
		graphicsContext->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_UNPACK_RGBA_DEPTH_TEXTURE), true);
}

void AppStage_TextureSourceSettings::update(float deltaSeconds)
{
	AppStage::update(deltaSeconds);

	// Manually publish fake camera frame events that emulate new video frames.
	// This forces a connected client to render new frames.
	CameraComponentPtr cameraComponent= m_cameraComponent.lock();
	if (cameraComponent)
	{
		if (m_newFrameTimer <= 0.f)
		{
			if (MikanCameraNewFrameEvent newFrameEvent;
				cameraComponent->makeNewCameraFrameEvent(-1,        // Skip video frame index
														 1280, 720, // fallback render target size
														 newFrameEvent))
			{
				getOwnerWindow()->getMikanServer()->getCameraRequestHandler()->publishCameraNewFrameEvent(
					newFrameEvent);
			}

			m_newFrameTimer= k_newFrameTimerDuration;
		}
		else
		{
			m_newFrameTimer-= deltaSeconds;
		}
	}
}

void AppStage_TextureSourceSettings::onGui()
{
	AppStage::onGui();

	const float panelWidth= 415.f * MkGui::getUiScale();
	const float displayWidth= m_ownerWindow->getWidth();
	const float displayHeight= m_ownerWindow->getHeight();
	ImGui::SetNextWindowPos(ImVec2(displayWidth - panelWidth, 0.f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelWidth, displayHeight), ImGuiCond_Always);

	constexpr ImGuiWindowFlags k_flags=
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

	MkGuiScopedWindow panel("##TextureSourceSettings", nullptr, k_flags);
	if (!panel)
		return;

	if (ImGui::Button(locLabel("textureSourceSettings.return")))
		onReturnEvent();
	ImGui::Separator();

	for (IGuiPanel* guiPanel : m_guiPanels)
		guiPanel->onGui();
}

void AppStage_TextureSourceSettings::render(IMkViewportPtr targetViewport)
{
	AppStage::render(targetViewport);

	TextureSourceComponentPtr textureSourceComponent= m_textureSourceComponent.lock();

	IMkTriangulatedMeshPtr fullscreenQuad;
	IMkTexturePtr videoTexture;
	eUniformSemantic videoTextureSemantic= eUniformSemantic::INVALID;

	eTextureSourceDisplayBufferType displayBufferType= eTextureSourceDisplayBufferType::Color;
	if (m_clientTextureSourceComponentPanel->getComponent() != nullptr)
	{
		displayBufferType= m_clientTextureSourceComponentPanel->getDisplayBufferType();
	}

	switch (displayBufferType)
	{
	case eTextureSourceDisplayBufferType::Color:
	{
		videoTexture=
			textureSourceComponent->getClientColorSourceTexture(m_cameraId, eTextureSourceColorType::colorRGBA);
		if (videoTexture)
		{
			fullscreenQuad= m_fullscreenRGBAQuad;
			videoTextureSemantic= eUniformSemantic::rgbaTexture;
		}
		else
		{
			videoTexture=
				textureSourceComponent->getClientColorSourceTexture(m_cameraId, eTextureSourceColorType::colorRGB);
			if (videoTexture)
			{
				fullscreenQuad= m_fullscreenRGBQuad;
				videoTextureSemantic= eUniformSemantic::rgbTexture;
			}
		}
	}
	break;

	case eTextureSourceDisplayBufferType::Depth:
	{
		videoTexture=
			textureSourceComponent->getClientDepthSourceTexture(m_cameraId, eTextureSourceDepthType::depthPackRGBA);
		if (videoTexture)
		{
			fullscreenQuad= m_fullscreenDepthUnpackQuad;
			videoTextureSemantic= eUniformSemantic::rgbaTexture;
		}
	}
	break;
	};

	if (videoTexture && fullscreenQuad)
	{
		MkMaterialInstancePtr materialInstance= fullscreenQuad->getMaterialInstance();
		MkMaterialConstPtr material= materialInstance->getMaterial();

		if (auto materialBinding= material->bindMaterial())
		{
			// Bind the color texture
			materialInstance->setTextureBySemantic(videoTextureSemantic, videoTexture);

			// Draw the color texture
			if (auto materialInstanceBinding= materialInstance->bindMaterialInstance(materialBinding))
			{
				fullscreenQuad->drawElements();
			}
		}
	}
}

void AppStage_TextureSourceSettings::exit()
{
	// Resume all compositor components when exiting the texture source settings stage
	getObjectSystemOfType<CompositorObjectSystem>()->setAllCompositorsPaused(false);

	AppStage::exit();
}

void AppStage_TextureSourceSettings::onReturnEvent() { getOwnerWindow()->popAppState(); }

// Remote Control
bool AppStage_TextureSourceSettings::handleRemoteControlCommand(const std::string& command,
																const std::vector<std::string>& parameters,
																std::vector<std::string>& outResults)
{
	if (command == "get_texture_source_component_id")
	{
		return handleGetTextureSourceComponentId(outResults);
	}
	else if (command == "return")
	{
		return handleReturnRequest(outResults);
	}

	return AppStage::handleRemoteControlCommand(command, parameters, outResults);
}

bool AppStage_TextureSourceSettings::handleGetTextureSourceComponentId(std::vector<std::string>& outResults)
{
	TextureSourceComponentPtr textureSource= m_textureSourceComponent.lock();
	MikanTextureSourceID textureSourceId= textureSource ? textureSource->getComponentId() : INVALID_MIKAN_ID;
	outResults.push_back(std::to_string(textureSourceId));

	return true;
}

bool AppStage_TextureSourceSettings::handleReturnRequest(std::vector<std::string>& outResults)
{
	getOwnerWindow()->popAppState();
	outResults.push_back(IRemoteControllable::k_success);
	return true;
}
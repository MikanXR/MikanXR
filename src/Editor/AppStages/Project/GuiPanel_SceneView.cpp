#include "GuiPanel_SceneView.h"
#include "App.h"
#include "EditorObjectSystem.h"
#include "IMkFrameBuffer.h"
#include "IMkTexture.h"
#include "LocText.h"
#include "Logger.h"
#include "MikanViewport.h"
#include "MkGuiDrawUtils.h"
#include "Project/AppStage_Project.h"

#include "imgui.h"

#include <easy/profiler.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
// Inset of the overlays from the image's corners
const float k_viewModeComboInset= 8.f;
const float k_viewModeComboWidth= 110.f;
const float k_frameRateInset= 4.f;
} // namespace

bool GuiPanel_SceneView::init(MikanViewportPtr viewport)
{
	m_viewport= viewport;

	return m_viewport != nullptr;
}

void GuiPanel_SceneView::dispose()
{
	if (m_frameBuffer)
	{
		m_frameBuffer->disposeResources();
		m_frameBuffer= nullptr;
	}

	if (m_viewport)
	{
		m_viewport->setRenderTarget(nullptr);
		m_viewport= nullptr;
	}

	GuiPanel::dispose();
}

bool GuiPanel_SceneView::consumeDrawnThisFrame()
{
	const bool bDrawn= m_bDrawnThisFrame;
	m_bDrawnThisFrame= false;

	// A panel that was not drawn holds no hover or press, so it claims no input
	if (!bDrawn)
	{
		m_bHovered= false;
		m_bActive= false;
	}

	return bDrawn;
}

void GuiPanel_SceneView::onGui()
{
	EASY_FUNCTION();

	m_bHovered= false;
	m_bActive= false;

	if (!m_viewport)
		return;

	const ImVec2 available= ImGui::GetContentRegionAvail();
	const int imageWidth= std::max(1, (int)std::floor(available.x));
	const int imageHeight= std::max(1, (int)std::floor(available.y));
	const ImVec2 imageSize((float)imageWidth, (float)imageHeight);

	if (!ensureFrameBuffer(imageWidth, imageHeight))
		return;

	IMkTexturePtr colorTexture= m_frameBuffer->getColorTexture();
	if (!colorTexture || colorTexture->getGlTextureId() == 0)
		return;

	// The viewport's rect is the image's screen rect, so the mouse rays and the
	// camera aspect follow the panel while the render goes to the frame buffer
	const ImVec2 imagePos= ImGui::GetCursorScreenPos();
	m_viewport->setViewport({(int)imagePos.x, (int)imagePos.y}, {imageWidth, imageHeight});
	m_viewport->setRenderTarget(m_frameBuffer);
	m_viewport->setIsRenderingEnabled(true);
	m_bDrawnThisFrame= true;

	// The image alpha blends, and the scene's blended geometry lowers the frame
	// buffer's alpha, so an opaque backing in the clear colour keeps the panel
	// from showing through
	ImDrawList* drawList= ImGui::GetWindowDrawList();
	const ImVec2 imageMax(imagePos.x + imageSize.x, imagePos.y + imageSize.y);
	const glm::vec4& backgroundColor= m_viewport->getBackgroundColor();
	drawList->AddRectFilled(
		imagePos, imageMax,
		ImGui::ColorConvertFloat4ToU32(ImVec4(backgroundColor.r, backgroundColor.g, backgroundColor.b, 1.f)));

	// The framebuffer texture is bottom-up, so flip V for the top-down image
	drawList->AddImage((ImTextureID)(intptr_t)colorTexture->getGlTextureId(), imagePos, imageMax, ImVec2(0.f, 1.f),
					   ImVec2(1.f, 0.f));

	// The overlays are submitted before the capture item below, so they win the hover
	drawViewModeCombo(imagePos);
	drawFrameRate(imagePos, imageSize);

	// The press is owned by this item, so a left drag never starts an ImGui window
	// move, and a drag that leaves the panel keeps the viewport's input until the
	// button is released
	ImGui::SetCursorScreenPos(imagePos);
	ImGui::InvisibleButton("##SceneViewCapture", imageSize,
						   ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle
							   | ImGuiButtonFlags_MouseButtonRight);
	m_bHovered= ImGui::IsItemHovered();
	m_bActive= ImGui::IsItemActive();
}

void GuiPanel_SceneView::drawViewModeCombo(const ImVec2& imagePos)
{
	AppStage_Project* projectStage= static_cast<AppStage_Project*>(m_ownerAppStage);

	// Perspective / orthographic selector, editor camera only. Compositor cameras
	// (pool index > 0) use real lens intrinsics and must not be switched.
	if (m_viewport->getCurrentCameraIndex() != 0 || !m_viewport->getMikanCameraByIndex(0))
		return;

	const char* k_viewLabels[]= {locText("project.viewPerspective"), locText("project.viewTop"),
								 locText("project.viewBottom"),      locText("project.viewFront"),
								 locText("project.viewBack"),        locText("project.viewLeft"),
								 locText("project.viewRight")};

	int currentIndex= projectStage->getCurrentCameraViewIndex();

	const float uiScale= MkGui::getUiScale();
	ImGui::SetCursorScreenPos(
		ImVec2(imagePos.x + k_viewModeComboInset * uiScale, imagePos.y + k_viewModeComboInset * uiScale));
	ImGui::SetNextItemWidth(k_viewModeComboWidth * uiScale);
	if (ImGui::Combo("##ViewMode", &currentIndex, k_viewLabels, IM_ARRAYSIZE(k_viewLabels)))
	{
		projectStage->applyCameraView(currentIndex);
	}
}

void GuiPanel_SceneView::drawFrameRate(const ImVec2& imagePos, const ImVec2& imageSize)
{
	if (!m_ownerAppStage->getEditorSettings().bRenderFrameRate)
		return;

	char frameRateText[32];
	snprintf(frameRateText, sizeof(frameRateText), "%.1ffps", App::getInstance()->getFPS());

	const ImVec2 textSize= ImGui::CalcTextSize(frameRateText);
	const ImVec2 textPos(imagePos.x + imageSize.x - textSize.x - k_frameRateInset,
						 imagePos.y + imageSize.y - textSize.y - k_frameRateInset);
	ImGui::GetWindowDrawList()->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), frameRateText);
}

bool GuiPanel_SceneView::ensureFrameBuffer(int width, int height)
{
	if (m_frameBuffer && m_frameBuffer->isValid() && m_frameBuffer->getWidth() == width
		&& m_frameBuffer->getHeight() == height)
	{
		return true;
	}

	if (m_frameBuffer)
	{
		m_frameBuffer->disposeResources();
	}

	// The COLOR type's DEPTH24_STENCIL8 renderbuffer matches the selection outline
	// mask's depth/stencil format, which the outline pass's depth copy needs. The
	// COLOR_AND_DEPTH type's 32F depth texture would fail that copy every frame.
	m_frameBuffer= createMkFrameBuffer("scene_view");
	m_frameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_frameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::RGBA);
	m_frameBuffer->setSize(width, height);
	m_frameBuffer->setClearColor(m_viewport->getBackgroundColor());

	if (!m_frameBuffer->createResources())
	{
		MIKAN_LOG_ERROR("GuiPanel_SceneView::ensureFrameBuffer")
			<< "Failed to create the " << width << "x" << height << " scene view frame buffer";
		m_frameBuffer= nullptr;
		return false;
	}

	return true;
}

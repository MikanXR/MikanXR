#include "GuiPanel_SceneView.h"
#include "App.h"
#include "EditorObjectSystem.h"
#include "IconsForkAwesome.h"
#include "IMkFrameBuffer.h"
#include "IMkTexture.h"
#include "LocText.h"
#include "Logger.h"
#include "MikanViewport.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiScopedStyleColor.h"
#include "Project/AppStage_Project.h"
#include "TransformGizmo.h"

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
// The gizmo toolbar sits to the right of the view combo
const float k_gizmoToolbarGap= 12.f;
const float k_gizmoToolbarButtonSize= 24.f;
const float k_gizmoToolbarButtonGap= 2.f;
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

	// Whether the capture item still owned a press as of last frame
	const bool bCaptureHeld= m_bActive;

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

	// The viewport's rect is the image's ImGui screen rect (desktop pixels, so it
	// holds in a torn-out window too), so the mouse rays and the camera aspect
	// follow the panel while the render goes to the frame buffer. The render
	// target goes first, since it selects the space setViewport takes the rect in.
	const ImVec2 imagePos= ImGui::GetCursorScreenPos();
	m_viewport->setRenderTarget(m_frameBuffer);
	m_viewport->setViewport({(int)imagePos.x, (int)imagePos.y}, {imageWidth, imageHeight});
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

	// The overlays are submitted before the capture item below, so they win the
	// hover for their own behavior. IsItemHovered is rect based, so the capture
	// item still reports hovered beneath them and their hover is subtracted below,
	// or a click on an overlay would reach the viewport as a pick
	const bool bComboHovered= drawViewModeCombo(imagePos);
	const bool bToolbarHovered= drawGizmoToolbar(imagePos);
	const bool bOverlayHovered= bComboHovered || bToolbarHovered;
	drawFrameRate(imagePos, imageSize);

	// The transform gizmo takes the mouse while a handle is under the cursor or a
	// handle drag is held, and the panel then reports no hover and no press, which
	// keeps viewport input out of the handle. A press the capture item already owns
	// keeps it, so a camera drag passing over a handle carries on.
	//
	// ImGuizmo starts a drag only while no ImGui item is hovered, and ImGui counts
	// last frame's hover, so the capture item is skipped while a handle is merely
	// under the cursor. Once the drag is held the item is submitted again to own
	// the press, or ImGui would move a floating panel along with the handle.
	TransformGizmo* gizmo= drawTransformGizmo(imagePos, imageSize);
	const bool bGizmoDragging= gizmo != nullptr && gizmo->isDragging();
	const bool bGizmoHasMouse= gizmo != nullptr && (gizmo->isHovered() || bGizmoDragging) && !bCaptureHeld;
	if (bGizmoHasMouse && !bGizmoDragging)
		return;

	// The press is owned by this item, so a left drag never starts an ImGui window
	// move, and a drag that leaves the panel keeps the viewport's input until the
	// button is released
	ImGui::SetCursorScreenPos(imagePos);
	ImGui::InvisibleButton("##SceneViewCapture", imageSize,
						   ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle
							   | ImGuiButtonFlags_MouseButtonRight);
	m_bHovered= ImGui::IsItemHovered() && !bOverlayHovered && !bGizmoHasMouse;
	m_bActive= ImGui::IsItemActive() && !bGizmoHasMouse;
}

bool GuiPanel_SceneView::drawGizmoToolbar(const ImVec2& imagePos)
{
	EditorObjectSystemPtr editorSystem= m_ownerAppStage->getObjectSystemOfType<EditorObjectSystem>();
	TransformGizmo* gizmo= editorSystem ? editorSystem->getTransformGizmo() : nullptr;
	if (gizmo == nullptr)
		return false;

	const float uiScale= MkGui::getUiScale();
	const float buttonSize= k_gizmoToolbarButtonSize * uiScale;
	const float buttonGap= k_gizmoToolbarButtonGap * uiScale;
	ImGui::SetCursorScreenPos(
		ImVec2(imagePos.x + (k_viewModeComboInset + k_viewModeComboWidth + k_gizmoToolbarGap) * uiScale,
			   imagePos.y + k_viewModeComboInset * uiScale));

	bool bAnyHovered= false;

	// A square glyph button, drawn pressed while its state is the current one
	auto drawToggle= [&](const char* fieldName, const char* glyph, const char* tooltipKey, bool bIsCurrent)
	{
		MkGuiScopedStyleColor buttonColor;
		if (bIsCurrent)
			buttonColor.push(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

		const std::string label= std::string(glyph) + "##" + fieldName;
		const bool bClicked= ImGui::Button(label.c_str(), ImVec2(buttonSize, buttonSize));
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", locText(tooltipKey));
			bAnyHovered= true;
		}
		ImGui::SameLine(0.f, buttonGap);

		return bClicked;
	};

	const eGizmoMode mode= gizmo->getMode();
	if (drawToggle("gizmoTranslate", ICON_FK_ARROWS, "project.gizmoTranslate", mode == eGizmoMode::translate))
		gizmo->setMode(eGizmoMode::translate);
	if (drawToggle("gizmoRotate", ICON_FK_REPEAT, "project.gizmoRotate", mode == eGizmoMode::rotate))
		gizmo->setMode(eGizmoMode::rotate);
	if (drawToggle("gizmoScale", ICON_FK_EXPAND, "project.gizmoScale", mode == eGizmoMode::scale))
		gizmo->setMode(eGizmoMode::scale);

	// One button for the space, showing the space in effect
	const bool bLocal= gizmo->getSpace() == eGizmoSpace::local;
	if (drawToggle("gizmoSpace", bLocal ? ICON_FK_CUBE : ICON_FK_GLOBE,
				   bLocal ? "project.gizmoSpaceLocal" : "project.gizmoSpaceWorld", bLocal))
		gizmo->setSpace(bLocal ? eGizmoSpace::world : eGizmoSpace::local);

	if (drawToggle("gizmoSnap", ICON_FK_MAGNET, "project.gizmoSnap", gizmo->getSnapEnabled()))
		gizmo->setSnapEnabled(!gizmo->getSnapEnabled());
	ImGui::NewLine();

	return bAnyHovered;
}

TransformGizmo* GuiPanel_SceneView::drawTransformGizmo(const ImVec2& imagePos, const ImVec2& imageSize)
{
	EditorObjectSystemPtr editorSystem= m_ownerAppStage->getObjectSystemOfType<EditorObjectSystem>();
	TransformGizmo* gizmo= editorSystem ? editorSystem->getTransformGizmo() : nullptr;
	if (gizmo != nullptr)
	{
		gizmo->drawGui(imagePos, imageSize, m_viewport->getCurrentMikanCamera());
	}

	return gizmo;
}

bool GuiPanel_SceneView::drawViewModeCombo(const ImVec2& imagePos)
{
	AppStage_Project* projectStage= static_cast<AppStage_Project*>(m_ownerAppStage);

	// Perspective / orthographic selector, editor camera only. Compositor cameras
	// (pool index > 0) use real lens intrinsics and must not be switched.
	if (m_viewport->getCurrentCameraIndex() != 0 || !m_viewport->getMikanCameraByIndex(0))
		return false;

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

	return ImGui::IsItemHovered();
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

#include "TransformGizmo.h"
#include "Colors.h"
#include "EditorObjectSystem.h"
#include "IEditorWindow.h"
#include "InputManager.h"
#include "MikanCamera.h"
#include "MikanObject.h"
#include "MkGuiDrawUtils.h"
#include "SelectionComponent.h"
#include "TransactionHistory.h"
#include "TransformComponent.h"

#include "imgui.h"
#include "ImGuizmo.h"

#include "glm/gtc/type_ptr.hpp"

#include <cassert>

namespace
{
const char* k_dragGestureId= "gizmo_drag";

// ImGuizmo's line widths and marker sizes are authored at 96 DPI, so the live
// style is rebuilt from this untouched copy at the current UI scale each frame
const ImGuizmo::Style k_baseStyle;

ImVec4 toImVec4(const glm::vec3& color, float alpha) { return ImVec4(color.r, color.g, color.b, alpha); }

ImGuizmo::OPERATION toOperation(eGizmoMode mode)
{
	switch (mode)
	{
	case eGizmoMode::rotate:
		return ImGuizmo::ROTATE;
	case eGizmoMode::scale:
		return ImGuizmo::SCALE;
	case eGizmoMode::translate:
	default:
		return ImGuizmo::TRANSLATE;
	}
}

ImGuizmo::MODE toMode(eGizmoSpace space) { return space == eGizmoSpace::local ? ImGuizmo::LOCAL : ImGuizmo::WORLD; }

void applyGizmoStyle()
{
	const float uiScale= MkGui::getUiScale();
	ImGuizmo::Style& style= ImGuizmo::GetStyle();

	style.TranslationLineThickness= k_baseStyle.TranslationLineThickness * uiScale;
	style.TranslationLineArrowSize= k_baseStyle.TranslationLineArrowSize * uiScale;
	style.RotationLineThickness= k_baseStyle.RotationLineThickness * uiScale;
	style.RotationOuterLineThickness= k_baseStyle.RotationOuterLineThickness * uiScale;
	style.ScaleLineThickness= k_baseStyle.ScaleLineThickness * uiScale;
	style.ScaleLineCircleSize= k_baseStyle.ScaleLineCircleSize * uiScale;
	style.HatchedAxisLineThickness= k_baseStyle.HatchedAxisLineThickness * uiScale;
	style.CenterCircleSize= k_baseStyle.CenterCircleSize * uiScale;

	// The axis colors the line-drawn gizmo used, with the handle under the cursor in yellow
	style.Colors[ImGuizmo::DIRECTION_X]= toImVec4(Colors::Red, 1.f);
	style.Colors[ImGuizmo::DIRECTION_Y]= toImVec4(Colors::Green, 1.f);
	style.Colors[ImGuizmo::DIRECTION_Z]= toImVec4(Colors::Blue, 1.f);
	style.Colors[ImGuizmo::PLANE_X]= toImVec4(Colors::Red, 0.38f);
	style.Colors[ImGuizmo::PLANE_Y]= toImVec4(Colors::Green, 0.38f);
	style.Colors[ImGuizmo::PLANE_Z]= toImVec4(Colors::Blue, 0.38f);
	style.Colors[ImGuizmo::SELECTION]= toImVec4(Colors::Yellow, 0.75f);
}
} // namespace

TransformGizmo::TransformGizmo(EditorObjectSystem* ownerSystem)
	: m_ownerSystem(ownerSystem)
{
}

TransformGizmo::~TransformGizmo()
{
	clearSelectionTarget();
	unbindInput();
}

void TransformGizmo::setSelectionTarget(SelectionComponentPtr selectionTarget)
{
	assert(selectionTarget);

	clearSelectionTarget();

	if (!selectionTarget->getIsTransformGizmoAllowed())
		return;

	// Edits apply to the root component of the object that owns the selection target
	selectionTarget->notifyTransformGizmoBound();
	m_selectionTarget= selectionTarget;
	m_transformTarget= selectionTarget->getOwnerObject()->getRootComponent();
}

void TransformGizmo::clearSelectionTarget()
{
	// A target that goes away mid-drag closes the drag's gesture
	setDragging(false);
	m_bHovered= false;

	SelectionComponentPtr oldSelectionTarget= m_selectionTarget.lock();
	if (oldSelectionTarget)
	{
		oldSelectionTarget->notifyTransformGizmoUnbound();
	}

	m_selectionTarget.reset();
	m_transformTarget.reset();
}

SelectionComponentPtr TransformGizmo::getSelectionTarget() const { return m_selectionTarget.lock(); }

TransformComponentPtr TransformGizmo::getTransformTarget() const { return m_transformTarget.lock(); }

eGizmoMode TransformGizmo::getMode() const { return m_ownerSystem->getEditorSettings().gizmoMode; }

void TransformGizmo::setMode(eGizmoMode mode) { m_ownerSystem->getEditorSystemConfig()->setGizmoMode(mode); }

eGizmoSpace TransformGizmo::getSpace() const { return m_ownerSystem->getEditorSettings().gizmoSpace; }

void TransformGizmo::setSpace(eGizmoSpace space) { m_ownerSystem->getEditorSystemConfig()->setGizmoSpace(space); }

bool TransformGizmo::getSnapEnabled() const { return m_ownerSystem->getEditorSettings().bSnapEnabled; }

void TransformGizmo::setSnapEnabled(bool bEnabled) { m_ownerSystem->getEditorSystemConfig()->setSnapEnabled(bEnabled); }

bool TransformGizmo::isSnapActive() const { return getSnapEnabled() != ImGui::GetIO().KeyShift; }

void TransformGizmo::bindInput(InputManager* inputManager)
{
	if (m_boundInputManager == inputManager)
		return;

	unbindInput();

	inputManager->fetchOrAddKeyBindings(MkKey::LETTER_t)->OnKeyPressed+=
		MakeDelegate(this, &TransformGizmo::selectTranslateMode);
	inputManager->fetchOrAddKeyBindings(MkKey::LETTER_r)->OnKeyPressed+=
		MakeDelegate(this, &TransformGizmo::selectRotateMode);
	inputManager->fetchOrAddKeyBindings(MkKey::LETTER_y)->OnKeyPressed+=
		MakeDelegate(this, &TransformGizmo::selectScaleMode);

	m_boundInputManager= inputManager;
}

void TransformGizmo::unbindInput()
{
	if (m_boundInputManager == nullptr)
		return;

	m_boundInputManager->fetchOrAddKeyBindings(MkKey::LETTER_t)->OnKeyPressed-=
		MakeDelegate(this, &TransformGizmo::selectTranslateMode);
	m_boundInputManager->fetchOrAddKeyBindings(MkKey::LETTER_r)->OnKeyPressed-=
		MakeDelegate(this, &TransformGizmo::selectRotateMode);
	m_boundInputManager->fetchOrAddKeyBindings(MkKey::LETTER_y)->OnKeyPressed-=
		MakeDelegate(this, &TransformGizmo::selectScaleMode);

	m_boundInputManager= nullptr;
}

void TransformGizmo::selectTranslateMode() { setMode(eGizmoMode::translate); }

void TransformGizmo::selectRotateMode() { setMode(eGizmoMode::rotate); }

void TransformGizmo::selectScaleMode() { setMode(eGizmoMode::scale); }

void TransformGizmo::setDragging(bool bDragging)
{
	if (bDragging == m_bDragging)
		return;

	m_bDragging= bDragging;

	TransactionHistory* transactionHistory= m_ownerSystem->getOwnerWindow()->getTransactionHistory();
	if (transactionHistory != nullptr)
	{
		if (bDragging)
			transactionHistory->beginGesture(k_dragGestureId);
		else
			transactionHistory->endGesture();
	}
}

void TransformGizmo::drawGui(const ImVec2& rectPos, const ImVec2& rectSize, MikanCameraConstPtr camera)
{
	TransformComponentPtr target= m_transformTarget.lock();
	if (!target || !camera)
	{
		setDragging(false);
		m_bHovered= false;
		return;
	}

	applyGizmoStyle();

	ImGuizmo::SetDrawlist();
	ImGuizmo::SetRect(rectPos.x, rectPos.y, rectSize.x, rectSize.y);
	ImGuizmo::SetOrthographic(camera->isOrthographic());

	// Translate snaps to the ruler's grid increment, the other two to their own steps
	const EditorSettings& settings= m_ownerSystem->getEditorSettings();
	const eGizmoMode mode= getMode();
	float snapSteps[3]= {0.f, 0.f, 0.f};
	const float* snap= nullptr;
	if (isSnapActive())
	{
		switch (mode)
		{
		case eGizmoMode::translate:
			snapSteps[0]= snapSteps[1]= snapSteps[2]= settings.snapIncrement;
			break;
		case eGizmoMode::rotate:
			snapSteps[0]= settings.rotationSnapDegrees;
			break;
		case eGizmoMode::scale:
			snapSteps[0]= settings.scaleSnapIncrement;
			break;
		}
		if (snapSteps[0] > 0.f)
			snap= snapSteps;
	}

	// The full world matrix goes in, scale included, and comes back edited
	glm::mat4 worldTransform= target->getWorldTransform();
	const bool bModified=
		ImGuizmo::Manipulate(glm::value_ptr(camera->getViewMatrix()), glm::value_ptr(camera->getProjectionMatrix()),
							 toOperation(mode), toMode(getSpace()), glm::value_ptr(worldTransform), nullptr, snap);

	m_bHovered= ImGuizmo::IsOver();
	// The gesture opens before the first write of the drag lands
	setDragging(ImGuizmo::IsUsing());

	if (bModified)
	{
		target->setWorldTransform(worldTransform);
	}
}

#pragma once

#include "ComponentFwd.h"
#include "MikanRendererFwd.h"

struct ImVec2;
class EditorObjectSystem;
class InputManager;

// Which handles the transform gizmo shows
enum class eGizmoMode : int
{
	translate,
	rotate,
	scale,
};

// The axes the translate and rotate handles follow. Scale is always local.
enum class eGizmoSpace : int
{
	world,
	local,
};

// The viewport's transform gizmo: an ImGuizmo manipulator bound to the selected
// scene actor's root transform. It owns no scene object and no colliders. The
// scene panel calls drawGui once per frame over its image, and an edit is written
// straight to the target's world transform, so a drag edits the component the way
// a property panel does and coalesces into one undo step.
class TransformGizmo
{
public:
	TransformGizmo(EditorObjectSystem* ownerSystem);
	~TransformGizmo();

	// Binding. A target that refuses the gizmo (SelectionComponent::getIsTransformGizmoAllowed)
	// leaves it hidden.
	void setSelectionTarget(SelectionComponentPtr selectionTarget);
	void clearSelectionTarget();
	SelectionComponentPtr getSelectionTarget() const;
	TransformComponentPtr getTransformTarget() const;

	// The mode, the space, and the snap flag live in EditorSettings, so they
	// persist with the project. The snap flag is the ruler's, so one setting
	// and one Shift rule cover both tools.
	eGizmoMode getMode() const;
	void setMode(eGizmoMode mode);
	eGizmoSpace getSpace() const;
	void setSpace(eGizmoSpace space);
	bool getSnapEnabled() const;
	void setSnapEnabled(bool bEnabled);
	// The snap a drag uses right now: the configured flag, inverted while Shift is held
	bool isSnapActive() const;

	// T, R, and Y select the mode while the project stage is up
	void bindInput(InputManager* inputManager);
	void unbindInput();

	// Draws the gizmo into the current ImGui window over the given screen rect and
	// applies any edit to the target. Call it before the panel's capture item, since
	// ImGuizmo only starts a drag while no ImGui item is hovered or active.
	void drawGui(const ImVec2& rectPos, const ImVec2& rectSize, MikanCameraConstPtr camera);

	// Whether the cursor is over a handle, and whether a handle drag is held, as of the last drawGui
	bool isHovered() const { return m_bHovered; }
	bool isDragging() const { return m_bDragging; }

protected:
	void selectTranslateMode();
	void selectRotateMode();
	void selectScaleMode();
	// Brackets the drag as one undo gesture on its edges
	void setDragging(bool bDragging);

	EditorObjectSystem* m_ownerSystem= nullptr;
	InputManager* m_boundInputManager= nullptr;
	SelectionComponentWeakPtr m_selectionTarget;
	TransformComponentWeakPtr m_transformTarget;
	bool m_bHovered= false;
	bool m_bDragging= false;
};

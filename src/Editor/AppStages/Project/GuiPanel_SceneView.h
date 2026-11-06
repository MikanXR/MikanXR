#pragma once

#include "MikanRendererFwd.h"
#include "Shared/GuiPanel.h"

struct ImVec2;

// The project's 3d scene view. The viewport renders into a frame buffer this panel
// owns, sized to the panel each frame, and the panel presents it as an image with
// the projection selector, the gizmo toolbar, the transform gizmo, the view cube,
// and the frame rate readout drawn over it.
class GuiPanel_SceneView : public GuiPanel
{
public:
	GuiPanel_SceneView(AppStage* ownerAppStage)
		: GuiPanel(ownerAppStage)
	{
	}

	bool init(MikanViewportPtr viewport);
	virtual void onGui() override;
	virtual void dispose() override;

	// True while the image is hovered or a press on it is still held, which is
	// when window input belongs to the viewport
	bool wantsViewportInput() const { return m_bHovered || m_bActive; }
	// Whether onGui presented the viewport since the last call, clearing the flag
	bool consumeDrawnThisFrame();

private:
	bool ensureFrameBuffer(int width, int height);
	// The overlays reply whether one of their items is under the cursor
	bool drawViewModeCombo(const ImVec2& imagePos);
	bool drawGizmoToolbar(const ImVec2& imagePos);
	// Draws the transform gizmo over the image, replying it for its hover and
	// drag state this frame, or null when the stage has none
	TransformGizmo* drawTransformGizmo(const ImVec2& imagePos, const ImVec2& imageSize);
	// Draws the view cube in the image's top right and writes its view back to the
	// editor camera, replying whether the cube is under the cursor and whether a
	// drag or a face snap is in progress
	void drawViewCube(const ImVec2& imagePos, const ImVec2& imageSize, bool& outHovered, bool& outUsing);
	// The distance ahead of the camera the view cube orbits about
	float computeViewCubePivotDistance(const MikanCamera& camera) const;
	void drawFrameRate(const ImVec2& imagePos, const ImVec2& imageSize);

	MikanViewportPtr m_viewport;
	IMkFrameBufferPtr m_frameBuffer;
	bool m_bHovered= false;
	bool m_bActive= false;
	bool m_bDrawnThisFrame= false;
};

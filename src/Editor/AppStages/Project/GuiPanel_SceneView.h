#pragma once

#include "MikanRendererFwd.h"
#include "Shared/GuiPanel.h"

struct ImVec2;

// The project's 3d scene view. The viewport renders into a frame buffer this panel
// owns, sized to the panel each frame, and the panel presents it as an image with
// the view-mode selector and the frame rate readout drawn over it.
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
	void drawViewModeCombo(const ImVec2& imagePos);
	void drawFrameRate(const ImVec2& imagePos, const ImVec2& imageSize);

	MikanViewportPtr m_viewport;
	IMkFrameBufferPtr m_frameBuffer;
	bool m_bHovered= false;
	bool m_bActive= false;
	bool m_bDrawnThisFrame= false;
};

#pragma once

#include "MkGuiExport.h"
#include "MkGuiFwd.h"
#include "IMkWindowContext.h"
#include "IMkWindowEventListener.h"
#include "IMkGraphicsContext.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

// One ImGui viewport of a context, in plain types so callers outside MikanGUI
// can list and address the OS windows a context's panels are torn out into
struct MkGuiViewportInfo
{
	uint32_t viewportId= 0;
	// The SDL window id the viewport renders into
	uint32_t platformWindowId= 0;
	bool bIsMainViewport= false;
	// Desktop pixels when multi-viewports are on
	int x= 0;
	int y= 0;
	int width= 0;
	int height= 0;
	// The stable id of each window drawn into the viewport last frame (the part
	// of its ImGui name after "###", or the whole name)
	std::vector<std::string> windowIds;
};

class MIKAN_GUI_CLASS MkGuiContext : public IMkWindowEventListener
{
public:
	MkGuiContext()= delete;
	// An empty iniFilePath keeps ImGui's default (imgui.ini in the working
	// directory). Docking is opt-in per context: only a window that hosts a
	// dockspace wants it, and the single-window editors do not. Multi-viewports
	// are opt-in the same way: they let a panel dragged past the window's edge
	// become its own OS window, and they make ImGui screen space desktop space
	// for the whole context.
	MkGuiContext(class IMkWindowContext* window, const std::string& iniFilePath= std::string(),
				 bool bEnableDocking= false, bool bEnableViewports= false);
	virtual ~MkGuiContext();

	bool startup();
	void shutdown();
	void makeCurrent();
	void submitDrawData();

	// Called once per torn-out viewport, with its GL context current, after its
	// draw data has rendered and before its buffers swap, so its back buffer is
	// still readable
	using ViewportRenderedCallback= std::function<void(uint32_t viewportId, int drawableWidth, int drawableHeight)>;

	// Creates, moves and destroys the torn-out panels' OS windows and renders
	// each of them. ImGui requires this every frame the context ran NewFrame once
	// multi-viewports are on, so the owner calls it whether or not it drew
	// anything this frame. A no-op on a context without multi-viewports.
	void renderPlatformWindows(const ViewportRenderedCallback& viewportRenderedCallback);

	// True when the OS window with this id is one of this context's torn-out
	// viewports, rather than the window the context was created on
	bool ownsPlatformWindow(uint32_t windowId);

	// The context's viewports, the main viewport first
	std::vector<MkGuiViewportInfo> getViewports();

	// Undocks the window whose ImGui name is windowId or ends in "###windowId",
	// and moves it to a desktop position, resizing it when width and height are
	// positive. A position outside this window gives it its own OS window. The
	// move is queued and applied at the start of the next GUI frame. Fails when
	// no window drawn last frame matches.
	bool requestFloatWindow(const std::string& windowId, int x, int y, int width, int height, std::string& outError);
	// Applies the queued requestFloatWindow moves. Called right after NewFrame.
	void applyPendingFloatRequests();

	// An app-wide multiplier the user sets on top of the monitor's own content
	// scale. Static because it is one preference shared by every window, and
	// windows come and go while the preference stands.
	static void setUserUiScale(float scale);
	static float getUserUiScale();

	// Re-derives the ImGui style from the monitor's content scale and the user
	// scale. Called at the top of each frame, so dragging the window to a
	// display with a different scale re-sizes the UI on the next frame.
	void refreshUiScale();

	// Re-proportions the dock splits and the floating window sizes when the UI
	// scale no longer matches the one the layout was arranged at, whether that
	// layout came from an ini written on another display or from the scale
	// changing under this one. Runs after ImGui::NewFrame, since a settings load
	// builds the dock nodes and window settings there.
	void refreshLayoutScale();

	struct ImFont* getNormalIconFont() const { return m_NormalIconFont; }
	struct ImFont* getBigIconFont() const { return m_BigIconFont; }
	class IMkTextureCache* getTextureCache() const;

	// Whether a text field held the keyboard on the last frame, so a window
	// can keep its own key bindings (Escape to quit) out of an edit
	bool wantsTextInput();

	// -- IMkWindowEventListener
	virtual bool onWindowEvent(const MkWindowEvent& event) override;

protected:
	bool initImGuiSDLBackend();
	bool initImGuiOpenGlBackend();

	void configImGui();

private:
	class IMkWindowContext* m_window= nullptr;
	// Owns the string io.IniFilename points at, so it must outlive the ImGui context
	std::string m_iniFilePath;
	bool m_bEnableDocking= false;
	bool m_bEnableViewports= false;
	struct PendingFloatRequest
	{
		std::string windowName;
		int x= 0;
		int y= 0;
		int width= 0;
		int height= 0;
	};
	std::vector<PendingFloatRequest> m_pendingFloatRequests;
	struct ImGuiContext* m_imguiContext= nullptr;
	// The unscaled style the theme authored, kept so each scale change re-derives
	// from it rather than compounding ScaleAllSizes onto the live style
	std::unique_ptr<struct ImGuiStyle> m_baseStyle;
	float m_appliedUiScale= 0.f;
	// The UI scale the saved layout is proportioned for. Owned here rather than in
	// the dockspace wrapper because the ini settings handler reads into it, and
	// each window's context has its own layout and its own ini.
	float m_layoutRefScale= 0.f;
	struct ImFont* m_NormalIconFont= nullptr;
	struct ImFont* m_BigIconFont= nullptr;
	eWindowAPI m_imguiWindowAPI= eWindowAPI::INVALID;
	eGraphicsAPI m_imguiGraphicsAPI= eGraphicsAPI::INVALID;
};
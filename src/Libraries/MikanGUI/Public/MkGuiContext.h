#pragma once

#include "MkGuiExport.h"
#include "MkGuiFwd.h"
#include "IMkWindowContext.h"
#include "IMkWindowEventListener.h"
#include "IMkGraphicsContext.h"

#include <memory>
#include <string>

class MIKAN_GUI_CLASS MkGuiContext : public IMkWindowEventListener
{
public:
	MkGuiContext()= delete;
	// An empty iniFilePath keeps ImGui's default (imgui.ini in the working
	// directory). Docking is opt-in per context: only a window that hosts a
	// dockspace wants it, and the single-window editors do not.
	MkGuiContext(class IMkWindowContext* window, const std::string& iniFilePath= std::string(),
				 bool bEnableDocking= false);
	virtual ~MkGuiContext();

	bool startup();
	void shutdown();
	void makeCurrent();
	void submitDrawData();

	// An app-wide multiplier the user sets on top of the monitor's own content
	// scale. Static because it is one preference shared by every window, and
	// windows come and go while the preference stands.
	static void setUserUiScale(float scale);
	static float getUserUiScale();

	// Re-derives the ImGui style from the monitor's content scale and the user
	// scale. Called at the top of each frame, so dragging the window to a
	// display with a different scale re-sizes the UI on the next frame.
	void refreshUiScale();

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
	struct ImGuiContext* m_imguiContext= nullptr;
	// The unscaled style the theme authored, kept so each scale change re-derives
	// from it rather than compounding ScaleAllSizes onto the live style
	std::unique_ptr<struct ImGuiStyle> m_baseStyle;
	float m_appliedUiScale= 0.f;
	struct ImFont* m_NormalIconFont= nullptr;
	struct ImFont* m_BigIconFont= nullptr;
	eWindowAPI m_imguiWindowAPI= eWindowAPI::INVALID;
	eGraphicsAPI m_imguiGraphicsAPI= eGraphicsAPI::INVALID;
};
#pragma once

#include "MkGuiExport.h"
#include "MkGuiFwd.h"
#include "IMkWindowContext.h"
#include "IMkWindowEventListener.h"
#include "IMkGraphicsContext.h"

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
	struct ImFont* m_NormalIconFont= nullptr;
	struct ImFont* m_BigIconFont= nullptr;
	eWindowAPI m_imguiWindowAPI= eWindowAPI::INVALID;
	eGraphicsAPI m_imguiGraphicsAPI= eGraphicsAPI::INVALID;
};
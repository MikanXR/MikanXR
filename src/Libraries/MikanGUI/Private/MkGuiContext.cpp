#include "MkGuiContext.h"
#include "MkGuiScopedContext.h"
#include "MkGuiDockspace.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiTheme.h"
#include "Logger.h"
#include "IMkWindowContext.h"
#include "IMkGraphicsContext.h"
#include "IMkTextureCache.h"
#include "MkWindowEvent.h"
#include <GL/glew.h>

#if defined(_WIN32)
#include <SDL.h>
#include <SDL_events.h>
#include <SDL_mouse.h>
#include <SDL_syswm.h>
#include <SDL_image.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#include <SDL_opengles2_gl2.h>
#else
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#endif
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_syswm.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL2/SDL_opengles2.h>
#include <SDL2/SDL_opengles2_gl2.h>
#else
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#endif
#endif

#include "imgui.h"
// IMGUI_DEFINE_MATH_OPERATORS comes from the MikanGUI target definitions
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

#include <cstring>

// The app-wide user scale multiplier, applied on top of each monitor's content scale
static float g_userUiScale= 1.f;

MkGuiContext::MkGuiContext(class IMkWindowContext* window, const std::string& iniFilePath, bool bEnableDocking,
						   bool bEnableViewports)
	: m_window(window)
	, m_iniFilePath(iniFilePath)
	, m_bEnableDocking(bEnableDocking)
	, m_bEnableViewports(bEnableViewports)
{
}

MkGuiContext::~MkGuiContext() { shutdown(); }

IMkTextureCache* MkGuiContext::getTextureCache() const { return m_window->getGraphicsContext()->getTextureCache(); }

bool MkGuiContext::startup()
{
	bool success= true;

	// Store the previous context so we can restore it after we're done setting up our own context
	ImGuiContext* prevImGuiContext= ImGui::GetCurrentContext();

	// Setup ImGui context
	IMGUI_CHECKVERSION();
	m_imguiContext= ImGui::CreateContext();
	if (m_imguiContext != nullptr)
	{
		// Set the current ImGui context to the one we just created
		ImGui::SetCurrentContext(m_imguiContext);

		// Setup ImGui configuration
		configImGui();
	}
	else
	{
		MIKAN_LOG_ERROR("MkGuiContext::startup") << "Unable to create imgui context";
		success= false;
	}

	// Setup ImGui window backend (e.g. SDL)
	if (success)
	{
		bool bInitializedBackend= false;

		switch (m_window->getWindowAPI())
		{
		case eWindowAPI::SDL:
			success= initImGuiSDLBackend();
			break;
		default:
			MIKAN_LOG_ERROR("MkGuiContext::startup") << "Unsupported window API";
			break;
		}
	}

	// Setup ImGui graphics backend
	if (success)
	{
		switch (m_window->getGraphicsContext()->getGraphicsAPI())
		{
		case eGraphicsAPI::OpenGL:
			success= initImGuiOpenGlBackend();
			break;
		default:
			MIKAN_LOG_ERROR("MkGuiContext::startup") << "Unsupported graphics API";
			break;
		}
	}

	// Size the UI to the display this window opened on, so the first frame is
	// already scaled rather than snapping a frame later
	if (success)
	{
		refreshUiScale();
	}

	// Restore the previous context
	ImGui::SetCurrentContext(prevImGuiContext);

	return success;
}

void MkGuiContext::setUserUiScale(float scale) { g_userUiScale= scale > 0.f ? scale : 1.f; }

float MkGuiContext::getUserUiScale() { return g_userUiScale; }

void MkGuiContext::refreshLayoutScale()
{
	MkGuiScopedContext scopedContext(*this);

	const float uiScale= MkGui::getUiScale();

	// An ini with no reference (and a layout built fresh this run) is already
	// proportioned for the scale it was laid out at
	if (m_layoutRefScale <= 0.f)
	{
		m_layoutRefScale= uiScale;
		return;
	}

	if (uiScale == m_layoutRefScale)
		return;

	MkGui::scaleWindowLayout(uiScale / m_layoutRefScale);
	m_layoutRefScale= uiScale;
}

void MkGuiContext::refreshUiScale()
{
	MkGuiScopedContext scopedContext(*this);

	if (m_baseStyle == nullptr)
		return;

	float monitorScale= 1.f;
	if (m_imguiWindowAPI == eWindowAPI::SDL)
	{
		SDL_Window* sdlWindow= (SDL_Window*)m_window->getNativeWindowHandle();
		monitorScale= ImGui_ImplSDL2_GetContentScaleForWindow(sdlWindow);
	}
	if (monitorScale <= 0.f)
	{
		monitorScale= 1.f;
	}

	const float effectiveScale= monitorScale * g_userUiScale;
	if (effectiveScale == m_appliedUiScale)
		return;

	// ScaleAllSizes compounds and truncates, so each change re-derives the metrics
	// from the theme's authored values rather than scaling the live style again
	ImGuiStyle& style= ImGui::GetStyle();
	style= *m_baseStyle;
	style.ScaleAllSizes(effectiveScale);

	// Fonts take the two factors separately. ImGui multiplies both into every font
	// size, including the sizes MkGuiScopedFont pushes explicitly.
	style.FontScaleDpi= monitorScale;
	style.FontScaleMain= g_userUiScale;

	m_appliedUiScale= effectiveScale;
}

bool MkGuiContext::initImGuiSDLBackend()
{
	SDL_Window* sdlWindow= (SDL_Window*)m_window->getNativeWindowHandle();
	bool success= false;

	switch (m_window->getGraphicsContext()->getGraphicsAPI())
	{
	case eGraphicsAPI::OpenGL:
	{
		void* glContext= m_window->getGraphicsContext()->getNativeGraphicsContext();
		success= ImGui_ImplSDL2_InitForOpenGL(sdlWindow, glContext);
	}
	break;
	default:
		MIKAN_LOG_ERROR("MkGuiContext::initImGuiSDLBackend") << "Unsupported graphics API";
		break;
	}

	if (success)
	{
		m_imguiWindowAPI= eWindowAPI::SDL;
	}
	else
	{
		MIKAN_LOG_ERROR("MkGuiContext::startup") << "Unable to initialize imgui SDL backend";
	}

	return success;
}

bool MkGuiContext::initImGuiOpenGlBackend()
{
	// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
	// GL ES 2.0 + GLSL 100
	const char* glsl_version= "#version 100";
#elif defined(__APPLE__)
	// GL 3.2 Core + GLSL 150
	const char* glsl_version= "#version 150";
#else
	// GL 3.0 + GLSL 130
	const char* glsl_version= "#version 130";
#endif

	if (ImGui_ImplOpenGL3_Init(glsl_version))
	{
		m_imguiGraphicsAPI= eGraphicsAPI::OpenGL;
		return true;
	}
	else
	{
		MIKAN_LOG_ERROR("MkGuiContext::startup") << "Unable to initialize imgui openGL backend";
	}

	return false;
}

bool MkGuiContext::wantsTextInput()
{
	MkGuiScopedContext scopedContext(*this);

	return ImGui::GetIO().WantTextInput;
}

bool MkGuiContext::onWindowEvent(const MkWindowEvent& event)
{
	// Make sure this ImGui context is current when reading IO state
	MkGuiScopedContext scopedContext(*this);

	const SDL_Event* sdlEvent= (const SDL_Event*)event.getInternalWindowEvent();
	ImGui_ImplSDL2_ProcessEvent(sdlEvent);

	// Only block the event from reaching the rest of the app if ImGui is actually
	// consuming that input type. WantCaptureMouse/WantCaptureKeyboard reflect the
	// previous frame's state, which is the standard ImGui approach for event-time
	// filtering when events are processed before ImGui::NewFrame().
	const ImGuiIO& io= ImGui::GetIO();
	switch (event.getEventType())
	{
	case eMkWindowEventType::MouseButtonDown:
	case eMkWindowEventType::MouseButtonUp:
	case eMkWindowEventType::MouseMotion:
	case eMkWindowEventType::MouseWheel:
		return io.WantCaptureMouse;
	case eMkWindowEventType::KeyDown:
	case eMkWindowEventType::KeyUp:
	case eMkWindowEventType::TextInput:
		return io.WantCaptureKeyboard;
	default:
		return false;
	}
}

void MkGuiContext::shutdown()
{
	// Store the previous context so we can restore it after teardown
	ImGuiContext* prevImGuiContext= ImGui::GetCurrentContext();

	// Set the current ImGui context to the one we are about to
	if (m_imguiContext != nullptr)
	{
		ImGui::SetCurrentContext(m_imguiContext);

		// Free the ImGui graphics backend
		switch (m_imguiGraphicsAPI)
		{
		case eGraphicsAPI::OpenGL:
			ImGui_ImplOpenGL3_Shutdown();
			m_imguiGraphicsAPI= eGraphicsAPI::INVALID;
			break;
		}

		// Free the ImGui window backend
		switch (m_imguiWindowAPI)
		{
		case eWindowAPI::SDL:
			ImGui_ImplSDL2_Shutdown();
			m_imguiWindowAPI= eWindowAPI::INVALID;
			break;
		}

		ImGui::DestroyContext(m_imguiContext);
		m_imguiContext= nullptr;
		m_baseStyle.reset();
		m_appliedUiScale= 0.f;
	}

	// Restore the previous context
	ImGui::SetCurrentContext(prevImGuiContext);
}

void MkGuiContext::makeCurrent() { ImGui::SetCurrentContext(m_imguiContext); }

void MkGuiContext::submitDrawData()
{
	// Make sure this ImGui context is current before we try to render with it
	MkGuiScopedContext scopedContext(*this);

	// Submit the ImGui draw data to the OpenGL backend for rendering
	switch (m_imguiGraphicsAPI)
	{
	case eGraphicsAPI::OpenGL:
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		break;
	}
}

void MkGuiContext::configImGui()
{
	ImGuiIO& io= ImGui::GetIO();

	if (!m_iniFilePath.empty())
	{
		io.IniFilename= m_iniFilePath.c_str();
	}

	// Set before the first NewFrame, which is when the ini loads the torn-out
	// windows' viewport positions. The viewport config stays at ImGui's defaults:
	// ImGui-drawn title bars on borderless OS windows, a taskbar entry per torn-out
	// window, and a floating panel becoming an OS window only once it crosses the
	// main window's edge.
	if (m_bEnableViewports)
	{
		io.ConfigFlags|= ImGuiConfigFlags_ViewportsEnable;
	}

	if (m_bEnableDocking)
	{
		io.ConfigFlags|= ImGuiConfigFlags_DockingEnable;

		// Registered before the first frame loads the ini, so the scale the saved
		// layout was arranged at is in hand when the dock nodes come back
		MkGui::installLayoutSettings(&m_layoutRefScale);
	}

	MkGuiTheme::applyStyle();

	// The authored, unscaled metrics. refreshUiScale re-derives the live style from
	// this copy every time the UI scale changes.
	m_baseStyle= std::make_unique<ImGuiStyle>(ImGui::GetStyle());

	// One UI font with the icons merged in. The normal/big icon distinction is
	// now a push size on the same font (see MkGuiStyleManager's font block).
	ImFont* uiFont= MkGuiTheme::loadFonts();
	m_NormalIconFont= uiFont;
	m_BigIconFont= uiFont;
}

void MkGuiContext::renderPlatformWindows(const ViewportRenderedCallback& viewportRenderedCallback)
{
	if (!m_bEnableViewports || m_imguiContext == nullptr || m_imguiGraphicsAPI != eGraphicsAPI::OpenGL)
		return;

	MkGuiScopedContext scopedContext(*this);

	if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0)
		return;

	// Each torn-out window renders on its own GL context, which the SDL backend
	// creates sharing objects with this window's, so this window's context has to
	// be current again before its own frame presents
	SDL_Window* backupWindow= SDL_GL_GetCurrentWindow();
	SDL_GLContext backupContext= SDL_GL_GetCurrentContext();

	ImGui::UpdatePlatformWindows();

	// RenderPlatformWindowsDefault, walked here so a torn-out window's back buffer
	// can be read between its render and its swap
	ImGuiPlatformIO& platformIO= ImGui::GetPlatformIO();
	for (int viewportIndex= 1; viewportIndex < platformIO.Viewports.Size; ++viewportIndex)
	{
		ImGuiViewport* viewport= platformIO.Viewports[viewportIndex];
		if (viewport->Flags & ImGuiViewportFlags_IsMinimized)
			continue;

		if (platformIO.Platform_RenderWindow)
			platformIO.Platform_RenderWindow(viewport, nullptr);
		if (platformIO.Renderer_RenderWindow)
			platformIO.Renderer_RenderWindow(viewport, nullptr);

		if (viewportRenderedCallback)
		{
			SDL_Window* viewportWindow= SDL_GetWindowFromID((Uint32)(intptr_t)viewport->PlatformHandle);
			int drawableWidth= 0;
			int drawableHeight= 0;
			if (viewportWindow != nullptr)
			{
				SDL_GL_GetDrawableSize(viewportWindow, &drawableWidth, &drawableHeight);
			}

			viewportRenderedCallback(viewport->ID, drawableWidth, drawableHeight);
		}
	}

	for (int viewportIndex= 1; viewportIndex < platformIO.Viewports.Size; ++viewportIndex)
	{
		ImGuiViewport* viewport= platformIO.Viewports[viewportIndex];
		if (viewport->Flags & ImGuiViewportFlags_IsMinimized)
			continue;

		if (platformIO.Platform_SwapBuffers)
			platformIO.Platform_SwapBuffers(viewport, nullptr);
		if (platformIO.Renderer_SwapBuffers)
			platformIO.Renderer_SwapBuffers(viewport, nullptr);
	}

	SDL_GL_MakeCurrent(backupWindow, backupContext);
}

bool MkGuiContext::ownsPlatformWindow(uint32_t windowId)
{
	if (!m_bEnableViewports || m_imguiContext == nullptr || windowId == 0)
		return false;

	MkGuiScopedContext scopedContext(*this);

	ImGuiViewport* viewport= ImGui::FindViewportByPlatformHandle((void*)(intptr_t)windowId);
	return viewport != nullptr && viewport != ImGui::GetMainViewport();
}

// The part of a window's ImGui name its ID hashes from: what follows "###" when
// the name carries one, the whole name otherwise
static const char* getStableWindowId(const ImGuiWindow* window) { return ImHashSkipUncontributingPrefix(window->Name); }

// A window the user sees as a panel, drawn last frame: not a child region, dock
// node host or popup, and not one of the "##" hosts that carry no title
static bool isListedWindow(const ImGuiWindow* window)
{
	if (!window->WasActive)
		return false;
	if ((window->Flags & ImGuiWindowFlags_ChildWindow) && !window->DockIsActive)
		return false;
	if (window->Flags
		& (ImGuiWindowFlags_DockNodeHost | ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip
		   | ImGuiWindowFlags_ChildMenu))
		return false;

	const char* stableId= getStableWindowId(window);
	return stableId[0] != 0 && strncmp(stableId, "##", 2) != 0;
}

std::vector<MkGuiViewportInfo> MkGuiContext::getViewports()
{
	std::vector<MkGuiViewportInfo> viewportInfos;
	if (m_imguiContext == nullptr)
		return viewportInfos;

	MkGuiScopedContext scopedContext(*this);

	ImGuiContext& context= *ImGui::GetCurrentContext();
	const ImGuiViewport* mainViewport= ImGui::GetMainViewport();
	for (ImGuiViewportP* viewport : context.Viewports)
	{
		MkGuiViewportInfo info;
		info.viewportId= viewport->ID;
		info.platformWindowId= (uint32_t)(intptr_t)viewport->PlatformHandle;
		info.bIsMainViewport= viewport == mainViewport;
		info.x= (int)viewport->Pos.x;
		info.y= (int)viewport->Pos.y;
		info.width= (int)viewport->Size.x;
		info.height= (int)viewport->Size.y;

		for (ImGuiWindow* window : context.Windows)
		{
			if (window->Viewport == viewport && isListedWindow(window))
			{
				info.windowIds.push_back(getStableWindowId(window));
			}
		}

		// The main viewport leads, whatever order ImGui keeps them in
		if (info.bIsMainViewport)
			viewportInfos.insert(viewportInfos.begin(), info);
		else
			viewportInfos.push_back(info);
	}

	return viewportInfos;
}

bool MkGuiContext::requestFloatWindow(const std::string& windowId, int x, int y, int width, int height,
									  std::string& outError)
{
	if (!m_bEnableViewports || m_imguiContext == nullptr)
	{
		outError= "this window's GUI has no multi-viewports";
		return false;
	}

	MkGuiScopedContext scopedContext(*this);

	const std::string stableSuffix= "###" + windowId;
	ImGuiWindow* matchedWindow= nullptr;
	for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
	{
		if (!isListedWindow(window))
			continue;

		const std::string windowName= window->Name;
		const bool bEndsWithStableId=
			windowName.size() >= stableSuffix.size()
			&& windowName.compare(windowName.size() - stableSuffix.size(), stableSuffix.size(), stableSuffix) == 0;
		if (windowName == windowId || bEndsWithStableId)
		{
			matchedWindow= window;
			break;
		}
	}

	if (matchedWindow == nullptr)
	{
		outError= "no open window '" + windowId + "'";
		return false;
	}

	PendingFloatRequest request;
	request.windowName= matchedWindow->Name;
	request.x= x;
	request.y= y;
	request.width= width;
	request.height= height;
	m_pendingFloatRequests.push_back(request);

	return true;
}

void MkGuiContext::applyPendingFloatRequests()
{
	if (m_pendingFloatRequests.empty())
		return;

	MkGuiScopedContext scopedContext(*this);

	for (const PendingFloatRequest& request : m_pendingFloatRequests)
	{
		ImGuiWindow* window= ImGui::FindWindowByName(request.windowName.c_str());
		if (window == nullptr)
			continue;

		// The same undock a tab dragged out of its node gets
		ImGui::DockContextProcessUndockWindow(ImGui::GetCurrentContext(), window, true);

		const ImVec2 desktopPos((float)request.x, (float)request.y);
		ImGui::SetWindowPos(window, desktopPos, ImGuiCond_Always);
		if (request.width > 0 && request.height > 0)
		{
			ImGui::SetWindowSize(window, ImVec2((float)request.width, (float)request.height), ImGuiCond_Always);
		}

		// A window that already owns its viewport carries it to the new position.
		// One still in the main viewport would only be clamped back to that
		// viewport's edge, so it drops the viewport and takes the path a window
		// restored from the ini takes: its next Begin creates a viewport at the
		// saved position, which stays its own unless the window fits inside the
		// main window.
		const bool bOwnsViewport= window->Viewport != nullptr && window->Viewport->Window == window;
		if (!bOwnsViewport)
		{
			window->Viewport= nullptr;
			window->ViewportId= window->ID;
			window->ViewportPos= desktopPos;
		}
	}

	m_pendingFloatRequests.clear();
}

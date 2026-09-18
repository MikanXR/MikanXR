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
#include "misc/cpp/imgui_stdlib.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

// The app-wide user scale multiplier, applied on top of each monitor's content scale
static float g_userUiScale= 1.f;

MkGuiContext::MkGuiContext(class IMkWindowContext* window, const std::string& iniFilePath, bool bEnableDocking)
	: m_window(window)
	, m_iniFilePath(iniFilePath)
	, m_bEnableDocking(bEnableDocking)
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
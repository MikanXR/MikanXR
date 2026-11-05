#include "IMkGraphicsContext.h"
#include "IMkWindowEventListener.h"
#include "IMkWindowContextManager.h"
#include "SdlWindowContext.h"
#include "SdlWindowContextManager.h"
#include "SdlWindowEventListener.h"
#include "Logger.h"

#include <algorithm>
#include <assert.h>
#include <cstring>

#if defined(_WIN32)
#include <SDL.h>
#include <SDL_events.h>
#include <SDL_mouse.h>
#include <SDL_syswm.h>
#include <SDL_image.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_syswm.h>
#endif

#include "SdlCommon.h"

SdlWindowContext::SdlWindowContext(IMkWindowContextManagerPtr ownerWindowManager, IMkGraphicsContextPtr graphicsContext)
	: m_ownerWindowManager(ownerWindowManager)
	, m_graphicsContext(graphicsContext)
{
}

SdlWindowContext::~SdlWindowContext() { shutdown(); }

void SdlWindowContext::enableGLDataSharing()
{
	assert(m_sdlWindow == nullptr);
	m_bGLDataSharingEnabled= true;
}

void SdlWindowContext::useExistingGLContext()
{
	assert(m_sdlWindow == nullptr); // must be called before startup()
	m_bOwnsGLContext= false;
}

void SdlWindowContext::setTitle(const std::string& title)
{
	m_title= title;

	if (m_sdlWindow != nullptr)
	{
		SDL_SetWindowTitle(m_sdlWindow, title.c_str());
	}
}

void SdlWindowContext::setSize(int width, int height)
{
	m_width= width;
	m_height= height;

	if (m_sdlWindow != nullptr)
	{
		SDL_SetWindowSize(m_sdlWindow, width, height);
	}
}

bool SdlWindowContext::startup()
{
	IMkWindowContextManagerPtr ownerWindowManager= m_ownerWindowManager.lock();
	assert(ownerWindowManager != nullptr && ownerWindowManager->getIsInitialized());
	IMkGraphicsContextPtr graphicsContext= m_graphicsContext.lock();
	assert(graphicsContext != nullptr && graphicsContext->getGraphicsAPI() == eGraphicsAPI::OpenGL);

	bool success= true;

	if (m_bGLDataSharingEnabled)
	{
		SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
	}

	SDL_WindowFlags window_flags=
		(SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	m_sdlWindow= SDL_CreateWindow(m_title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_width, m_height,
								  window_flags);
	if (m_sdlWindow == nullptr)
	{
		MIKAN_LOG_ERROR("SdlWindowContext::startup") << "Unable to initialize SDL window: " << SDL_GetError();
		success= false;
	}
	else
	{
		graphicsContext->onWindowSizeChanged(m_width, m_height);
	}

	if (success)
	{
		if (m_bOwnsGLContext)
		{
			// Create a new GL context owned by this window
			m_glContext= SDL_GL_CreateContext(m_sdlWindow);
			if (m_glContext != NULL)
			{
				ownerWindowManager->pushCurrentWindowContext(this);

				// Disable vsync on swap
				SDL_GL_SetSwapInterval(0);
			}
			else
			{
				MIKAN_LOG_ERROR("SdlWindowContext::startup")
					<< "Unable to initialize SDL OpenGL context: " << SDL_GetError();
				success= false;
			}
		}
		else
		{
			// Attach this window to the currently active GL context (owned by another window)
			m_glContext= SDL_GL_GetCurrentContext();
			if (m_glContext != NULL)
			{
				// pushCurrentWindowContext calls makeContextCurrent which binds m_sdlWindow+m_glContext.
				ownerWindowManager->pushCurrentWindowContext(this);

				// wglSwapIntervalEXT is per-drawable on Windows, so call SetSwapInterval(0) afterwards
				// to ensure this window doesn't inherit the default vsync-on swap interval.
				SDL_GL_SetSwapInterval(0);
			}
			else
			{
				MIKAN_LOG_ERROR("SdlWindowContext::startup") << "No active GL context to attach to";
				success= false;
			}
		}
	}

	if (success && m_bOwnsGLContext)
	{
		// Only initialize the graphics context for the context owner.
		// Secondary windows share an already-initialized graphics context.
		graphicsContext->onNativeContextCreated(m_glContext);
		if (!graphicsContext->startup())
		{
			MIKAN_LOG_ERROR("SdlWindowContext::startup") << "Unable to initialize graphics context";
			success= false;
		}
	}

	if (success)
	{
		m_windowId= SDL_GetWindowID(m_sdlWindow);

		m_isShown= true;
		m_hasMouseFocus= true;
		m_hasKeyboardFocus= true;
	}

	return success;
}

void SdlWindowContext::shutdown()
{
	IMkGraphicsContextPtr graphicsContext= m_graphicsContext.lock();
	if (graphicsContext && m_bOwnsGLContext)
	{
		graphicsContext->shutdown();
	}

	if (m_glContext != NULL)
	{
		if (m_bOwnsGLContext)
		{
			SDL_GL_DeleteContext(m_glContext);
		}
		m_glContext= NULL;
	}

	if (m_sdlWindow != NULL)
	{
		SDL_DestroyWindow(m_sdlWindow);
		m_sdlWindow= NULL;
	}

	m_hasKeyboardFocus= false;
	m_hasMouseFocus= false;
	m_width= 0;
	m_height= 0;
}

void SdlWindowContext::update(float deltaSeconds)
{
	// Base implementation - subclasses override for app logic
}

void SdlWindowContext::render()
{
	// Base implementation - subclasses override for render logic
}

void SdlWindowContext::present() { SDL_GL_SwapWindow(m_sdlWindow); }

IMkViewportPtr SdlWindowContext::getRenderingViewport() const
{
	IMkGraphicsContextPtr graphicsContext= m_graphicsContext.lock();

	return graphicsContext ? graphicsContext->getRenderingViewport() : IMkViewportPtr();
}

void SdlWindowContext::getMouseScreenPosition(int& outScreenX, int& outScreenY) const
{
	SDL_GetMouseState(&outScreenX, &outScreenY);
}

void SdlWindowContext::getMouseDesktopPosition(int& outDesktopX, int& outDesktopY) const
{
	SDL_GetGlobalMouseState(&outDesktopX, &outDesktopY);
}

void SdlWindowContext::handleEvents(IMkWindowEventListener* eventListener)
{
	IMkWindowContextManagerPtr ownerWindowManager= m_ownerWindowManager.lock();
	assert(ownerWindowManager != nullptr);

	std::vector<SDL_Event>& events= static_cast<SdlWindowContextManager*>(ownerWindowManager.get())->getEvents();
	auto it= events.begin();
	while (it != events.end())
	{
		SDL_Event& sdlEvent= *it;

		bool bHandled= false;

		if (sdlEvent.window.windowID == m_windowId)
		{
			bHandled= handleSDLWindowEvent(&sdlEvent);

			if (eventListener != nullptr)
			{
				SdlWindowEvent mkEvent(&sdlEvent);
				if (!bHandled)
				{
					bHandled= eventListener->onWindowEvent(mkEvent);
				}
				else if (sdlEvent.type == SDL_WINDOWEVENT)
				{
					// A window event this window has already applied still reaches the listener.
					// An ImGui context with multi-viewports tracks which of its OS windows holds
					// the mouse from the enter events, and without this window's own it keeps
					// treating the last torn-out window the mouse entered as hovered.
					eventListener->onWindowEvent(mkEvent);
				}
			}
		}
		else if (eventListener != nullptr)
		{
			// A child window's resize, focus or close is that window's own, so it skips
			// handleSDLWindowEvent rather than touch this window's size, focus or destroy flag
			const uint32_t childWindowId= getSdlEventWindowId(&sdlEvent);
			if (childWindowId != 0 && eventListener->ownsChildWindow(childWindowId))
			{
				SdlWindowEvent mkEvent(&sdlEvent);
				bHandled= eventListener->onWindowEvent(mkEvent);
			}
		}

		if (bHandled)
		{
			it= events.erase(it);
		}
		else
		{
			it++;
		}
	}
}

bool SdlWindowContext::handleSDLWindowEvent(const SDL_Event* event)
{
	bool bHandled= true;

	if (event->type == SDL_WINDOWEVENT)
	{
		switch (event->window.event)
		{
		case SDL_WINDOWEVENT_SHOWN:
			m_isShown= true;
			break;

		case SDL_WINDOWEVENT_HIDDEN:
			m_isShown= false;
			break;

		case SDL_WINDOWEVENT_SIZE_CHANGED:
		{
			m_width= event->window.data1;
			m_height= event->window.data2;
			IMkGraphicsContextPtr graphicsContext= m_graphicsContext.lock();
			if (graphicsContext)
			{
				graphicsContext->onWindowSizeChanged(m_width, m_height);
			}
		}
		break;

		case SDL_WINDOWEVENT_ENTER:
			m_hasMouseFocus= true;
			break;

		case SDL_WINDOWEVENT_LEAVE:
			m_hasMouseFocus= false;
			break;

		case SDL_WINDOWEVENT_FOCUS_GAINED:
			m_hasKeyboardFocus= true;
			break;

		case SDL_WINDOWEVENT_FOCUS_LOST:
			m_hasKeyboardFocus= false;
			break;

		case SDL_WINDOWEVENT_MINIMIZED:
			m_isMinimized= true;
			break;

		case SDL_WINDOWEVENT_MAXIMIZED:
			m_isMinimized= false;
			break;

		case SDL_WINDOWEVENT_RESTORED:
			m_isMinimized= false;
			break;

		case SDL_WINDOWEVENT_CLOSE:
			SDL_HideWindow(m_sdlWindow);
			m_wantsDestroy= true;
			break;

		default:
			bHandled= false;
		}
	}
	else
	{
		bHandled= false;
	}

	return bHandled;
}

void SdlWindowContext::focus()
{
	if (!m_isShown)
	{
		SDL_ShowWindow(m_sdlWindow);
	}

	SDL_RaiseWindow(m_sdlWindow);
}

// -- Synthetic input ----
namespace
{
uint16_t mkKeyModToSdlKeyMod(uint16_t keyMod)
{
	uint16_t sdlMod= KMOD_NONE;

	if (keyMod & MkKeyMod::SHIFT)
		sdlMod|= KMOD_LSHIFT;
	if (keyMod & MkKeyMod::CTRL)
		sdlMod|= KMOD_LCTRL;
	if (keyMod & MkKeyMod::ALT)
		sdlMod|= KMOD_LALT;
	if (keyMod & MkKeyMod::GUI)
		sdlMod|= KMOD_LGUI;

	return sdlMod;
}

uint8_t mkMouseButtonToSdlButton(int mkMouseButton)
{
	switch (mkMouseButton)
	{
	case MkMouseButton::LEFT:
		return SDL_BUTTON_LEFT;
	case MkMouseButton::MIDDLE:
		return SDL_BUTTON_MIDDLE;
	case MkMouseButton::RIGHT:
		return SDL_BUTTON_RIGHT;
	default:
		return 0;
	}
}
} // namespace

uint32_t SdlWindowContext::resolveInjectedWindowId(uint32_t childWindowId) const
{
	return childWindowId != 0 ? childWindowId : (uint32_t)m_windowId;
}

void SdlWindowContext::raiseWindow() { focus(); }

void SdlWindowContext::raiseChildWindow(uint32_t childWindowId)
{
	if (SDL_Window* childWindow= SDL_GetWindowFromID(childWindowId))
	{
		SDL_RaiseWindow(childWindow);
	}
}

void SdlWindowContext::warpMouseToWindowPosition(int windowX, int windowY)
{
	// Moves the real cursor, which is deliberate: see the note on IMkWindowContext
	SDL_WarpMouseInWindow(m_sdlWindow, windowX, windowY);
}

void SdlWindowContext::warpMouseToChildWindowPosition(uint32_t childWindowId, int windowX, int windowY)
{
	if (SDL_Window* childWindow= SDL_GetWindowFromID(childWindowId))
	{
		SDL_WarpMouseInWindow(childWindow, windowX, windowY);
	}
}

void SdlWindowContext::injectMouseButton(int mkMouseButton, bool bPressed, int windowX, int windowY, int clickCount,
										 uint32_t childWindowId)
{
	const uint8_t sdlButton= mkMouseButtonToSdlButton(mkMouseButton);
	if (sdlButton == 0)
		return;

	SDL_Event sdlEvent{};
	sdlEvent.type= bPressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	sdlEvent.button.timestamp= SDL_GetTicks();
	sdlEvent.button.windowID= resolveInjectedWindowId(childWindowId);
	sdlEvent.button.which= 0;
	sdlEvent.button.button= sdlButton;
	sdlEvent.button.state= bPressed ? SDL_PRESSED : SDL_RELEASED;
	sdlEvent.button.clicks= (uint8_t)(clickCount > 0 ? clickCount : 1);
	sdlEvent.button.x= windowX;
	sdlEvent.button.y= windowY;

	SDL_PushEvent(&sdlEvent);
}

void SdlWindowContext::injectMouseWheel(int windowX, int windowY, int scrollX, int scrollY, uint32_t childWindowId)
{
	SDL_Event sdlEvent{};
	sdlEvent.type= SDL_MOUSEWHEEL;
	sdlEvent.wheel.timestamp= SDL_GetTicks();
	sdlEvent.wheel.windowID= resolveInjectedWindowId(childWindowId);
	sdlEvent.wheel.which= 0;
	sdlEvent.wheel.x= scrollX;
	sdlEvent.wheel.y= scrollY;
	sdlEvent.wheel.direction= SDL_MOUSEWHEEL_NORMAL;
	sdlEvent.wheel.preciseX= (float)scrollX;
	sdlEvent.wheel.preciseY= (float)scrollY;
	sdlEvent.wheel.mouseX= windowX;
	sdlEvent.wheel.mouseY= windowY;

	SDL_PushEvent(&sdlEvent);
}

void SdlWindowContext::injectKey(MkKeySym keySym, uint16_t keyMod, bool bPressed, uint32_t childWindowId)
{
	SDL_Event sdlEvent{};
	sdlEvent.type= bPressed ? SDL_KEYDOWN : SDL_KEYUP;
	sdlEvent.key.timestamp= SDL_GetTicks();
	sdlEvent.key.windowID= resolveInjectedWindowId(childWindowId);
	sdlEvent.key.state= bPressed ? SDL_PRESSED : SDL_RELEASED;
	sdlEvent.key.repeat= 0;
	sdlEvent.key.keysym.sym= (SDL_Keycode)keySym;
	sdlEvent.key.keysym.scancode= SDL_GetScancodeFromKey((SDL_Keycode)keySym);
	sdlEvent.key.keysym.mod= mkKeyModToSdlKeyMod(keyMod);

	SDL_PushEvent(&sdlEvent);
}

void SdlWindowContext::injectText(const std::string& utf8Text, uint32_t childWindowId)
{
	// SDL_TextInputEvent carries a fixed 32 byte buffer, so longer text goes out as several events.
	// Splitting only on a UTF-8 lead byte keeps multi-byte characters intact across the boundary.
	constexpr size_t k_maxTextBytes= SDL_TEXTINPUTEVENT_TEXT_SIZE - 1;

	size_t offset= 0;
	while (offset < utf8Text.size())
	{
		size_t chunkBytes= std::min(k_maxTextBytes, utf8Text.size() - offset);

		while (chunkBytes > 0 && (utf8Text[offset + chunkBytes] & 0xC0) == 0x80)
		{
			--chunkBytes;
		}

		if (chunkBytes == 0)
			break;

		SDL_Event sdlEvent{};
		sdlEvent.type= SDL_TEXTINPUT;
		sdlEvent.text.timestamp= SDL_GetTicks();
		sdlEvent.text.windowID= resolveInjectedWindowId(childWindowId);
		memcpy(sdlEvent.text.text, utf8Text.data() + offset, chunkBytes);
		sdlEvent.text.text[chunkBytes]= '\0';

		SDL_PushEvent(&sdlEvent);

		offset+= chunkBytes;
	}
}

void SdlWindowContext::requestClose()
{
	if (m_sdlWindow)
	{
		SDL_HideWindow(m_sdlWindow);
	}
	m_wantsDestroy= true;
}

void SdlWindowContext::makeContextCurrent()
{
	int result= SDL_GL_MakeCurrent(m_sdlWindow, m_glContext);
	if (result != 0)
	{
		const char* errorMessage= SDL_GetError();
		MIKAN_LOG_ERROR("SdlWindowContext::makeContextCurrent") << "Error with SDL_GL_MakeCurrent: " << errorMessage;
	}

	// Keep graphics context width/height in sync with whichever window is currently active.
	// This is required for text rendering and other screen-space calculations when a single
	// GL context is shared across multiple windows.
	IMkGraphicsContextPtr graphicsContext= m_graphicsContext.lock();
	if (graphicsContext)
	{
		graphicsContext->onWindowSizeChanged(m_width, m_height);
	}
}

IMkWindowContextPtr createMkWindowContext(IMkWindowContextManagerPtr ownerWindowManager,
										  IMkGraphicsContextPtr graphicsContext)
{
	// TODO: Use the graphics context API to determine which window type to create
	// (e.g. OpenGL = SdlGlWindow, Vulkan = SdlVulkanWindow, etc.)
	return std::make_shared<SdlWindowContext>(ownerWindowManager, graphicsContext);
}

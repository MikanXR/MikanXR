#include "SdlWindowEventListener.h"

#if defined(_WIN32)
#include <SDL.h>
#include <SDL_events.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#endif

uint32_t getSdlEventWindowId(const SDL_Event* event)
{
	switch (event->type)
	{
	case SDL_WINDOWEVENT:
		return event->window.windowID;
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		return event->key.windowID;
	case SDL_TEXTEDITING:
		return event->edit.windowID;
	case SDL_TEXTINPUT:
		return event->text.windowID;
	case SDL_MOUSEMOTION:
		return event->motion.windowID;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		return event->button.windowID;
	case SDL_MOUSEWHEEL:
		return event->wheel.windowID;
	case SDL_DROPFILE:
	case SDL_DROPTEXT:
	case SDL_DROPBEGIN:
	case SDL_DROPCOMPLETE:
		return event->drop.windowID;
	default:
		return 0;
	}
}

SdlWindowEvent::SdlWindowEvent(const SDL_Event* event)
	: m_event(event)
{
}

uint32_t SdlWindowEvent::getWindowId() const { return getSdlEventWindowId(m_event); }

eMkWindowEventType SdlWindowEvent::getEventType() const
{
	switch (m_event->type)
	{
	case SDL_QUIT:
		return eMkWindowEventType::Quit;
	case SDL_KEYDOWN:
		return eMkWindowEventType::KeyDown;
	case SDL_KEYUP:
		return eMkWindowEventType::KeyUp;
	case SDL_TEXTINPUT:
		return eMkWindowEventType::TextInput;
	case SDL_MOUSEWHEEL:
		return eMkWindowEventType::MouseWheel;
	case SDL_MOUSEBUTTONDOWN:
		return eMkWindowEventType::MouseButtonDown;
	case SDL_MOUSEBUTTONUP:
		return eMkWindowEventType::MouseButtonUp;
	case SDL_MOUSEMOTION:
		return eMkWindowEventType::MouseMotion;
	case SDL_WINDOWEVENT:
		return eMkWindowEventType::WindowEvent;
	default:
		return eMkWindowEventType::Other;
	}
}

MkKeySym SdlWindowEvent::getKeySym() const
{
	if (m_event->type == SDL_KEYDOWN || m_event->type == SDL_KEYUP)
	{
		return m_event->key.keysym.sym;
	}

	return MkKey::UNKNOWN;
}

uint16_t SdlWindowEvent::getKeyMod() const
{
	if (m_event->type == SDL_KEYDOWN || m_event->type == SDL_KEYUP)
	{
		const uint16_t sdlMod= m_event->key.keysym.mod;

		uint16_t keyMod= MkKeyMod::NONE;
		if (sdlMod & KMOD_SHIFT)
			keyMod|= MkKeyMod::SHIFT;
		if (sdlMod & KMOD_CTRL)
			keyMod|= MkKeyMod::CTRL;
		if (sdlMod & KMOD_ALT)
			keyMod|= MkKeyMod::ALT;
		if (sdlMod & KMOD_GUI)
			keyMod|= MkKeyMod::GUI;

		return keyMod;
	}

	return MkKeyMod::NONE;
}

int SdlWindowEvent::getKeyRepeat() const
{
	if (m_event->type == SDL_KEYDOWN || m_event->type == SDL_KEYUP)
	{
		return m_event->key.repeat;
	}
	return 0;
}

const char* SdlWindowEvent::getText() const
{
	if (m_event->type == SDL_TEXTINPUT)
		return m_event->text.text;

	return "";
}

eMkWindowEventID SdlWindowEvent::getWindowEventID() const
{
	if (m_event->type == SDL_WINDOWEVENT)
	{
		switch (m_event->window.event)
		{
		case SDL_WINDOWEVENT_SHOWN:
			return eMkWindowEventID::Shown;
		case SDL_WINDOWEVENT_HIDDEN:
			return eMkWindowEventID::Hidden;
		case SDL_WINDOWEVENT_SIZE_CHANGED:
			return eMkWindowEventID::SizeChanged;
		case SDL_WINDOWEVENT_ENTER:
			return eMkWindowEventID::Enter;
		case SDL_WINDOWEVENT_LEAVE:
			return eMkWindowEventID::Leave;
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			return eMkWindowEventID::FocusGained;
		case SDL_WINDOWEVENT_FOCUS_LOST:
			return eMkWindowEventID::FocusLost;
		case SDL_WINDOWEVENT_MINIMIZED:
			return eMkWindowEventID::Minimized;
		case SDL_WINDOWEVENT_MAXIMIZED:
			return eMkWindowEventID::Maximized;
		case SDL_WINDOWEVENT_RESTORED:
			return eMkWindowEventID::Restored;
		case SDL_WINDOWEVENT_CLOSE:
			return eMkWindowEventID::Close;
		default:
			return eMkWindowEventID::Unknown;
		}
	}
	return eMkWindowEventID::Unknown;
}

int SdlWindowEvent::getMouseWheelScrollAmount() const
{
	if (m_event->type == SDL_MOUSEWHEEL)
		return m_event->wheel.y;
	return 0;
}

int SdlWindowEvent::getMouseWheelScrollAmountX() const
{
	if (m_event->type == SDL_MOUSEWHEEL)
		return m_event->wheel.x;
	return 0;
}

int SdlWindowEvent::getMouseButton() const
{
	if (m_event->type == SDL_MOUSEBUTTONDOWN || m_event->type == SDL_MOUSEBUTTONUP)
		return m_event->button.button;
	return 0;
}

int SdlWindowEvent::getMouseClickCount() const
{
	if (m_event->type == SDL_MOUSEBUTTONDOWN || m_event->type == SDL_MOUSEBUTTONUP)
		return m_event->button.clicks;
	return 0;
}

int SdlWindowEvent::getMouseX() const
{
	if (m_event->type == SDL_MOUSEMOTION)
		return m_event->motion.x;
	if (m_event->type == SDL_MOUSEBUTTONDOWN || m_event->type == SDL_MOUSEBUTTONUP)
		return m_event->button.x;
	if (m_event->type == SDL_MOUSEWHEEL)
		return m_event->wheel.mouseX;
	return 0;
}

int SdlWindowEvent::getMouseY() const
{
	if (m_event->type == SDL_MOUSEMOTION)
		return m_event->motion.y;
	if (m_event->type == SDL_MOUSEBUTTONDOWN || m_event->type == SDL_MOUSEBUTTONUP)
		return m_event->button.y;
	if (m_event->type == SDL_MOUSEWHEEL)
		return m_event->wheel.mouseY;
	return 0;
}

int SdlWindowEvent::getMouseMotionXRel() const
{
	if (m_event->type == SDL_MOUSEMOTION)
		return m_event->motion.xrel;
	return 0;
}

int SdlWindowEvent::getMouseMotionYRel() const
{
	if (m_event->type == SDL_MOUSEMOTION)
		return m_event->motion.yrel;
	return 0;
}

int SdlWindowEvent::getWindowData1() const
{
	if (m_event->type == SDL_WINDOWEVENT)
		return m_event->window.data1;
	return 0;
}

int SdlWindowEvent::getWindowData2() const
{
	if (m_event->type == SDL_WINDOWEVENT)
		return m_event->window.data2;
	return 0;
}

const void* SdlWindowEvent::getInternalWindowEvent() const { return m_event; }

#pragma once

#include "MulticastDelegate.h"
#include "MkWindowEvent.h"

#include <map>
#include <vector>

#include <stdint.h>

class KeyEventBindings
{
public:
	KeyEventBindings()= default;
	virtual ~KeyEventBindings();

	MulticastDelegate<void()> OnKeyPressed;
	MulticastDelegate<void()> OnKeyReleased;
	MulticastDelegate<void()> OnKeyRepeated;

	void clear();
};

class EventBindingSet
{
public:
	EventBindingSet()= default;
	virtual ~EventBindingSet();

	// Bindings key on (keysym, required MkKeyMod mask). A mask of
	// MkKeyMod::ANY fires regardless of held modifiers.
	static uint64_t makeKeyBindingKey(MkKeySym key, uint16_t modMask)
	{
		return ((uint64_t)modMask << 32) | (uint32_t)key;
	}

	std::map<uint64_t, KeyEventBindings*> keybindings;
	MulticastDelegate<void(int dx, int dy)> OnMouseMotionEvent;
	MulticastDelegate<void(int button)> OnMouseButtonPressedEvent;
	MulticastDelegate<void(int button)> OnMouseButtonReleasedEvent;
	MulticastDelegate<void(int scroll)> OnMouseWheelScrolledEvent;

	void clear();
};

class InputManager
{
public:
	InputManager()= delete;
	InputManager(class IEditorWindow* ownerWindow);
	virtual ~InputManager();

	bool onWindowEvent(const MkWindowEvent& event);
	// The cursor in client pixels of whichever window has mouse focus
	void getMouseScreenPosition(int& outScreenX, int& outScreenY) const;
	// The cursor in desktop pixels, the space of the main window's ImGui positions
	void getMouseDesktopPosition(int& outDesktopX, int& outDesktopY) const;

	KeyEventBindings* getKeyBindings(MkKeySym key, uint16_t modMask= MkKeyMod::ANY);
	KeyEventBindings* fetchOrAddKeyBindings(MkKeySym key, uint16_t modMask= MkKeyMod::ANY);

	EventBindingSet* pushEventBindingSet();
	void popEventBindingSet();
	EventBindingSet* getCurrentEventBindingSet();

private:
	class IEditorWindow* m_ownerWindow;
	std::vector<EventBindingSet*> m_eventBindings;
};

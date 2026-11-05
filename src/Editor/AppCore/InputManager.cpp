#include "IEditorWindow.h"
#include "InputManager.h"
#include "MkWindowEvent.h"

// -- InputManager ------
InputManager::InputManager(class IEditorWindow* ownerWindow)
	: m_ownerWindow(ownerWindow)
{
}

InputManager::~InputManager()
{
	while (m_eventBindings.size() > 0)
	{
		popEventBindingSet();
	}
}

bool InputManager::onWindowEvent(const MkWindowEvent& event)
{
	bool bHandled= false;

	switch (event.getEventType())
	{
	case eMkWindowEventType::KeyDown:
	{
		// Fire the exact modifier-mask binding plus the modifier-agnostic one
		KeyEventBindings* keybindsList[2]= {getKeyBindings(event.getKeySym(), event.getKeyMod()),
											getKeyBindings(event.getKeySym(), MkKeyMod::ANY)};
		for (KeyEventBindings* keybinds : keybindsList)
		{
			if (keybinds == nullptr)
				continue;

			// A held key generates repeated KeyDown events (key.repeat != 0). Only the
			// initial press fires OnKeyPressed; repeats fire OnKeyRepeated. This keeps
			// press/release balanced for state tracking and one-shot actions.
			if (event.getKeyRepeat())
			{
				if (keybinds->OnKeyRepeated)
				{
					keybinds->OnKeyRepeated();
					bHandled= true;
				}
			}
			else
			{
				if (keybinds->OnKeyPressed)
				{
					keybinds->OnKeyPressed();
					bHandled= true;
				}
			}
		}
	}
	break;
	case eMkWindowEventType::KeyUp:
	{
		KeyEventBindings* keybindsList[2]= {getKeyBindings(event.getKeySym(), event.getKeyMod()),
											getKeyBindings(event.getKeySym(), MkKeyMod::ANY)};
		for (KeyEventBindings* keybinds : keybindsList)
		{
			if (keybinds != nullptr && keybinds->OnKeyReleased)
			{
				keybinds->OnKeyReleased();
				bHandled= true;
			}
		}
	}
	break;
	case eMkWindowEventType::MouseWheel:
	{
		EventBindingSet* bindingSet= getCurrentEventBindingSet();
		if (bindingSet != nullptr && bindingSet->OnMouseWheelScrolledEvent)
		{
			bindingSet->OnMouseWheelScrolledEvent(event.getMouseWheelScrollAmount());
			bHandled= true;
		}
	}
	break;
	case eMkWindowEventType::MouseButtonDown:
	{
		EventBindingSet* bindingSet= getCurrentEventBindingSet();
		if (bindingSet != nullptr && bindingSet->OnMouseButtonPressedEvent)
		{
			bindingSet->OnMouseButtonPressedEvent(event.getMouseButton());
			bHandled= true;
		}
	}
	break;
	case eMkWindowEventType::MouseButtonUp:
	{
		EventBindingSet* bindingSet= getCurrentEventBindingSet();
		if (bindingSet != nullptr && bindingSet->OnMouseButtonReleasedEvent)
		{
			bindingSet->OnMouseButtonReleasedEvent(event.getMouseButton());
			bHandled= true;
		}
	}
	break;
	case eMkWindowEventType::MouseMotion:
	{
		EventBindingSet* bindingSet= getCurrentEventBindingSet();
		if (bindingSet != nullptr && bindingSet->OnMouseMotionEvent)
		{
			bindingSet->OnMouseMotionEvent(event.getMouseMotionXRel(), event.getMouseMotionYRel());
			bHandled= true;
		}
	}
	break;
	default:
		break;
	}

	return bHandled;
}

void InputManager::getMouseScreenPosition(int& outScreenX, int& outScreenY) const
{
	if (m_ownerWindow)
	{
		m_ownerWindow->getMouseScreenPosition(outScreenX, outScreenY);
	}
	else
	{
		outScreenX= 0;
		outScreenY= 0;
	}
}

void InputManager::getMouseDesktopPosition(int& outDesktopX, int& outDesktopY) const
{
	if (m_ownerWindow)
	{
		m_ownerWindow->getMouseDesktopPosition(outDesktopX, outDesktopY);
	}
	else
	{
		outDesktopX= 0;
		outDesktopY= 0;
	}
}

KeyEventBindings* InputManager::getKeyBindings(MkKeySym key, uint16_t modMask)
{
	EventBindingSet* bindingSet= getCurrentEventBindingSet();
	if (bindingSet == nullptr)
		return nullptr;

	auto it= bindingSet->keybindings.find(EventBindingSet::makeKeyBindingKey(key, modMask));
	if (it != bindingSet->keybindings.end())
	{
		return it->second;
	}

	return nullptr;
}

KeyEventBindings* InputManager::fetchOrAddKeyBindings(MkKeySym key, uint16_t modMask)
{
	EventBindingSet* bindingSet= getCurrentEventBindingSet();
	if (bindingSet == nullptr)
	{
		bindingSet= pushEventBindingSet();
	}

	const uint64_t bindingKey= EventBindingSet::makeKeyBindingKey(key, modMask);
	auto it= bindingSet->keybindings.find(bindingKey);
	if (it != bindingSet->keybindings.end())
	{
		return it->second;
	}
	else
	{
		KeyEventBindings* emptyBindings= new KeyEventBindings();

		bindingSet->keybindings.insert({bindingKey, emptyBindings});

		return emptyBindings;
	}
}

EventBindingSet* InputManager::pushEventBindingSet()
{
	EventBindingSet* newEventBindingSet= new EventBindingSet();

	m_eventBindings.push_back(newEventBindingSet);

	return newEventBindingSet;
}

void InputManager::popEventBindingSet()
{
	if (m_eventBindings.size() > 0)
	{
		delete m_eventBindings[m_eventBindings.size() - 1];
		m_eventBindings.pop_back();
	}
}

EventBindingSet* InputManager::getCurrentEventBindingSet()
{
	if (m_eventBindings.size() > 0)
	{
		return m_eventBindings[m_eventBindings.size() - 1];
	}
	else
	{
		return nullptr;
	}
}

// -- KeyEventBindings -----
KeyEventBindings::~KeyEventBindings() { clear(); }

void KeyEventBindings::clear()
{
	OnKeyPressed.Clear();
	OnKeyReleased.Clear();
	OnKeyRepeated.Clear();
}

// -- EventBindingSet -----
EventBindingSet::~EventBindingSet() { clear(); }

void EventBindingSet::clear()
{
	for (auto it= keybindings.begin(); it != keybindings.end(); ++it)
	{
		delete it->second;
	}
	keybindings.clear();

	OnMouseMotionEvent.Clear();
	OnMouseButtonPressedEvent.Clear();
	OnMouseButtonReleasedEvent.Clear();
	OnMouseWheelScrolledEvent.Clear();
}

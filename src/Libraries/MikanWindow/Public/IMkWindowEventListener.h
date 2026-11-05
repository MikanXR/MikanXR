#pragma once

#include <stdint.h>

class MkWindowEvent;

class IMkWindowEventListener
{
public:
	virtual bool onWindowEvent(const MkWindowEvent& event)= 0;

	// Whether an OS window the listener spawned beside its own (a panel torn out
	// of an ImGui multi-viewport context) has this window id, so that window's
	// events route to this listener rather than to nobody
	virtual bool ownsChildWindow(uint32_t windowId) const { return false; }
};

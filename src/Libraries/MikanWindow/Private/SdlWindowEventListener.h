#pragma once

#include "MkWindowEvent.h"

typedef union SDL_Event SDL_Event;

// The window id an SDL event carries, read from the field its type puts it in,
// or 0 for an event type that belongs to no window
uint32_t getSdlEventWindowId(const SDL_Event* event);

class SdlWindowEvent : public MkWindowEvent
{
public:
	SdlWindowEvent(const SDL_Event* event);

	virtual eMkWindowEventType getEventType() const override;
	virtual MkKeySym getKeySym() const override;
	virtual uint16_t getKeyMod() const override;
	virtual int getKeyRepeat() const override;
	virtual const char* getText() const override;
	virtual eMkWindowEventID getWindowEventID() const override;
	virtual uint32_t getWindowId() const override;
	virtual int getMouseWheelScrollAmount() const override;
	virtual int getMouseWheelScrollAmountX() const override;
	virtual int getMouseButton() const override;
	virtual int getMouseClickCount() const override;
	virtual int getMouseX() const override;
	virtual int getMouseY() const override;
	virtual int getMouseMotionXRel() const override;
	virtual int getMouseMotionYRel() const override;
	virtual int getWindowData1() const override;
	virtual int getWindowData2() const override;
	virtual const void* getInternalWindowEvent() const override;

private:
	const SDL_Event* m_event;
};

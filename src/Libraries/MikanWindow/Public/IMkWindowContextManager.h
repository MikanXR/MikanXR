#pragma once

#include "MkWindowExport.h"

#include <cstdint>
#include <memory>
#include <string>

class IMkWindowContext;

class IMkWindowContextManager;
using IMkWindowContextManagerPtr= std::shared_ptr<IMkWindowContextManager>;

class MIKAN_WINDOW_CLASS IMkWindowContextManager
{
public:
	virtual ~IMkWindowContextManager() {}

	virtual bool startup()= 0;
	virtual void shutdown()= 0;
	virtual void pollEvents()= 0;
	// Runs the OS message pump without consuming any event, so the windows stay responsive
	// across a long synchronous step. The events are still there for the next pollEvents.
	virtual void pumpEvents()= 0;

	virtual bool getIsInitialized() const= 0;
	virtual const std::string& getGlslVersion() const= 0;
	// The primary display's content scale (1.0 at 96 DPI). A window is created in
	// physical pixels, so a size authored for 96 DPI is multiplied by this to open
	// at the same apparent size on a high DPI display.
	virtual float getPrimaryDisplayContentScale() const= 0;
	virtual void setMouseCursor(const std::string& cursorName)= 0;
	// A native OS error dialog that blocks until dismissed. Needs no window or graphics
	// context, so it can report a failure that prevented either from starting.
	virtual void showErrorMessageBox(const std::string& title, const std::string& message)= 0;

	// A borderless window showing one image while the app starts. Needs no graphics context,
	// so it can open before the first real window. The pixels are 8-bit RGBA rows, top row
	// first, and are copied. The window is sized to the image at the display's content scale.
	virtual bool openSplashWindow(const std::string& title, const uint8_t* rgbaPixels, int width, int height)= 0;
	virtual void closeSplashWindow()= 0;

	virtual void pushCurrentWindowContext(IMkWindowContext* window)= 0;
	virtual IMkWindowContext* getCurrentWindowContext() const= 0;
	virtual void popCurrentWindowContext(IMkWindowContext* window)= 0;
};

MIKAN_WINDOW_FUNC(IMkWindowContextManagerPtr) createMkWindowContextManager();

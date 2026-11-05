#pragma once

#include "MkWindowExport.h"
#include "MkWindowFwd.h"
#include "MkRendererFwd.h"
#include "MkWindowEvent.h"

#include <string>

enum class eWindowAPI
{
	INVALID= -1,

	SDL= 0,

	COUNT
};

class IMkWindowContext
{
public:
	virtual ~IMkWindowContext() {}

	virtual bool startup()= 0;
	virtual void update(float deltaSeconds)= 0;
	virtual void render()= 0;
	virtual void present()= 0;
	virtual void shutdown()= 0;

	virtual void enableGLDataSharing() {}  // enable GL resource sharing before startup()
	virtual void useExistingGLContext() {} // attach to the currently active GL context instead of creating a new one

	virtual const char* getTitle() const= 0;
	virtual float getWidth() const= 0;
	virtual float getHeight() const= 0;
	virtual float getAspectRatio() const= 0;
	virtual bool getIsRenderingStage() const= 0;

	// The cursor in client pixels of whichever window has mouse focus
	virtual void getMouseScreenPosition(int& outScreenX, int& outScreenY) const= 0;
	// The cursor in desktop pixels, the space ImGui positions are in when its
	// multi-viewports are on
	virtual void getMouseDesktopPosition(int& outDesktopX, int& outDesktopY) const= 0;

	virtual eWindowAPI getWindowAPI() const= 0;
	virtual void* getNativeWindowHandle() const= 0;
	virtual IMkGraphicsContextPtr getGraphicsContext() const= 0;
	virtual IMkViewportPtr getRenderingViewport() const= 0;

	virtual void makeContextCurrent()= 0;
	virtual bool wantsDestroy() const= 0;
	virtual void requestClose() {}

	virtual void setTitle(const std::string& title)= 0;
	virtual void setSize(int width, int height)= 0;

	virtual void handleEvents(class IMkWindowEventListener* eventListener)= 0;
	virtual bool hasMouseFocus() const= 0;
	virtual bool hasKeyboardFocus() const= 0;

	// -- Synthetic input, for the automation channel ----
	// Injected input is posted to the real event queue rather than handed straight to a listener,
	// so it travels the same path a user's input does: the same window routing, the same ImGui
	// capture arbitration, the same listener. That is what makes a driven session evidence about
	// the shipping input path instead of about a test-only shortcut.
	//
	// Mouse position is warped rather than fabricated. ImGui's backend re-reads the OS cursor every
	// frame whenever the pointer is not over one of our windows, and would overwrite a fabricated
	// position before the widget under it ever saw the click. Moving the real cursor is therefore
	// load-bearing, and it is why injection visibly takes the pointer over.
	//
	// A childWindowId targets an OS window the listener owns beside this one (a torn-out ImGui
	// panel): the event is stamped with that window's id, so it reaches the listener through the
	// child-window claim in handleEvents the way a user's input in that window does. Zero means
	// this window. Child positions are that window's client pixels.
	virtual int getWindowId() const= 0;
	virtual void raiseWindow()= 0;
	virtual void raiseChildWindow(uint32_t childWindowId)= 0;
	virtual void warpMouseToWindowPosition(int windowX, int windowY)= 0;
	virtual void warpMouseToChildWindowPosition(uint32_t childWindowId, int windowX, int windowY)= 0;
	virtual void injectMouseButton(int mkMouseButton, bool bPressed, int windowX, int windowY, int clickCount,
								   uint32_t childWindowId)= 0;
	virtual void injectMouseWheel(int windowX, int windowY, int scrollX, int scrollY, uint32_t childWindowId)= 0;
	virtual void injectKey(MkKeySym keySym, uint16_t keyMod, bool bPressed, uint32_t childWindowId)= 0;
	virtual void injectText(const std::string& utf8Text, uint32_t childWindowId)= 0;
};

MIKAN_WINDOW_FUNC(IMkWindowContextPtr) createMkWindowContext(IMkWindowContextManagerPtr ownerWindowManager,
															 IMkGraphicsContextPtr graphicsContext);

#pragma once

#include "IMkWindowContext.h"
#include "SdlFwd.h"

#include <memory>
#include <string>

class IMkWindowContextManager;

class SdlWindowContext : public IMkWindowContext
{
public:
	SdlWindowContext()= delete;
	SdlWindowContext(IMkWindowContextManagerPtr ownerWindowManager, IMkGraphicsContextPtr graphicsContext);
	virtual ~SdlWindowContext();

	// -- IMkWindowContext interface --
	virtual bool startup() override;
	virtual void update(float deltaSeconds) override;
	virtual void render() override;
	virtual void present() override;
	virtual void shutdown() override;

	virtual const char* getTitle() const override { return m_title.c_str(); }
	virtual float getWidth() const override { return (float)m_width; }
	virtual float getHeight() const override { return (float)m_height; }
	virtual float getAspectRatio() const override { return m_height > 0 ? (float)m_width / (float)m_height : 1.f; }
	virtual bool getIsRenderingStage() const override { return m_isRenderingStage; }

	virtual void getMouseScreenPosition(int& outScreenX, int& outScreenY) const override;
	virtual void getMouseDesktopPosition(int& outDesktopX, int& outDesktopY) const override;

	virtual eWindowAPI getWindowAPI() const override { return eWindowAPI::SDL; }
	virtual void* getNativeWindowHandle() const override { return m_sdlWindow; }
	virtual IMkGraphicsContextPtr getGraphicsContext() const override { return m_graphicsContext.lock(); }
	virtual IMkViewportPtr getRenderingViewport() const override;
	virtual void makeContextCurrent() override;
	virtual bool wantsDestroy() const override { return m_wantsDestroy; }
	virtual void requestClose() override;

	// -- IMkWindowContext overrides --
	virtual void setTitle(const std::string& title) override;
	virtual void setSize(int width, int height) override;
	virtual void handleEvents(class IMkWindowEventListener* eventListener) override;
	virtual bool hasMouseFocus() const override { return m_hasMouseFocus; }
	virtual bool hasKeyboardFocus() const override { return m_hasKeyboardFocus; }

	virtual int getWindowId() const override { return m_windowId; }
	virtual void raiseWindow() override;
	virtual void raiseChildWindow(uint32_t childWindowId) override;
	virtual void warpMouseToWindowPosition(int windowX, int windowY) override;
	virtual void warpMouseToChildWindowPosition(uint32_t childWindowId, int windowX, int windowY) override;
	virtual void injectMouseButton(int mkMouseButton, bool bPressed, int windowX, int windowY, int clickCount,
								   uint32_t childWindowId) override;
	virtual void injectMouseWheel(int windowX, int windowY, int scrollX, int scrollY, uint32_t childWindowId) override;
	virtual void injectKey(MkKeySym keySym, uint16_t keyMod, bool bPressed, uint32_t childWindowId) override;
	virtual void injectText(const std::string& utf8Text, uint32_t childWindowId) override;

	// -- SdlWindowContext specific --
	void focus();

	virtual void enableGLDataSharing() override;
	bool isGlDataSharingEnabled() const { return m_bGLDataSharingEnabled; }
	virtual void useExistingGLContext() override;

	bool isMinimized() const { return m_isMinimized; }
	bool isShown() const { return m_isShown; }

protected:
	bool handleSDLWindowEvent(const SDL_Event* event);
	// The id an injected event is stamped with: the child window's, or this window's for zero
	uint32_t resolveInjectedWindowId(uint32_t childWindowId) const;

private:
	IMkWindowContextManagerWeakPtr m_ownerWindowManager;
	IMkGraphicsContextWeakPtr m_graphicsContext;
	SDL_Window* m_sdlWindow= nullptr;
	void* m_glContext= nullptr;
	int m_windowId= -1;
	bool m_isRenderingStage= false;

	bool m_bGLDataSharingEnabled= false;
	bool m_bOwnsGLContext= true; // false when attaching to an existing GL context

	std::string m_title= "Mikan Window";

	int m_width= 0;
	int m_height= 0;

	bool m_hasMouseFocus= false;
	bool m_hasKeyboardFocus= false;
	bool m_isfullScreen= false;
	bool m_isMinimized= false;
	bool m_isShown= false;
	bool m_wantsDestroy= false;
};
using SdlWindowUniquePtr= std::unique_ptr<SdlWindowContext>;

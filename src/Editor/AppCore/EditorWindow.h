#pragma once

//-- includes -----
#include "IMkWindowContext.h"
#include "IEditorWindow.h"
#include "IMkWindowEventListener.h"
#include "MkGuiFwd.h"
#include "MikanRendererFwd.h"
#include "MkWindowFwd.h"

#include <memory>
#include <string>

//-- definitions -----
class EditorWindow : public IEditorWindow, public IMkWindowEventListener
{
public:
	EditorWindow(class App* ownerApp);
	virtual ~EditorWindow();

	// -- IEditorWindow ----
	// virtual bool startup() = 0;
	// virtual void update(float deltaSeconds) = 0;
	// virtual void render() = 0;
	// virtual void shutdown() = 0;

	// virtual bool getIsRenderingStage() const = 0;
	// virtual IMkViewportPtr getRenderingViewport() const = 0;
	virtual const char* getTitle() const override;
	virtual float getWidth() const override;
	virtual float getHeight() const override;
	virtual float getAspectRatio() const override;
	virtual void getMouseScreenPosition(int& outScreenX, int& outScreenY) const override;
	virtual void getMouseDesktopPosition(int& outDesktopX, int& outDesktopY) const override;

	// -- IMkWindowEventListener ----
	// The OS windows this window's panels are torn out into
	virtual bool ownsChildWindow(uint32_t windowId) const override;

	class MkGuiContext* getGuiContext() const { return m_guiContext.get(); }

	virtual MikanModelResourceManager* getModelResourceManager() override;
	virtual MikanTextureCache* getTextureCache() override;
	virtual class MkGuiStyleManager* getMkGuiStyleManager() const override;

	virtual IMkGraphicsContextPtr getGraphicsContext() const override;
	virtual IMkWindowContextPtr getMkWindowContext() const override;
	virtual class App* getOwnerApp() const override;

	// Default implementations delegate to the MainWindow, which is the only
	// window type that actually owns these services. MainWindow overrides
	// each of these to return its own members directly.
	virtual class MainWindow* getMainWindow() const;
	virtual ProjectManagerPtr getProjectManager() const override;
	virtual class ProjectAssetCatalog* getAssetCatalog() const override;
	virtual class MikanServer* getMikanServer() const override;
	virtual class IMkFontManager* getFontManager() const override;
	virtual class InputManager* getInputManager() const override;
	virtual class OpenCVManager* getOpenCVManager() const override;
	virtual class ClientSourceManager* getClientSourceManager() const override;
	virtual class LocalizationManager* getLocalizationManager() const override;
	virtual class EventBus* getEventBus() const override;
	virtual class AppStage* getCurrentAppStage() const override;
	virtual class AppStage* getParentAppStage() const override;
	virtual class AppStage* pushAppStage(const std::string& appStageName) override;
	virtual void popAppState() override;

	// -- IMkWindowContext Helpers ----
	eWindowAPI getWindowAPI() const;
	void* getNativeWindowHandle() const;
	void makeContextCurrent();
	virtual bool wantsDestroy() const;
	void present();
	// Captures this window for a pending automation screenshot, then presents. Windows call this at
	// the end of render() instead of present() so any of them can be captured, not just the main one.
	void presentFrame();
	void setTitle(const std::string& title);
	void setSize(int width, int height);
	void handleEvents(class IMkWindowEventListener* eventListener);
	bool hasMouseFocus() const;
	bool hasKeyboardFocus() const;

protected:
	// Shares the MainWindow's GL context instead of creating a new one, so scene resources
	// (VAOs, FBOs, textures) allocated on the MainWindow remain accessible. Used by satellite
	// windows (e.g. NodeEditorWindow); MainWindow does not call this.
	void shareGraphicsContextWithMainWindow();

	bool startupWindow(const std::string& title, int width, int height);
	bool startupGuiContext(const std::string& iniName, bool bEnableDocking= false, bool bEnableViewports= false);
	bool startupStyleManager();
	bool startupTextureCache();
	bool startupModelResourceManager();

	void shutdownModelResourceManager();
	void shutdownTextureCache();
	void shutdownStyleManager();
	void shutdownGuiContext();
	void shutdownWindow();

	class App* m_ownerApp= nullptr;
	IMkWindowContextPtr m_mkWindowContext;
	IMkGraphicsContextPtr m_graphicsContext;
	MikanTextureCacheUniquePtr m_textureCache;
	MkGuiContextPtr m_guiContext;
	std::unique_ptr<class MkGuiStyleManager> m_styleManager;
	MikanModelResourceManagerUniquePtr m_modelResourceManager;
};

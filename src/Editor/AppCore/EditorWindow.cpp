//-- includes -----
#include "EditorWindow.h"
#include "Logger.h"

#include "App.h"
#include "AutomationServer.h"
#include "IMkWindowContext.h"
#include "IMkWindowContextManager.h"
#include "IMkGraphicsContext.h"
#include "MainWindow.h"
#include "MikanModelResourceManager.h"
#include "MikanShaderCache.h"
#include "MikanTextureCache.h"
#include "MkGuiContext.h"
#include "MkGuiStyleManager.h"
#include "PathUtils.h"

#include <easy/profiler.h>

//-- public methods -----
EditorWindow::EditorWindow(App* ownerApp)
	: m_ownerApp(ownerApp)
{
}

EditorWindow::~EditorWindow() {}

// -- IEditorWindow ----
IMkGraphicsContextPtr EditorWindow::getGraphicsContext() const { return m_graphicsContext; }

IMkWindowContextPtr EditorWindow::getMkWindowContext() const { return m_mkWindowContext; }

const char* EditorWindow::getTitle() const { return m_mkWindowContext->getTitle(); }

float EditorWindow::getWidth() const { return m_mkWindowContext->getWidth(); }

float EditorWindow::getHeight() const { return m_mkWindowContext->getHeight(); }

float EditorWindow::getAspectRatio() const { return m_mkWindowContext->getAspectRatio(); }

void EditorWindow::getMouseScreenPosition(int& outScreenX, int& outScreenY) const
{
	m_mkWindowContext->getMouseScreenPosition(outScreenX, outScreenY);
}

MikanModelResourceManager* EditorWindow::getModelResourceManager() { return m_modelResourceManager.get(); }

MikanTextureCache* EditorWindow::getTextureCache() { return m_textureCache.get(); }

MkGuiStyleManager* EditorWindow::getMkGuiStyleManager() const { return m_styleManager.get(); }

App* EditorWindow::getOwnerApp() const { return m_ownerApp; }

// -- MainWindow service delegation ----
// Default implementations delegate to the MainWindow. MainWindow overrides all of these
// to return its own members directly, so they never recurse through this delegation.
MainWindow* EditorWindow::getMainWindow() const { return m_ownerApp->getMainWindow(); }

ProjectManagerPtr EditorWindow::getProjectManager() const { return getMainWindow()->getProjectManager(); }

ProjectAssetCatalog* EditorWindow::getAssetCatalog() const { return getMainWindow()->getAssetCatalog(); }

MikanServer* EditorWindow::getMikanServer() const { return getMainWindow()->getMikanServer(); }

IMkFontManager* EditorWindow::getFontManager() const { return getMainWindow()->getFontManager(); }

InputManager* EditorWindow::getInputManager() const { return getMainWindow()->getInputManager(); }

OpenCVManager* EditorWindow::getOpenCVManager() const { return getMainWindow()->getOpenCVManager(); }

ClientSourceManager* EditorWindow::getClientSourceManager() const { return getMainWindow()->getClientSourceManager(); }

LocalizationManager* EditorWindow::getLocalizationManager() const { return getMainWindow()->getLocalizationManager(); }

EventBus* EditorWindow::getEventBus() const { return getMainWindow()->getEventBus(); }

AppStage* EditorWindow::getCurrentAppStage() const { return getMainWindow()->getCurrentAppStage(); }

AppStage* EditorWindow::getParentAppStage() const { return getMainWindow()->getParentAppStage(); }

AppStage* EditorWindow::pushAppStage(const std::string& appStageName)
{
	return getMainWindow()->pushAppStage(appStageName);
}

void EditorWindow::popAppState() { getMainWindow()->popAppState(); }

// -- IMkWindowContext delegation ----
eWindowAPI EditorWindow::getWindowAPI() const { return m_mkWindowContext->getWindowAPI(); }

void* EditorWindow::getNativeWindowHandle() const { return m_mkWindowContext->getNativeWindowHandle(); }

void EditorWindow::makeContextCurrent() { m_mkWindowContext->makeContextCurrent(); }

bool EditorWindow::wantsDestroy() const { return m_mkWindowContext->wantsDestroy(); }

void EditorWindow::present() { m_mkWindowContext->present(); }

void EditorWindow::presentFrame()
{
	// The back buffer is only readable between the last draw and the swap, so an automation capture
	// of this window has to land here rather than anywhere in the window's own render body.
	MainWindow* mainWindow= getMainWindow();
	if (mainWindow && mainWindow->getAutomationServer())
	{
		mainWindow->getAutomationServer()->servicePendingWindowCapture(this, (int)m_mkWindowContext->getWidth(),
																	   (int)m_mkWindowContext->getHeight());
	}

	m_mkWindowContext->present();
}

void EditorWindow::setTitle(const std::string& title) { m_mkWindowContext->setTitle(title); }

void EditorWindow::setSize(int width, int height) { m_mkWindowContext->setSize(width, height); }

void EditorWindow::handleEvents(IMkWindowEventListener* eventListener)
{
	m_mkWindowContext->handleEvents(eventListener);
}

bool EditorWindow::hasMouseFocus() const { return m_mkWindowContext->hasMouseFocus(); }

bool EditorWindow::hasKeyboardFocus() const { return m_mkWindowContext->hasKeyboardFocus(); }

// -- Protected startup/shutdown helpers ----

void EditorWindow::shareGraphicsContextWithMainWindow()
{
	m_graphicsContext= getMainWindow()->getGraphicsContext();
	m_mkWindowContext= createMkWindowContext(m_ownerApp->getWindowManager(), m_graphicsContext);
	m_mkWindowContext->useExistingGLContext(); // attach to main window's context, don't create a new one
	m_modelResourceManager= MikanModelResourceManagerUniquePtr(new MikanModelResourceManager(m_graphicsContext.get()));
}

bool EditorWindow::startupWindow(const std::string& title, int width, int height)
{
	EASY_FUNCTION();

	// The size each window asks for is authored against a 96 DPI display, and SDL
	// creates windows in physical pixels, so it takes the display's content scale
	// the same way the UI inside the window does
	const float displayScale= m_ownerApp->getWindowManager()->getPrimaryDisplayContentScale();

	m_mkWindowContext->setTitle(title);
	m_mkWindowContext->setSize((int)(width * displayScale), (int)(height * displayScale));
	if (!m_mkWindowContext->startup())
	{
		MIKAN_LOG_ERROR("EditorWindow::startupWindow") << "Unable to initialize window: " << title;
		return false;
	}

	return true;
}

bool EditorWindow::startupGuiContext(const std::string& iniName, bool bEnableDocking)
{
	EASY_FUNCTION();

	// Each window persists its ImGui layout to its own ini beside the app
	// config, rather than every context fighting over one imgui.ini in the
	// working directory
	const std::filesystem::path iniDir= PathUtils::getHomeDirectory() / "Mikan";
	std::filesystem::create_directories(iniDir);
	const std::filesystem::path iniPath= iniDir / ("imgui_" + iniName + ".ini");

	m_guiContext= std::make_shared<MkGuiContext>(getMkWindowContext().get(), iniPath.string(), bEnableDocking);
	if (!m_guiContext->startup())
	{
		MIKAN_LOG_ERROR("EditorWindow::startupGuiContext") << "Unable to create GUI context";
		return false;
	}
	return true;
}

bool EditorWindow::startupStyleManager()
{
	EASY_FUNCTION();

	m_styleManager= std::make_unique<MkGuiStyleManager>();
	const auto stylesPath= PathUtils::getResourceDirectory() / "gui_styles";
	if (!m_styleManager->startup(m_guiContext.get(), stylesPath))
	{
		MIKAN_LOG_ERROR("EditorWindow::startupStyleManager") << "Failed to initialize style manager";
		return false;
	}
	return true;
}

bool EditorWindow::startupTextureCache()
{
	EASY_FUNCTION();

	m_textureCache= std::make_unique<MikanTextureCache>(getMkWindowContext()->getGraphicsContext().get());

	if (!m_textureCache->startup())
	{
		MIKAN_LOG_ERROR("EditorWindow::startupModelResourceManager") << "Unable to initialize texture cache";
		return false;
	}
	return true;
}

bool EditorWindow::startupModelResourceManager()
{
	EASY_FUNCTION();

	// Material default textures resolve through this window's texture cache
	if (m_textureCache)
	{
		m_modelResourceManager->getShaderCache()->setTextureCache(m_textureCache.get());
	}

	if (!m_modelResourceManager->startup())
	{
		MIKAN_LOG_ERROR("EditorWindow::startupModelResourceManager") << "Unable to initialize model resource manager";
		return false;
	}
	return true;
}

void EditorWindow::shutdownTextureCache()
{
	if (m_textureCache != nullptr)
	{
		m_textureCache->shutdown();
		m_textureCache= nullptr;
	}
}

void EditorWindow::shutdownModelResourceManager()
{
	if (m_modelResourceManager != nullptr)
	{
		m_modelResourceManager->shutdown();
		m_modelResourceManager= nullptr;
	}
}

void EditorWindow::shutdownStyleManager()
{
	if (m_styleManager != nullptr)
	{
		m_styleManager->shutdown();
		m_styleManager= nullptr;
	}
}

void EditorWindow::shutdownGuiContext()
{
	if (m_guiContext != nullptr)
	{
		m_guiContext->shutdown();
		m_guiContext= nullptr;
	}
}

void EditorWindow::shutdownWindow()
{
	if (m_mkWindowContext != nullptr)
	{
		m_mkWindowContext->shutdown();
		m_mkWindowContext= nullptr;
	}
}

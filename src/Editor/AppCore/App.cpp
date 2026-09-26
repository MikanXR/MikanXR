//-- includes -----
#include "App.h"
#include "AppSettingsConfig.h"
#include "AutomationLogBuffer.h"
#include "CommonConfig.h"
#include "CrashHandler.h"
#include "EventBus.h"
#include "FrameTimer.h"
#include "Graphs/CompositorNodeGraph.h"
#include "Graphs/MaterialNodeGraph.h"
#include "Graphs/ShapeNodeGraph.h"
#include "IEditorWindow.h"
#include "IMkGraphicsContext.h"
#include "IMkWindowContext.h"
#include "MkError.h"
#include "MkGuiContext.h"
#include "MkStateStack.h"
#include "LocalizationManager.h"
#include "Logger.h"
#include "MainWindow.h"
#include "MikanModuleManager.h"
#include "PathUtils.h"
#include "ProjectConfig.h"
#include "IMkWindowContextManager.h"
#include "SpoutLogRelay.h"
#include "TypeRegistry.h"
#include "Version.h"

// #include "Windows/TestNodeEditorWindow.h"
#include "Windows/CompositorNodeEditorWindow.h"

#include <easy/profiler.h>

#include <chrono>
#include <thread>

#ifdef _WIN32
#include "Objbase.h"
#endif //_WIN32

#include "MikanCefApp.h"

#define SETTINGS_SAVE_COOLDOWN 3.f

//-- static members -----
App* App::m_instance= nullptr;

//-- App -----
App::App()
	: m_appSettings(std::make_shared<AppSettingsConfig>())
	, m_eventBus(std::make_unique<EventBus>())
	, m_localizationManager(new LocalizationManager())
	, m_windowManager(createMkWindowContextManager())
	, m_spoutLogRelay(std::make_unique<SpoutLogRelay>())
	, m_bShutdownRequested(false)
{
	m_instance= this;
}

App::~App()
{
	m_mainWindow= nullptr;

	m_windowManager.reset();
	delete m_localizationManager;

	m_appSettings.reset();

	// Clear the global event bus reference
	m_eventBus.reset();

	m_instance= nullptr;
}

int App::exec(int argc, char** argv)
{
	int result= 0;

	if (startup(argc, argv))
	{
		while (!m_bShutdownRequested && m_mainWindow != nullptr)
		{
			FrameTimer frameTimer(11); // 11ms = 90fps

			tick();

			frameTimer.waitForNextFrame();
		}
	}
	else
	{
		MIKAN_LOG_ERROR("App::exec") << "Failed to initialize application!";
		result= -1;
	}

	shutdown();

	return result;
}

IEditorWindow* App::getCurrentlyRenderingWindow() const { return m_renderingWindow; }

bool App::hasCommandLineFlag(const std::string& flag) const { return m_commandLineFlags.count(flag) > 0; }

std::string App::getCommandLineStringArg(const std::string& key, const std::string& defaultValue) const
{
	auto it= m_commandLineParams.find(key);
	return it != m_commandLineParams.end() ? it->second : defaultValue;
}

//-- private methods -----
bool App::startup(int argc, char** argv)
{
	bool success= true;

	// Parse command line arguments
	for (int i= 1; i < argc; ++i)
	{
		std::string arg= argv[i];
		if (arg.size() > 1 && arg[0] == '-')
		{
			std::string key= arg.substr(1); // strip leading '-'
			auto eqPos= key.find('=');
			if (eqPos != std::string::npos)
				m_commandLineParams[key.substr(0, eqPos)]= key.substr(eqPos + 1);
			else
				m_commandLineFlags.insert(key);
		}
	}

	// The log and the crash reports live in the user's MikanXR folder: an
	// installed build runs from a directory it cannot write to
	const std::filesystem::path userDataDirectory= PathUtils::getProjectsRootDirectory();

	LoggerSettings settings= {};
	settings.min_log_level= LogSeverityLevel::debug;
	// No console window: the editor shows the log in its own panel, and the
	// same lines still go to MikanXR.log
	settings.enable_console= false;
	settings.log_filename= (userDataDirectory / "MikanXR.log").string();
	// Feeds both the automation server's log command and the editor's log panel
	settings.log_callback= AutomationLogBuffer::logCallback;

	log_init(settings);

	m_crashReportDirectory= userDataDirectory / "CrashReports";
	CrashHandlerSettings crashSettings= {};
	crashSettings.reportDirectory= m_crashReportDirectory;
	crashSettings.logFilePath= settings.log_filename;
	crashSettings.appName= "Mikan";
	crashSettings.appVersion= MIKAN_RELEASE_VERSION_STRING;
	CrashHandler::install(crashSettings);

	profiler::startListen();

	if (hasCommandLineFlag("waitForProfiler"))
	{
		MIKAN_LOG_INFO("App::init") << "Waiting for profiler client to connect...";
		while (!profiler::isCapturing())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		MIKAN_LOG_INFO("App::init") << "Profiler client connected, continuing startup.";
	}

#ifdef _WIN32
	HRESULT hr= CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (!SUCCEEDED(hr))
	{
		MIKAN_LOG_ERROR("App::init") << "Could not initialize COM";
		success= false;
	}
#endif // _WIN32

	// Initialize registry of reflection types
	Serialization::TypeRegistry::build();

	// Initialize the module manager
	if (success && !initMikanModuleManager())
	{
		MIKAN_LOG_ERROR("App::init") << "Failed to initialize module manager!";
		success= false;
	}

	// Load any saved app settings config
	if (success && !m_appSettings->load())
	{
		MIKAN_LOG_INFO("App::init") << "Failed to load app settings config. Creating new settings.";
	}

	// Enable auto-save on a cooldown when settings are changed
	m_appSettings->setAutoSaveCooldownDuration(SETTINGS_SAVE_COOLDOWN);

	// The user's scale preference, in place before any window builds its GUI context
	MkGuiContext::setUserUiScale(m_appSettings->getUiScale());

	// Configure Spout's own logging before anything opens a Spout sender or receiver
	m_spoutLogRelay->setEnabled(m_appSettings->getSpoutLogEnabled());

	const std::filesystem::path localizationDir= PathUtils::getResourceDirectory() / "localization";
	if (success && !m_localizationManager->startup(localizationDir, m_appSettings))
	{
		MIKAN_LOG_ERROR("App::init") << "Failed to initialize localization manager!";
		success= false;
	}

	if (success && !m_windowManager->startup())
	{
		MIKAN_LOG_ERROR("App::init") << "Failed to initialize window manager!";
		success= false;
	}

	if (success)
	{
		CefSettings settings;
		settings.windowless_rendering_enabled= true;
		settings.multi_threaded_message_loop= false;
		settings.no_sandbox= false;

#ifdef WIN32
		CefMainArgs main_args(GetModuleHandle(nullptr));
#else
		CefMainArgs main_args(0, nullptr);
#endif

		CefRefPtr<MikanCefApp> cef_app= new MikanCefApp();
		if (!CefInitialize(main_args, settings, cef_app, nullptr))
		{
			MIKAN_LOG_ERROR("CEFTextureSourceSystem") << "CefInitialize failed";
			return false;
		}

		// Chromium installs its own crash hooks during initialization
		CrashHandler::ensureInstalled();
	}

	if (success)
	{
		// Register node graph factories spawned by windows
		NodeGraphFactory::registerFactory<CompositorNodeGraphFactory>();
		NodeGraphFactory::registerFactory<ShapeNodeGraphFactory>();
		NodeGraphFactory::registerFactory<MaterialNodeGraphFactory>();

		// Create the main window
		m_mainWindow= createAppWindow<MainWindow>();
		if (m_mainWindow == nullptr)
		{
			MIKAN_LOG_ERROR("App::init") << "Failed to initialize Main App Window!";
			success= false;
		}
	}

	if (success)
	{
		m_appStartTimestamp= std::chrono::steady_clock::now();
		m_lastFrameTimestamp= m_appStartTimestamp;
	}

	return success;
}

void App::shutdown()
{
	// Drain any remaining Spout logs and close the relay's log file
	m_spoutLogRelay->setEnabled(false);

	// Tear down Chromium Embedded Framework
	CefShutdown();

	// Dispose all app windows in reverse order (destroying main window last)
	while (m_appWindows.size() > 0)
	{
		destroyAppWindow(m_appWindows[m_appWindows.size() - 1]);
	}
	m_appWindows.clear();

	assert(m_windowManager != nullptr);
	m_windowManager->shutdown();

	assert(m_localizationManager != nullptr);
	m_localizationManager->shutdown();

	// Shutdown the module manager
	shutdownMikanModuleManager();

#ifdef _WIN32
	CoUninitialize();
#endif // _WIN32

	CrashHandler::uninstall();
}

double App::getSecondsSinceAppStart() const
{
	const auto now= std::chrono::steady_clock::now();
	const double secondsSinceStart= std::chrono::duration<double>(now - m_appStartTimestamp).count();

	return secondsSinceStart;
}

void App::tick()
{
	EASY_FUNCTION();

	// Update the frame rate
	const auto now= std::chrono::steady_clock::now();
	const float deltaSeconds= fminf(std::chrono::duration<float>(now - m_lastFrameTimestamp).count(), 0.1f);
	m_fps= deltaSeconds > 0.f ? (1.0f / deltaSeconds) : 0.f;
	m_lastFrameTimestamp= now;

	// Pump the CEF message loop so browser callbacks (including OnPaint) fire
	CefDoMessageLoopWork();

	// Refresh the latest events from SDL
	// Each window will process the events it cares about
	m_windowManager->pollEvents();

	// Tick the sim and then render each window
	tickWindows(deltaSeconds);

	// Forward anything Spout logged this frame into the editor log
	m_spoutLogRelay->update();

	// Update app settings auto-save
	m_appSettings->updateAutoSave(deltaSeconds);
}

void App::tickWindows(const float deltaSeconds)
{
	EASY_FUNCTION();

	assert(m_windowManager->getCurrentWindowContext() == nullptr);

	// Update each window
	// Capture size before the loop — new windows added during update() (e.g. from UI events)
	// will be ticked starting next frame, avoiding iterator invalidation on m_appWindows.
	static bool bDebugPrintStack= false;
	const int windowCount= (int)m_appWindows.size();
	for (int windowIndex= 0; windowIndex < windowCount; ++windowIndex)
	{
		EditorWindow* appWindow= m_appWindows[windowIndex];
		IMkWindowContext* appWindowContext= appWindow->getMkWindowContext().get();

		// Skip any windows that have been marked for destruction
		if (appWindow->wantsDestroy())
		{
			continue;
		}

		// Mark this window as the current window getting updated
		m_windowManager->pushCurrentWindowContext(appWindowContext);

		// Process window simulation based on time
		{
			EASY_BLOCK("UpdateWindow");
			appWindow->update(deltaSeconds);
		}

		// Render the window
		{
			EASY_BLOCK("RenderWindow");

			MkStateStack& mkStateStack= appWindow->getGraphicsContext()->getMkStateStack();
			mkStateStack.setDebugPrintEnabled(bDebugPrintStack);

			m_renderingWindow= appWindow;
			appWindow->render();
			m_renderingWindow= nullptr;

			mkStateStack.setDebugPrintEnabled(false);
		}

		// Restore back to the main window
		m_windowManager->popCurrentWindowContext(appWindowContext);
	}
	bDebugPrintStack= false;

	// Destroy any windows that have been marked for destruction
	for (int windowIndex= (int)m_appWindows.size() - 1; windowIndex >= 0; windowIndex--)
	{
		EditorWindow* window= m_appWindows[windowIndex];

		if (window->wantsDestroy())
		{
			if (window == m_mainWindow)
			{
				// Special case for main window
				requestShutdown();
			}
			else
			{
				// remove the window from the window list
				destroyAppWindow(window);
			}
		}
	}
}

bool App::createAppWindowInternal(EditorWindow* appWindow)
{
	// Destroy the window if it fails to initialize properly
	if (!appWindow->startup())
	{
		destroyAppWindow(appWindow);
		return false;
	}

	// pop this window context this window added if it created one
	// and return back to the previous window context
	IMkWindowContext* appWindowContext= appWindow->getMkWindowContext().get();
	if (m_windowManager->getCurrentWindowContext() == appWindowContext)
	{
		m_windowManager->popCurrentWindowContext(appWindowContext);
	}

	// Add the window to the list of windows
	m_appWindows.push_back(appWindow);

	return true;
}

void App::destroyAppWindow(EditorWindow* appWindow)
{
	// If this window was the current window, pop it from the current window stack
	IMkWindowContext* appWindowContext= appWindow->getMkWindowContext().get();
	if (m_windowManager->getCurrentWindowContext() == appWindowContext)
	{
		m_windowManager->popCurrentWindowContext(appWindowContext);
	}

	// Teardown does GL work (ImGui backend, texture cache, model resources),
	// and destroying an earlier window can leave no GL context current
	appWindow->makeContextCurrent();

	// Tear down the window and graphics context it owns
	appWindow->shutdown();

	// Remove the window from the list of windows (should deallocate it)
	auto it= std::find(m_appWindows.begin(), m_appWindows.end(), appWindow);
	if (it != m_appWindows.end())
	{
		m_appWindows.erase(it);
	}

	// If this was the main window pointer, make sure to invalidate that pointer
	if (m_mainWindow == appWindow)
	{
		m_mainWindow= nullptr;
	}

	delete appWindow;
}
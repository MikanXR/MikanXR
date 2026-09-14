#pragma once

//-- includes -----
#include "AppStage.h"
#include "IMkWindowContext.h"
#include "IMkWindowContextManager.h"
#include "IEditorWindow.h"
#include "ObjectSystemConfigFwd.h"

#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <assert.h>
#include <stdint.h>

// Forward declarations
class EditorWindow;
class EventBus;
class LocalizationManager;
class MainWindow;
class SpoutLogRelay;

//-- definitions -----
class App
{
public:
	App();
	virtual ~App();

	static App* getInstance() { return m_instance; }

	inline AppSettingsConfigPtr getAppSettings() const { return m_appSettings; }
	inline class MainWindow* getMainWindow() const { return m_mainWindow; }
	inline IMkWindowContextManagerPtr getWindowManager() const { return m_windowManager; }
	IEditorWindow* getCurrentlyRenderingWindow() const;
	inline EventBus* getEventBus() const { return m_eventBus.get(); }
	inline class LocalizationManager* getLocalizationManager() const { return m_localizationManager; }
	inline SpoutLogRelay* getSpoutLogRelay() const { return m_spoutLogRelay.get(); }
	inline const std::filesystem::path& getCrashReportDirectory() const { return m_crashReportDirectory; }

	double getSecondsSinceAppStart() const;
	inline float getFPS() const { return m_fps; }

	bool hasCommandLineFlag(const std::string& flag) const;
	std::string getCommandLineStringArg(const std::string& key, const std::string& defaultValue= "") const;

	int exec(int argc, char** argv);

	inline void requestShutdown() { m_bShutdownRequested= true; }

	template <typename t_app_window>
	t_app_window* createAppWindow()
	{
		t_app_window* appWindow= new t_app_window(this);

		// Setup the window
		// Destroy the window if setup fails
		if (!createAppWindowInternal(appWindow))
		{
			return nullptr;
		}

		return appWindow;
	}

	void destroyAppWindow(EditorWindow* appWindow);

	// Open windows in creation order, the main window first. The automation channel indexes into
	// this to name a window for a screenshot or an injected input event.
	const std::vector<EditorWindow*>& getAppWindows() const { return m_appWindows; }

	template <typename t_app_window>
	bool hasWindowOfType() const
	{
		for (EditorWindow* window : m_appWindows)
		{
			if (dynamic_cast<t_app_window*>(window) != nullptr)
			{
				return true;
			}
		}

		return false;
	}

	template <typename t_app_window>
	t_app_window* getWindowOfType() const
	{
		for (EditorWindow* window : m_appWindows)
		{
			t_app_window* typedWindow= dynamic_cast<t_app_window*>(window);
			if (typedWindow != nullptr)
				return typedWindow;
		}

		return nullptr;
	}

	// For window types that can have more than one instance open at a time, each bound to a
	// different subject (e.g. one browser window per CEF texture source).
	template <typename t_app_window>
	std::vector<t_app_window*> getWindowsOfType() const
	{
		std::vector<t_app_window*> typedWindows;

		for (EditorWindow* window : m_appWindows)
		{
			t_app_window* typedWindow= dynamic_cast<t_app_window*>(window);
			if (typedWindow != nullptr)
				typedWindows.push_back(typedWindow);
		}

		return typedWindows;
	}

protected:
	bool startup(int argc, char** argv);
	void shutdown();

	void tick();
	void tickWindows(const float deltaSeconds);

	bool createAppWindowInternal(EditorWindow* appWindow);

private:
	static App* m_instance;

	// App Settings Config
	AppSettingsConfigPtr m_appSettings;

	// Global event bus for property changes and other events
	std::unique_ptr<EventBus> m_eventBus;

	// Localization manager
	LocalizationManager* m_localizationManager= nullptr;

	// Window Context Manager
	IMkWindowContextManagerPtr m_windowManager;

	// Forwards Spout's own logs into the editor log
	std::unique_ptr<SpoutLogRelay> m_spoutLogRelay;

	// Open windows (including the MainWindow)
	std::vector<EditorWindow*> m_appWindows;

	// The window being currently rendered
	EditorWindow* m_renderingWindow= nullptr;

	// The main window for the application
	MainWindow* m_mainWindow= nullptr;

	// Flag requesting that we exit the update loop
	bool m_bShutdownRequested= false;

	// Where the crash handler writes its reports
	std::filesystem::path m_crashReportDirectory;

	// Command line arguments parsed at startup
	std::map<std::string, std::string> m_commandLineParams;
	std::set<std::string> m_commandLineFlags;

	// Current FPS rate
	std::chrono::steady_clock::time_point m_appStartTimestamp;
	std::chrono::steady_clock::time_point m_lastFrameTimestamp;
	float m_fps= 0.f;
};
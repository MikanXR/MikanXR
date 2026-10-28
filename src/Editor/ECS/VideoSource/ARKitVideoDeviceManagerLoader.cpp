#include "ARKitVideoDeviceManagerLoader.h"
#include "GStreamerUtils.h"
#include "IARKitVideoDeviceManager.h"
#include "IARKitVideoDeviceModule.h"
#include "Logger.h"
#include "MikanModuleManager.h"

#include <assert.h>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <objbase.h>
#endif //_WIN32

#define ARKIT_VIDEO_DEVICE_MODULE_NAME "MikanARKitVideo"

ARKitVideoDeviceManagerLoader::ARKitVideoDeviceManagerLoader() {}

ARKitVideoDeviceManagerLoader::~ARKitVideoDeviceManagerLoader() { dispose(); }

void ARKitVideoDeviceManagerLoader::update(float deltaTime)
{
	// Lazy-launch async init on first update (after all startup work has completed)
	ensureARKitDeviceManager();

	// Poll for async manager init completion
	if (m_arkitVideoManagerState == eARKitVideoManagerState::initializing)
	{
		if (m_arkitVideoManagerFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			auto result= m_arkitVideoManagerFuture.get();
			if (result.manager)
			{
				m_arkitVideoDeviceModule= result.module;
				m_arkitVideoDeviceManager= result.manager;
				m_arkitVideoManagerState= eARKitVideoManagerState::ready;
				MIKAN_LOG_INFO("ARKitVideoDeviceManagerLoader::update") << "ARKit video device manager is ready";

				// Notify anything waiting to retry a deferred openVideoSource()
				// (see class comment in the header for why this is a callback list
				// rather than a component-map walk).
				std::vector<std::function<void()>> callbacks= std::move(m_pendingManagerReadyCallbacks);
				m_pendingManagerReadyCallbacks.clear();
				for (auto& callback : callbacks)
					callback();
			}
			else
			{
				m_arkitVideoManagerState= eARKitVideoManagerState::failed;
				MIKAN_LOG_ERROR("ARKitVideoDeviceManagerLoader::update")
					<< "Async ARKit video device manager init failed";
			}
		}
	}

	if (m_arkitVideoDeviceManager)
	{
		m_arkitVideoDeviceManager->update(deltaTime);
	}
}

void ARKitVideoDeviceManagerLoader::dispose()
{
	// If async init is still in-flight, block until it completes so we can
	// safely clean up whatever it allocated
	if (m_arkitVideoManagerFuture.valid())
	{
		auto result= m_arkitVideoManagerFuture.get();
		m_arkitVideoDeviceModule= result.module;
		m_arkitVideoDeviceManager= result.manager;
	}

	disposeARKitVideoDeviceManager();
}

void ARKitVideoDeviceManagerLoader::addManagerReadyCallback(std::function<void()> callback)
{
	if (m_arkitVideoManagerState == eARKitVideoManagerState::ready)
	{
		callback();
	}
	else
	{
		m_pendingManagerReadyCallbacks.push_back(std::move(callback));
	}
}

bool ARKitVideoDeviceManagerLoader::ensureARKitDeviceManager()
{
	switch (m_arkitVideoManagerState)
	{
	case eARKitVideoManagerState::ready:
		return true;
	case eARKitVideoManagerState::initializing:
		return false;
	case eARKitVideoManagerState::failed:
		return false;
	default:
		break;
	}

	const std::string moduleName= ARKIT_VIDEO_DEVICE_MODULE_NAME;

	// Cheap check on the main thread before launching the async task: the plugin
	// uses GStreamer internally for its RTP video receive path (see Track C), so
	// gate on the same precondition NetworkVideoSourceSystem checks for its own
	// GStreamer-backed module, which also makes the GStreamer DLLs loadable.
	std::filesystem::path runtimeDir;
	if (!GStreamerUtils::ensureRuntimeLoadable(runtimeDir))
	{
		MIKAN_LOG_WARNING("ARKitVideoDeviceManagerLoader::ensureARKitDeviceManager")
			<< "GStreamer not installed. Skipping ARKit video manager init.";
		// Not a failure of this loader - just no manager available
		m_arkitVideoManagerState= eARKitVideoManagerState::failed;
		return false;
	}

	MIKAN_LOG_INFO("ARKitVideoDeviceManagerLoader::ensureARKitDeviceManager")
		<< "Using GStreamer runtime at " << runtimeDir.string();

	// Launch async init to avoid blocking the main thread
	MIKAN_LOG_INFO("ARKitVideoDeviceManagerLoader::ensureARKitDeviceManager")
		<< "Launching async init for ARKit video device module " << moduleName;
	m_arkitVideoManagerState= eARKitVideoManagerState::initializing;
	auto promise= std::make_shared<std::promise<ARKitVideoDeviceManagerInitResult>>();
	m_arkitVideoManagerFuture= promise->get_future();
	std::thread([promise, moduleName]() mutable
				{ promise->set_value(initARKitVideoDeviceManagerOnThread(moduleName)); })
		.detach();

	return false;
}

ARKitVideoDeviceManagerLoader::ARKitVideoDeviceManagerInitResult ARKitVideoDeviceManagerLoader::
	initARKitVideoDeviceManagerOnThread(const std::string& moduleName)
{
#ifdef _WIN32
	// Initialize COM in MTA on this thread to prevent any COM-based operations
	// from marshaling calls back to the main thread's COM apartment
	const HRESULT comInitResult= CoInitializeEx(NULL, COINIT_MULTITHREADED);
	const bool bComInitialized= (comInitResult == S_OK);
#endif // _WIN32

	ARKitVideoDeviceManagerInitResult result;

	// Attempt to load the video device module
	result.module= getMikanModuleManager()->getModule<IARKitVideoDeviceModule>(moduleName);
	if (result.module)
	{
		MIKAN_LOG_INFO("ARKitVideoDeviceManagerLoader::initARKitVideoDeviceManagerOnThread")
			<< "Loaded module " << moduleName;

		// Attempt to create a device manager
		result.manager= result.module->createARKitVideoDeviceManager();
		if (result.manager)
		{
			MIKAN_LOG_INFO("ARKitVideoDeviceManagerLoader::initARKitVideoDeviceManagerOnThread")
				<< "Allocated ARKit video device manager for " << moduleName;

			// Attempt to startup the ARKit video device manager
			if (result.manager->startup())
			{
				MIKAN_LOG_INFO("ARKitVideoDeviceManagerLoader::initARKitVideoDeviceManagerOnThread")
					<< "Started ARKitVideoDeviceManager for " << moduleName;

#ifdef _WIN32
				if (bComInitialized)
					CoUninitialize();
#endif // _WIN32
				return result;
			}
			else
			{
				MIKAN_LOG_WARNING("ARKitVideoDeviceManagerLoader::initARKitVideoDeviceManagerOnThread")
					<< "Failed to startup ARKitVideoDeviceManager for " << moduleName;
			}
		}
		else
		{
			MIKAN_LOG_WARNING("ARKitVideoDeviceManagerLoader::initARKitVideoDeviceManagerOnThread")
				<< "Failed to allocate ARKitVideoDeviceManager for " << moduleName;
		}
	}
	else
	{
		MIKAN_LOG_ERROR("ARKitVideoDeviceManagerLoader::initARKitVideoDeviceManagerOnThread")
			<< "Failed to load module " << moduleName;
	}

	// Clean up on failure
	if (result.manager)
	{
		result.manager->shutdown();
		result.manager= nullptr;
	}
	if (result.module)
	{
		getMikanModuleManager()->disposeModule(result.module);
		result.module= nullptr;
	}

#ifdef _WIN32
	if (bComInitialized)
		CoUninitialize();
#endif // _WIN32

	return result;
}

void ARKitVideoDeviceManagerLoader::disposeARKitVideoDeviceManager()
{
	if (m_arkitVideoDeviceManager)
	{
		m_arkitVideoDeviceManager->shutdown();
		m_arkitVideoDeviceManager= nullptr;
	}

	if (m_arkitVideoDeviceModule)
	{
		getMikanModuleManager()->disposeModule(m_arkitVideoDeviceModule);
		m_arkitVideoDeviceModule= nullptr;
	}
}

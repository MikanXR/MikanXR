#include "App.h"
#include "GStreamerUtils.h"
#include "INetworkVideoDeviceManager.h"
#include "INetworkVideoDeviceModule.h"
#include "Logger.h"
#include "MikanObject.h"
#include "MikanModuleManager.h"
#include "MikanPropertyDatabase.h"
#include "NetworkVideoSourceSystem.h"
#include "NetworkVideoSourceComponent.h"
#include "ProjectConfig.h"

#include <algorithm>
#include <assert.h>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <objbase.h>
#endif //_WIN32

#define GSTREAMER_VIDEO_DEVICE_MODULE_NAME "MikanGStreamerVideo"
#define NETWORK_VIDEO_DEVICE_MODULE_NAME GSTREAMER_VIDEO_DEVICE_MODULE_NAME

// -- NetworkVideoSourceSystemDefinition -----
NetworkVideoSourceSystemDefinition::NetworkVideoSourceSystemDefinition(const std::string& configName,
																	   IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

configuru::Config NetworkVideoSourceSystemDefinition::writeToJSON()
{
	configuru::Config pt= Super::writeToJSON();

	return pt;
}

void NetworkVideoSourceSystemDefinition::readFromJSON(const configuru::Config& pt) { Super::readFromJSON(pt); }

// -- NetworkVideoSourceSystem -----
NetworkVideoSourceSystem::NetworkVideoSourceSystem(ProjectManagerPtr ownerObjectSystem)
	: Super::MikanTypedObjectSystem(ownerObjectSystem)
{
}

void NetworkVideoSourceSystem::update(float deltaTime)
{
	// Lazy-launch async init on first update (after all startup work has completed)
	ensureNetworkDeviceManager();

	// Poll for async manager init completion
	if (m_networkVideoManagerState == eNetworkVideoManagerState::initializing)
	{
		if (m_networkVideoManagerFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			auto result= m_networkVideoManagerFuture.get();
			if (result.manager)
			{
				m_networkVideoDeviceModule= result.module;
				m_networkVideoDeviceManager= result.manager;
				m_networkVideoManagerState= eNetworkVideoManagerState::ready;
				MIKAN_LOG_INFO("NetworkVideoSourceSystem::update") << "Network video device manager is ready";

				// Retry openVideoSource() on any components that were waiting
				for (const auto& [id, weakComp] : Super::getComponentMap())
				{
					if (auto comp= weakComp.lock(); comp && comp->isPendingOpen())
						comp->openVideoSource();
				}
			}
			else
			{
				m_networkVideoManagerState= eNetworkVideoManagerState::failed;
				m_networkVideoManagerFailReason= "GStreamer plugin failed to load";
				MIKAN_LOG_ERROR("NetworkVideoSourceSystem::update") << "Async network video device manager init failed";
			}
		}
	}

	Super::update(deltaTime);

	if (m_networkVideoDeviceManager)
	{
		m_networkVideoDeviceManager->update(deltaTime);
	}
}

void NetworkVideoSourceSystem::dispose()
{
	// If async init is still in-flight, block until it completes so we can
	// safely clean up whatever it allocated
	if (m_networkVideoManagerFuture.valid())
	{
		auto result= m_networkVideoManagerFuture.get();
		m_networkVideoDeviceModule= result.module;
		m_networkVideoDeviceManager= result.manager;
	}

	// A sender's code lives in the module, so none may keep running past this system
	for (const auto& weakSender : m_rtpVideoSenders)
	{
		if (IRtpVideoSenderPtr sender= weakSender.lock())
		{
			sender->close();
		}
	}
	m_rtpVideoSenders.clear();

	Super::dispose();
}

VideoSourceIdList NetworkVideoSourceSystem::getVideoSourceIdList() const
{
	VideoSourceIdList videoSourceIdList;
	for (const auto& it : Super::getComponentMap())
	{
		NetworkVideoSourceComponentPtr componentPtr= it.second.lock();
		if (componentPtr)
		{
			videoSourceIdList.push_back(componentPtr->getVideoSourceId());
		}
	}
	return videoSourceIdList;
}

bool NetworkVideoSourceSystem::ensureNetworkDeviceManager()
{
	switch (m_networkVideoManagerState)
	{
	case eNetworkVideoManagerState::ready:
		return true;
	case eNetworkVideoManagerState::initializing:
		return false;
	case eNetworkVideoManagerState::failed:
		return false;
	default:
		break;
	}

	// Bail if we didn't select a valid runtime type to use
	const std::string moduleName= NETWORK_VIDEO_DEVICE_MODULE_NAME;
	if (moduleName.empty())
	{
		m_networkVideoManagerState= eNetworkVideoManagerState::failed;
		return false;
	}

	// Special case: If GStreamer is selected, see if it is installed and make its DLLs loadable.
	// Do this cheap check on the main thread before launching the async task.
	if (moduleName == GSTREAMER_VIDEO_DEVICE_MODULE_NAME)
	{
		std::filesystem::path runtimeDir;
		if (!GStreamerUtils::ensureRuntimeLoadable(runtimeDir))
		{
			MIKAN_LOG_WARNING("NetworkVideoSourceSystem::ensureNetworkDeviceManager")
				<< "GStreamer not installed. Skipping network video manager init.";
			// Not a failure — just no manager available
			m_networkVideoManagerState= eNetworkVideoManagerState::failed;
			m_networkVideoManagerFailReason= "GStreamer not installed";
			return false;
		}

		MIKAN_LOG_INFO("NetworkVideoSourceSystem::ensureNetworkDeviceManager")
			<< "Using GStreamer runtime at " << runtimeDir.string();
	}

	// Launch async init to avoid blocking the main thread
	MIKAN_LOG_INFO("NetworkVideoSourceSystem::ensureNetworkDeviceManager")
		<< "Launching async init for network video device module " << moduleName;
	m_networkVideoManagerState= eNetworkVideoManagerState::initializing;
	auto promise= std::make_shared<std::promise<NetworkVideoDeviceManagerInitResult>>();
	m_networkVideoManagerFuture= promise->get_future();
	std::thread([promise, moduleName]() mutable
				{ promise->set_value(initNetworkVideoDeviceManagerOnThread(moduleName)); })
		.detach();

	return false;
}

IRtpVideoSenderPtr NetworkVideoSourceSystem::createRtpVideoSender()
{
	if (m_networkVideoManagerState != eNetworkVideoManagerState::ready || m_networkVideoDeviceModule == nullptr)
		return IRtpVideoSenderPtr();

	IRtpVideoSenderPtr sender= m_networkVideoDeviceModule->createRtpVideoSender();
	if (sender)
	{
		m_rtpVideoSenders.erase(std::remove_if(m_rtpVideoSenders.begin(), m_rtpVideoSenders.end(),
											   [](const std::weak_ptr<IRtpVideoSender>& weakSender)
											   { return weakSender.expired(); }),
								m_rtpVideoSenders.end());
		m_rtpVideoSenders.push_back(sender);
	}

	return sender;
}

bool NetworkVideoSourceSystem::isRtpVideoSenderAvailable(std::string& outReason)
{
	// Kicks the module load for a project that has no network video source
	ensureNetworkDeviceManager();

	switch (m_networkVideoManagerState)
	{
	case eNetworkVideoManagerState::ready:
		return true;
	case eNetworkVideoManagerState::failed:
		outReason= m_networkVideoManagerFailReason;
		return false;
	default:
		outReason= "loading the GStreamer plugin";
		return false;
	}
}

NetworkVideoSourceSystem::NetworkVideoDeviceManagerInitResult NetworkVideoSourceSystem::
	initNetworkVideoDeviceManagerOnThread(const std::string& moduleName)
{
#ifdef _WIN32
	// Initialize COM in MTA on this thread to prevent any COM-based operations
	// from marshaling calls back to the main thread's COM apartment
	const HRESULT comInitResult= CoInitializeEx(NULL, COINIT_MULTITHREADED);
	const bool bComInitialized= (comInitResult == S_OK);
#endif // _WIN32

	NetworkVideoDeviceManagerInitResult result;

	// Attempt to load the video device module
	result.module= getMikanModuleManager()->getModule<INetworkVideoDeviceModule>(moduleName);
	if (result.module)
	{
		MIKAN_LOG_INFO("NetworkVideoSourceSystem::initNetworkVideoDeviceManagerOnThread")
			<< "Loaded module " << moduleName;

		// Attempt to create a device manager
		result.manager= result.module->createNetworkVideoDeviceManager();
		if (result.manager)
		{
			MIKAN_LOG_INFO("NetworkVideoSourceSystem::initNetworkVideoDeviceManagerOnThread")
				<< "Allocated network video device manager for " << moduleName;

			// Attempt to startup the network video device manager
			if (result.manager->startup())
			{
				MIKAN_LOG_INFO("NetworkVideoSourceSystem::initNetworkVideoDeviceManagerOnThread")
					<< "Started NetworkVideoDeviceManger for " << moduleName;

				if (bComInitialized)
					CoUninitialize();
				return result;
			}
			else
			{
				MIKAN_LOG_WARNING("NetworkVideoSourceSystem::initNetworkVideoDeviceManagerOnThread")
					<< "Failed to startup NetworkVideoDeviceManger for " << moduleName;
			}
		}
		else
		{
			MIKAN_LOG_WARNING("NetworkVideoSourceSystem::initNetworkVideoDeviceManagerOnThread")
				<< "Failed to allocate NetworkVideoDeviceManger for " << moduleName;
		}
	}
	else
	{
		MIKAN_LOG_ERROR("NetworkVideoSourceSystem::initNetworkVideoDeviceManagerOnThread")
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

void NetworkVideoSourceSystem::disposeNetworkVideoDeviceManager()
{
	if (m_networkVideoDeviceManager)
	{
		m_networkVideoDeviceManager->shutdown();
		m_networkVideoDeviceManager= nullptr;
	}

	if (m_networkVideoDeviceModule)
	{
		getMikanModuleManager()->disposeModule(m_networkVideoDeviceModule);
		m_networkVideoDeviceModule= nullptr;
	}
}
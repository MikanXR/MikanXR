#include "USBVideoSourceSystem.h"
#include "USBVideoSourceComponent.h"
#include "IUsbVideoDeviceModule.h"
#include "Logger.h"
#include "MikanModuleManager.h"
#include "MikanPropertyDatabase.h"
#include "MikanVideoSourceTypes.h"
#include "App.h"
#include "ProjectConfig.h"
#include "MikanObject.h"

#include <assert.h>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <objbase.h>
#endif //_WIN32

#define USB_VIDEO_DEVICE_MODULE_NAME "MikanWMFVideo"

// -- USBVideoSourceSystemDefinition -----
USBVideoSourceSystemDefinition::USBVideoSourceSystemDefinition(const std::string& configName,
															   IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

// -- USBVideoSourceSystem ----
USBVideoSourceSystem::USBVideoSourceSystem(ProjectManagerPtr ownerObjectSystem)
	: Super::MikanTypedObjectSystem(ownerObjectSystem)
{
}

bool USBVideoSourceSystem::init(MikanObjectSystemDefinitionPtr definitionPtr)
{
	// NOTE: intentionally NOT launching the async thread here.
	// Deferring to the first update() avoids DLL loader-lock contention
	// with the websocket server and ImGui startup happening on the main thread.
	Super::init(definitionPtr);

	return true;
}

void USBVideoSourceSystem::update(float deltaTime)
{
	// Lazy-launch async init on first update (after all startup work has completed)
	ensureUsbVideoDeviceManager();

	// Poll for async manager init completion
	if (m_usbVideoManagerState == eUsbVideoManagerState::initializing)
	{
		if (m_usbVideoManagerFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			auto result= m_usbVideoManagerFuture.get();
			if (result.manager)
			{
				m_usbVideoDeviceModule= result.module;
				m_usbVideoDeviceManager= result.manager;
				m_usbVideoDeviceManager->addListener(this);
				m_usbVideoManagerState= eUsbVideoManagerState::ready;
				MIKAN_LOG_INFO("USBVideoSourceSystem::update") << "USB video device manager is ready";

				// Retry openVideoSource() on any components that were waiting
				for (const auto& [id, weakComp] : Super::getComponentMap())
				{
					if (auto comp= weakComp.lock(); comp && comp->isPendingOpen())
						comp->openVideoSource();
				}
			}
			else
			{
				m_usbVideoManagerState= eUsbVideoManagerState::failed;
				MIKAN_LOG_ERROR("USBVideoSourceSystem::update") << "Async USB video device manager init failed";
			}
		}
	}

	// Pumps the hotplug window, which lives on this thread from its first tick
	if (m_usbVideoDeviceManager)
	{
		m_usbVideoDeviceManager->update(deltaTime);
	}

	Super::update(deltaTime);
}

// The device manager and its plugin live for the process, not the project.
// Disposing them with the project unloaded the DLL under a receive thread and
// left the loader state at ready with no manager behind it, so the next project
// could never open a camera. The components close their devices in
// Super::dispose(); the manager waits for app shutdown.
void USBVideoSourceSystem::dispose() { Super::dispose(); }

USBVideoSourceSystem::~USBVideoSourceSystem()
{
	// If async init is still in-flight, block until it completes so we can safely clean up
	if (m_usbVideoManagerFuture.valid())
	{
		auto result= m_usbVideoManagerFuture.get();
		m_usbVideoDeviceModule= result.module;
		m_usbVideoDeviceManager= result.manager;
	}

	disposeUsbVideoDeviceManager();
}

bool USBVideoSourceSystem::ensureUsbVideoDeviceManager()
{
	switch (m_usbVideoManagerState)
	{
	case eUsbVideoManagerState::ready:
		return true;
	case eUsbVideoManagerState::initializing:
		return false;
	case eUsbVideoManagerState::failed:
		return false;
	default:
		break;
	}

	const std::string moduleName= USB_VIDEO_DEVICE_MODULE_NAME;
	if (moduleName.empty())
	{
		m_usbVideoManagerState= eUsbVideoManagerState::failed;
		return false;
	}

	MIKAN_LOG_INFO("USBVideoSourceSystem::ensureUsbVideoDeviceManager")
		<< "Launching async init for USB video device module " << moduleName;
	m_usbVideoManagerState= eUsbVideoManagerState::initializing;
	auto promise= std::make_shared<std::promise<UsbVideoDeviceManagerInitResult>>();
	m_usbVideoManagerFuture= promise->get_future();
	std::thread([promise, moduleName]() mutable { promise->set_value(initUsbVideoDeviceManagerOnThread(moduleName)); })
		.detach();

	return false;
}

USBVideoSourceSystem::UsbVideoDeviceManagerInitResult USBVideoSourceSystem::initUsbVideoDeviceManagerOnThread(
	const std::string& moduleName)
{
#ifdef _WIN32
	// Initialize COM in MTA on this thread so that WMF device enumeration
	// doesn't try to marshal calls to the main thread's COM apartment
	const HRESULT comInitResult= CoInitializeEx(NULL, COINIT_MULTITHREADED);
	const bool bComInitialized= (comInitResult == S_OK);
#endif // _WIN32

	UsbVideoDeviceManagerInitResult result;

	// Attempt to load the usb device module
	result.module= getMikanModuleManager()->getModule<IUsbVideoDeviceModule>(moduleName);
	if (result.module)
	{
		MIKAN_LOG_INFO("USBVideoSourceSystem::initUsbVideoDeviceManagerOnThread") << "Loaded module " << moduleName;

		// Attempt to create a device manager
		result.manager= result.module->createUsbVideoDeviceManager();
		if (result.manager)
		{
			MIKAN_LOG_INFO("USBVideoSourceSystem::initUsbVideoDeviceManagerOnThread")
				<< "Allocated USB device manager for " << moduleName;

			// Attempt to startup the usb device manager
			if (result.manager->startup())
			{
				MIKAN_LOG_INFO("USBVideoSourceSystem::initUsbVideoDeviceManagerOnThread")
					<< "Started USBDeviceManger for " << moduleName;

				if (bComInitialized)
					CoUninitialize();
				return result;
			}
			else
			{
				MIKAN_LOG_WARNING("USBVideoSourceSystem::initUsbVideoDeviceManagerOnThread")
					<< "Failed to startup UsbVideoDeviceManger for " << moduleName;
			}
		}
		else
		{
			MIKAN_LOG_WARNING("USBVideoSourceSystem::initUsbVideoDeviceManagerOnThread")
				<< "Failed to allocate UsbVideoDeviceManger for " << moduleName;
		}
	}
	else
	{
		MIKAN_LOG_ERROR("USBVideoSourceSystem::initUsbVideoDeviceManagerOnThread")
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

void USBVideoSourceSystem::disposeUsbVideoDeviceManager()
{
	if (m_usbVideoDeviceManager)
	{
		m_usbVideoDeviceManager->removeListener(this);
		m_usbVideoDeviceManager->shutdown();
		m_usbVideoDeviceManager= nullptr;
	}

	if (m_usbVideoDeviceModule)
	{
		getMikanModuleManager()->disposeModule(m_usbVideoDeviceModule);
		m_usbVideoDeviceModule= nullptr;
	}
}

bool USBVideoSourceSystem::getConnectedUSBVideoSourcePaths(USBVideoSourcePathList& outVideoSourcePathList) const
{
	outVideoSourcePathList.clear();

	if (m_usbVideoDeviceManager)
	{
		for (size_t index= 0; index < m_usbVideoDeviceManager->getDeviceCount(); index++)
		{
			IUsbVideoDevice* usbVideoDevice= m_usbVideoDeviceManager->getDeviceByIndex(index);

			outVideoSourcePathList.push_back(usbVideoDevice->getDevicePath());
		}

		return true;
	}

	return false;
}

bool USBVideoSourceSystem::getConnectedUSBVideoSourcePathMap(USBVideoSourcePathMap& outVideoSourcePathMap) const
{
	outVideoSourcePathMap.clear();

	if (m_usbVideoDeviceManager)
	{
		for (size_t index= 0; index < m_usbVideoDeviceManager->getDeviceCount(); index++)
		{
			IUsbVideoDevice* usbVideoDevice= m_usbVideoDeviceManager->getDeviceByIndex(index);

			outVideoSourcePathMap[usbVideoDevice->getDevicePath()]= usbVideoDevice->getFriendlyName();
		}

		return true;
	}

	return false;
}

VideoSourceIdList USBVideoSourceSystem::getVideoSourceIdList() const
{
	VideoSourceIdList videoSourceIdList;
	for (const auto& it : Super::getComponentMap())
	{
		USBVideoSourceComponentPtr componentPtr= it.second.lock();
		if (componentPtr)
		{
			videoSourceIdList.push_back(componentPtr->getVideoSourceId());
		}
	}
	return videoSourceIdList;
}

USBVideoSourceComponentPtr USBVideoSourceSystem::getUSBVideoSourceByPath(const std::string& videoSourcePath) const
{
	for (auto it= Super::getComponentMap().begin(); it != Super::getComponentMap().end(); it++)
	{
		USBVideoSourceComponentPtr componentPtr= it->second.lock();
		if (componentPtr && componentPtr->getUSBVideoSourceDefinition()->getDevicePath() == videoSourcePath)
		{
			return componentPtr;
		}
	}

	return USBVideoSourceComponentPtr();
}

USBVideoSourceComponentPtr USBVideoSourceSystem::addNewUSBVideoSource()
{
	// If available, get the first connected device by default
	if (m_usbVideoDeviceManager && m_usbVideoDeviceManager->getDeviceCount() > 0)
	{
		// Find the first valid device with at least one video mode
		int firstValidDeviceIdex= -1;
		for (int deviceIndex= 0; deviceIndex < m_usbVideoDeviceManager->getDeviceCount(); deviceIndex++)
		{
			IUsbVideoDevice* testVideoDevice= m_usbVideoDeviceManager->getDeviceByIndex(deviceIndex);
			if (testVideoDevice && testVideoDevice->getAvailableVideoModesCount() > 0)
			{
				if (testVideoDevice->getVideoModeIndex() < 0)
				{
					testVideoDevice->setVideoModeByIndex(0);
				}

				firstValidDeviceIdex= deviceIndex;
				break;
			}
		}

		IUsbVideoDevice* usbVideoDevice= m_usbVideoDeviceManager->getDeviceByIndex(firstValidDeviceIdex);
		if (usbVideoDevice)
		{
			// Use base class method with custom definition init
			return Super::addNewObjectByTypedDefinition(
				[usbVideoDevice](USBVideoSourceDefinitionPtr def)
				{
					const char* videoModeName= usbVideoDevice->getVideoModeName();

					def->setComponentName(usbVideoDevice->getFriendlyName());
					def->setDevicePath(usbVideoDevice->getDevicePath());
					def->setVideoMode(videoModeName ? videoModeName : "<INVALID>");

					return true;
				});
		}
	}

	return USBVideoSourceComponentPtr();
}

void USBVideoSourceSystem::onConnectedDeviceListChanged()
{
	// Broadcast any sources were disconnected
	if (OnVideoSourceListChanged)
	{
		OnVideoSourceListChanged();
	}

	// A source whose camera was unplugged closed itself; if the camera is back
	// in the list, reopen it. A source whose camera is still missing bails out
	// of openVideoSource without side effects.
	for (const auto& [id, weakComp] : Super::getComponentMap())
	{
		if (auto comp= weakComp.lock(); comp && comp->getDevicePath().empty())
			comp->openVideoSource();
	}
}

// -- IEntityAccessor ----
rfk::Struct const* USBVideoSourceSystem::getClientAPIValuesStructType() const
{
	return &MikanUSBVideoSourceSystemValues::staticGetArchetype();
}

// -- IPropertyInterface ----
const std::string USBVideoSourceSystem::k_usbDeviceMapPropertyId= "usb_device_map";

void USBVideoSourceSystem::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	Super::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(USBVideoSourceSystem::k_usbDeviceMapPropertyId,
																  MikanVariantType::STRING_MAP)
								 ->setReadOnly());
}

bool USBVideoSourceSystem::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{

	if (propertyName == USBVideoSourceSystem::k_usbDeviceMapPropertyId)
	{
		USBVideoSourcePathMap usbDevicePathMap;
		getConnectedUSBVideoSourcePathMap(usbDevicePathMap);

		// Convert std::map to Serialization::Map
		Serialization::Map<Serialization::String, Serialization::String> serializableMap;
		for (const auto& [path, friendlyName] : usbDevicePathMap)
		{
			serializableMap[path.c_str()]= Serialization::String(friendlyName.c_str());
		}

		outValue= serializableMap;
		return true;
	}

	return Super::getPropertyValue(propertyName, outValue);
}
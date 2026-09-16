#include "DeviceHotplugNotifier.h"
#include "MikanWMFVideoDevice.h"
#include "MikanWMFVideoDeviceManager.h"
#include "Logger.h"
#include "WMFDeviceList.h"

// -- MikanWMFVideoDeviceManager ------
MikanWMFVideoDeviceManager::MikanWMFVideoDeviceManager()
	: m_wmfDeviceList(new WMFDeviceList())
	, m_deviceHotplugNotifier(new DeviceHotplugNotifier())
{
}

MikanWMFVideoDeviceManager::~MikanWMFVideoDeviceManager()
{
	shutdown();
	delete m_deviceHotplugNotifier;
	delete m_wmfDeviceList;
}

void MikanWMFVideoDeviceManager::addListener(IUsbVideoDeviceManagerListener* eventListener)
{
	auto it= std::find(m_eventListeners.begin(), m_eventListeners.end(), eventListener);
	if (it == m_eventListeners.end())
	{
		m_eventListeners.push_back(eventListener);
	}
}

void MikanWMFVideoDeviceManager::removeListener(IUsbVideoDeviceManagerListener* eventListener)
{
	auto it= std::find(m_eventListeners.begin(), m_eventListeners.end(), eventListener);
	if (it != m_eventListeners.end())
	{
		m_eventListeners.erase(it);
	}
}

bool MikanWMFVideoDeviceManager::startup()
{
	// Enumeration is what every consumer needs; hotplug is a convenience on top
	// of it, so losing the notifier leaves a working manager with a static list
	rebuildDeviceList();

	if (!m_deviceHotplugNotifier->startup(this))
	{
		MIKAN_LOG_WARNING("MikanWMFVideoDeviceManager::startup")
			<< "Failed to start hotplug listener, camera arrival and removal will not be noticed";
	}

	return true;
}

void MikanWMFVideoDeviceManager::update(float deltaTime) { m_deviceHotplugNotifier->update(); }

void MikanWMFVideoDeviceManager::shutdown() { m_deviceHotplugNotifier->shutdown(); }

size_t MikanWMFVideoDeviceManager::getDeviceCount() const { return m_deviceMap.size(); }

IUsbVideoDevice* MikanWMFVideoDeviceManager::getDeviceByIndex(size_t index)
{
	if (index < m_deviceMap.size())
	{
		auto it= m_deviceMap.begin();
		std::advance(it, index);
		return it->second.get();
	}

	return nullptr;
}

IUsbVideoDevice* MikanWMFVideoDeviceManager::getDeviceByPath(const char* devicePath)
{
	auto it= m_deviceMap.find(devicePath);
	if (it != m_deviceMap.end())
	{
		return it->second.get();
	}

	return nullptr;
}

void MikanWMFVideoDeviceManager::onDeviceConnected(const std::string& path)
{
	MIKAN_LOG_INFO("MikanWMFVideoDeviceManager") << "Device connected: " << path;
	rebuildDeviceList();
}

void MikanWMFVideoDeviceManager::onDeviceDisconnected(const std::string& path)
{
	MIKAN_LOG_INFO("MikanWMFVideoDeviceManager") << "Device disconnected: " << path;
	rebuildDeviceList();
}

void MikanWMFVideoDeviceManager::rebuildDeviceList()
{
	// Update the WMF device list
	m_wmfDeviceList->rebuild();

	// Create any new devices that are not already in the map
	for (const auto& deviceInfo : m_wmfDeviceList->getDevices())
	{
		const std::string devicePath= deviceInfo.deviceSymbolicLink;

		if (m_deviceMap.find(devicePath) == m_deviceMap.end())
		{
			m_deviceMap[devicePath]= std::make_unique<MikanWMFVideoDevice>(this, deviceInfo);

			MIKAN_LOG_INFO("MikanWMFVideoDeviceManager") << "New device added: " << devicePath;
		}
	}

	// Remove devices that are no longer present
	for (auto it= m_deviceMap.begin(); it != m_deviceMap.end();)
	{
		if (!m_wmfDeviceList->isDevicePresent(it->first))
		{
			// Ensure the device is properly shutdown and listeners notified before removal
			it->second->notifyVideoDeviceDisconnected();

			MIKAN_LOG_INFO("MikanWMFVideoDeviceManager") << "Device removed: " << it->first;
			it= m_deviceMap.erase(it);
		}
		else
		{
			++it;
		}
	}

	// Notify all listeners that the device list has changed
	for (IUsbVideoDeviceManagerListener* listener : m_eventListeners)
	{
		listener->onConnectedDeviceListChanged();
	}
}
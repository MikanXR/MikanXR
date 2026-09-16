#pragma once

#include <string>

class IDeviceHotplugListener
{
public:
	virtual void onDeviceConnected(const std::string& path)= 0;
	virtual void onDeviceDisconnected(const std::string& path)= 0;
};

// Raises device arrival and removal for imaging devices through a message-only
// window. The window is created lazily by the first update() and pumped by every
// update() after it, so it belongs to the thread that ticks the owning manager
// and never to the loader thread that ran startup().
class DeviceHotplugNotifier
{
public:
	DeviceHotplugNotifier();
	virtual ~DeviceHotplugNotifier();

	bool startup(IDeviceHotplugListener* listener);
	void update();
	void shutdown();

	struct DeviceHotplugNotifierImpl* getPrivateImpl() { return m_impl; }

private:
	bool ensureWindow();

	struct DeviceHotplugNotifierImpl* m_impl;
};

// -- include -----
#include "DeviceHotplugNotifier.h"
#include "Logger.h"

#define ANSI
#define WIN32_LEAN_AND_MEAN

#include <windows.h> // Required for data types
#include <winuser.h>
#include <Dbt.h>
#include <guiddef.h>
#include <setupapi.h> // Device setup APIs
#include <assert.h>
#include <strsafe.h>
#include <winreg.h>
#include <Shellapi.h>

#include <string>
#include <vector>
#include <iostream>
#include <iomanip>

//-- constants -----
// Interface arrival and removal are keyed by device interface class, and the
// interface every camera Media Foundation enumerates exposes is
// KSCATEGORY_VIDEO_CAMERA, the GUID at the end of each device's symbolic link.
// The Image setup class GUID this used to register does not match an interface
// notification, so no event ever arrived.
GUID KSCATEGORY_VIDEO_CAMERA_GUID= {0xe5323777, 0xf976, 0x4f5b, 0x9b, 0x55, 0xb9, 0x46, 0x99, 0xc4, 0x6e, 0x44};

#define CLS_NAME "DEVICE_LISTENER_CLASS"
#define HWND_MESSAGE_ONLY ((HWND) - 3)

// -- globals ----
struct DeviceHotplugNotifierImpl
{
	IDeviceHotplugListener* listener= nullptr;
	HDEVNOTIFY hImageDeviceNotify= nullptr;
	HWND hWnd= nullptr;
	HINSTANCE hInstance= nullptr;
	bool bWindowFailed= false;
};

//-- private prototypes -----
static HDEVNOTIFY register_device_class_notification(HWND__* hwnd, const GUID& guid);
static LRESULT message_handler(HWND__* hwnd, UINT uint, WPARAM wparam, LPARAM lparam);

// -- definitions -----
DeviceHotplugNotifier::DeviceHotplugNotifier()
	: m_impl(new DeviceHotplugNotifierImpl)
{
}

DeviceHotplugNotifier::~DeviceHotplugNotifier()
{
	shutdown();
	delete m_impl;
}

bool DeviceHotplugNotifier::startup(IDeviceHotplugListener* listener)
{
	if (listener == nullptr)
		return false;

	m_impl->listener= listener;
	return true;
}

// Creates the window on the calling thread. Called from update(), so the window
// lives on the thread that pumps it. A failure is remembered rather than retried
// every tick.
bool DeviceHotplugNotifier::ensureWindow()
{
	if (m_impl->hWnd != nullptr)
		return true;
	if (m_impl->bWindowFailed)
		return false;

	// The window class belongs to this DLL, whose procedure it names, not to the
	// executable. Registering against the executable's handle would outlive an
	// unload of the plugin and point at unmapped code.
	HMODULE thisModule= nullptr;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
					   reinterpret_cast<LPCWSTR>(&message_handler), &thisModule);
	m_impl->hInstance= thisModule;

	WNDCLASSEX wx;
	ZeroMemory(&wx, sizeof(wx));
	wx.cbSize= sizeof(WNDCLASSEX);
	wx.lpfnWndProc= reinterpret_cast<WNDPROC>(message_handler);
	wx.hInstance= m_impl->hInstance;
	wx.lpszClassName= CLS_NAME;

	// A class left registered by an earlier notifier in this process is fine to reuse
	if (!RegisterClassEx(&wx) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
	{
		MIKAN_LOG_ERROR("DeviceHotplugNotifier::ensureWindow")
			<< "Could not register the device listener window class: " << GetLastError();
		m_impl->bWindowFailed= true;
		return false;
	}

	m_impl->hWnd= CreateWindow(CLS_NAME, "DevNotifWnd", WS_ICONIC, 0, 0, CW_USEDEFAULT, 0, HWND_MESSAGE_ONLY, NULL,
							   m_impl->hInstance,
							   this); // Pass 'this' as the window lpParam
	if (m_impl->hWnd == nullptr)
	{
		MIKAN_LOG_ERROR("DeviceHotplugNotifier::ensureWindow")
			<< "Could not create the device listener window: " << GetLastError();
		m_impl->bWindowFailed= true;
		return false;
	}

	MIKAN_LOG_INFO("DeviceHotplugNotifier::ensureWindow") << "Listening for imaging device arrival and removal";
	return true;
}

void DeviceHotplugNotifier::update()
{
	if (m_impl->listener == nullptr || !ensureWindow())
		return;

	// Only this window's messages. On the main thread a null filter would drain
	// the application window's queue as well.
	MSG msg;
	while (PeekMessage(&msg, m_impl->hWnd, 0, 0, PM_REMOVE) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void DeviceHotplugNotifier::shutdown()
{
	if (m_impl->hImageDeviceNotify != nullptr)
	{
		UnregisterDeviceNotification(m_impl->hImageDeviceNotify);
		m_impl->hImageDeviceNotify= nullptr;
	}

	if (m_impl->hWnd != nullptr)
	{
		DestroyWindow(m_impl->hWnd);
		m_impl->hWnd= nullptr;
	}

	if (m_impl->hInstance != nullptr)
	{
		UnregisterClassA(CLS_NAME, m_impl->hInstance);
		m_impl->hInstance= nullptr;
	}

	m_impl->listener= nullptr;
	m_impl->bWindowFailed= false;
}

//-- private helper methods -----
LRESULT message_handler(HWND__* hwnd, UINT msg_type, WPARAM wparam, LPARAM lparam)
{
	auto pThis= reinterpret_cast<DeviceHotplugNotifier*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	switch (msg_type)
	{
	case WM_NCCREATE:
	{
		auto cs= reinterpret_cast<CREATESTRUCT*>(lparam);
		auto pThis= static_cast<DeviceHotplugNotifier*>(cs->lpCreateParams);

		// Store pointer for later messages
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
	}
	break;

	case WM_DESTROY:
	{
		// No PostQuitMessage: the window shares the main thread's queue with the
		// application window, and a WM_QUIT there would end the application
		SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
	}
		return 0;

	case WM_CREATE:
	{
		if (pThis != nullptr)
		{
			pThis->getPrivateImpl()->hImageDeviceNotify=
				register_device_class_notification(hwnd, KSCATEGORY_VIDEO_CAMERA_GUID);
		}
		break;
	}

	case WM_DEVICECHANGE:
	{
		PDEV_BROADCAST_HDR lpdb= (PDEV_BROADCAST_HDR)lparam;

		if (pThis != nullptr && lpdb != nullptr && lpdb->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
		{
			PDEV_BROADCAST_DEVICEINTERFACE lpdbv= (PDEV_BROADCAST_DEVICEINTERFACE)lpdb;
			std::string path= std::string(lpdbv->dbcc_name);

			if (IsEqualCLSID(lpdbv->dbcc_classguid, KSCATEGORY_VIDEO_CAMERA_GUID))
			{
				switch (wparam)
				{
				case DBT_DEVICEARRIVAL:
					pThis->getPrivateImpl()->listener->onDeviceConnected(path);
					break;

				case DBT_DEVICEREMOVECOMPLETE:
					pThis->getPrivateImpl()->listener->onDeviceDisconnected(path);
					break;
				}
			}
		}
		break;
	}
	}

	return DefWindowProc(hwnd, msg_type, wparam, lparam);
}

static HDEVNOTIFY register_device_class_notification(HWND__* hwnd, const GUID& guid)
{
	DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
	ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
	NotificationFilter.dbcc_size= sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	NotificationFilter.dbcc_devicetype= DBT_DEVTYP_DEVICEINTERFACE;
	NotificationFilter.dbcc_classguid= guid;
	HDEVNOTIFY dev_notify= RegisterDeviceNotification(hwnd, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);

	if (dev_notify == nullptr)
	{
		MIKAN_LOG_ERROR("RegisterDeviceClassNotification") << "Could not register for device notifications!";
	}

	return dev_notify;
}

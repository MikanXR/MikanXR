//-- includes -----
#include "App.h"
#include "DiscreteGpuPreference.h"
#include "MikanCefApp.h"
#include "ThreadUtils.h"

#ifdef WIN32
#include <windows.h>
#endif

MIKAN_REQUEST_DISCRETE_GPU()

//-- entry point -----
#ifdef WIN32
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	// Allow Mikan.exe to act as its own CEF subprocess handler.
	// Subprocess invocations return >= 0 and must exit immediately.
	// MikanCefApp must be passed here so that OnBeforeCommandLineProcessing
	// runs in every subprocess instance of Mikan.exe.
	CefMainArgs cef_args(hInstance);
	CefRefPtr<MikanCefApp> cef_app= new MikanCefApp();
	int cef_exit= CefExecuteProcess(cef_args, cef_app, nullptr);
	if (cef_exit >= 0)
		return cef_exit;

	ThreadUtils::initMainThreadId();
	App app;

	return app.exec(__argc, __argv);
}
#else
extern "C" int main(int argc, char* argv[])
{
	// On Linux/macOS, CefMainArgs wraps (argc, argv) instead of an HINSTANCE.
	CefMainArgs cef_args(argc, argv);
	CefRefPtr<MikanCefApp> cef_app= new MikanCefApp();
	int cef_exit= CefExecuteProcess(cef_args, cef_app, nullptr);
	if (cef_exit >= 0)
		return cef_exit;

	ThreadUtils::initMainThreadId();
	App app;

	return app.exec(argc, argv);
}
#endif
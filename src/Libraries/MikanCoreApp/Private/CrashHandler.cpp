//-- includes -----
#include "CrashHandler.h"
#include "Logger.h"

#include <cstdlib>
#include <exception>
#include <fstream>
#include <string>

#if defined WIN32 || defined _WIN32 || defined WINCE
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#include <intrin.h>

#include <csignal>
#include <cstdio>
#include <cstring>
#endif

//-- constants -----
namespace
{
const char* k_pendingMarkerFileName= "pending_report.txt";
const char* k_testCrashKinds= "access abort terminate purecall invalidparam stackoverflow";
} // namespace

//-- private implementation (Windows) -----
#if defined WIN32 || defined _WIN32 || defined WINCE
namespace
{
// Exception codes for the crashes the CRT reports through a callback rather than
// an SEH exception. They sit in the user-defined range so the summary can name them.
constexpr DWORD k_exceptionCodeAbort= 0xE0000001;
constexpr DWORD k_exceptionCodePurecall= 0xE0000002;
constexpr DWORD k_exceptionCodeInvalidParameter= 0xE0000003;
constexpr DWORD k_exceptionCodeCppException= 0xE06D7363;

// How long the crashing thread waits for the writer thread before giving up
constexpr DWORD k_reportTimeoutMs= 60000;

constexpr size_t k_pathBufferSize= 1024;
constexpr size_t k_prefixBufferSize= 128;
constexpr size_t k_summaryBufferSize= 2048;

// Everything a crash needs is captured here at install time, so the crashing thread
// never touches the heap. The scratch buffers are only used by one report at a time.
struct CrashState
{
	bool bInstalled= false;

	// Install-time data
	wchar_t reportDirectory[k_pathBufferSize]= {}; // ends in a backslash
	wchar_t logFilePath[k_pathBufferSize]= {};     // empty when there is no log copy
	wchar_t namePrefix[k_prefixBufferSize]= {};    // "<appName>_<appVersion>"
	char narrowNamePrefix[k_prefixBufferSize]= {};

	// Per-report scratch
	wchar_t baseName[k_prefixBufferSize + 32]= {};
	wchar_t filePath[k_pathBufferSize]= {};
	char summary[k_summaryBufferSize]= {};

	// Writer thread handoff
	HANDLE writerThread= nullptr;
	HANDLE requestEvent= nullptr;
	HANDLE completeEvent= nullptr;
	volatile bool bWriterShouldExit= false;
	EXCEPTION_POINTERS* exceptionPointers= nullptr;
	DWORD crashingThreadId= 0;
	volatile long reportInProgress= 0;

	// Hooks that were in place before install
	LPTOP_LEVEL_EXCEPTION_FILTER previousFilter= nullptr;
	_invalid_parameter_handler previousInvalidParameterHandler= nullptr;
	_purecall_handler previousPurecallHandler= nullptr;
};

CrashState g_state;

const char* describeExceptionCode(DWORD code)
{
	switch (code)
	{
	case EXCEPTION_ACCESS_VIOLATION:
		return "Access violation";
	case EXCEPTION_STACK_OVERFLOW:
		return "Stack overflow";
	case EXCEPTION_ILLEGAL_INSTRUCTION:
		return "Illegal instruction";
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
		return "Integer divide by zero";
	case EXCEPTION_BREAKPOINT:
		return "Breakpoint";
	case k_exceptionCodeCppException:
		return "Unhandled C++ exception";
	case k_exceptionCodeAbort:
		return "abort() called";
	case k_exceptionCodePurecall:
		return "Pure virtual function call";
	case k_exceptionCodeInvalidParameter:
		return "CRT invalid parameter";
	default:
		return "Unknown";
	}
}

bool buildReportPath(const wchar_t* extension)
{
	return _snwprintf_s(g_state.filePath, k_pathBufferSize, _TRUNCATE, L"%s%s%s", g_state.reportDirectory,
						g_state.baseName, extension)
		   > 0;
}

void writeWholeFile(const wchar_t* path, const char* text)
{
	HANDLE file= CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return;

	DWORD written= 0;
	WriteFile(file, text, (DWORD)strlen(text), &written, nullptr);
	CloseHandle(file);
}

bool writeMinidump()
{
	if (!buildReportPath(L".dmp"))
		return false;

	HANDLE file=
		CreateFileW(g_state.filePath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return false;

	MINIDUMP_EXCEPTION_INFORMATION exceptionInfo= {};
	exceptionInfo.ThreadId= g_state.crashingThreadId;
	exceptionInfo.ExceptionPointers= g_state.exceptionPointers;
	exceptionInfo.ClientPointers= FALSE;

	// Data segments are left out on purpose: with the browser, vision, and ML
	// runtimes loaded they push the file past what an issue attachment accepts
	const MINIDUMP_TYPE dumpType= (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo
												  | MiniDumpWithUnloadedModules | MiniDumpWithHandleData);

	const BOOL success=
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, dumpType, &exceptionInfo, nullptr, nullptr);
	CloseHandle(file);

	return success == TRUE;
}

void writeSummary(const SYSTEMTIME& time, bool bDumpWritten)
{
	const EXCEPTION_RECORD* record= g_state.exceptionPointers->ExceptionRecord;
	const DWORD code= record->ExceptionCode;
	const void* address= record->ExceptionAddress;

	wchar_t modulePath[k_pathBufferSize]= L"<unknown>";
	uintptr_t moduleOffset= 0;
	HMODULE module= nullptr;
	if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
						   (LPCWSTR)address, &module)
		&& module != nullptr)
	{
		GetModuleFileNameW(module, modulePath, k_pathBufferSize);
		moduleOffset= (uintptr_t)address - (uintptr_t)module;
	}

	_snprintf_s(g_state.summary, k_summaryBufferSize, _TRUNCATE,
				"MikanXR crash report\n"
				"Application: %s\n"
				"Timestamp: %04d-%02d-%02d %02d:%02d:%02d\n"
				"Exception: 0x%08X (%s)\n"
				"Address: 0x%p\n"
				"Module: %ls\n"
				"Module offset: 0x%llX\n"
				"Thread: %lu\n"
				"Minidump: %s\n"
				"\n"
				"Attach the .dmp, .txt and .log files of this report to a GitHub issue.\n",
				g_state.narrowNamePrefix, time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond,
				code, describeExceptionCode(code), address, modulePath, (unsigned long long)moduleOffset,
				g_state.crashingThreadId, bDumpWritten ? "written" : "failed");

	if (buildReportPath(L".txt"))
		writeWholeFile(g_state.filePath, g_state.summary);
}

void copyLog()
{
	if (g_state.logFilePath[0] == L'\0')
		return;

	// The logger flushes every line, so the copy is complete up to the crash
	if (buildReportPath(L".log"))
		CopyFileW(g_state.logFilePath, g_state.filePath, FALSE);
}

void writePendingMarker()
{
	char baseName[k_prefixBufferSize + 32]= {};
	_snprintf_s(baseName, sizeof(baseName), _TRUNCATE, "%ls", g_state.baseName);

	if (_snwprintf_s(g_state.filePath, k_pathBufferSize, _TRUNCATE, L"%s%hs", g_state.reportDirectory,
					 k_pendingMarkerFileName)
		> 0)
	{
		writeWholeFile(g_state.filePath, baseName);
	}
}

// Runs on the writer thread (or on the crashing thread when there is none)
void writeReportFiles()
{
	SYSTEMTIME time= {};
	GetLocalTime(&time);

	_snwprintf_s(g_state.baseName, sizeof(g_state.baseName) / sizeof(wchar_t), _TRUNCATE,
				 L"%s_%04d-%02d-%02d_%02d-%02d-%02d", g_state.namePrefix, time.wYear, time.wMonth, time.wDay,
				 time.wHour, time.wMinute, time.wSecond);

	const bool bDumpWritten= writeMinidump();
	writeSummary(time, bDumpWritten);
	copyLog();
	writePendingMarker();
}

DWORD WINAPI writerThreadProc(LPVOID)
{
	for (;;)
	{
		WaitForSingleObject(g_state.requestEvent, INFINITE);
		if (g_state.bWriterShouldExit)
			return 0;

		writeReportFiles();
		SetEvent(g_state.completeEvent);
	}
}

// Hands the report to the writer thread and waits for it. A crashing thread whose
// own stack is exhausted could not run the dump writer, which is why the work
// happens on a thread created while everything was still healthy.
void handleCrash(EXCEPTION_POINTERS* exceptionPointers)
{
	if (InterlockedCompareExchange(&g_state.reportInProgress, 1, 0) != 0)
	{
		// A second crash while a report is in flight: let the first one finish,
		// it terminates the process when done
		Sleep(INFINITE);
	}

	g_state.exceptionPointers= exceptionPointers;
	g_state.crashingThreadId= GetCurrentThreadId();

	if (g_state.writerThread != nullptr)
	{
		SetEvent(g_state.requestEvent);
		WaitForSingleObject(g_state.completeEvent, k_reportTimeoutMs);
	}
	else
	{
		writeReportFiles();
	}
}

// For the crashes the CRT reports through a callback: capture where we are and
// report it as if it had been an exception
__declspec(noinline) void reportSyntheticCrash(DWORD exceptionCode)
{
	CONTEXT context= {};
	context.ContextFlags= CONTEXT_FULL;
	RtlCaptureContext(&context);

	EXCEPTION_RECORD record= {};
	record.ExceptionCode= exceptionCode;
	record.ExceptionAddress= _ReturnAddress();

	EXCEPTION_POINTERS pointers= {&record, &context};
	handleCrash(&pointers);

	TerminateProcess(GetCurrentProcess(), exceptionCode);
}

LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionPointers)
{
	const DWORD code= exceptionPointers->ExceptionRecord->ExceptionCode;

	// OutputDebugString raises these to talk to a debugger; they are not crashes
	if (code == DBG_PRINTEXCEPTION_C || code == DBG_PRINTEXCEPTION_WIDE_C)
		return EXCEPTION_CONTINUE_SEARCH;

	handleCrash(exceptionPointers);
	TerminateProcess(GetCurrentProcess(), code);

	return EXCEPTION_EXECUTE_HANDLER;
}

void invalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
{
	reportSyntheticCrash(k_exceptionCodeInvalidParameter);
}

void purecallHandler() { reportSyntheticCrash(k_exceptionCodePurecall); }

void abortSignalHandler(int) { reportSyntheticCrash(k_exceptionCodeAbort); }

// Installs every hook. Returns true when the exception filter was already ours,
// which is how ensureInstalled tells whether something replaced it.
bool installHooks()
{
	const LPTOP_LEVEL_EXCEPTION_FILTER previousFilter= SetUnhandledExceptionFilter(unhandledExceptionFilter);
	const _invalid_parameter_handler previousInvalidParameter= _set_invalid_parameter_handler(invalidParameterHandler);
	const _purecall_handler previousPurecall= _set_purecall_handler(purecallHandler);

	// abort() would otherwise fast-fail, which no user-mode handler can see.
	// Without those flags it raises SIGABRT and then exits, so the signal
	// handler is the only place the report can be written.
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	signal(SIGABRT, abortSignalHandler);

	if (!g_state.bInstalled)
	{
		g_state.previousFilter= previousFilter;
		g_state.previousInvalidParameterHandler= previousInvalidParameter;
		g_state.previousPurecallHandler= previousPurecall;
	}

	return previousFilter == unhandledExceptionFilter && previousInvalidParameter == invalidParameterHandler
		   && previousPurecall == purecallHandler;
}

bool copyPathToBuffer(const std::filesystem::path& path, wchar_t* buffer, size_t bufferSize)
{
	const std::wstring text= path.wstring();
	if (text.size() + 1 > bufferSize)
		return false;

	wcscpy_s(buffer, bufferSize, text.c_str());
	return true;
}

bool startWriterThread()
{
	g_state.requestEvent= CreateEventW(nullptr, FALSE, FALSE, nullptr);
	g_state.completeEvent= CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (g_state.requestEvent == nullptr || g_state.completeEvent == nullptr)
		return false;

	g_state.bWriterShouldExit= false;
	g_state.writerThread= CreateThread(nullptr, 0, writerThreadProc, nullptr, 0, nullptr);

	return g_state.writerThread != nullptr;
}

void stopWriterThread()
{
	if (g_state.writerThread != nullptr)
	{
		g_state.bWriterShouldExit= true;
		SetEvent(g_state.requestEvent);
		WaitForSingleObject(g_state.writerThread, 1000);
		CloseHandle(g_state.writerThread);
		g_state.writerThread= nullptr;
	}

	if (g_state.requestEvent != nullptr)
	{
		CloseHandle(g_state.requestEvent);
		g_state.requestEvent= nullptr;
	}

	if (g_state.completeEvent != nullptr)
	{
		CloseHandle(g_state.completeEvent);
		g_state.completeEvent= nullptr;
	}
}

//-- test crashes -----
struct PurecallBase
{
	PurecallBase() { callFromConstructor(); }
	virtual ~PurecallBase()= default;

	// Calling through a separate member keeps the call virtual, and during the
	// base constructor the vtable still points at the pure entry
	void callFromConstructor() { pureMethod(); }
	virtual void pureMethod()= 0;
};

struct PurecallDerived : public PurecallBase
{
	void pureMethod() override {}
};

__declspec(noinline) int overflowTheStack(volatile int depth)
{
	volatile char padding[4096];
	padding[0]= (char)depth;

	if (depth < 0)
		return 0;

	return overflowTheStack(depth + 1) + padding[0];
}
} // namespace
#endif // Windows

//-- public interface -----
namespace CrashHandler
{
#if defined WIN32 || defined _WIN32 || defined WINCE
bool install(const CrashHandlerSettings& settings)
{
	if (g_state.bInstalled)
		return true;

	std::error_code error;
	std::filesystem::create_directories(settings.reportDirectory, error);
	if (error)
	{
		MIKAN_LOG_ERROR("CrashHandler::install")
			<< "Failed to create crash report directory " << settings.reportDirectory.string();
		return false;
	}

	const std::filesystem::path reportDirectory= std::filesystem::absolute(settings.reportDirectory, error);
	if (!copyPathToBuffer(reportDirectory / "", g_state.reportDirectory, k_pathBufferSize))
	{
		MIKAN_LOG_ERROR("CrashHandler::install") << "Crash report directory path is too long";
		return false;
	}

	g_state.logFilePath[0]= L'\0';
	if (!settings.logFilePath.empty())
	{
		const std::filesystem::path logFilePath= std::filesystem::absolute(settings.logFilePath, error);
		copyPathToBuffer(logFilePath, g_state.logFilePath, k_pathBufferSize);
	}

	const std::string prefix= settings.appName + "_" + settings.appVersion;
	strncpy_s(g_state.narrowNamePrefix, k_prefixBufferSize, prefix.c_str(), _TRUNCATE);
	_snwprintf_s(g_state.namePrefix, k_prefixBufferSize, _TRUNCATE, L"%hs", g_state.narrowNamePrefix);

	if (!startWriterThread())
	{
		MIKAN_LOG_WARNING("CrashHandler::install")
			<< "Failed to start the crash writer thread; reports will be written on the crashing thread";
		stopWriterThread();
	}

	installHooks();
	g_state.bInstalled= true;

	MIKAN_LOG_INFO("CrashHandler::install") << "Crash reports are written to " << reportDirectory.string();

	return true;
}

void ensureInstalled()
{
	if (!g_state.bInstalled)
		return;

	if (!installHooks())
	{
		MIKAN_LOG_WARNING("CrashHandler::ensureInstalled") << "Crash hooks had been replaced and were re-installed";
	}
}

void uninstall()
{
	if (!g_state.bInstalled)
		return;

	SetUnhandledExceptionFilter(g_state.previousFilter);
	_set_invalid_parameter_handler(g_state.previousInvalidParameterHandler);
	_set_purecall_handler(g_state.previousPurecallHandler);
	signal(SIGABRT, SIG_DFL);

	stopWriterThread();
	g_state.bInstalled= false;
}

bool triggerTestCrash(const std::string& kind)
{
	if (kind == "access")
	{
		volatile int* nullPointer= nullptr;
		*nullPointer= 1;
	}
	else if (kind == "abort")
	{
		abort();
	}
	else if (kind == "terminate")
	{
		std::terminate();
	}
	else if (kind == "purecall")
	{
		PurecallDerived derived;
	}
	else if (kind == "invalidparam")
	{
		char tooSmall[4];
		strcpy_s(tooSmall, sizeof(tooSmall), "this string does not fit");
	}
	else if (kind == "stackoverflow")
	{
		overflowTheStack(0);
	}
	else
	{
		return false;
	}

	return true;
}
#else
bool install(const CrashHandlerSettings&) { return false; }
void ensureInstalled() {}
void uninstall() {}
bool triggerTestCrash(const std::string&) { return false; }
#endif // Windows

const char* getTestCrashKinds() { return k_testCrashKinds; }

std::filesystem::path findPendingReport(const std::filesystem::path& reportDirectory)
{
	std::ifstream marker(reportDirectory / k_pendingMarkerFileName);
	if (!marker.is_open())
		return {};

	std::string baseName;
	std::getline(marker, baseName);
	while (!baseName.empty() && (baseName.back() == '\r' || baseName.back() == '\n' || baseName.back() == ' '))
		baseName.pop_back();

	if (baseName.empty())
		return {};

	const std::filesystem::path dumpPath= reportDirectory / (baseName + ".dmp");
	std::error_code error;
	if (!std::filesystem::exists(dumpPath, error))
		return {};

	return dumpPath;
}

void clearPendingReport(const std::filesystem::path& reportDirectory)
{
	std::error_code error;
	std::filesystem::remove(reportDirectory / k_pendingMarkerFileName, error);
}
} // namespace CrashHandler

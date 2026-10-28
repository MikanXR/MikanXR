// -- includes -----
#include "GStreamerUtils.h"

#include <mutex>
#include <string>
#include <vector>

#if defined WIN32 || defined _WIN32 || defined WINCE
#include <windows.h>
#endif

// -- private helpers -----
namespace
{
#if defined WIN32 || defined _WIN32 || defined WINCE
std::wstring readEnvironmentVariable(const wchar_t* name)
{
	const DWORD length= GetEnvironmentVariableW(name, nullptr, 0);
	if (length == 0)
		return std::wstring();

	std::wstring value(length, L'\0');
	const DWORD written= GetEnvironmentVariableW(name, value.data(), length);
	value.resize(written);

	return value;
}

// The GStreamer MSI records its install directory here alongside the environment variable
std::wstring readRegistryInstallDir()
{
	const wchar_t* keyPath= L"SOFTWARE\\GStreamer1.0\\x86_64";
	const wchar_t* valueName= L"InstallDir";

	DWORD byteCount= 0;
	if (RegGetValueW(HKEY_LOCAL_MACHINE, keyPath, valueName, RRF_RT_REG_SZ, nullptr, nullptr, &byteCount)
		!= ERROR_SUCCESS)
		return std::wstring();

	std::wstring value(byteCount / sizeof(wchar_t), L'\0');
	if (RegGetValueW(HKEY_LOCAL_MACHINE, keyPath, valueName, RRF_RT_REG_SZ, nullptr, value.data(), &byteCount)
		!= ERROR_SUCCESS)
		return std::wstring();

	// byteCount includes the terminating null
	const size_t charCount= byteCount / sizeof(wchar_t);
	value.resize(charCount > 0 ? charCount - 1 : 0);

	return value;
}

// PATH entries may or may not carry a trailing separator
std::filesystem::path normalizeDir(const std::filesystem::path& dir)
{
	const std::filesystem::path normalized= dir.lexically_normal();
	return normalized.has_filename() ? normalized : normalized.parent_path();
}

bool pathListContains(const std::wstring& pathList, const std::filesystem::path& dir)
{
	const std::filesystem::path normalizedDir= normalizeDir(dir);

	size_t start= 0;
	while (start <= pathList.length())
	{
		size_t end= pathList.find(L';', start);
		if (end == std::wstring::npos)
			end= pathList.length();

		const std::wstring entry= pathList.substr(start, end - start);
		if (!entry.empty() && _wcsicmp(normalizeDir(entry).c_str(), normalizedDir.c_str()) == 0)
			return true;

		start= end + 1;
	}

	return false;
}

std::filesystem::path locateRuntimeDir()
{
	std::vector<std::filesystem::path> candidateRoots;

	const std::wstring envRoot= readEnvironmentVariable(L"GSTREAMER_1_0_ROOT_MINGW_X86_64");
	if (!envRoot.empty())
		candidateRoots.push_back(std::filesystem::path(envRoot));

	const std::wstring installDir= readRegistryInstallDir();
	if (!installDir.empty())
		candidateRoots.push_back(std::filesystem::path(installDir) / L"1.0" / L"mingw_x86_64");

	// A stale environment variable or a partly removed install fails this check
	// and falls through to the next candidate
	std::error_code error;
	for (const std::filesystem::path& root : candidateRoots)
	{
		const std::filesystem::path binDir= root / L"bin";
		if (std::filesystem::exists(binDir / L"libgstreamer-1.0-0.dll", error))
			return binDir;
	}

	return std::filesystem::path();
}

// The plugins link GStreamer implicitly, so its DLLs resolve through the standard
// search order. PATH is used rather than AddDllDirectory, which only applies to
// loads that opt into it, and the gst-plugin-scanner child process inherits PATH.
void prependToProcessPath(const std::filesystem::path& dir)
{
	const std::wstring pathList= readEnvironmentVariable(L"PATH");
	if (pathListContains(pathList, dir))
		return;

	const std::wstring newPathList= pathList.empty() ? dir.wstring() : dir.wstring() + L";" + pathList;
	SetEnvironmentVariableW(L"PATH", newPathList.c_str());
}
#endif
} // namespace

// -- public methods -----
namespace GStreamerUtils
{
bool ensureRuntimeLoadable(std::filesystem::path& outRuntimeDir)
{
#if defined WIN32 || defined _WIN32 || defined WINCE
	// Located once per process: both GStreamer-backed plugin loaders call this
	static std::once_flag s_onceFlag;
	static std::filesystem::path s_runtimeDir;
	std::call_once(s_onceFlag,
				   []()
				   {
					   s_runtimeDir= locateRuntimeDir();
					   if (!s_runtimeDir.empty())
						   prependToProcessPath(s_runtimeDir);
				   });

	outRuntimeDir= s_runtimeDir;
	return !s_runtimeDir.empty();
#else
	outRuntimeDir.clear();
	return true;
#endif
}
}; // namespace GStreamerUtils

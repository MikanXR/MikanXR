// -- includes -----
#include "PathUtils.h"

#include <algorithm>
#include "StringUtils.h"

#include <assert.h>
#include <filesystem>

#if defined WIN32 || defined _WIN32 || defined WINCE
#include <windows.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <time.h>
#include <unistd.h>
#endif

// -- public methods -----
namespace PathUtils
{
void addDllSearchDirectory(const std::filesystem::path& dllPath)
{
#if defined WIN32 || defined _WIN32 || defined WINCE
	HMODULE hKernel32= GetModuleHandle("kernel32.dll");

	if (hKernel32 != NULL)
	{
		typedef DLL_DIRECTORY_COOKIE(WINAPI * AddDllDirectoryFunc)(PCWSTR);
		AddDllDirectoryFunc addDllDirectory= (AddDllDirectoryFunc)GetProcAddress(hKernel32, "AddDllDirectory");

		if (addDllDirectory != NULL)
		{
			const std::wstring wideDllPath= StringUtils::convertUTF8StringToWString(dllPath.string());

			addDllDirectory(wideDllPath.c_str());
		}
	}
#endif
}

std::filesystem::path getModulePath()
{
	std::filesystem::path moduleFilePath;

#if defined WIN32 || defined _WIN32 || defined WINCE
	char result[MAX_PATH];

	GetModuleFileName(NULL, result, MAX_PATH);
	moduleFilePath= std::filesystem::path(result);
#else
	moduleFilePath= std::filesystem::canonical("/proc/self/exe");
#endif

	return moduleFilePath.parent_path();
}

std::filesystem::path getResourceDirectory()
{
	std::filesystem::path resourceDir= std::filesystem::current_path();
	resourceDir/= std::string("resources");

	return resourceDir;
}

std::filesystem::path getFontPath(const std::string& fontName)
{
	const std::string fileFilename= fontName + ".ttf";

	return getResourceDirectory() / "font" / fileFilename;
}

std::filesystem::path makeAbsoluteResourceFilePath(const std::filesystem::path& relative_path)
{
	if (relative_path.is_absolute())
		return relative_path;

	std::filesystem::path full_path(getResourceDirectory());
	full_path/= relative_path;

	return full_path;
}

std::filesystem::path getHomeDirectory()
{
	std::filesystem::path home_dir;

#if defined WIN32 || defined _WIN32 || defined WINCE
	size_t homedir_buffer_req_size;
	char homedir_buffer[512];
	getenv_s(&homedir_buffer_req_size, homedir_buffer, "APPDATA");
	assert(homedir_buffer_req_size <= sizeof(homedir_buffer));
	home_dir= homedir_buffer;
#else
	// if run as root, use system-wide data directory
	if (geteuid() == 0)
	{
		home_dir= "/etc/";
	}
	else
	{
		homedir= getenv("HOME");
	}
#endif
	return home_dir;
}

std::filesystem::path getLocalDataDirectory()
{
	std::filesystem::path local_dir;

#if defined WIN32 || defined _WIN32 || defined WINCE
	size_t buffer_req_size= 0;
	char buffer[512];
	getenv_s(&buffer_req_size, buffer, "LOCALAPPDATA");
	assert(buffer_req_size <= sizeof(buffer));
	local_dir= buffer;

	// A machine with no LOCALAPPDATA is not one this runs on, but falling back
	// to the roaming location beats returning an empty path to a caller about
	// to create directories under it.
	if (local_dir.empty())
		local_dir= getHomeDirectory();
#else
	const char* xdgCacheHome= getenv("XDG_CACHE_HOME");
	if (xdgCacheHome != nullptr && xdgCacheHome[0] != '\0')
	{
		local_dir= xdgCacheHome;
	}
	else
	{
		local_dir= getHomeDirectory() / ".cache";
	}
#endif
	return local_dir;
}

// Static variable to track the current project directory
static std::filesystem::path g_projectDirectory;

std::filesystem::path getProjectDirectory() { return g_projectDirectory; }

void setProjectDirectory(const std::filesystem::path& projectDir) { g_projectDirectory= projectDir; }

std::filesystem::path getProjectsRootDirectory()
{
	std::filesystem::path projectsRoot;

#if defined WIN32 || defined _WIN32 || defined WINCE
	size_t homedir_buffer_req_size;
	char homedir_buffer[512];
	getenv_s(&homedir_buffer_req_size, homedir_buffer, "USERPROFILE");
	assert(homedir_buffer_req_size <= sizeof(homedir_buffer));
	projectsRoot= homedir_buffer;
	projectsRoot/= "Documents";
#else
	const char* homedir= getenv("HOME");
	if (homedir)
	{
		projectsRoot= homedir;
	}
	else
	{
		projectsRoot= "/";
	}
#endif
	projectsRoot/= "MikanXR";

	// Create the directory if it doesn't exist
	if (!std::filesystem::exists(projectsRoot))
	{
		std::filesystem::create_directories(projectsRoot);
	}

	return projectsRoot;
}

std::string makeStoredProjectPath(const std::filesystem::path& assetPath)
{
	std::filesystem::path storedPath= assetPath.lexically_normal();

	const std::filesystem::path projectDirectory= getProjectDirectory().lexically_normal();
	if (!projectDirectory.empty())
	{
		const std::filesystem::path relativePath= storedPath.lexically_relative(projectDirectory);
		if (!relativePath.empty() && relativePath.begin()->string() != "..")
		{
			storedPath= relativePath;
		}
	}

	std::string universalPath= storedPath.string();
	std::replace(universalPath.begin(), universalPath.end(), '\\', '/');

	return universalPath;
}

std::filesystem::path resolveProjectResource(const std::filesystem::path& path)
{
	if (!path.empty())
	{
		if (path.is_absolute())
		{
			if (std::filesystem::exists(path))
			{
				return path;
			}
		}
		else
		{
			// Check project folder first
			if (!g_projectDirectory.empty())
			{
				std::filesystem::path projectResourcePath= g_projectDirectory / path;
				if (std::filesystem::exists(projectResourcePath))
				{
					return projectResourcePath;
				}
			}

			// Fall	back to app resource folder
			std::filesystem::path appResourcePath= getResourceDirectory() / path;
			if (std::filesystem::exists(appResourcePath))
			{
				return appResourcePath;
			}
		}
	}

	return std::filesystem::path();
}

std::vector<std::string> listFilenamesInDirectory(const std::filesystem::path& path,
												  const std::string& extension_filter)
{
	std::vector<std::string> filenames;

	for (auto const& dir_entry : std::filesystem::directory_iterator{path})
	{
		if (dir_entry.is_regular_file())
		{
			const std::filesystem::path& filename= dir_entry.path().filename();

			if (extension_filter.empty() || filename.extension() == extension_filter)
			{
				filenames.push_back(filename.string());
			}
		}
	}

	return filenames;
}

std::vector<std::string> listDirectoriesInDirectory(const std::filesystem::path& path)
{
	std::vector<std::string> dirnames;

	for (auto const& dir_entry : std::filesystem::directory_iterator{path})
	{
		if (dir_entry.is_directory())
		{
			const std::filesystem::path& filename= dir_entry.path().filename();

			dirnames.push_back(filename.string());
		}
	}

	return dirnames;
}

std::vector<std::string> listVolumes()
{
	std::vector<std::string> result;

#if defined WIN32 || defined _WIN32 || defined WINCE
	char szLogicalDrives[MAX_PATH]= {0};
	DWORD dwResult= GetLogicalDriveStringsA(MAX_PATH, szLogicalDrives);
	if (dwResult > 0 && dwResult <= MAX_PATH)
	{
		char* szSingleDrive= szLogicalDrives;
		while (*szSingleDrive)
		{
			result.push_back(szSingleDrive);

			// get the next drive
			szSingleDrive+= strlen(szSingleDrive) + 1;
		}
	}
#else
	// TODO: Just list the root directory for now unix systems
	result.push_back("/");
#endif

	return result;
}

std::string makeTimestampedFileName(const std::string& prefix, const std::string& suffix)
{
	time_t t= time(0);
	struct tm* now= localtime(&t);

	char dateTimeString[128];
	strftime(dateTimeString, 80, "%Y_%m_%d_%H_%M_%S", now);

	return StringUtils::stringify(prefix, "_", dateTimeString, suffix);
}

std::filesystem::path makeTimestampedFilePath(const std::filesystem::path& parentDir, const std::string& prefix,
											  const std::string& suffix)
{
	const std::string filename= makeTimestampedFileName(prefix, suffix);

	std::filesystem::path result= parentDir;
	result/= filename;

	return result;
}

std::string createTrimmedPathString(const std::filesystem::path& path, const size_t maxLength)
{
	const std::string pathString= path.string();

	if (pathString.length() > maxLength)
	{
		return "..." + pathString.substr(pathString.length() - maxLength);
	}
	else
	{
		return pathString;
	}
}
}; // namespace PathUtils
#pragma once

#include "MikanUtilityExport.h"

#include <filesystem>
#include <string>
#include <vector>

//-- utility methods -----
namespace PathUtils
{
// Add the given directory to the DLL search path
MIKAN_UTILITY_FUNC(void) addDllSearchDirectory(const std::filesystem::path& dllPath);

/// Get the location of the owning module path
MIKAN_UTILITY_FUNC(std::filesystem::path) getModulePath();

/// Get the location of resource files
MIKAN_UTILITY_FUNC(std::filesystem::path) getResourceDirectory();

MIKAN_UTILITY_FUNC(std::filesystem::path) getFontPath(const std::string& fontName);

/// Make a relative
MIKAN_UTILITY_FUNC(std::filesystem::path) makeAbsoluteResourceFilePath(const std::filesystem::path& relative_path);

/// Get the "home" location where config files can be stored
MIKAN_UTILITY_FUNC(std::filesystem::path) getHomeDirectory();

/// Get the per-machine location for large generated or downloaded data
/// (%LOCALAPPDATA% on Windows). Deliberately not getHomeDirectory(), which is
/// %APPDATA% and roams: several gigabytes of ML models must not follow a
/// roaming profile between machines.
MIKAN_UTILITY_FUNC(std::filesystem::path) getLocalDataDirectory();

/// Get the directory of the currently loaded project (empty if no project is loaded)
MIKAN_UTILITY_FUNC(std::filesystem::path) getProjectDirectory();

/// The stored form of a project asset path: forward slashes, and relative to the
/// project directory when the file sits under it, so resolveProjectResource finds it again
MIKAN_UTILITY_FUNC(std::string) makeStoredProjectPath(const std::filesystem::path& assetPath);

/// Set the project directory (called by ProjectManager when loading/creating a project)
MIKAN_UTILITY_FUNC(void) setProjectDirectory(const std::filesystem::path& projectDir);

/// Get the root directory for all projects (%USERPROFILE%/Documents/MikanXR)
MIKAN_UTILITY_FUNC(std::filesystem::path) getProjectsRootDirectory();

/// Resolve a resource path: check project folder first, fall back to bundled resources/
MIKAN_UTILITY_FUNC(std::filesystem::path) resolveProjectResource(const std::filesystem::path& path);

/// Attempts to build a list of all of the files in a directory
MIKAN_UTILITY_FUNC(std::vector<std::string>) listFilenamesInDirectory(
	const std::filesystem::path& path, const std::string& extension_filter= std::string());
MIKAN_UTILITY_FUNC(std::vector<std::string>) listDirectoriesInDirectory(const std::filesystem::path& path);
MIKAN_UTILITY_FUNC(std::vector<std::string>) listVolumes();

// Strips the extension from a filename
MIKAN_UTILITY_FUNC(std::string) removeFileExtension(std::string& filename);

// Create a unique timestamped filename
MIKAN_UTILITY_FUNC(std::string) makeTimestampedFileName(const std::string& prefix, const std::string& suffix);

// Create a unique timestamped filepath
MIKAN_UTILITY_FUNC(std::filesystem::path) makeTimestampedFilePath(const std::filesystem::path& parentDir,
																  const std::string& prefix, const std::string& suffix);

// Create a string from a path but trims it to a max character limit
// Prefixes the resulting string with "..." if max length is exceeded
MIKAN_UTILITY_FUNC(std::string) createTrimmedPathString(const std::filesystem::path& path, const size_t maxLength);

// Convert a filesystem path to a UTF-8 encoded std::string.
// Use this (NOT path::string(), which yields the active code page on Windows) whenever a path
// crosses a UTF-8 boundary, e.g. JSON/binary serialization or a Serialization::String/MikanVariant.
// Keep using path::string() for OS file APIs (fopen, libharu, etc.) which expect the active code page.
// Header-only so the std::u8string (C++20) vs std::string (C++17) handling resolves per translation unit.
inline std::string pathToUtf8(const std::filesystem::path& path)
{
#if defined(__cpp_lib_char8_t)
	const std::u8string utf8= path.u8string();
	return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.length());
#else
	return path.u8string();
#endif
}

inline std::filesystem::path utf8ToPath(const std::string& utf8)
{
#if defined(__cpp_lib_char8_t)
	// C++20 and later: Convert std::string to std::u8string first
	return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
#else
	// C++17 fallback: Use the now-deprecated u8path
	return std::filesystem::u8path(utf8);
#endif
}

inline std::string utf8CStrToPathString(const char* utf8Value)
{
	const std::string utf8String(utf8Value);
	std::filesystem::path path= utf8ToPath(utf8String);

	return path.string();
}

}; // namespace PathUtils

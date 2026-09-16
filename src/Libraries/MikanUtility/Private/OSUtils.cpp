// -- includes -----
#include "OSUtils.h"

#if defined WIN32 || defined _WIN32 || defined WINCE
#include <windows.h>
#include <shellapi.h>
#else
#include <cstdlib>
#endif

// -- private helpers -----
namespace
{
// Splits "exe [args]" on the first space, the rule openPathsWithApplication
// and runCommandLine both split a command line on.
void splitCommandLine(const std::string& commandLine, std::string& outExe, std::string& outArgs)
{
	const size_t spacePos= commandLine.find(' ');
	outExe= commandLine.substr(0, spacePos);
	outArgs= (spacePos != std::string::npos) ? commandLine.substr(spacePos + 1) : "";
}

// Launches an already-split executable and argument string, the shared tail
// end of openPathsWithApplication and runCommandLine.
bool launchExeWithArgs(const std::string& exe, const std::string& args)
{
#if defined WIN32 || defined _WIN32 || defined WINCE
	// ShellExecute gets exe and parameters separately (avoids a console window)
	HINSTANCE result= ShellExecuteA(NULL, "open", exe.c_str(), args.c_str(), NULL, SW_SHOWNORMAL);
	return reinterpret_cast<intptr_t>(result) > 32;
#else
	const std::string cmd= args.empty() ? exe : exe + " " + args;
	return system(cmd.c_str()) == 0;
#endif
}
} // namespace

// -- public methods -----
namespace OSUtils
{
bool openFileWithApplication(const std::filesystem::path& filePath, const std::string& editorCommand)
{
	return openPathsWithApplication({filePath}, editorCommand);
}

bool openPathsWithApplication(const std::vector<std::filesystem::path>& paths, const std::string& editorCommand)
{
	if (paths.empty())
		return false;

	if (editorCommand.empty())
		return openFileWithDefaultApplication(paths.back());

	std::string quotedPaths;
	for (const std::filesystem::path& path : paths)
	{
		if (!quotedPaths.empty())
			quotedPaths+= " ";
		quotedPaths+= "\"" + path.generic_string() + "\"";
	}

	std::string exe, existingArgs;
	splitCommandLine(editorCommand, exe, existingArgs);
	const std::string fullArgs= existingArgs.empty() ? quotedPaths : existingArgs + " " + quotedPaths;

	return launchExeWithArgs(exe, fullArgs);
}

bool runCommandLine(const std::string& commandLine)
{
	std::string exe, args;
	splitCommandLine(commandLine, exe, args);

	return launchExeWithArgs(exe, args);
}

bool openFileWithDefaultApplication(const std::filesystem::path& filePath)
{
#if defined WIN32 || defined _WIN32 || defined WINCE
	// Use ShellExecute on Windows
	HINSTANCE result= ShellExecuteA(NULL, "open", filePath.string().c_str(), NULL, NULL, SW_SHOWNORMAL);

	// ShellExecute returns a value greater than 32 on success
	return reinterpret_cast<intptr_t>(result) > 32;
#elif defined(__APPLE__)
	// Use 'open' command on macOS
	std::string command= "open \"" + filePath.string() + "\"";
	return system(command.c_str()) == 0;
#else
	// Use 'xdg-open' command on Linux
	std::string command= "xdg-open \"" + filePath.string() + "\"";
	return system(command.c_str()) == 0;
#endif
}
}; // namespace OSUtils

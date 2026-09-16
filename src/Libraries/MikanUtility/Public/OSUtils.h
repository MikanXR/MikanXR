#pragma once

#include "MikanUtilityExport.h"

#include <filesystem>
#include <string>
#include <vector>

//-- utility methods -----
namespace OSUtils
{
/// Open a file with the default system application
/// On Windows: uses ShellExecute
/// On macOS/Linux: can be extended to use open/xdg-open
MIKAN_UTILITY_FUNC(bool) openFileWithDefaultApplication(const std::filesystem::path& filePath);

/// Open a file with a specific editor command (e.g. "code --reuse-window").
/// The file path is appended as a quoted argument after the command.
/// Falls back to openFileWithDefaultApplication when editorCommand is empty.
MIKAN_UTILITY_FUNC(bool) openFileWithApplication(const std::filesystem::path& filePath,
												 const std::string& editorCommand);

/// Open several paths with one editor command, each appended as a quoted
/// argument in order. A folder followed by a file inside it is how VS Code and
/// Sublime style launchers open a workspace with that file focused. With an
/// empty editorCommand only the last path opens, with the default application.
MIKAN_UTILITY_FUNC(bool) openPathsWithApplication(const std::vector<std::filesystem::path>& paths,
												  const std::string& editorCommand);

/// Launch a command line as-is, splitting the executable from its arguments
/// the same way openPathsWithApplication does. For a caller that has already
/// expanded any placeholders in the command (e.g. a script editor command
/// with {file}/{line} substituted) rather than appending paths after it.
MIKAN_UTILITY_FUNC(bool) runCommandLine(const std::string& commandLine);
}; // namespace OSUtils

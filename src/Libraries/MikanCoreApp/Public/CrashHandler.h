#pragma once

#include "MikanCoreAppExport.h"

#include <filesystem>
#include <string>

//-- definitions -----
struct CrashHandlerSettings
{
	// Where the report files land. Created at install time.
	std::filesystem::path reportDirectory;

	// The process log, copied next to the dump so a report carries the lines
	// leading up to the crash. Empty means no copy.
	std::filesystem::path logFilePath;

	// File name prefix of each report, "<appName>_<appVersion>_<timestamp>"
	std::string appName;
	std::string appVersion;
};

//-- utility methods -----
namespace CrashHandler
{
/// Installs the process crash hooks. Every report is written to the report
/// directory as a minidump (.dmp), a text summary (.txt), a copy of the log
/// (.log), and a pending_report.txt marker naming the newest report.
/// Windows only: elsewhere this returns false and installs nothing.
MIKAN_COREAPP_FUNC(bool) install(const CrashHandlerSettings& settings);

/// Re-asserts every hook. Libraries initialized after install (Chromium in
/// particular) replace the unhandled exception filter and the CRT's
/// invalid-parameter, purecall, and abort hooks with their own.
MIKAN_COREAPP_FUNC(void) ensureInstalled();

/// Restores the hooks that were in place before install and stops the writer thread
MIKAN_COREAPP_FUNC(void) uninstall();

/// Crashes the process on purpose to exercise a report path. Kinds:
/// access, abort, terminate, purecall, invalidparam, stackoverflow.
/// Returns false without crashing when the kind is unknown.
MIKAN_COREAPP_FUNC(bool) triggerTestCrash(const std::string& kind);

/// The crash kinds triggerTestCrash accepts, space separated, for usage text
MIKAN_COREAPP_FUNC(const char*) getTestCrashKinds();

/// The .dmp path named by the report directory's pending marker, or an empty
/// path when there is no marker
MIKAN_COREAPP_FUNC(std::filesystem::path) findPendingReport(const std::filesystem::path& reportDirectory);

/// Removes the pending marker once the user has been told about the report
MIKAN_COREAPP_FUNC(void) clearPendingReport(const std::filesystem::path& reportDirectory);
}; // namespace CrashHandler

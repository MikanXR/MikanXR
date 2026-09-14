//-- includes -----
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "CrashHandler.h"
#include "unit_test.h"

#include <filesystem>
#include <fstream>

//-- private helpers -----
namespace
{
// A scratch report directory that disappears with the test
struct ScopedReportDirectory
{
	ScopedReportDirectory()
	{
		m_path= std::filesystem::temp_directory_path() / "mikan_crash_handler_tests";
		std::filesystem::remove_all(m_path);
	}

	~ScopedReportDirectory()
	{
		std::error_code ignored;
		std::filesystem::remove_all(m_path, ignored);
	}

	void writeFile(const std::string& name, const std::string& content) const
	{
		std::filesystem::create_directories(m_path);
		std::ofstream file(m_path / name, std::ios::binary);
		file << content;
	}

	std::filesystem::path m_path;
};
} // namespace

//-- public interface -----
bool run_crash_handler_unit_tests()
{
	UNIT_TEST_MODULE_BEGIN("crash_handler")
	UNIT_TEST_MODULE_CALL_TEST(crash_handler_test_install_creates_report_directory);
	UNIT_TEST_MODULE_CALL_TEST(crash_handler_test_pending_report_round_trip);
	UNIT_TEST_MODULE_CALL_TEST(crash_handler_test_pending_report_without_dump);
	UNIT_TEST_MODULE_CALL_TEST(crash_handler_test_unknown_crash_kind);
	UNIT_TEST_MODULE_END()
}

//-- private functions -----
bool crash_handler_test_install_creates_report_directory()
{
	UNIT_TEST_BEGIN("install creates the report directory")

	ScopedReportDirectory scratch;

	CrashHandlerSettings settings= {};
	settings.reportDirectory= scratch.m_path / "reports";
	settings.appName= "UnitTest";
	settings.appVersion= "0.0.0.0";

#if defined WIN32 || defined _WIN32
	success= CrashHandler::install(settings);
	assert(success);

	success&= std::filesystem::is_directory(settings.reportDirectory);
	assert(success);

	// A second install is a no-op rather than a second set of hooks
	success&= CrashHandler::install(settings);
	assert(success);

	CrashHandler::uninstall();
#else
	success= !CrashHandler::install(settings);
	assert(success);
#endif

	UNIT_TEST_COMPLETE()
}

bool crash_handler_test_pending_report_round_trip()
{
	UNIT_TEST_BEGIN("pending report marker round trip")

	ScopedReportDirectory scratch;

	// No marker, no report
	success= CrashHandler::findPendingReport(scratch.m_path).empty();
	assert(success);

	// The marker names a report whose dump exists
	scratch.writeFile("pending_report.txt", "UnitTest_0.0.0.0_2026-09-13_12-00-00\r\n");
	scratch.writeFile("UnitTest_0.0.0.0_2026-09-13_12-00-00.dmp", "not really a dump");

	const std::filesystem::path expected= scratch.m_path / "UnitTest_0.0.0.0_2026-09-13_12-00-00.dmp";
	success&= CrashHandler::findPendingReport(scratch.m_path) == expected;
	assert(success);

	// Clearing removes the marker and only the marker
	CrashHandler::clearPendingReport(scratch.m_path);
	success&= CrashHandler::findPendingReport(scratch.m_path).empty();
	assert(success);
	success&= std::filesystem::exists(expected);
	assert(success);

	// Clearing again is harmless
	CrashHandler::clearPendingReport(scratch.m_path);

	UNIT_TEST_COMPLETE()
}

bool crash_handler_test_pending_report_without_dump()
{
	UNIT_TEST_BEGIN("pending report marker without its dump")

	ScopedReportDirectory scratch;

	// A marker whose dump is gone names nothing
	scratch.writeFile("pending_report.txt", "UnitTest_0.0.0.0_2026-09-13_12-00-00");
	success= CrashHandler::findPendingReport(scratch.m_path).empty();
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool crash_handler_test_unknown_crash_kind()
{
	UNIT_TEST_BEGIN("unknown crash kind is refused")

	success= !CrashHandler::triggerTestCrash("bogus");
	assert(success);

	success&= CrashHandler::getTestCrashKinds() != nullptr && CrashHandler::getTestCrashKinds()[0] != '\0';
	assert(success);

	UNIT_TEST_COMPLETE()
}

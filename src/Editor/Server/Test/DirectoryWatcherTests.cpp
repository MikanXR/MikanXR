#include "DirectoryWatcherTests.h"
#include "unit_test.h"

#include "DirectoryWatcher.h"

#include <assert.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
// A throwaway folder under the OS temp directory, removed on the way out
class ScopedTempDir
{
public:
	ScopedTempDir(const std::string& name)
	{
		m_dir= std::filesystem::temp_directory_path() / name;
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
	}

	~ScopedTempDir()
	{
		std::error_code ec;
		std::filesystem::remove_all(m_dir, ec);
	}

	const std::filesystem::path& path() const { return m_dir; }

private:
	std::filesystem::path m_dir;
};

void writeFile(const std::filesystem::path& filePath, const std::string& content)
{
	std::filesystem::create_directories(filePath.parent_path());

	std::ofstream file(filePath, std::ios::binary);
	file << content;
}

bool containsPath(const std::vector<std::filesystem::path>& paths, const std::filesystem::path& target)
{
	for (const std::filesystem::path& path : paths)
	{
		if (path == target)
		{
			return true;
		}
	}

	return false;
}

// Pumps poll() until it reports a change or the deadline elapses
bool pollUntilReported(DirectoryWatcher& watcher, DirectoryWatcher::ChangeSet& outChanges)
{
	const auto deadline= std::chrono::steady_clock::now() + std::chrono::seconds(5);

	while (std::chrono::steady_clock::now() < deadline)
	{
		if (watcher.poll(0.05f, outChanges))
		{
			return true;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	return false;
}
} // namespace

bool run_directory_watcher_tests()
{
	UNIT_TEST_MODULE_BEGIN("directory_watcher")
	UNIT_TEST_MODULE_CALL_TEST(directory_watcher_test_diff_is_pure);
	UNIT_TEST_MODULE_CALL_TEST(directory_watcher_test_scan_filters_extension);
	UNIT_TEST_MODULE_CALL_TEST(directory_watcher_test_reports_changes);
	UNIT_TEST_MODULE_CALL_TEST(directory_watcher_test_debounces);
	UNIT_TEST_MODULE_END()
}

bool directory_watcher_test_diff_is_pure()
{
	UNIT_TEST_BEGIN("diff reports added, removed, modified, and a rename as removed plus added")

	const auto now= std::filesystem::file_time_type::clock::now();

	DirectoryWatcher::Snapshot before;
	before[std::filesystem::path("unchanged.lua")]= now;
	before[std::filesystem::path("modified.lua")]= now;
	before[std::filesystem::path("removed.lua")]= now;
	before[std::filesystem::path("old_name.lua")]= now;

	DirectoryWatcher::Snapshot after;
	after[std::filesystem::path("unchanged.lua")]= now;
	after[std::filesystem::path("modified.lua")]= now + std::chrono::seconds(5);
	after[std::filesystem::path("added.lua")]= now;
	after[std::filesystem::path("new_name.lua")]= now;

	DirectoryWatcher::ChangeSet changes;
	DirectoryWatcher::diff(before, after, changes);

	// A rename is a removed old name plus an added new name, so this is two of each
	success&= changes.added.size() == 2;
	success&= containsPath(changes.added, "added.lua");
	success&= containsPath(changes.added, "new_name.lua");

	success&= changes.removed.size() == 2;
	success&= containsPath(changes.removed, "removed.lua");
	success&= containsPath(changes.removed, "old_name.lua");

	success&= changes.modified.size() == 1;
	success&= containsPath(changes.modified, "modified.lua");

	success&= !containsPath(changes.added, "unchanged.lua");
	success&= !containsPath(changes.removed, "unchanged.lua");
	success&= !containsPath(changes.modified, "unchanged.lua");

	assert(success);

	UNIT_TEST_COMPLETE()
}

bool directory_watcher_test_scan_filters_extension()
{
	UNIT_TEST_BEGIN("scan finds only the requested extension, case-insensitively, recursing into subfolders")

	ScopedTempDir dir("mikan_directory_watcher_test_scan");
	writeFile(dir.path() / "a.lua", "return 1\n");
	writeFile(dir.path() / "b.txt", "not lua\n");
	writeFile(dir.path() / "sub" / "c.LUA", "return 2\n");

	DirectoryWatcher::Snapshot snapshot;
	DirectoryWatcher::scan(dir.path(), ".lua", snapshot);

	success&= snapshot.size() == 2;
	success&= snapshot.find(dir.path() / "a.lua") != snapshot.end();
	success&= snapshot.find(dir.path() / "sub" / "c.LUA") != snapshot.end();
	success&= snapshot.find(dir.path() / "b.txt") == snapshot.end();

	assert(success);

	UNIT_TEST_COMPLETE()
}

bool directory_watcher_test_reports_changes()
{
	UNIT_TEST_BEGIN("watch/poll reports a created, then modified, then removed file")

	ScopedTempDir dir("mikan_directory_watcher_test_reports");

	DirectoryWatcher watcher;
	success&= watcher.watch(dir.path(), ".lua");
	assert(success);

	const std::filesystem::path scriptPath= dir.path() / "a.lua";

	// Created
	writeFile(scriptPath, "return 1\n");
	DirectoryWatcher::ChangeSet changes;
	bool bReported= pollUntilReported(watcher, changes);
	success&= bReported;
	assert(bReported && "watcher never reported the created file within the 5s deadline");
	success&= containsPath(changes.added, scriptPath);

	// Modified: rewrite the content and bump the write time explicitly, since the
	// filesystem's write-time resolution can otherwise land in the same tick
	writeFile(scriptPath, "return 2\n");
	std::error_code ec;
	const std::filesystem::file_time_type currentTime= std::filesystem::last_write_time(scriptPath, ec);
	std::filesystem::last_write_time(scriptPath, currentTime + std::chrono::seconds(1), ec);
	bReported= pollUntilReported(watcher, changes);
	success&= bReported;
	assert(bReported && "watcher never reported the modified file within the 5s deadline");
	success&= containsPath(changes.modified, scriptPath);

	// Removed
	std::filesystem::remove(scriptPath, ec);
	bReported= pollUntilReported(watcher, changes);
	success&= bReported;
	assert(bReported && "watcher never reported the removed file within the 5s deadline");
	success&= containsPath(changes.removed, scriptPath);

	UNIT_TEST_COMPLETE()
}

bool directory_watcher_test_debounces()
{
	UNIT_TEST_BEGIN("two writes inside the debounce window collapse into one report")

	ScopedTempDir dir("mikan_directory_watcher_test_debounce");

	DirectoryWatcher watcher;
	success&= watcher.watch(dir.path(), ".lua");
	assert(success);

	const std::filesystem::path scriptPath= dir.path() / "a.lua";

	writeFile(scriptPath, "return 1\n");
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	writeFile(scriptPath, "return 2\n");

	DirectoryWatcher::ChangeSet changes;
	bool bReported= pollUntilReported(watcher, changes);
	success&= bReported;
	assert(bReported && "watcher never reported the debounced burst within the 5s deadline");

	// Both writes are one file appearing for the first time, so it names the file once
	const size_t totalNamedPaths= changes.added.size() + changes.modified.size();
	success&= totalNamedPaths == 1;
	success&= containsPath(changes.added, scriptPath) || containsPath(changes.modified, scriptPath);

	// Pumping further should find nothing more to report for the same burst
	for (int i= 0; i < 5 && success; ++i)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		DirectoryWatcher::ChangeSet extraChanges;
		success&= !watcher.poll(0.05f, extraChanges);
	}

	assert(success);

	UNIT_TEST_COMPLETE()
}

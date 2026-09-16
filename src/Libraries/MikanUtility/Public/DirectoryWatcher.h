#pragma once

#include "MikanUtilityExport.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

// Recursive, extension-filtered watch of a directory, serviced by a per-frame poll
// rather than a background thread. Backed by FindFirstChangeNotification on Windows.
// A burst of OS signals debounces into one rescan, which is diffed against the last
// snapshot to report what changed.
class MIKAN_UTILITY_CLASS DirectoryWatcher
{
public:
	struct ChangeSet
	{
		std::vector<std::filesystem::path> added;
		std::vector<std::filesystem::path> removed;
		std::vector<std::filesystem::path> modified;

		inline bool empty() const { return added.empty() && removed.empty() && modified.empty(); }
	};

	DirectoryWatcher();
	~DirectoryWatcher();
	DirectoryWatcher(const DirectoryWatcher&)= delete;
	DirectoryWatcher& operator=(const DirectoryWatcher&)= delete;

	// Recursive watch of every file whose extension matches (".lua"). Takes the
	// initial snapshot. False when the directory does not exist or the OS handle fails.
	bool watch(const std::filesystem::path& directory, const std::string& extension);
	void stop();
	bool isWatching() const;
	void setDebounceSeconds(float seconds); // default 0.25f

	// Call once per frame. Services the OS signal with WaitForSingleObject(handle, 0),
	// marks a change pending on a signal, and once no signal has arrived for the
	// debounce window rescans, diffs against the last snapshot, and returns true
	// with the diff. Returns false otherwise.
	bool poll(float deltaSeconds, ChangeSet& outChanges);

	// Rescan now, replacing the snapshot without reporting; for a forced refresh
	void takeSnapshot();

	// Pure helpers, public so tests can drive them without the OS
	using Snapshot= std::map<std::filesystem::path, std::filesystem::file_time_type>;
	static void scan(const std::filesystem::path& directory, const std::string& extension, Snapshot& outSnapshot);
	static void diff(const Snapshot& before, const Snapshot& after, ChangeSet& outChanges);

private:
	std::filesystem::path m_directory;
	std::string m_extension;
	void* m_changeHandle= nullptr; // HANDLE from FindFirstChangeNotificationW
	Snapshot m_snapshot;
	bool m_bChangePending= false;
	float m_quietSeconds= 0.f;
	float m_debounceSeconds= 0.25f;
};

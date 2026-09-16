// -- includes -----
#include "DirectoryWatcher.h"

#include <algorithm>
#include <cctype>

#if defined WIN32 || defined _WIN32 || defined WINCE
#include <windows.h>
#endif

namespace
{
std::string toLower(const std::string& value)
{
	std::string result= value;
	std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return (char)std::tolower(c); });

	return result;
}
} // namespace

// -- DirectoryWatcher -----
DirectoryWatcher::DirectoryWatcher() {}

DirectoryWatcher::~DirectoryWatcher() { stop(); }

bool DirectoryWatcher::watch(const std::filesystem::path& directory, const std::string& extension)
{
	stop();

#if defined WIN32 || defined _WIN32 || defined WINCE
	std::error_code ec;
	if (!std::filesystem::is_directory(directory, ec))
	{
		return false;
	}

	HANDLE handle= FindFirstChangeNotificationW(directory.c_str(), TRUE,
												FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME
													| FILE_NOTIFY_CHANGE_LAST_WRITE);
	if (handle == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	m_directory= directory;
	m_extension= extension;
	m_changeHandle= (void*)handle;
	m_bChangePending= false;
	m_quietSeconds= 0.f;
	takeSnapshot();

	return true;
#else
	return false;
#endif
}

void DirectoryWatcher::stop()
{
#if defined WIN32 || defined _WIN32 || defined WINCE
	if (m_changeHandle != nullptr)
	{
		FindCloseChangeNotification((HANDLE)m_changeHandle);
	}
#endif

	m_changeHandle= nullptr;
	m_directory.clear();
	m_extension.clear();
	m_snapshot.clear();
	m_bChangePending= false;
	m_quietSeconds= 0.f;
}

bool DirectoryWatcher::isWatching() const { return m_changeHandle != nullptr; }

void DirectoryWatcher::setDebounceSeconds(float seconds) { m_debounceSeconds= seconds; }

bool DirectoryWatcher::poll(float deltaSeconds, ChangeSet& outChanges)
{
	outChanges= ChangeSet();

#if defined WIN32 || defined _WIN32 || defined WINCE
	if (!isWatching())
	{
		return false;
	}

	HANDLE handle= (HANDLE)m_changeHandle;
	if (WaitForSingleObject(handle, 0) == WAIT_OBJECT_0)
	{
		FindNextChangeNotification(handle);
		m_bChangePending= true;
		m_quietSeconds= 0.f;
	}
	else if (m_bChangePending)
	{
		m_quietSeconds+= deltaSeconds;
	}

	if (m_bChangePending && m_quietSeconds >= m_debounceSeconds)
	{
		Snapshot newSnapshot;
		scan(m_directory, m_extension, newSnapshot);
		diff(m_snapshot, newSnapshot, outChanges);
		m_snapshot= std::move(newSnapshot);
		m_bChangePending= false;
		m_quietSeconds= 0.f;

		return true;
	}
#endif

	return false;
}

void DirectoryWatcher::takeSnapshot() { scan(m_directory, m_extension, m_snapshot); }

void DirectoryWatcher::scan(const std::filesystem::path& directory, const std::string& extension, Snapshot& outSnapshot)
{
	outSnapshot.clear();

	std::error_code ec;
	if (!std::filesystem::is_directory(directory, ec))
	{
		return;
	}

	const std::string lowerExtension= toLower(extension);

	for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(
			 directory, std::filesystem::directory_options::skip_permission_denied, ec))
	{
		if (!dirEntry.is_regular_file(ec))
		{
			continue;
		}

		const std::filesystem::path& filePath= dirEntry.path();
		if (toLower(filePath.extension().string()) != lowerExtension)
		{
			continue;
		}

		std::error_code timeEc;
		const std::filesystem::file_time_type writeTime= dirEntry.last_write_time(timeEc);
		if (timeEc)
		{
			continue;
		}

		outSnapshot[filePath]= writeTime;
	}
}

void DirectoryWatcher::diff(const Snapshot& before, const Snapshot& after, ChangeSet& outChanges)
{
	outChanges= ChangeSet();

	for (const auto& afterEntry : after)
	{
		auto beforeIt= before.find(afterEntry.first);
		if (beforeIt == before.end())
		{
			outChanges.added.push_back(afterEntry.first);
		}
		else if (beforeIt->second != afterEntry.second)
		{
			outChanges.modified.push_back(afterEntry.first);
		}
	}

	for (const auto& beforeEntry : before)
	{
		if (after.find(beforeEntry.first) == after.end())
		{
			outChanges.removed.push_back(beforeEntry.first);
		}
	}
}

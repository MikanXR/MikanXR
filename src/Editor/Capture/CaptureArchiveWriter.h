#pragma once

#include "CaptureManifest.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

// Writes the uncompressed POSIX ustar archive CaptureArchiveReader reads: the
// manifest as the first member, then each member the manifest names, from a
// file or from memory. A file member streams through in chunks rather than
// being loaded. The archive is written to a sibling temporary file and renamed
// into place by finish, so a reader never sees a partial capture.
class CaptureArchiveWriter
{
public:
	CaptureArchiveWriter()= default;
	~CaptureArchiveWriter();
	CaptureArchiveWriter(const CaptureArchiveWriter&)= delete;
	CaptureArchiveWriter& operator=(const CaptureArchiveWriter&)= delete;

	// Validates the manifest, checks the path's extension names its kind, and
	// writes the manifest member. Logs and returns false with outError.
	bool open(const std::filesystem::path& path, const CaptureManifest& manifest, std::string& outError);
	// Adds a member the manifest names. Adding a name the manifest does not
	// declare, or the same name twice, fails.
	bool addMemberFromMemory(const std::string& name, const void* data, size_t size, std::string& outError);
	bool addMemberFromFile(const std::string& name, const std::filesystem::path& sourcePath, std::string& outError);
	// Checks every manifest member was added, writes the end-of-archive
	// blocks, and renames the file into place
	bool finish(std::string& outError);
	// Drops the partial archive. Called by the destructor when finish never ran.
	void abort();

	bool isOpen() const { return m_bIsOpen; }
	const std::filesystem::path& getPath() const { return m_finalPath; }

private:
	bool beginMember(const std::string& name, uint64_t size, std::string& outError);
	bool endMember(uint64_t size, std::string& outError);
	bool writeBytes(const void* data, size_t size, std::string& outError);

	std::ofstream m_file;
	std::filesystem::path m_finalPath;
	std::filesystem::path m_tempPath;
	CaptureManifest m_manifest;
	std::set<std::string> m_writtenNames;
	bool m_bIsOpen= false;
};

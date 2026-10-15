#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// One regular file inside a capture archive. The data starts on a 512-byte
// boundary at a fixed offset, which is what lets a movie member play through
// ffmpeg's subfile protocol and a depth member be read record by record.
struct CaptureArchiveMember
{
	std::string name;
	uint64_t dataOffset= 0;
	uint64_t size= 0;
};

// Reads the uncompressed POSIX ustar archive CaptureArchiveWriter and the
// phone write. The accepted subset is exactly what those writers produce, and
// anything else is rejected with a message:
// - regular-file members only, no directories, links, or pax and GNU
//   extension headers
// - member names of at most 100 ASCII characters with no path separators
// - members under the ustar size limit of 8 GiB
// - the manifest capture.json as the first member
class CaptureArchiveReader
{
public:
	static constexpr uint64_t k_blockSize= 512;
	// Eleven octal digits: the largest size a ustar header can carry
	static constexpr uint64_t k_maxMemberSize= 077777777777ull;

	CaptureArchiveReader()= default;
	~CaptureArchiveReader();
	CaptureArchiveReader(const CaptureArchiveReader&)= delete;
	CaptureArchiveReader& operator=(const CaptureArchiveReader&)= delete;

	// Parses every header into the member table and keeps the file open for
	// member reads. Logs and returns false with outError on any rejection.
	bool open(const std::filesystem::path& path, std::string& outError);
	void close();
	bool isOpen() const { return m_bIsOpen; }

	const std::filesystem::path& getPath() const { return m_path; }
	const std::vector<CaptureArchiveMember>& getMembers() const { return m_members; }
	const CaptureArchiveMember* findMember(const std::string& name) const;

	// Reads a whole member into memory
	bool readMember(const CaptureArchiveMember& member, std::vector<uint8_t>& outBytes, std::string& outError);
	bool readMemberString(const std::string& name, std::string& outText, std::string& outError);

	// A member name the format accepts: 1 to 100 printable ASCII characters,
	// no slash of either kind, and neither "." nor ".."
	static bool isValidMemberName(const std::string& name);

private:
	bool parseHeaders(uint64_t fileSize, std::string& outError);

	std::ifstream m_file;
	std::filesystem::path m_path;
	std::vector<CaptureArchiveMember> m_members;
	bool m_bIsOpen= false;
};

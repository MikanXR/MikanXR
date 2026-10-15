#pragma once

#include <cstdint>
#include <string>

// The 512-byte POSIX ustar header block shared by the reader and the writer:
// field offsets, the octal number encoding, and the checksum. The reader
// accepts only a regular-file header with the POSIX magic and an empty prefix,
// and the writer produces nothing else.
namespace CaptureUstarHeader
{
constexpr size_t k_blockSize= 512;
constexpr size_t k_nameFieldSize= 100;

// Fills a header for a regular file of the given size, with the checksum
// computed. The name must already be a valid member name.
void write(char* block, const std::string& name, uint64_t size, int64_t modificationTimeSeconds);

// Reads a header into its name and size. Rejects a header outside the accepted
// subset with a message. A block of all zeros is not a header and is tested
// for with isZeroBlock first.
bool parse(const char* block, std::string& outName, uint64_t& outSize, std::string& outError);

bool isZeroBlock(const char* block);
} // namespace CaptureUstarHeader

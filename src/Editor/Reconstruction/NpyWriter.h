#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

// Writes one-dimensional arrays in NumPy's .npy format (version 1.0,
// little-endian, C order) so numpy.load reads the editor's measurements
// directly
class NpyWriter
{
public:
	static bool write(const std::filesystem::path& path, const std::vector<uint8_t>& values);
	static bool write(const std::filesystem::path& path, const std::vector<uint16_t>& values);
	static bool write(const std::filesystem::path& path, const std::vector<float>& values);

	// The header for an array of count elements of the NumPy type descriptor
	// given (such as "<f4"), padded with spaces and a newline so the payload
	// starts on a 64 byte boundary
	static std::vector<char> makeHeader(const char* typeDescriptor, size_t count);

private:
	static bool writeRaw(const std::filesystem::path& path, const char* typeDescriptor, const void* data,
						 size_t elementSize, size_t count);
};

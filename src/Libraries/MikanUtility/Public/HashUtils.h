#pragma once

#include "MikanUtilityExport.h"

#include <atomic>
#include <filesystem>
#include <string>

//-- utility methods -----
namespace HashUtils
{
/// SHA256 of a file's contents, lowercase hex, or an empty string on failure
/// or cancellation. Streams the file, so the multi-gigabyte model downloads
/// this verifies never land in memory at once.
/// Pass a cancel flag to abandon a hash in progress; a cancelled hash returns
/// empty rather than a partial digest.
MIKAN_UTILITY_FUNC(std::string)
sha256File(const std::filesystem::path& filePath, const std::atomic<bool>* cancelFlag= nullptr);

/// True when the file hashes to expectedSha256, compared case-insensitively.
/// An empty expectation is treated as "nothing to check" and passes, which is
/// how a source that publishes no hash is handled.
MIKAN_UTILITY_FUNC(bool)
verifyFileSha256(const std::filesystem::path& filePath, const std::string& expectedSha256,
				 const std::atomic<bool>* cancelFlag= nullptr);
}; // namespace HashUtils

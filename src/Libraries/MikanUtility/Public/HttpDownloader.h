#pragma once

#include "MikanUtilityExport.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

//-- utility methods -----
namespace HttpDownloader
{
/// Called as bytes arrive. totalBytes is 0 when the server sends no
/// Content-Length. Return false to abandon the download.
using ProgressCallback= std::function<bool(uint64_t receivedBytes, uint64_t totalBytes)>;

/// Returned by value and never exported as a class: it has no methods to
/// export, and dllexporting it only earns a C4251 for its std::string.
struct Result
{
	bool bSuccess= false;
	bool bCancelled= false;
	int httpStatus= 0;
	uint64_t receivedBytes= 0;
	std::string errorMessage;
};

/// Streams an HTTPS GET straight to disk. Never holds the body in memory, so
/// this is usable for the multi-gigabyte model assets.
///
/// Writes to "<destPath>.part" and renames on success, so an interrupted or
/// failed download never leaves something that looks like a finished file.
/// Redirects are followed, which a GitHub release asset URL requires.
/// Retries a failed transfer up to retryCount times; a cancel is not retried.
MIKAN_UTILITY_FUNC(Result)
downloadToFile(const std::string& url, const std::filesystem::path& destPath,
			   const ProgressCallback& progressCallback= {}, const std::atomic<bool>* cancelFlag= nullptr,
			   int retryCount= 2);

/// Fetches a small resource into a string. For manifests and other documents
/// measured in kilobytes; use downloadToFile for anything large.
MIKAN_UTILITY_FUNC(Result)
downloadToString(const std::string& url, std::string& outBody, const std::atomic<bool>* cancelFlag= nullptr,
				 int retryCount= 2);

/// Free space on the volume holding the given directory, 0 when it cannot be
/// determined. Used to fail a multi-gigabyte download before it starts rather
/// than when the disk fills.
MIKAN_UTILITY_FUNC(uint64_t) getFreeDiskSpace(const std::filesystem::path& directory);
}; // namespace HttpDownloader

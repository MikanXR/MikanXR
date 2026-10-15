#pragma once

#include "CaptureManifest.h"

#include <filesystem>
#include <string>

// What a working directory packs into
struct CapturePackRequest
{
	eCaptureKind kind= eCaptureKind::INVALID;
	// Holds the members as loose files named by role: plate.<ext>, marker.<ext>,
	// marker.pose.json, movie.<ext>, movie.pose.json, movie.depth.bin, movie.conf.bin
	std::filesystem::path workingDir;
	// The capture to write, whose extension names the kind
	std::filesystem::path outputPath;
	std::string sessionId;
	eCapturePoseSpace poseSpace= eCapturePoseSpace::stage;
	std::string producer;
	bool bInterrupted= false;
};

// Packs a recording's working directory into a capture archive. The recorder
// writes media and tracks as files first, because a movie writer needs a file of
// its own and a member's size must be known before its header, then packs them
// in one pass once the flow completes.
namespace CapturePacker
{
inline constexpr const char* k_plateStem= "plate";
inline constexpr const char* k_markerStem= "marker";
inline constexpr const char* k_movieStem= "movie";

// The file in workingDir playing a role, empty when there is none. Images and
// movies are found by stem with any image or movie extension; the tracks and
// depth members by their fixed names.
std::filesystem::path findMemberFile(const std::filesystem::path& workingDir, eCaptureMemberRole role);

// Writes outputPath from every member file present, streaming each from its
// file. Fails with outError when a member the kind requires is missing or the
// archive cannot be written. Leaves the working directory as it was.
bool pack(const CapturePackRequest& request, std::string& outError);
} // namespace CapturePacker

#pragma once

#include <configuru.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

// What kind of shot a capture holds, which decides how the editor treats the
// camera. The file extension only routes the file to the captures folder and
// must agree with the manifest.
enum class eCaptureKind : int
{
	INVALID= -1,

	// A stationary camera's still: a clean plate, and a marker image with its
	// one-frame pose track
	stillphoto,
	// A stationary camera's movie with a moving subject: the stillphoto
	// members plus the movie, which has no pose track of its own
	stillcamera,
	// A moving camera's movie with its per-frame pose track, and on a LiDAR
	// phone the depth and confidence tracks
	trackcamera,

	COUNT
};

// The space the archive's pose tracks are expressed in
enum class eCapturePoseSpace : int
{
	INVALID= -1,

	// A phone capture, whose poses need the marker alignment's offset
	arkit_world,
	// An editor recording, whose poses are already stage space
	stage,

	COUNT
};

// The roles a member can play. The manifest maps each role to a member name,
// so an image may be a jpg or a png.
enum class eCaptureMemberRole : int
{
	INVALID= -1,

	plate_image,
	marker_image,
	marker_pose,
	movie,
	movie_pose,
	depth,
	confidence,

	COUNT
};

namespace CaptureKindUtils
{
const char* kindToString(eCaptureKind kind);
eCaptureKind kindFromString(const std::string& name);
// The capture file extension for a kind, with the leading dot
const char* kindToExtension(eCaptureKind kind);
// The kind a capture path's extension names, INVALID for anything else
eCaptureKind kindFromExtension(const std::filesystem::path& path);
bool isCaptureExtension(const std::filesystem::path& path);

const char* poseSpaceToString(eCapturePoseSpace poseSpace);
eCapturePoseSpace poseSpaceFromString(const std::string& name);

const char* roleToString(eCaptureMemberRole role);
eCaptureMemberRole roleFromString(const std::string& name);
// The roles a kind must carry; everything else in the role list is optional
const std::vector<eCaptureMemberRole>& requiredRoles(eCaptureKind kind);
} // namespace CaptureKindUtils

// The capture.json member at the front of every capture archive: what kind of
// shot the archive holds, the session its tracks were recorded in, and which
// member plays which role.
struct CaptureManifest
{
	inline static const std::string k_formatName= "mikan-capture";
	static constexpr int k_version= 1;
	inline static const std::string k_manifestMemberName= "capture.json";

	eCaptureKind kind= eCaptureKind::INVALID;
	// The ARKit world origin's UUID, or the editor recorder's session id. Every
	// pose track in the archive names it.
	std::string sessionId;
	eCapturePoseSpace poseSpace= eCapturePoseSpace::INVALID;
	// ISO 8601 UTC, as makeUtcTimestampNow produces
	std::string createdUtc;
	// The application and version that wrote the archive
	std::string producer;
	// A stillcamera the phone stopped because the camera moved
	bool bInterrupted= false;
	std::map<eCaptureMemberRole, std::string> members;

	bool hasMember(eCaptureMemberRole role) const;
	// The member name for a role, empty when the role is absent
	std::string getMemberName(eCaptureMemberRole role) const;
	// Every member name the manifest declares, in role order
	std::vector<std::string> getMemberNames() const;

	configuru::Config toConfig() const;
	std::string toJsonString() const;
	// Parses and validates. On failure the manifest is left cleared and outError
	// says what was wrong, in plain text for the log.
	bool loadFromConfig(const configuru::Config& config, std::string& outError);
	bool loadFromJsonString(const std::string& json, std::string& outError);
	void clear();

	// The kind is known, every role it requires is present, depth and
	// confidence come together, and no two roles share a member name. The
	// manifest member name itself is reserved.
	bool validate(std::string& outError) const;

	static std::string makeUtcTimestampNow();
	// "MikanXR <release version>", for archives the editor writes
	static std::string makeEditorProducer();
};

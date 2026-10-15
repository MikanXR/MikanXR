#pragma once

#include "CaptureArchiveReader.h"
#include "CaptureManifest.h"
#include "PoseTrack.h"

#include <opencv2/core.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// An opened capture archive: its manifest, the member table, and the pose
// tracks, with accessors that read a member by role. Opening checks the
// manifest against the file's extension, that every member the manifest names
// is in the archive, and that every pose track names the manifest's session.
// The archive stays open for member reads until close, which is safe because
// a capture is never rewritten.
class Capture
{
public:
	Capture()= default;
	Capture(const Capture&)= delete;
	Capture& operator=(const Capture&)= delete;

	// Logs and returns false with a plain-text reason
	bool open(const std::filesystem::path& path, std::string& outError);
	void close();
	bool isOpen() const { return m_reader.isOpen(); }

	const std::filesystem::path& getPath() const { return m_reader.getPath(); }
	const CaptureManifest& getManifest() const { return m_manifest; }
	eCaptureKind getKind() const { return m_manifest.kind; }
	// An editor recording, whose poses are stage space already and whose
	// playback needs no alignment offset
	bool isStageSpace() const { return m_manifest.poseSpace == eCapturePoseSpace::stage; }

	bool hasMember(eCaptureMemberRole role) const;
	// The archive member playing a role, null when the manifest has none
	const CaptureArchiveMember* findMember(eCaptureMemberRole role) const;
	bool readMemberBytes(eCaptureMemberRole role, std::vector<uint8_t>& outBytes, std::string& outError);

	// The movie_pose or marker_pose track, loaded at open. An absent role's
	// track reports isLoaded false.
	const PoseTrack& getPoseTrack(eCaptureMemberRole role) const;
	// Decodes an image member to continuous CV_8UC3 BGR
	bool decodeImage(eCaptureMemberRole role, cv::Mat& outBgr, std::string& outError);

	// The ffmpeg subfile URL that plays a member in place through
	// cv::VideoCapture with the ffmpeg backend, empty when the role is absent
	std::string makeSubfileUrl(eCaptureMemberRole role) const;
	static std::string makeSubfileUrl(const std::filesystem::path& archivePath, const CaptureArchiveMember& member);

private:
	bool loadPoseTrack(eCaptureMemberRole role, PoseTrack& outTrack, std::string& outError);

	CaptureArchiveReader m_reader;
	CaptureManifest m_manifest;
	PoseTrack m_moviePoseTrack;
	PoseTrack m_markerPoseTrack;
};

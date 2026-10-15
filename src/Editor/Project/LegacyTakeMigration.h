#pragma once

#include "CaptureManifest.h"

#include <filesystem>
#include <string>

// Packs the loose takes a project recorded before captures existed into
// capture archives, once, on project load:
//   - a movie or photo in movies/ or textures/ with its <stem>.pose.json, and
//     for a LiDAR take its <stem>.depth.bin and <stem>.conf.bin, plus the
//     <stem>_marker take that pairs with it, becomes one captures/<stem>.<kind>
//   - a <stem>.refined.pose.json moves to captures/ beside the new capture
//   - every file video source naming a packed take is repointed in the project
//     file, keeping its pose offset, since the capture holds the same session
//   - the originals and any orphan sidecar move to legacy_takes/, never deleted
// Detection is by the sidecars' presence, so an interrupted pack reruns on the
// next load and a project with no loose takes is left alone.
namespace LegacyTakeMigration
{
// The pose space a track's session id implies: the editor's recorder makes a
// 32-character hex id without dashes and records stage-space poses, while the
// phone's ARKit session UUID carries dashes and its poses need the marker
// alignment. Neither writer records which it was, so the id is the only judge.
eCapturePoseSpace poseSpaceForSessionId(const std::string& sessionId);

// Whether movies/ or textures/ holds any loose pose, depth, or confidence track
bool projectHasLooseTakes(const std::filesystem::path& projectDir);

// Packs the project's loose takes and rewrites the project file. Returns true
// when anything changed.
bool migrateProject(const std::filesystem::path& projectDir, const std::filesystem::path& projectFilePath);
} // namespace LegacyTakeMigration

#pragma once

// Validates the pieces the editor's take recorder writes with, none of which
// need a window:
//   - MovieWriter picks a backend this build can drive, and MovieDecoder reads
//     the movie back with the written frame count, size, rate, and colours
//   - PoseTrackWriter round-trips through PoseTrack: header fields, frame
//     order, the row-major transpose, and the pinhole intrinsics
//   - take names carry the local timestamp and the marker suffix rule
bool run_video_recording_tests();

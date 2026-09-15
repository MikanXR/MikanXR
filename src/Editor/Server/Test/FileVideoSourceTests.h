#pragma once

// Validates the file video source pieces that need no project:
//   - advancePlaybackTime wraps when looping, clamps and finishes otherwise,
//     and leaves an unbounded clock alone
//   - pinhole intrinsics land in the column-major camera matrices with the
//     field of view derived from the focal lengths
//   - a pose track loads from JSON with its row-major transforms transposed,
//     sorts out-of-order frames, rejects a bad header, and answers nearest
//     frame lookups within a tolerance
//   - the movie decoder reads an MJPG clip sequentially with per-frame
//     presentation times, hits end of stream, and seeks
//   - a still image decodes as a one-frame movie that repeats on every read
//   - the definition round-trips its media and track paths, the loop flag,
//     and a solved pose offset, and an absent offset stays absent
//   - play, pause, stop, and seek move the playback state on a component with
//     no media open, and a trackless component offers no pose
bool run_file_video_source_tests();

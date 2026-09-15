#pragma once

// Advance a playback clock by delta against a duration (zero or less is
// unbounded). Looping wraps the time; otherwise it clamps to the duration and
// outFinished is set so the caller can run a final update and stop. Shared by
// every component that plays timed content, so the wrap and clamp semantics
// cannot drift between them.
void advancePlaybackTime(float& inoutTime, float delta, float duration, bool bLoop, bool& outWrapped,
						 bool& outFinished);

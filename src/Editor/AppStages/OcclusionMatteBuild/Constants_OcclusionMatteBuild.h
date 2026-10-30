#pragma once

#include <string>

enum class eOcclusionMatteBuildMenuState : int
{
	INVALID= -1,

	inactive,
	// The take is summarised; the operator picks the settings and hits Build
	verifyTake,
	// The worker is segmenting; progress is frames of the take
	building,
	failed,
	// The sidecar is written; the summary and the last frame's preview are shown
	buildComplete,

	COUNT
};
extern const std::string* k_OcclusionMatteBuildMenuStateStrings;

// The SAM 2.1 checkpoint the segmenter runs. Small is a little better on the
// edge; tiny is faster.
enum class eOcclusionMatteModel : int
{
	INVALID= -1,

	small,
	tiny,

	COUNT
};
extern const std::string* k_OcclusionMatteModelStrings;

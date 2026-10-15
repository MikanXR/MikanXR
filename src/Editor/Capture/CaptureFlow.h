#pragma once

#include "CaptureManifest.h"

#include <vector>

// The steps a guided capture walks. The media steps are what the operator acts
// on; packing follows the last of them and leads back to idle.
enum class eCaptureFlowStep : int
{
	idle,
	// Place the origin marker in view and take the marker image
	markerImage,
	// Remove the marker and take the clean plate
	plateImage,
	// Record when ready
	readyToRecord,
	// Recording until stopped
	recording,
	// The members are on disk and the archive is being written
	packing,

	COUNT
};

namespace CaptureFlowUtils
{
const char* stepToString(eCaptureFlowStep step);
}

// The step machine behind a guided capture, pure logic so the recorder and the
// tests share it. A stillphoto takes the marker image then the plate. A
// stillcamera takes both then records until stopped. A trackcamera records
// straight away: its poses are stage space, so no marker is needed to align
// them. Advancing past the last media step enters packing, and completing or
// cancelling returns to idle.
class CaptureFlow
{
public:
	static const std::vector<eCaptureFlowStep>& stepsForKind(eCaptureKind kind);

	bool isActive() const { return m_step != eCaptureFlowStep::idle; }
	eCaptureKind getKind() const { return m_kind; }
	eCaptureFlowStep getStep() const { return m_step; }
	// Whether the current step records a movie rather than a still
	bool isMovieStep() const;

	// From idle only
	bool start(eCaptureKind kind);
	// The next media step, or packing after the last one. False when idle or packing.
	bool advance();
	// Back to idle, from any step
	void complete();
	void cancel() { complete(); }

private:
	eCaptureKind m_kind= eCaptureKind::INVALID;
	eCaptureFlowStep m_step= eCaptureFlowStep::idle;
	size_t m_stepIndex= 0;
};

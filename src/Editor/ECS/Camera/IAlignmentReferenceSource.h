#pragma once

// Implemented by video sources that can stand in a second recording of the same
// shot with the origin marker in frame, so the marker alignment stages can solve
// the camera placement from footage the main take never shows. A recorded take
// keeps the marker out of shot; its reference file is what the stage samples.
//
// A stateless mixin discovered by dynamic_cast, the same way
// IFrameCoupledPoseProvider is. Runtime only: which media is active is stage
// state, never a persisted property, so a cancelled stage cannot leave the
// definition pointing at the reference file.
class IAlignmentReferenceSource
{
public:
	virtual ~IAlignmentReferenceSource() {}

	// Whether a usable reference is configured. False when none is set, when the
	// main take carries a pose track but the reference does not, or when the two
	// tracks come from different tracking sessions and the solved offset would
	// describe a world the main take never used.
	virtual bool hasAlignmentReference() const= 0;

	// Switch the source's frames and poses to the reference media, looping from
	// the start. Active stream subscribers keep their subscriptions.
	virtual void beginAlignmentReference()= 0;

	// Switch back to the main media, restoring the playback state it had.
	virtual void endAlignmentReference()= 0;
};

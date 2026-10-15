#pragma once

// Implemented by video sources that can stand in an image of the same shot with
// the origin marker in frame, so the marker alignment stages can solve the camera
// placement from a view the main take never shows. A recorded take keeps the
// marker out of shot; its capture's marker image is what the stage samples.
//
// A stateless mixin discovered by dynamic_cast, the same way
// IFrameCoupledPoseProvider is. Runtime only: which media is active is stage
// state, never a persisted property, so a cancelled stage cannot leave the
// definition pointing at the reference file.
class IAlignmentReferenceSource
{
public:
	virtual ~IAlignmentReferenceSource() {}

	// Whether a usable reference is configured. False when the capture has no
	// marker image, or when its poses are stage space already and need no
	// alignment.
	virtual bool hasAlignmentReference() const= 0;

	// Switch the source's frames and poses to the reference media, looping from
	// the start. Active stream subscribers keep their subscriptions.
	virtual void beginAlignmentReference()= 0;

	// Switch back to the main media, restoring the playback state it had.
	virtual void endAlignmentReference()= 0;
};

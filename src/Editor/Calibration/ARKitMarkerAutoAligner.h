#pragma once

#include "ComponentFwd.h"

#include "glm/ext/matrix_double4x4.hpp"

#include <memory>
#include <vector>

class ArucoMarkerPoseSampler;
class IEditorWindow;
class VideoFrameDistortionView;

// Solves a frame-coupled video source's marker alignment without the Align
// Camera stage, for the phone's stream prompt: while the phone asks whether its
// stream is aligned, the aligner watches the source's frames for the origin
// marker at a reduced rate and, once the marker has been seen steadily for a
// sample window, averages the samples the way the stage does and stores the
// offset on the source. It stops the moment the source is aligned or the phone
// stops asking.
//
// Detection runs on the main thread, where the frame readback and undistort
// already happen, a few times a second rather than every frame, so the cost is a
// few milliseconds a few times a second and only while a prompt is up.
class ARKitMarkerAutoAligner
{
public:
	// Detections per second while active
	static constexpr double k_detectIntervalSeconds= 0.25;
	// Samples of the marker before the offset is solved
	static constexpr int k_sampleCount= 12;
	// A marker not seen for this long starts the window over
	static constexpr double k_lostResetSeconds= 1.0;
	// How long after the phone's last poll the aligner keeps going
	static constexpr double k_pollKeepAliveSeconds= 5.0;

	// videoSource must implement IFrameCoupledPoseProvider
	ARKitMarkerAutoAligner(IEditorWindow* ownerWindow, VideoSourceComponentPtr videoSource);
	~ARKitMarkerAutoAligner();

	void notifyPolled(double nowSeconds);
	// Runs a detection when one is due and solves once the window is full
	void update(double nowSeconds);
	// The phone stopped asking, the source is aligned, or the source is gone
	bool shouldRetire(double nowSeconds) const;

	bool isMarkerVisible() const { return m_bMarkerVisible; }

private:
	// Creates the sampler once frames arrive and the camera can be sampled
	bool ensureSampler();
	void releaseStream();

	IEditorWindow* m_ownerWindow;
	VideoSourceComponentWeakPtr m_videoSource;
	CameraComponentPtr m_camera;
	std::unique_ptr<VideoFrameDistortionView> m_view;
	std::unique_ptr<ArucoMarkerPoseSampler> m_sampler;
	std::vector<glm::dmat4> m_samples;
	double m_lastPollSeconds= 0.0;
	double m_lastDetectSeconds= -1.0;
	double m_lastSeenSeconds= -1.0;
	bool m_bMarkerVisible= false;
	bool m_bSolved= false;
};

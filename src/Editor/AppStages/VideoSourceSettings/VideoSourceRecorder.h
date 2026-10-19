#pragma once

#include "CaptureFlow.h"
#include "ComponentFwd.h"

#include "glm/ext/matrix_float4x4.hpp"

#include <opencv2/core.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

class IEditorWindow;
class VideoFrameDistortionView;

enum class eVideoRecorderState : int
{
	idle,
	// A flow is active and waiting for the operator's next action
	waiting,
	recording,
	finishing,
	packing,
};

// Why a take gets no pose track. The media is still written, except for a
// trackcamera, which refuses to start without one.
enum class ePoseTrackSkipReason : int
{
	none,
	noCamera,     // no camera names this video source
	uncalibrated, // the source has no valid intrinsics
	stereo,       // stereo intrinsics have no pinhole row
};

enum class eVideoRecorderError : int
{
	none,
	noProject,
	sourceNotStreaming,
	writerOpenFailed,
	writeFailed,
	packFailed,
	trackCameraNeedsPose,
};

struct VideoRecorderStatus
{
	eVideoRecorderState state= eVideoRecorderState::idle;
	eCaptureKind kind= eCaptureKind::INVALID;
	eCaptureFlowStep step= eCaptureFlowStep::idle;
	double elapsedSeconds= 0.0;
	int64_t framesWritten= 0;
	int64_t framesDropped= 0;
	// Project-relative stored path of the last capture written, empty until one has
	std::string lastStoredPath;
	std::string backendName;
	bool bWritesPoseTrack= false;
	ePoseTrackSkipReason poseTrackSkipReason= ePoseTrackSkipReason::none;
	eVideoRecorderError lastError= eVideoRecorderError::none;
};

// Records the live feed of one video source into a capture archive, the file
// FileVideoSourceComponent plays back, through the guided flow of its kind
// (CaptureFlow): the marker image, the clean plate, and the movie as the kind
// needs them, each written as a file into a hidden working directory under the
// captures folder, then packed into captures/<take>.<kind> when the flow
// completes. The working directory is removed after packing and on cancel.
//
// Frames are taken undistorted from a calibration-mode VideoFrameDistortionView
// the recorder owns only while a job is in progress, so the CPU undistort costs
// nothing otherwise. Each main-thread update() pulls the view's new frame and
// pairs it with the camera's stage-space pose at that moment, then hands the pair
// to an encoder thread over a bounded queue. A frame the queue cannot take is
// dropped together with its pose, so the movie and the track stay in lockstep.
// The track's time_us is the constant-rate presentation time of the written
// frame. Poses are stage space, so the capture is pose_space stage and plays
// back with no alignment step.
//
// A file source playing a capture is a camera like any other here: recording it
// is how a capture is trimmed, at the cost of a re-encode and no depth.
//
// The view reads the source's intrinsics when its buffers are first sized and
// does not follow a change mid-recording.
//
// Main-thread API. finishNow() cancels a flow in progress for a stage that is
// pausing or exiting and will get no further update().
class VideoSourceRecorder
{
public:
	VideoSourceRecorder(IEditorWindow* ownerWindow, VideoSourceComponentPtr videoSource);
	~VideoSourceRecorder();

	// Whether a trackcamera can be recorded: a camera on this source whose pose
	// comes from a tracking mount or a frame-coupled source, with mono intrinsics
	bool canRecordTrackCamera() const;

	// Begins the flow of a kind. fallbackFps is used when the source does not
	// report a frame rate, normally the settings stage's measured rate.
	bool startCapture(eCaptureKind kind, float fallbackFps);
	// The current step's primary action: take the marker image, take the plate,
	// start recording, or stop recording. Refused while a job is in progress,
	// except for stopping the recording job.
	bool advanceCapture();
	// Drops the flow and its working directory
	bool cancelCapture();
	void update();
	void finishNow();

	const VideoRecorderStatus& getStatus() const { return m_status; }
	bool isFlowActive() const { return m_flow.isActive(); }
	// A job is running, so the flow cannot be advanced
	bool isBusy() const;

private:
	struct FramePacket
	{
		cv::Mat bgr;
		int64_t captureTimestampUs= 0;
		bool bHasPose= false;
		glm::mat4 cameraToStage= glm::mat4(1.f);
	};

	struct Job
	{
		bool bStill= false;
		std::filesystem::path stemPath;
		std::string sessionId;
		double fps= 30.0;
		bool bWritePoseTrack= false;
		double fx= 0.0;
		double fy= 0.0;
		double cx= 0.0;
		double cy= 0.0;
	};

	struct JobResult
	{
		bool bOk= false;
		eVideoRecorderError error= eVideoRecorderError::none;
		std::filesystem::path mediaPath;
		std::string backendName;
		int64_t framesWritten= 0;
	};

	struct PackResult
	{
		bool bOk= false;
		std::filesystem::path capturePath;
	};

	bool beginJob(const char* stem, bool bStill, bool bWantPoseTrack);
	bool stopJob();
	void decidePoseTrack(Job& job);
	void sampleFrame();
	void completeJob();
	void releaseView();
	void onStepMediaLanded();
	void beginPacking();
	void completePacking();
	void removeWorkingDirectory();
	CameraComponentPtr findCamera() const;

	void encoderThreadMain(std::shared_ptr<Job> job);
	void packThreadMain();
	static int64_t nowUs();

	IEditorWindow* m_ownerWindow;
	VideoSourceComponentWeakPtr m_videoSource;
	std::shared_ptr<VideoFrameDistortionView> m_view;
	int64_t m_lastReadIndex= -1;

	std::string m_sessionId;
	float m_fallbackFps= 0.f;
	CaptureFlow m_flow;
	std::string m_takeName;
	std::filesystem::path m_workingDir;
	std::chrono::steady_clock::time_point m_startTime;
	VideoRecorderStatus m_status;

	// Handoff to the encoder thread
	std::thread m_encoderThread;
	std::mutex m_queueMutex;
	std::condition_variable m_queueCondVar;
	std::deque<FramePacket> m_queue;
	bool m_bFinishRequested= false;
	std::atomic<bool> m_bJobDone{false};
	std::atomic<int64_t> m_framesWritten{0};
	int64_t m_framesDropped= 0;
	JobResult m_jobResult;
	std::shared_ptr<Job> m_job;

	// The pack thread, which runs after the last media job
	std::thread m_packThread;
	std::atomic<bool> m_bPackDone{false};
	PackResult m_packResult;
};

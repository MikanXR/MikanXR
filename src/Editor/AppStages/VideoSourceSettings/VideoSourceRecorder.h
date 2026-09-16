#pragma once

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
	recording,
	finishing,
};

// Why a take gets no pose track sidecar. The media is still written.
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
};

struct VideoRecorderStatus
{
	eVideoRecorderState state= eVideoRecorderState::idle;
	double elapsedSeconds= 0.0;
	int64_t framesWritten= 0;
	int64_t framesDropped= 0;
	// Project-relative stored path of the last media written, empty until one has
	std::string lastStoredPath;
	std::string backendName;
	bool bWritesPoseTrack= false;
	ePoseTrackSkipReason poseTrackSkipReason= ePoseTrackSkipReason::none;
	eVideoRecorderError lastError= eVideoRecorderError::none;
};

// Records the live feed of one video source into the project's asset folders as
// the files FileVideoSourceComponent plays back: a movie or a still plus, when a
// camera uses the source, a pose track sidecar in stage space.
//
// Frames are taken undistorted from a calibration-mode VideoFrameDistortionView
// the recorder owns only while a capture is in progress, so the CPU undistort
// costs nothing otherwise. Each main-thread update() pulls the view's new frame
// and pairs it with the camera's pose at that moment, then hands the pair to an
// encoder thread over a bounded queue. A frame the queue cannot take is dropped
// together with its pose, so the movie and the sidecar stay in lockstep. The
// sidecar's time_us is the constant-rate presentation time of the written frame.
//
// The view reads the source's intrinsics when its buffers are first sized and
// does not follow a change mid-recording.
//
// Main-thread API. finishNow() blocks until the encoder thread has closed the
// files, for a stage that is pausing or exiting and will get no further update().
class VideoSourceRecorder
{
public:
	VideoSourceRecorder(IEditorWindow* ownerWindow, VideoSourceComponentPtr videoSource);
	~VideoSourceRecorder();

	// fallbackFps is used when the source does not report a frame rate, normally
	// the settings stage's measured rate. bMarker names the take as the marker
	// reference of the most recent main take.
	bool startRecording(bool bMarker, float fallbackFps);
	bool stopRecording();
	bool captureImage(bool bMarker);
	void update();
	void finishNow();

	const VideoRecorderStatus& getStatus() const { return m_status; }
	bool isBusy() const { return m_status.state != eVideoRecorderState::idle; }

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
		std::string folderId;
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

	bool beginCapture(bool bMarker, bool bStill, float fallbackFps);
	void decidePoseTrack(Job& job);
	std::string chooseTakeName(bool bMarker);
	void sampleFrame();
	void completeJob();
	void releaseView();

	void encoderThreadMain(std::shared_ptr<Job> job);
	static int64_t nowUs();

	IEditorWindow* m_ownerWindow;
	VideoSourceComponentWeakPtr m_videoSource;
	std::shared_ptr<VideoFrameDistortionView> m_view;
	int64_t m_lastReadIndex= -1;

	std::string m_sessionId;
	std::string m_lastMainTakeName;
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
};

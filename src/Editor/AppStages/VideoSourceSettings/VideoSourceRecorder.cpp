#include "VideoSourceRecorder.h"
#include "CameraComponent.h"
#include "CameraObjectSystem.h"
#include "IEditorWindow.h"
#include "Logger.h"
#include "MovieWriter.h"
#include "PathUtils.h"
#include "PoseTrack.h"
#include "ProjectAssetCatalog.h"
#include "ProjectManager.h"
#include "TakeNaming.h"
#include "VideoFrameDistortionView.h"
#include "VideoSourceComponent.h"

#include <opencv2/imgcodecs.hpp>

#include <cmath>
#include <random>
#include <sstream>
#include <system_error>

namespace
{
// Frames waiting for the encoder. Deeper hides a slow encoder longer but holds
// that many full frames in memory; a full queue drops the newest frame.
constexpr size_t k_maxQueuedFrames= 8;
constexpr int k_jpegQuality= 92;
constexpr double k_defaultFps= 30.0;

std::string makeSessionId()
{
	std::random_device device;
	std::ostringstream stream;
	stream << std::hex;
	for (int i= 0; i < 4; ++i)
	{
		stream.width(8);
		stream.fill('0');
		stream << device();
	}
	return stream.str();
}
} // namespace

VideoSourceRecorder::VideoSourceRecorder(IEditorWindow* ownerWindow, VideoSourceComponentPtr videoSource)
	: m_ownerWindow(ownerWindow)
	, m_videoSource(videoSource)
	, m_sessionId(makeSessionId())
{
}

VideoSourceRecorder::~VideoSourceRecorder() { finishNow(); }

int64_t VideoSourceRecorder::nowUs()
{
	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
		.count();
}

bool VideoSourceRecorder::startRecording(bool bMarker, float fallbackFps)
{
	return beginCapture(bMarker, false, fallbackFps);
}

bool VideoSourceRecorder::captureImage(bool bMarker) { return beginCapture(bMarker, true, 0.f); }

bool VideoSourceRecorder::stopRecording()
{
	if (m_status.state != eVideoRecorderState::recording)
		return false;

	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		m_bFinishRequested= true;
	}
	m_queueCondVar.notify_one();
	m_status.state= eVideoRecorderState::finishing;
	return true;
}

std::string VideoSourceRecorder::chooseTakeName(bool bMarker)
{
	if (bMarker)
	{
		// The marker shot pairs with the main take of this stage visit, or stands
		// alone under its own timestamp when none has been captured yet
		return TakeNaming::makeMarkerName(m_lastMainTakeName.empty() ? TakeNaming::makeTakeNameNow()
																	 : m_lastMainTakeName);
	}

	m_lastMainTakeName= TakeNaming::makeTakeNameNow();
	return m_lastMainTakeName;
}

void VideoSourceRecorder::decidePoseTrack(Job& job)
{
	job.bWritePoseTrack= false;
	m_status.poseTrackSkipReason= ePoseTrackSkipReason::none;

	VideoSourceComponentPtr videoSource= m_videoSource.lock();
	ProjectManagerPtr projectManager= m_ownerWindow->getProjectManager();
	CameraObjectSystemPtr cameraSystem=
		projectManager ? projectManager->getSystemOfType<CameraObjectSystem>() : nullptr;
	CameraComponentPtr camera=
		cameraSystem && videoSource ? cameraSystem->findCameraForVideoSource(videoSource->getComponentId()) : nullptr;
	if (!camera)
	{
		m_status.poseTrackSkipReason= ePoseTrackSkipReason::noCamera;
		return;
	}

	MikanVideoSourceIntrinsics intrinsics;
	if (!videoSource->areCameraIntrinsicsValid() || !videoSource->getCameraIntrinsics(intrinsics))
	{
		m_status.poseTrackSkipReason= ePoseTrackSkipReason::uncalibrated;
		return;
	}
	if (intrinsics.intrinsics_type != MikanIntrinsicsType::MONO_CAMERA_INTRINSICS)
	{
		m_status.poseTrackSkipReason= ePoseTrackSkipReason::stereo;
		return;
	}

	// The frames are undistorted, so the sidecar carries the undistorted matrix
	// (column-major: fx x0, fy y1, cx z0, cy z1)
	const MikanMatrix3d& matrix= intrinsics.getMonoIntrinsics().undistorted_camera_matrix;
	job.fx= matrix.x0;
	job.fy= matrix.y1;
	job.cx= matrix.z0;
	job.cy= matrix.z1;
	job.bWritePoseTrack= true;
}

bool VideoSourceRecorder::beginCapture(bool bMarker, bool bStill, float fallbackFps)
{
	if (isBusy())
		return false;

	VideoSourceComponentPtr videoSource= m_videoSource.lock();
	if (!videoSource)
		return false;

	m_status.lastError= eVideoRecorderError::none;

	const char* folderId= bStill ? "textures" : "movies";
	const std::filesystem::path folderDir= ProjectAssetCatalog::getFolderDirectory(folderId);
	if (folderDir.empty())
	{
		m_status.lastError= eVideoRecorderError::noProject;
		return false;
	}

	auto job= std::make_shared<Job>();
	job->bStill= bStill;
	job->folderId= folderId;
	job->sessionId= m_sessionId;
	job->stemPath= folderDir / chooseTakeName(bMarker);
	decidePoseTrack(*job);

	float sourceFps= 0.f;
	if (videoSource->getFrameRate(sourceFps) && sourceFps > 1.f)
		job->fps= sourceFps;
	else if (fallbackFps > 1.f)
		job->fps= fallbackFps;
	else
		job->fps= k_defaultFps;

	std::error_code ec;
	std::filesystem::create_directories(folderDir, ec);

	// The recorder's own view: CPU undistortion into a BGR buffer, no grayscale pass
	m_view= std::make_shared<VideoFrameDistortionView>(videoSource, eVideoFrameProcessorMode::CALIBRATION);
	m_view->setVideoDisplayMode(eVideoDisplayMode::mode_undistored);
	m_view->setGrayscaleUndistortDisabled(true);
	videoSource->startVideoStream(m_view.get());
	m_lastReadIndex= m_view->getLastVideoFrameWriteIndex();

	m_status.state= eVideoRecorderState::recording;
	m_status.bWritesPoseTrack= job->bWritePoseTrack;
	m_status.framesWritten= 0;
	m_status.framesDropped= 0;
	m_status.backendName.clear();
	m_framesWritten= 0;
	m_framesDropped= 0;
	m_bJobDone= false;
	m_bFinishRequested= false;
	m_startTime= std::chrono::steady_clock::now();
	m_job= job;
	m_encoderThread= std::thread(&VideoSourceRecorder::encoderThreadMain, this, job);

	return true;
}

void VideoSourceRecorder::sampleFrame()
{
	VideoSourceComponentPtr videoSource= m_videoSource.lock();
	if (!m_view || !videoSource)
		return;

	const int64_t readIndex= m_view->readAndProcessVideoFrame();
	if (readIndex == m_lastReadIndex)
		return;
	m_lastReadIndex= readIndex;

	// The read index also advances on a GPU-direct source whose frame never
	// reached the CPU, so the buffer decides whether there is a frame to take
	const cv::Mat* undistorted= m_view->getBGRUndistortBuffer();
	if (undistorted == nullptr || undistorted->empty())
		return;

	FramePacket packet;
	packet.bgr= undistorted->clone();
	packet.captureTimestampUs= nowUs();

	if (m_job && m_job->bWritePoseTrack)
	{
		ProjectManagerPtr projectManager= m_ownerWindow->getProjectManager();
		CameraObjectSystemPtr cameraSystem=
			projectManager ? projectManager->getSystemOfType<CameraObjectSystem>() : nullptr;
		CameraComponentPtr camera=
			cameraSystem ? cameraSystem->findCameraForVideoSource(videoSource->getComponentId()) : nullptr;
		if (camera)
		{
			// The camera's relative transform is its stage-space aperture pose
			// however it is driven: CameraComponent::update writes the tracking
			// mount pose or the frame-coupled pose into it every tick, and an
			// authored camera holds its authored pose there
			packet.cameraToStage= camera->getRelativeTransform().getMat4();
			packet.bHasPose= true;
		}
	}

	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		if (m_queue.size() >= k_maxQueuedFrames)
		{
			++m_framesDropped;
			return;
		}
		m_queue.push_back(std::move(packet));
	}
	m_queueCondVar.notify_one();

	// A still wants exactly one frame
	if (m_job && m_job->bStill)
	{
		stopRecording();
	}
}

void VideoSourceRecorder::update()
{
	if (m_status.state == eVideoRecorderState::idle)
		return;

	if (m_status.state == eVideoRecorderState::recording)
	{
		sampleFrame();
	}

	m_status.elapsedSeconds= std::chrono::duration<double>(std::chrono::steady_clock::now() - m_startTime).count();
	m_status.framesWritten= m_framesWritten.load();
	m_status.framesDropped= m_framesDropped;

	if (m_bJobDone)
	{
		completeJob();
	}
}

void VideoSourceRecorder::finishNow()
{
	if (m_status.state == eVideoRecorderState::idle)
	{
		if (m_encoderThread.joinable())
			m_encoderThread.join();
		return;
	}

	stopRecording();
	if (m_encoderThread.joinable())
		m_encoderThread.join();
	completeJob();
}

void VideoSourceRecorder::completeJob()
{
	if (m_encoderThread.joinable())
		m_encoderThread.join();

	releaseView();

	m_status.framesWritten= m_jobResult.framesWritten;
	m_status.framesDropped= m_framesDropped;
	m_status.backendName= m_jobResult.backendName;
	if (m_jobResult.bOk)
	{
		m_status.lastStoredPath= PathUtils::makeStoredProjectPath(m_jobResult.mediaPath);
		if (ProjectAssetCatalog* catalog= m_ownerWindow->getAssetCatalog())
		{
			catalog->refresh();
		}
	}
	else
	{
		m_status.lastError= m_jobResult.error;
	}

	m_status.state= eVideoRecorderState::idle;
	m_job.reset();
	m_bJobDone= false;
}

void VideoSourceRecorder::releaseView()
{
	if (!m_view)
		return;

	if (VideoSourceComponentPtr videoSource= m_videoSource.lock())
	{
		videoSource->stopVideoStream(m_view.get());
	}
	m_view.reset();
}

// Encoder thread. Owns the writers; the only shared state is the queue, the
// finish flag, the written-frame counter, and the result handed back once done.
void VideoSourceRecorder::encoderThreadMain(std::shared_ptr<Job> job)
{
	JobResult result;
	MovieWriter movieWriter;
	std::unique_ptr<PoseTrackWriter> poseTrackWriter;
	int64_t frameIndex= 0;
	bool bWriteFailed= false;

	while (true)
	{
		FramePacket packet;
		bool bHavePacket= false;
		{
			std::unique_lock<std::mutex> lock(m_queueMutex);
			m_queueCondVar.wait(lock, [this]() { return !m_queue.empty() || m_bFinishRequested; });
			if (!m_queue.empty())
			{
				packet= std::move(m_queue.front());
				m_queue.pop_front();
				bHavePacket= true;
			}
			else if (m_bFinishRequested)
			{
				break;
			}
		}
		if (!bHavePacket)
			continue;

		if (job->bWritePoseTrack && !poseTrackWriter)
		{
			poseTrackWriter= std::make_unique<PoseTrackWriter>(job->sessionId, packet.bgr.cols, packet.bgr.rows);
		}

		if (job->bStill)
		{
			// One frame only; anything after it is ignored
			if (frameIndex > 0)
				continue;

			std::filesystem::path imagePath= job->stemPath;
			imagePath+= ".jpg";
			if (!cv::imwrite(imagePath.string(), packet.bgr, {cv::IMWRITE_JPEG_QUALITY, k_jpegQuality}))
			{
				bWriteFailed= true;
				result.error= eVideoRecorderError::writeFailed;
				break;
			}
			result.mediaPath= imagePath;
			result.backendName= "jpeg";
		}
		else
		{
			if (!movieWriter.isOpen())
			{
				if (!movieWriter.open(job->stemPath, packet.bgr.cols, packet.bgr.rows, job->fps))
				{
					bWriteFailed= true;
					result.error= eVideoRecorderError::writerOpenFailed;
					break;
				}
				result.mediaPath= movieWriter.getPath();
				result.backendName= movieWriter.getBackendName();
			}

			if (!movieWriter.write(packet.bgr))
			{
				bWriteFailed= true;
				result.error= eVideoRecorderError::writeFailed;
				break;
			}
		}

		if (poseTrackWriter && packet.bHasPose)
		{
			// The presentation time a constant-rate container gives this frame
			const int64_t timeUs= static_cast<int64_t>(std::llround(frameIndex * 1000000.0 / job->fps));
			poseTrackWriter->append(timeUs, packet.captureTimestampUs, packet.cameraToStage, job->fx, job->fy, job->cx,
									job->cy);
		}

		++frameIndex;
		m_framesWritten= frameIndex;
	}

	result.framesWritten= frameIndex;

	if (!bWriteFailed)
	{
		if (!job->bStill)
		{
			if (frameIndex == 0)
			{
				result.error= eVideoRecorderError::sourceNotStreaming;
			}
			else if (!movieWriter.close())
			{
				result.error= eVideoRecorderError::writeFailed;
			}
		}
		else if (frameIndex == 0)
		{
			result.error= eVideoRecorderError::sourceNotStreaming;
		}

		if (result.error == eVideoRecorderError::none && poseTrackWriter && poseTrackWriter->getFrameCount() > 0)
		{
			std::filesystem::path trackPath= job->stemPath;
			trackPath+= ".pose.json";
			if (!poseTrackWriter->writeToFile(trackPath))
			{
				result.error= eVideoRecorderError::writeFailed;
			}
		}
	}

	result.bOk= (result.error == eVideoRecorderError::none);
	if (result.bOk)
	{
		MIKAN_MT_LOG_INFO("VideoSourceRecorder")
			<< "Wrote " << result.mediaPath.filename() << " (" << frameIndex << " frame(s), " << result.backendName
			<< ")" << (poseTrackWriter ? " with pose track" : "");
	}
	else
	{
		// A failed movie leaves nothing behind
		std::error_code ec;
		if (!result.mediaPath.empty())
			std::filesystem::remove(result.mediaPath, ec);
		MIKAN_MT_LOG_ERROR("VideoSourceRecorder")
			<< "Capture failed for " << job->stemPath.filename() << " after " << frameIndex << " frame(s)";
	}

	m_jobResult= result;
	m_bJobDone= true;
}

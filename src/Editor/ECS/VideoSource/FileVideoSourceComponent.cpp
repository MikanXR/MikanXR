#include "FileVideoSourceComponent.h"
#include "AssetReference.h"
#include "AssetReferencePropertyMetaData.h"
#include "CameraMath.h"
#include "EnumPropertyMetaData.h"
#include "Logger.h"
#include "MikanObject.h"
#include "MikanServer.h"
#include "MovieAssetReference.h"
#include "MovieDecoder.h"
#include "PathUtils.h"
#include "PlaybackTime.h"
#include "PoseTrackAssetReference.h"
#include "ThreadUtils.h"
#include "VideoSourceRequestHandler.h"

#include "opencv2/opencv.hpp"

#include <algorithm>
#include <assert.h>
#include <cmath>

namespace
{
const std::string k_playbackStateLocKeys[]= {
	"propertyValues.playback_stopped",
	"propertyValues.playback_playing",
	"propertyValues.playback_paused",
};

// A frame this far behind the clock is skipped rather than shown
constexpr int k_skipBehindFramePeriods= 2;
// A frame this far behind the clock means the decoder lost the clock entirely
// (an editor stall), so the source seeks instead of decoding forward
constexpr int64_t k_seekBehindUs= 1000000;

// Where a phone leaves the pose sidecar for a piece of media: the same folder,
// the same stem, a two-part extension
std::filesystem::path sidecarPathForMedia(const std::filesystem::path& mediaPath)
{
	return mediaPath.parent_path() / (mediaPath.stem().string() + PoseTrackAssetReferenceFactory::k_sidecarSuffix);
}

// The sidecar's stored path when the file exists, otherwise empty
std::filesystem::path sidecarPathIfPresent(const std::filesystem::path& mediaPath)
{
	if (mediaPath.empty())
		return std::filesystem::path();

	const std::filesystem::path sidecar= sidecarPathForMedia(mediaPath);
	if (PathUtils::resolveProjectResource(sidecar).empty())
		return std::filesystem::path();

	return sidecar;
}
} // namespace

// -- FileVideoSourceDefinition -----
const std::string FileVideoSourceDefinition::k_mediaPathPropertyId= "media_path";
const std::string FileVideoSourceDefinition::k_markerMediaPathPropertyId= "marker_media_path";
const std::string FileVideoSourceDefinition::k_poseTrackPathPropertyId= "pose_track_path";
const std::string FileVideoSourceDefinition::k_markerPoseTrackPathPropertyId= "marker_pose_track_path";
const std::string FileVideoSourceDefinition::k_loopPropertyId= "loop";
const std::string FileVideoSourceDefinition::k_poseOffsetPropertyId= "pose_offset";

FileVideoSourceDefinition::FileVideoSourceDefinition()
	: VideoSourceDefinition()
	, m_mediaAssetRefConfig(MediaAssetReferenceFactory().allocateAssetReferenceConfig())
	, m_markerMediaAssetRefConfig(MediaAssetReferenceFactory().allocateAssetReferenceConfig())
	, m_poseTrackAssetRefConfig(PoseTrackAssetReferenceFactory().allocateAssetReferenceConfig())
	, m_markerPoseTrackAssetRefConfig(PoseTrackAssetReferenceFactory().allocateAssetReferenceConfig())
	, m_poseOffset(1.f)
{
}

FileVideoSourceDefinition::FileVideoSourceDefinition(MikanVideoSourceID videoSourceId)
	: VideoSourceDefinition(videoSourceId)
	, m_mediaAssetRefConfig(MediaAssetReferenceFactory().allocateAssetReferenceConfig())
	, m_markerMediaAssetRefConfig(MediaAssetReferenceFactory().allocateAssetReferenceConfig())
	, m_poseTrackAssetRefConfig(PoseTrackAssetReferenceFactory().allocateAssetReferenceConfig())
	, m_markerPoseTrackAssetRefConfig(PoseTrackAssetReferenceFactory().allocateAssetReferenceConfig())
	, m_poseOffset(1.f)
{
}

configuru::Config FileVideoSourceDefinition::writeToJSON()
{
	configuru::Config pt= VideoSourceDefinition::writeToJSON();

	if (m_mediaAssetRefConfig->isValid())
		pt[k_mediaPathPropertyId]= m_mediaAssetRefConfig->writeToJSON();
	if (m_markerMediaAssetRefConfig->isValid())
		pt[k_markerMediaPathPropertyId]= m_markerMediaAssetRefConfig->writeToJSON();
	if (m_poseTrackAssetRefConfig->isValid())
		pt[k_poseTrackPathPropertyId]= m_poseTrackAssetRefConfig->writeToJSON();
	if (m_markerPoseTrackAssetRefConfig->isValid())
		pt[k_markerPoseTrackPathPropertyId]= m_markerPoseTrackAssetRefConfig->writeToJSON();
	pt[k_loopPropertyId]= m_bLoop;

	// Written column-major, matching glm's own storage, and only when one has
	// been solved: an absent key reads back as never aligned
	if (m_bHasPoseOffset)
	{
		writePoseOffset(pt, m_poseOffset);
	}

	return pt;
}

void FileVideoSourceDefinition::writePoseOffset(configuru::Config& pt, const glm::mat4& poseOffset)
{
	std::vector<configuru::Config> offsetValues;
	offsetValues.reserve(16);
	for (int column= 0; column < 4; ++column)
	{
		for (int row= 0; row < 4; ++row)
		{
			offsetValues.push_back(configuru::Config(poseOffset[column][row]));
		}
	}
	pt[k_poseOffsetPropertyId]= offsetValues;
}

void FileVideoSourceDefinition::readFromJSON(const configuru::Config& pt)
{
	VideoSourceDefinition::readFromJSON(pt);

	m_mediaAssetRefConfig= MediaAssetReferenceFactory().allocateAssetReferenceConfig();
	if (pt.has_key(k_mediaPathPropertyId))
		m_mediaAssetRefConfig->readFromJSON(pt[k_mediaPathPropertyId]);

	m_markerMediaAssetRefConfig= MediaAssetReferenceFactory().allocateAssetReferenceConfig();
	if (pt.has_key(k_markerMediaPathPropertyId))
		m_markerMediaAssetRefConfig->readFromJSON(pt[k_markerMediaPathPropertyId]);

	m_poseTrackAssetRefConfig= PoseTrackAssetReferenceFactory().allocateAssetReferenceConfig();
	if (pt.has_key(k_poseTrackPathPropertyId))
		m_poseTrackAssetRefConfig->readFromJSON(pt[k_poseTrackPathPropertyId]);

	m_markerPoseTrackAssetRefConfig= PoseTrackAssetReferenceFactory().allocateAssetReferenceConfig();
	if (pt.has_key(k_markerPoseTrackPathPropertyId))
		m_markerPoseTrackAssetRefConfig->readFromJSON(pt[k_markerPoseTrackPathPropertyId]);

	m_bLoop= pt.get_or<bool>(k_loopPropertyId, m_bLoop);

	m_bHasPoseOffset= false;
	m_poseOffset= glm::mat4(1.f);
	if (pt.has_key(k_poseOffsetPropertyId))
	{
		const configuru::Config& offsetValues= pt[k_poseOffsetPropertyId];
		if (offsetValues.is_array() && offsetValues.array_size() == 16)
		{
			for (int index= 0; index < 16; ++index)
			{
				m_poseOffset[index / 4][index % 4]= (float)offsetValues[index].as_float();
			}
			m_bHasPoseOffset= true;
		}
		else
		{
			MIKAN_LOG_WARNING("FileVideoSourceDefinition::readFromJSON")
				<< "Ignoring malformed " << k_poseOffsetPropertyId << " (expected 16 floats)";
		}
	}
}

bool FileVideoSourceDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
												   const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!VideoSourceDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* values= initParams.getTypedPointer<MikanFileVideoSourceValues>();
	if (values)
	{
		m_mediaAssetRefConfig->assetPath= PathUtils::utf8CStrToPathString(values->media_path.getUtf8Value());
		m_markerMediaAssetRefConfig->assetPath=
			PathUtils::utf8CStrToPathString(values->marker_media_path.getUtf8Value());
		m_poseTrackAssetRefConfig->assetPath= PathUtils::utf8CStrToPathString(values->pose_track_path.getUtf8Value());
		m_markerPoseTrackAssetRefConfig->assetPath=
			PathUtils::utf8CStrToPathString(values->marker_pose_track_path.getUtf8Value());
		m_bLoop= values->loop;
	}

	return true;
}

bool FileVideoSourceDefinition::setAssetPath(AssetReferenceConfigPtr& assetRefConfig, const std::filesystem::path& path)
{
	const std::string stored= path.empty() ? std::string() : PathUtils::makeStoredProjectPath(path);
	if (stored == assetRefConfig->assetPath)
		return false;

	assetRefConfig->assetPath= stored;
	return true;
}

std::filesystem::path FileVideoSourceDefinition::getMediaPath() const { return m_mediaAssetRefConfig->assetPath; }

void FileVideoSourceDefinition::setMediaPath(const std::filesystem::path& mediaPath)
{
	if (setAssetPath(m_mediaAssetRefConfig, mediaPath))
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_mediaPathPropertyId));
}

std::filesystem::path FileVideoSourceDefinition::getMarkerMediaPath() const
{
	return m_markerMediaAssetRefConfig->assetPath;
}

void FileVideoSourceDefinition::setMarkerMediaPath(const std::filesystem::path& mediaPath)
{
	if (setAssetPath(m_markerMediaAssetRefConfig, mediaPath))
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_markerMediaPathPropertyId));
}

std::filesystem::path FileVideoSourceDefinition::getPoseTrackPath() const
{
	return m_poseTrackAssetRefConfig->assetPath;
}

void FileVideoSourceDefinition::setPoseTrackPath(const std::filesystem::path& trackPath)
{
	if (setAssetPath(m_poseTrackAssetRefConfig, trackPath))
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_poseTrackPathPropertyId));
}

std::filesystem::path FileVideoSourceDefinition::getMarkerPoseTrackPath() const
{
	return m_markerPoseTrackAssetRefConfig->assetPath;
}

void FileVideoSourceDefinition::setMarkerPoseTrackPath(const std::filesystem::path& trackPath)
{
	if (setAssetPath(m_markerPoseTrackAssetRefConfig, trackPath))
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_markerPoseTrackPathPropertyId));
}

void FileVideoSourceDefinition::setLoop(bool bLoop)
{
	if (bLoop != m_bLoop)
	{
		m_bLoop= bLoop;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_loopPropertyId));
	}
}

void FileVideoSourceDefinition::setPoseOffset(const glm::mat4& poseOffset)
{
	m_poseOffset= poseOffset;
	m_bHasPoseOffset= true;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_poseOffsetPropertyId));
}

void FileVideoSourceDefinition::clearPoseOffset()
{
	m_poseOffset= glm::mat4(1.f);
	m_bHasPoseOffset= false;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_poseOffsetPropertyId));
}

// -- FileVideoSourceComponent -----
const std::string FileVideoSourceComponent::k_playbackStatePropertyId= "playback_state";
const std::string FileVideoSourceComponent::k_playbackTimePropertyId= "playback_time";
const std::string FileVideoSourceComponent::k_durationSecondsPropertyId= "duration_seconds";
const std::string FileVideoSourceComponent::k_playFunctionId= "play";
const std::string FileVideoSourceComponent::k_pauseFunctionId= "pause";
const std::string FileVideoSourceComponent::k_stopFunctionId= "stop";

FileVideoSourceComponent::FileVideoSourceComponent(MikanObjectWeakPtr owner)
	: VideoSourceComponent(owner)
{
}

FileVideoSourceComponent::~FileVideoSourceComponent() { stopWorker(); }

Serialization::StructTypeHandle FileVideoSourceComponent::getClientAPIValuesStructType() const
{
	return &MikanFileVideoSourceValues::staticGetArchetype();
}

void FileVideoSourceComponent::init()
{
	m_bWantsUpdate= true;
	MikanComponent::init();

	openVideoSource();
}

void FileVideoSourceComponent::dispose()
{
	closeVideoSource();

	MikanComponent::dispose();
}

void FileVideoSourceComponent::setDefinition(MikanComponentDefinitionPtr definition)
{
	MikanComponent::setDefinition(definition);

	closeVideoSource();
}

// -- Video Source Interface ----
std::string FileVideoSourceComponent::getDevicePath() const { return resolveActiveMediaPath().string(); }

std::string FileVideoSourceComponent::getDeviceAPI() const { return "FileVideoSource"; }

std::filesystem::path FileVideoSourceComponent::resolveActiveMediaPath() const
{
	FileVideoSourceDefinitionPtr definition= getFileVideoSourceDefinition();
	const std::filesystem::path storedPath= (m_activeMedia == eActiveMedia::markerReference)
												? definition->getMarkerMediaPath()
												: definition->getMediaPath();
	if (storedPath.empty())
		return std::filesystem::path();

	return PathUtils::resolveProjectResource(storedPath);
}

bool FileVideoSourceComponent::openVideoSource()
{
	if (m_bIsOpen || m_bOpenPending)
		return true;

	const std::filesystem::path mediaPath= resolveActiveMediaPath();
	if (mediaPath.empty())
		return false;

	loadPoseTracks();
	m_bAutoPlayOnOpen= true;
	startWorker();
	requestOpen(mediaPath);
	return true;
}

// A path edit swaps the media in place. Unlike closeVideoSource this keeps every
// view subscribed, so a source being shown by the settings stage or the
// compositor picks up the new file rather than freezing on the last frame of
// the old one.
void FileVideoSourceComponent::reopenMedia()
{
	m_activeMedia= eActiveMedia::main;
	m_bMarkerReferenceActive= false;
	m_playbackState= eFilePlaybackState::stopped;
	m_playbackTime= 0.0;
	m_bOpenFailed= false;
	m_currentFrame.release();
	{
		std::lock_guard<std::mutex> lock(m_latestPoseMutex);
		m_latestPose.bValid= false;
	}

	loadPoseTracks();

	const std::filesystem::path mediaPath= resolveActiveMediaPath();
	if (mediaPath.empty())
	{
		stopWorker();
		m_bIsOpen= false;
		m_bOpenPending= false;
		return;
	}

	m_bAutoPlayOnOpen= true;
	startWorker();
	requestOpen(mediaPath);
}

void FileVideoSourceComponent::closeVideoSource()
{
	stopWorker();
	forceStopVideoStream();

	const bool bWasOpen= m_bEverOpened;
	clearRuntimeState();

	if (bWasOpen)
	{
		MikanServer::getInstance()->getVideoSourceRequestHandler()->publishVideoSourceClosedEvent();
		if (OnClosed)
		{
			OnClosed(getSelfPtr<VideoSourceComponent>());
		}
	}
}

void FileVideoSourceComponent::clearRuntimeState()
{
	m_activeMedia= eActiveMedia::main;
	m_playbackState= eFilePlaybackState::stopped;
	m_playbackTime= 0.0;
	m_durationSeconds= 0.0;
	m_fps= 0.0;
	m_width= 0;
	m_height= 0;
	m_bIsStill= false;
	m_bIsOpen= false;
	m_bOpenPending= false;
	m_bOpenFailed= false;
	m_bEverOpened= false;
	m_bAutoPlayOnOpen= false;
	m_bStreamRequested= false;
	m_bDeliverNextFrame= false;
	m_currentFrame.release();
	m_currentPtsUs= 0;
	m_secondsSinceEmit= 0.0;
	m_bMarkerReferenceActive= false;
	m_mainPoseTrack.clear();
	m_markerPoseTrack.clear();
	m_bHasAlignmentReference= false;

	std::lock_guard<std::mutex> lock(m_latestPoseMutex);
	m_latestPose= LatestPose();
}

eVideoStreamingStatus FileVideoSourceComponent::startVideoStreamInternal()
{
	m_bStreamRequested= true;

	if (!m_bIsOpen && !m_bOpenPending && !m_bOpenFailed)
	{
		openVideoSource();
	}

	const eVideoStreamingStatus status= getVideoStreamingStatus();
	if (OnStarted && (status == eVideoStreamingStatus::started || status == eVideoStreamingStatus::pendingStart))
	{
		OnStarted(getSelfPtr<VideoSourceComponent>());
	}

	return status;
}

void FileVideoSourceComponent::stopVideoStreamInternal()
{
	if (!m_bStreamRequested)
		return;

	m_bStreamRequested= false;
	if (OnStopped)
	{
		OnStopped(getSelfPtr<VideoSourceComponent>());
	}
}

eVideoStreamingStatus FileVideoSourceComponent::getVideoStreamingStatus() const
{
	if (!m_bStreamRequested)
		return eVideoStreamingStatus::stopped;
	if (m_bOpenFailed)
		return eVideoStreamingStatus::failed;
	if (m_bIsOpen)
		return eVideoStreamingStatus::started;

	return eVideoStreamingStatus::pendingStart;
}

bool FileVideoSourceComponent::getVideoPixelDimensions(int& outPixelWidth, int& outPixelHeight) const
{
	if (m_width > 0 && m_height > 0)
	{
		outPixelWidth= m_width;
		outPixelHeight= m_height;
		return true;
	}

	return false;
}

bool FileVideoSourceComponent::getVideoModeName(std::string& outVideoModeName) const
{
	if (!m_bIsOpen)
		return false;

	const std::filesystem::path mediaPath= resolveActiveMediaPath();
	outVideoModeName= mediaPath.filename().string() + " " + std::to_string(m_width) + "x" + std::to_string(m_height)
					  + "@" + std::to_string((int)std::lround(m_fps));
	return true;
}

bool FileVideoSourceComponent::getFrameRate(float& outFrameRate) const
{
	if (!m_bIsOpen)
		return false;

	outFrameRate= (float)m_fps;
	return true;
}

// -- Worker thread ----
void FileVideoSourceComponent::startWorker()
{
	if (m_workerThread.joinable())
		return;

	{
		std::lock_guard<std::mutex> lock(m_requestMutex);
		m_requests= DecodeRequests();
	}
	{
		std::lock_guard<std::mutex> lock(m_resultMutex);
		m_openResult= OpenResult();
		m_decodedFrame= DecodedFrame();
	}

	m_workerThread= std::thread([this]() { workerMain(); });
}

void FileVideoSourceComponent::stopWorker()
{
	if (!m_workerThread.joinable())
		return;

	{
		std::lock_guard<std::mutex> lock(m_requestMutex);
		m_requests.bQuit= true;
	}
	m_requestCondVar.notify_one();
	m_workerThread.join();

	std::lock_guard<std::mutex> lock(m_resultMutex);
	m_openResult= OpenResult();
	m_decodedFrame= DecodedFrame();
}

// The worker owns the decoder outright, so no decoder call ever crosses a
// thread. It touches nothing else on the component but the two mailboxes.
void FileVideoSourceComponent::workerMain()
{
	MovieDecoder decoder;
	uint32_t lastSeekGeneration= 0;

	while (true)
	{
		DecodeRequests requests;
		{
			std::unique_lock<std::mutex> lock(m_requestMutex);
			m_requestCondVar.wait(
				lock, [this]()
				{ return m_requests.bOpen || m_requests.bSeek || m_requests.bDecodeNext || m_requests.bQuit; });
			requests= m_requests;
			m_requests= DecodeRequests();
		}

		if (requests.bQuit)
			break;

		if (requests.bOpen)
		{
			decoder.close();
			const bool bSucceeded= decoder.open(requests.openPath);

			std::lock_guard<std::mutex> lock(m_resultMutex);
			m_openResult.bPending= true;
			m_openResult.bSucceeded= bSucceeded;
			m_openResult.bIsStill= decoder.isStill();
			m_openResult.width= decoder.getWidth();
			m_openResult.height= decoder.getHeight();
			m_openResult.fps= decoder.getFps();
			m_openResult.durationSeconds= decoder.getDurationSeconds();
			// A frame from the previous file must not be shown under the new one
			m_decodedFrame= DecodedFrame();
		}

		if (requests.bSeek && decoder.isOpen())
		{
			decoder.seek(requests.seekSeconds);
			lastSeekGeneration= requests.seekGeneration;
		}

		if (requests.bDecodeNext && decoder.isOpen())
		{
			cv::Mat frame;
			int64_t ptsUs= 0;
			const bool bGotFrame= decoder.readNext(frame, ptsUs);

			std::lock_guard<std::mutex> lock(m_resultMutex);
			m_decodedFrame.frame= std::move(frame);
			m_decodedFrame.ptsUs= ptsUs;
			m_decodedFrame.seekGeneration= lastSeekGeneration;
			m_decodedFrame.bEndOfStream= !bGotFrame;
			m_decodedFrame.bReady= true;
		}
	}

	decoder.close();
}

void FileVideoSourceComponent::requestOpen(const std::filesystem::path& absolutePath)
{
	// Nothing from the old file is shown while the new one opens
	m_bIsOpen= false;
	m_bOpenPending= true;
	m_bOpenFailed= false;
	m_currentFrame.release();
	{
		std::lock_guard<std::mutex> lock(m_latestPoseMutex);
		m_latestPose.bValid= false;
	}

	{
		std::lock_guard<std::mutex> lock(m_requestMutex);
		m_requests.bOpen= true;
		m_requests.openPath= absolutePath;
		m_requests.bSeek= false;
		m_requests.bDecodeNext= false;
	}
	m_requestCondVar.notify_one();
}

void FileVideoSourceComponent::requestSeek(double timeSeconds)
{
	// Frames decoded before this seek carry the previous generation and are
	// discarded when they arrive
	++m_seekGeneration;
	m_bDeliverNextFrame= true;

	{
		std::lock_guard<std::mutex> lock(m_requestMutex);
		m_requests.bSeek= true;
		m_requests.seekSeconds= timeSeconds;
		m_requests.seekGeneration= m_seekGeneration;
		m_requests.bDecodeNext= true;
	}
	m_requestCondVar.notify_one();
}

void FileVideoSourceComponent::requestDecodeNext()
{
	{
		std::lock_guard<std::mutex> lock(m_requestMutex);
		m_requests.bDecodeNext= true;
	}
	m_requestCondVar.notify_one();
}

// -- Per-tick playback ----
void FileVideoSourceComponent::update(float deltaSeconds)
{
	VideoSourceComponent::update(deltaSeconds);

	applyOpenResult();

	// Nobody is watching, so the clock holds and the worker idles
	if (!m_bIsOpen || !m_bHasAnyActiveViews)
		return;

	if (m_playbackState == eFilePlaybackState::playing && !m_bIsStill)
	{
		const bool bLoop= m_bMarkerReferenceActive || getFileVideoSourceDefinition()->getLoop();
		float playbackTime= (float)m_playbackTime;
		bool bWrapped= false;
		bool bFinished= false;
		advancePlaybackTime(playbackTime, deltaSeconds, (float)m_durationSeconds, bLoop, bWrapped, bFinished);
		m_playbackTime= playbackTime;

		if (bWrapped)
		{
			requestSeek(m_playbackTime);
		}
		else if (bFinished)
		{
			finishPlayback();
		}
	}

	tryDeliverDecodedFrame();
	reemitCurrentFrame(deltaSeconds);
}

void FileVideoSourceComponent::applyOpenResult()
{
	OpenResult result;
	{
		std::lock_guard<std::mutex> lock(m_resultMutex);
		if (!m_openResult.bPending)
			return;

		result= m_openResult;
		m_openResult.bPending= false;
	}

	m_bOpenPending= false;
	if (!result.bSucceeded)
	{
		m_bOpenFailed= true;
		m_bIsOpen= false;
		MIKAN_LOG_ERROR("FileVideoSourceComponent::applyOpenResult")
			<< "Failed to open media: " << resolveActiveMediaPath();
		return;
	}

	const bool bSizeChanged= (m_width != result.width || m_height != result.height);
	m_width= result.width;
	m_height= result.height;
	m_fps= result.fps;
	m_durationSeconds= result.durationSeconds;
	m_bIsStill= result.bIsStill;
	m_bIsOpen= true;
	m_bOpenFailed= false;

	seedIntrinsicsFromOpenMedia();

	// A movie plays from the moment it opens, so a headless or client-driven
	// session sees it behave like a live camera. A still is always showing. The
	// marker reference switch restores its own saved state instead.
	if (m_bAutoPlayOnOpen && !m_bIsStill && m_playbackState == eFilePlaybackState::stopped)
	{
		m_playbackState= eFilePlaybackState::playing;
	}
	m_bAutoPlayOnOpen= false;

	if (!m_bEverOpened)
	{
		m_bEverOpened= true;

		MikanServer::getInstance()->getVideoSourceRequestHandler()->publishVideoSourceOpenedEvent();
		if (OnOpened)
		{
			OnOpened(getSelfPtr<VideoSourceComponent>());
		}
	}
	else if (bSizeChanged)
	{
		recomputeCameraProjectionMatrix();

		MikanServer::getInstance()->getVideoSourceRequestHandler()->publishVideoSourceModeChangedEvent();
		if (OnFrameSizeChanged)
		{
			OnFrameSizeChanged(getSelfPtr<VideoSourceComponent>());
		}
	}

	// The first frame of the newly opened media shows whatever the clock says
	m_playbackTime= std::clamp(m_playbackTime, 0.0, m_durationSeconds);
	if (m_playbackTime > 0.0 && !m_bIsStill)
	{
		requestSeek(m_playbackTime);
	}
	else
	{
		m_bDeliverNextFrame= true;
		requestDecodeNext();
	}
}

// The align stages refuse a source without valid intrinsics, so the media
// seeds them the moment it opens: from the pose track when there is one,
// otherwise a default set from the frame size when nothing has ever been
// calibrated. A calibration the operator ran is never overwritten.
void FileVideoSourceComponent::seedIntrinsicsFromOpenMedia()
{
	const PoseTrack& track= getActivePoseTrack();

	MikanMonoIntrinsics monoIntrinsics;
	if (track.isLoaded() && track.getFrameCount() > 0)
	{
		const PoseTrackFrame& frame= track.getFrame(0);
		createMonoIntrinsicsFromPinhole(track.getImageWidth(), track.getImageHeight(), frame.fx, frame.fy, frame.cx,
										frame.cy, monoIntrinsics);
	}
	else if (!areCameraIntrinsicsValid())
	{
		createDefautMonoIntrinsics(m_width, m_height, monoIntrinsics);
	}
	else
	{
		return;
	}

	// Skip the write when nothing moved, since setting intrinsics marks the
	// definition dirty and every open would otherwise record a change
	MikanVideoSourceIntrinsics current;
	if (getCameraIntrinsics(current) && current.intrinsics_type == MikanIntrinsicsType::MONO_CAMERA_INTRINSICS)
	{
		const MikanMonoIntrinsics& mono= current.getMonoIntrinsics();
		const MikanMatrix3d& a= mono.undistorted_camera_matrix;
		const MikanMatrix3d& b= monoIntrinsics.undistorted_camera_matrix;
		if (mono.pixel_width == monoIntrinsics.pixel_width && mono.pixel_height == monoIntrinsics.pixel_height
			&& a.x0 == b.x0 && a.y1 == b.y1 && a.z0 == b.z0 && a.z1 == b.z1)
		{
			return;
		}
	}

	MikanVideoSourceIntrinsics intrinsics;
	intrinsics.makeMonoIntrinsics()= monoIntrinsics;
	setCameraIntrinsics(intrinsics);
}

void FileVideoSourceComponent::tryDeliverDecodedFrame()
{
	DecodedFrame decoded;
	{
		std::lock_guard<std::mutex> lock(m_resultMutex);
		if (!m_decodedFrame.bReady)
			return;

		// Decoded before the latest seek landed. The seek request carries its
		// own decode, so asking for another here would queue a second decode
		// behind it and overwrite the seek's frame in the slot before it is read.
		if (m_decodedFrame.seekGeneration != m_seekGeneration)
		{
			m_decodedFrame= DecodedFrame();
			return;
		}

		if (m_decodedFrame.bEndOfStream)
		{
			m_decodedFrame= DecodedFrame();
			const bool bLoop= m_bMarkerReferenceActive || getFileVideoSourceDefinition()->getLoop();
			if (bLoop && !m_bIsStill)
			{
				m_playbackTime= 0.0;
				requestSeek(0.0);
			}
			else
			{
				finishPlayback();
			}
			return;
		}

		if (!m_bDeliverNextFrame && !m_bIsStill)
		{
			const int64_t playbackUs= (int64_t)std::llround(m_playbackTime * 1000000.0);
			const int64_t framePeriodUs= (m_fps > 0.0) ? (int64_t)std::llround(1000000.0 / m_fps) : 0;

			// Not due yet
			if (m_decodedFrame.ptsUs > playbackUs)
				return;

			// Far enough behind that showing it would step backwards in time
			if (m_decodedFrame.ptsUs + framePeriodUs * k_skipBehindFramePeriods < playbackUs)
			{
				const int64_t staleFramePtsUs= m_decodedFrame.ptsUs;
				m_decodedFrame= DecodedFrame();
				if (staleFramePtsUs + k_seekBehindUs < playbackUs)
					requestSeek(m_playbackTime);
				else
					requestDecodeNext();
				return;
			}
		}

		decoded= std::move(m_decodedFrame);
		m_decodedFrame= DecodedFrame();
	}

	m_bDeliverNextFrame= false;
	deliverFrame(std::move(decoded.frame), decoded.ptsUs);

	// A still has one frame, which re-emission keeps alive
	if (!m_bIsStill)
	{
		requestDecodeNext();
	}
}

void FileVideoSourceComponent::deliverFrame(cv::Mat&& frame, int64_t ptsUs)
{
	if (frame.empty())
		return;

	m_currentFrame= std::move(frame);
	m_currentPtsUs= ptsUs;
	++m_frameSeq;

	publishPoseForFrame(ptsUs);

	const bool bIsFrameMirrored= getFileVideoSourceDefinition()->getIsFrameMirrored();
	writeVideoFrame(m_currentFrame.data, cv::Size(m_currentFrame.cols, m_currentFrame.rows), bIsFrameMirrored);
	m_secondsSinceEmit= 0.0;
}

// A paused movie or a still would otherwise deliver one frame and then go
// quiet, and the compositor and the client frame events are both paced by
// frames arriving. Re-sending the same pixels at the nominal rate keeps that
// loop alive. The pose and frameSeq stay put: nothing new was observed.
void FileVideoSourceComponent::reemitCurrentFrame(float deltaSeconds)
{
	if (m_currentFrame.empty())
		return;

	// A playing movie paces itself by delivering new frames
	if (m_playbackState == eFilePlaybackState::playing && !m_bIsStill)
		return;

	const double nominalFps= m_bIsStill ? MovieDecoder::k_stillNominalFps : m_fps;
	if (nominalFps <= 0.0)
		return;

	m_secondsSinceEmit+= deltaSeconds;
	if (m_secondsSinceEmit < 1.0 / nominalFps)
		return;

	const bool bIsFrameMirrored= getFileVideoSourceDefinition()->getIsFrameMirrored();
	writeVideoFrame(m_currentFrame.data, cv::Size(m_currentFrame.cols, m_currentFrame.rows), bIsFrameMirrored);
	m_secondsSinceEmit= 0.0;
}

void FileVideoSourceComponent::finishPlayback()
{
	m_playbackState= eFilePlaybackState::paused;
	m_playbackTime= m_durationSeconds;
}

// -- Playback controls ----
void FileVideoSourceComponent::play()
{
	if (m_playbackState == eFilePlaybackState::stopped)
	{
		m_playbackTime= 0.0;
		if (m_bIsOpen && !m_bIsStill)
			requestSeek(0.0);
	}
	m_playbackState= eFilePlaybackState::playing;
}

void FileVideoSourceComponent::pause()
{
	if (m_playbackState == eFilePlaybackState::playing)
	{
		m_playbackState= eFilePlaybackState::paused;
	}
}

void FileVideoSourceComponent::stop()
{
	m_playbackState= eFilePlaybackState::stopped;
	m_playbackTime= 0.0;
	if (m_bIsOpen && !m_bIsStill)
		requestSeek(0.0);
}

void FileVideoSourceComponent::seekTo(double timeSeconds)
{
	m_playbackTime= std::clamp(timeSeconds, 0.0, m_durationSeconds);
	if (m_bIsOpen && !m_bIsStill)
		requestSeek(m_playbackTime);
}

// -- Pose tracks ----
const PoseTrack& FileVideoSourceComponent::getActivePoseTrack() const
{
	return (m_activeMedia == eActiveMedia::markerReference) ? m_markerPoseTrack : m_mainPoseTrack;
}

bool FileVideoSourceComponent::loadPoseTrack(const std::filesystem::path& storedPath, PoseTrack& outTrack)
{
	outTrack.clear();
	if (storedPath.empty())
		return false;

	return outTrack.loadFromFile(storedPath);
}

void FileVideoSourceComponent::loadPoseTracks()
{
	FileVideoSourceDefinitionPtr definition= getFileVideoSourceDefinition();

	loadPoseTrack(definition->getPoseTrackPath(), m_mainPoseTrack);
	loadPoseTrack(definition->getMarkerPoseTrackPath(), m_markerPoseTrack);

	// The reference is only usable when the offset solved from it applies to
	// the main take: both carry poses from the same tracking session, or
	// neither carries poses and the camera is treated as fixed
	m_bHasAlignmentReference= false;
	if (definition->getMarkerMediaPath().empty())
		return;

	if (m_mainPoseTrack.isLoaded())
	{
		if (!m_markerPoseTrack.isLoaded())
		{
			MIKAN_LOG_WARNING("FileVideoSourceComponent::loadPoseTracks")
				<< "The main media has a pose track but the marker reference has none, so the reference cannot "
				<< "be used for alignment";
			return;
		}
		if (m_markerPoseTrack.getSessionId() != m_mainPoseTrack.getSessionId())
		{
			MIKAN_LOG_WARNING("FileVideoSourceComponent::loadPoseTracks")
				<< "The marker reference was recorded in a different tracking session ("
				<< m_markerPoseTrack.getSessionId() << ") than the main media (" << m_mainPoseTrack.getSessionId()
				<< "), so an alignment solved from it would not apply";
			return;
		}
	}

	m_bHasAlignmentReference= true;
}

void FileVideoSourceComponent::publishPoseForFrame(int64_t ptsUs)
{
	const PoseTrack& track= getActivePoseTrack();
	if (!track.isLoaded() || track.getFrameCount() == 0)
		return;

	// Half a frame period either side: a pose belongs to exactly one frame
	int64_t toleranceUs= (m_fps > 0.0) ? (int64_t)std::llround(500000.0 / m_fps) : 0;
	if (toleranceUs <= 0)
		toleranceUs= std::max<int64_t>(track.getMedianFramePeriodUs() / 2, 1);

	size_t frameIndex= 0;
	if (!track.findNearestFrame(ptsUs, toleranceUs, frameIndex))
		return;

	const PoseTrackFrame& poseFrame= track.getFrame(frameIndex);
	FileVideoSourceDefinitionPtr definition= getFileVideoSourceDefinition();

	// The single point where a recorded pose leaves its own world space
	const glm::mat4& cameraToWorld= poseFrame.cameraToWorld;
	const glm::mat4 cameraToStage=
		definition->hasPoseOffset() ? definition->getPoseOffset() * cameraToWorld : cameraToWorld;

	MikanVideoSourceIntrinsics intrinsics;
	MikanMonoIntrinsics monoIntrinsics;
	createMonoIntrinsicsFromPinhole(track.getImageWidth(), track.getImageHeight(), poseFrame.fx, poseFrame.fy,
									poseFrame.cx, poseFrame.cy, monoIntrinsics);
	intrinsics.makeMonoIntrinsics()= monoIntrinsics;

	std::lock_guard<std::mutex> lock(m_latestPoseMutex);
	m_latestPose.transform= cameraToStage;
	m_latestPose.sourceWorldTransform= cameraToWorld;
	m_latestPose.intrinsics= intrinsics;
	m_latestPose.frameSeq= m_frameSeq;
	m_latestPose.bValid= true;
}

// -- IFrameCoupledPoseProvider ----
void FileVideoSourceComponent::setPoseOffset(const glm::mat4& worldToStageXform)
{
	getFileVideoSourceDefinition()->setPoseOffset(worldToStageXform);
}

glm::mat4 FileVideoSourceComponent::getPoseOffset() const { return getFileVideoSourceDefinition()->getPoseOffset(); }

bool FileVideoSourceComponent::hasPoseOffset() const { return getFileVideoSourceDefinition()->hasPoseOffset(); }

bool FileVideoSourceComponent::getLatestFrameCoupledPose(glm::mat4& outTransform,
														 MikanVideoSourceIntrinsics& outIntrinsics,
														 uint32_t& outFrameSeq) const
{
	std::lock_guard<std::mutex> lock(m_latestPoseMutex);
	if (!m_latestPose.bValid)
		return false;

	outTransform= m_latestPose.transform;
	outIntrinsics= m_latestPose.intrinsics;
	outFrameSeq= m_latestPose.frameSeq;
	return true;
}

bool FileVideoSourceComponent::getLatestSourceWorldPose(glm::mat4& outTransform, uint32_t& outFrameSeq) const
{
	std::lock_guard<std::mutex> lock(m_latestPoseMutex);
	if (!m_latestPose.bValid)
		return false;

	outTransform= m_latestPose.sourceWorldTransform;
	outFrameSeq= m_latestPose.frameSeq;
	return true;
}

// -- IAlignmentReferenceSource ----
bool FileVideoSourceComponent::hasAlignmentReference() const { return m_bHasAlignmentReference; }

void FileVideoSourceComponent::beginAlignmentReference()
{
	if (!m_bHasAlignmentReference || m_bMarkerReferenceActive)
		return;

	m_bMarkerReferenceActive= true;
	m_savedPlaybackState= m_playbackState;
	m_savedPlaybackTime= m_playbackTime;

	// Not a close and reopen: that would drop the stage's own view subscription
	m_activeMedia= eActiveMedia::markerReference;
	m_playbackState= eFilePlaybackState::playing;
	m_playbackTime= 0.0;
	m_bAutoPlayOnOpen= false;
	startWorker();
	requestOpen(resolveActiveMediaPath());
}

void FileVideoSourceComponent::endAlignmentReference()
{
	if (!m_bMarkerReferenceActive)
		return;

	m_bMarkerReferenceActive= false;
	m_activeMedia= eActiveMedia::main;
	m_playbackState= m_savedPlaybackState;
	m_playbackTime= m_savedPlaybackTime;
	m_bAutoPlayOnOpen= false;
	startWorker();
	requestOpen(resolveActiveMediaPath());
}

// -- IPropertyInterface ----
void FileVideoSourceComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	VideoSourceComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(FileVideoSourceDefinition::k_mediaPathPropertyId, MikanVariantType::STRING)
			->addMetaData(std::make_shared<AssetReferenceFactoryMetaData>(
				AssetReferenceFactory::createFactory<MediaAssetReferenceFactory>())));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(FileVideoSourceDefinition::k_poseTrackPathPropertyId,
																  MikanVariantType::STRING)
								 ->addMetaData(std::make_shared<AssetReferenceFactoryMetaData>(
									 AssetReferenceFactory::createFactory<PoseTrackAssetReferenceFactory>())));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 FileVideoSourceDefinition::k_markerMediaPathPropertyId, MikanVariantType::STRING)
								 ->addMetaData(std::make_shared<AssetReferenceFactoryMetaData>(
									 AssetReferenceFactory::createFactory<MediaAssetReferenceFactory>())));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 FileVideoSourceDefinition::k_markerPoseTrackPathPropertyId, MikanVariantType::STRING)
								 ->addMetaData(std::make_shared<AssetReferenceFactoryMetaData>(
									 AssetReferenceFactory::createFactory<PoseTrackAssetReferenceFactory>())));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(FileVideoSourceDefinition::k_loopPropertyId, MikanVariantType::BOOL)
			->setDefaultValue(true));
	// Runtime playback state, drawn by the panel as a status line and a scrub
	// slider rather than the generic widgets
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(k_playbackStatePropertyId, MikanVariantType::INT)
								 ->setReadOnly()
								 ->setUIHidden()
								 ->addMetaData(std::make_shared<EnumPropertyMetaData>(k_playbackStateLocKeys, 3)));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(k_playbackTimePropertyId, MikanVariantType::FLOAT)->setUIHidden());
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(k_durationSecondsPropertyId, MikanVariantType::FLOAT)
								 ->setReadOnly()
								 ->setUIHidden());
}

bool FileVideoSourceComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	FileVideoSourceDefinitionPtr definition= getFileVideoSourceDefinition();

	if (propertyName == FileVideoSourceDefinition::k_mediaPathPropertyId)
	{
		outValue= definition->getMediaPath().string();
		return true;
	}
	else if (propertyName == FileVideoSourceDefinition::k_markerMediaPathPropertyId)
	{
		outValue= definition->getMarkerMediaPath().string();
		return true;
	}
	else if (propertyName == FileVideoSourceDefinition::k_poseTrackPathPropertyId)
	{
		outValue= definition->getPoseTrackPath().string();
		return true;
	}
	else if (propertyName == FileVideoSourceDefinition::k_markerPoseTrackPathPropertyId)
	{
		outValue= definition->getMarkerPoseTrackPath().string();
		return true;
	}
	else if (propertyName == FileVideoSourceDefinition::k_loopPropertyId)
	{
		outValue= definition->getLoop();
		return true;
	}
	else if (propertyName == k_playbackStatePropertyId)
	{
		outValue= (int)m_playbackState;
		return true;
	}
	else if (propertyName == k_playbackTimePropertyId)
	{
		outValue= getPlaybackTime();
		return true;
	}
	else if (propertyName == k_durationSecondsPropertyId)
	{
		outValue= getDurationSeconds();
		return true;
	}

	return VideoSourceComponent::getPropertyValue(propertyName, outValue);
}

bool FileVideoSourceComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	FileVideoSourceDefinitionPtr definition= getFileVideoSourceDefinition();

	if (propertyName == FileVideoSourceDefinition::k_mediaPathPropertyId)
	{
		definition->setMediaPath(PathUtils::utf8CStrToPathString(inValue.getUtf8Value()));
		return true;
	}
	else if (propertyName == FileVideoSourceDefinition::k_markerMediaPathPropertyId)
	{
		definition->setMarkerMediaPath(PathUtils::utf8CStrToPathString(inValue.getUtf8Value()));
		return true;
	}
	else if (propertyName == FileVideoSourceDefinition::k_poseTrackPathPropertyId)
	{
		definition->setPoseTrackPath(PathUtils::utf8CStrToPathString(inValue.getUtf8Value()));
		return true;
	}
	else if (propertyName == FileVideoSourceDefinition::k_markerPoseTrackPathPropertyId)
	{
		definition->setMarkerPoseTrackPath(PathUtils::utf8CStrToPathString(inValue.getUtf8Value()));
		return true;
	}
	else if (propertyName == FileVideoSourceDefinition::k_loopPropertyId)
	{
		definition->setLoop(inValue.getBoolValue());
		return true;
	}
	else if (propertyName == k_playbackTimePropertyId)
	{
		seekTo(inValue.getFloatValue());
		return true;
	}

	return VideoSourceComponent::setPropertyValue(propertyName, inValue);
}

// -- IFunctionInterface ----
void FileVideoSourceComponent::getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
{
	VideoSourceComponent::getFunctionDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<FunctionDescriptor>(k_playFunctionId, "Play"));
	outDescriptors.push_back(std::make_shared<FunctionDescriptor>(k_pauseFunctionId, "Pause"));
	outDescriptors.push_back(std::make_shared<FunctionDescriptor>(k_stopFunctionId, "Stop"));
}

bool FileVideoSourceComponent::invokeFunction(const std::string& functionName)
{
	if (functionName == k_playFunctionId)
	{
		play();
		return true;
	}
	else if (functionName == k_pauseFunctionId)
	{
		pause();
		return true;
	}
	else if (functionName == k_stopFunctionId)
	{
		stop();
		return true;
	}

	return VideoSourceComponent::invokeFunction(functionName);
}

void FileVideoSourceComponent::onDefinitionMarkedDirty(CommonConfigPtr configPtr,
													   const ConfigPropertyChangeSet& changedPropertySet)
{
	FileVideoSourceDefinitionPtr definition= getFileVideoSourceDefinition();

	// A pose track belongs to one media file, and a phone leaves it beside that
	// file. Naming the media therefore names the track: the sidecar when one
	// sits there, otherwise nothing, so a track from the previous media never
	// lingers under a new one. The path stays visible and editable on the panel
	// for the case where the track was moved.
	if (changedPropertySet.hasPropertyName(FileVideoSourceDefinition::k_mediaPathPropertyId))
	{
		definition->setPoseTrackPath(sidecarPathIfPresent(definition->getMediaPath()));
	}
	if (changedPropertySet.hasPropertyName(FileVideoSourceDefinition::k_markerMediaPathPropertyId))
	{
		definition->setMarkerPoseTrackPath(sidecarPathIfPresent(definition->getMarkerMediaPath()));
	}

	if (changedPropertySet.hasPropertyName(FileVideoSourceDefinition::k_mediaPathPropertyId)
		|| changedPropertySet.hasPropertyName(FileVideoSourceDefinition::k_markerMediaPathPropertyId)
		|| changedPropertySet.hasPropertyName(FileVideoSourceDefinition::k_poseTrackPathPropertyId)
		|| changedPropertySet.hasPropertyName(FileVideoSourceDefinition::k_markerPoseTrackPathPropertyId))
	{
		reopenMedia();
	}
	else
	{
		VideoSourceComponent::onDefinitionMarkedDirty(configPtr, changedPropertySet);
	}
}

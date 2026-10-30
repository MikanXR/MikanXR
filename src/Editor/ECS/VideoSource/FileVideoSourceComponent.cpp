#include "FileVideoSourceComponent.h"
#include "AssetReference.h"
#include "AssetReferencePropertyMetaData.h"
#include "CameraMath.h"
#include "CaptureMarkerAlignment.h"
#include "EnumPropertyMetaData.h"
#include "Logger.h"
#include "MikanObject.h"
#include "MikanServer.h"
#include "MovieAssetReference.h"
#include "MovieDecoder.h"
#include "PathUtils.h"
#include "PlaybackTime.h"
#include "OcclusionMatteBuild/AppStage_OcclusionMatteBuild.h"
#include "OcclusionMeshBuild/AppStage_OcclusionMeshBuild.h"
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

const PoseTrack& emptyPoseTrack()
{
	static const PoseTrack s_emptyTrack;
	return s_emptyTrack;
}
} // namespace

// -- FileVideoSourceDefinition -----
const std::string FileVideoSourceDefinition::k_mediaPathPropertyId= "media_path";
const std::string FileVideoSourceDefinition::k_poseTrackPathPropertyId= "pose_track_path";
const std::string FileVideoSourceDefinition::k_loopPropertyId= "loop";
const std::string FileVideoSourceDefinition::k_poseOffsetPropertyId= "pose_offset";
const std::string FileVideoSourceDefinition::k_poseOffsetSessionIdPropertyId= "pose_offset_session_id";
const std::string FileVideoSourceDefinition::k_poseOffsetHeightSnappedPropertyId= "pose_offset_height_snapped";

FileVideoSourceDefinition::FileVideoSourceDefinition()
	: VideoSourceDefinition()
	, m_mediaAssetRefConfig(MediaAssetReferenceFactory().allocateAssetReferenceConfig())
	, m_poseTrackAssetRefConfig(PoseTrackAssetReferenceFactory().allocateAssetReferenceConfig())
	, m_poseOffset(1.f)
{
}

FileVideoSourceDefinition::FileVideoSourceDefinition(MikanVideoSourceID videoSourceId)
	: VideoSourceDefinition(videoSourceId)
	, m_mediaAssetRefConfig(MediaAssetReferenceFactory().allocateAssetReferenceConfig())
	, m_poseTrackAssetRefConfig(PoseTrackAssetReferenceFactory().allocateAssetReferenceConfig())
	, m_poseOffset(1.f)
{
}

configuru::Config FileVideoSourceDefinition::writeToJSON()
{
	configuru::Config pt= VideoSourceDefinition::writeToJSON();

	if (m_mediaAssetRefConfig->isValid())
		pt[k_mediaPathPropertyId]= m_mediaAssetRefConfig->writeToJSON();
	if (m_poseTrackAssetRefConfig->isValid())
		pt[k_poseTrackPathPropertyId]= m_poseTrackAssetRefConfig->writeToJSON();
	pt[k_loopPropertyId]= m_bLoop;

	// Written column-major, matching glm's own storage, and only when one has
	// been solved: an absent key reads back as never aligned
	if (m_bHasPoseOffset)
	{
		writePoseOffset(pt, m_poseOffset);
		if (!m_poseOffsetSessionId.empty())
			pt[k_poseOffsetSessionIdPropertyId]= m_poseOffsetSessionId;
		if (m_bPoseOffsetHeightSnapped)
			pt[k_poseOffsetHeightSnappedPropertyId]= true;
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

	m_poseTrackAssetRefConfig= PoseTrackAssetReferenceFactory().allocateAssetReferenceConfig();
	if (pt.has_key(k_poseTrackPathPropertyId))
		m_poseTrackAssetRefConfig->readFromJSON(pt[k_poseTrackPathPropertyId]);

	m_bLoop= pt.get_or<bool>(k_loopPropertyId, m_bLoop);

	m_bHasPoseOffset= false;
	m_poseOffset= glm::mat4(1.f);
	m_poseOffsetSessionId= pt.get_or<std::string>(k_poseOffsetSessionIdPropertyId, std::string());
	m_bPoseOffsetHeightSnapped= pt.get_or<bool>(k_poseOffsetHeightSnappedPropertyId, false);
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
		m_poseTrackAssetRefConfig->assetPath= PathUtils::utf8CStrToPathString(values->pose_track_path.getUtf8Value());
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

std::filesystem::path FileVideoSourceDefinition::getPoseTrackPath() const
{
	return m_poseTrackAssetRefConfig->assetPath;
}

void FileVideoSourceDefinition::setPoseTrackPath(const std::filesystem::path& trackPath)
{
	if (setAssetPath(m_poseTrackAssetRefConfig, trackPath))
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_poseTrackPathPropertyId));
}

void FileVideoSourceDefinition::setLoop(bool bLoop)
{
	if (bLoop != m_bLoop)
	{
		m_bLoop= bLoop;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_loopPropertyId));
	}
}

void FileVideoSourceDefinition::setPoseOffset(const glm::mat4& poseOffset, const std::string& sessionId,
											  bool bHeightSnapped)
{
	m_poseOffset= poseOffset;
	m_bHasPoseOffset= true;
	m_poseOffsetSessionId= sessionId;
	m_bPoseOffsetHeightSnapped= bHeightSnapped;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_poseOffsetPropertyId));
}

void FileVideoSourceDefinition::clearPoseOffset()
{
	m_poseOffset= glm::mat4(1.f);
	m_bHasPoseOffset= false;
	m_poseOffsetSessionId.clear();
	m_bPoseOffsetHeightSnapped= false;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_poseOffsetPropertyId));
}

// -- FileVideoSourceComponent -----
const std::string FileVideoSourceComponent::k_playbackStatePropertyId= "playback_state";
const std::string FileVideoSourceComponent::k_playbackTimePropertyId= "playback_time";
const std::string FileVideoSourceComponent::k_durationSecondsPropertyId= "duration_seconds";
const std::string FileVideoSourceComponent::k_playFunctionId= "play";
const std::string FileVideoSourceComponent::k_pauseFunctionId= "pause";
const std::string FileVideoSourceComponent::k_stopFunctionId= "stop";
const std::string FileVideoSourceComponent::k_stepBackFunctionId= "step_back";
const std::string FileVideoSourceComponent::k_stepForwardFunctionId= "step_forward";
const std::string FileVideoSourceComponent::k_buildOcclusionMeshFunctionId= "build_occlusion_mesh";
const std::string FileVideoSourceComponent::k_buildOcclusionMatteFunctionId= "build_occlusion_matte";

FileVideoSourceComponent::FileVideoSourceComponent(MikanObjectWeakPtr owner)
	: VideoSourceComponent(owner)
{
}

FileVideoSourceComponent::~FileVideoSourceComponent()
{
	cancelHeightSnap();
	stopWorker();
}

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
	VideoSourceComponent::setDefinition(definition);

	closeVideoSource();
}

// -- Video Source Interface ----
std::string FileVideoSourceComponent::getDevicePath() const { return resolveMediaPath().string(); }

std::string FileVideoSourceComponent::getDeviceAPI() const { return "FileVideoSource"; }

std::filesystem::path FileVideoSourceComponent::resolveMediaPath() const
{
	const std::filesystem::path storedPath= getFileVideoSourceDefinition()->getMediaPath();
	if (storedPath.empty())
		return std::filesystem::path();

	return PathUtils::resolveProjectResource(storedPath);
}

// A capture plays its members in place: a movie member through the subfile URL
// the worker hands to ffmpeg, an image member from its bytes. A raw file plays
// by path. The marker image is the reference media of every capture kind.
FileVideoSourceComponent::MediaSource FileVideoSourceComponent::resolveActiveMediaSource()
{
	MediaSource source;
	const std::filesystem::path mediaPath= resolveMediaPath();
	if (mediaPath.empty())
		return source;

	if (!m_capture.isOpen())
	{
		if (m_activeMedia == eActiveMedia::main)
			source.path= mediaPath;
		return source;
	}

	eCaptureMemberRole imageRole= eCaptureMemberRole::INVALID;
	if (m_activeMedia == eActiveMedia::markerReference)
	{
		imageRole= eCaptureMemberRole::marker_image;
	}
	else if (m_capture.getKind() == eCaptureKind::stillphoto)
	{
		imageRole= eCaptureMemberRole::plate_image;
	}
	else
	{
		source.url= m_capture.makeSubfileUrl(eCaptureMemberRole::movie);
		return source;
	}

	auto bytes= std::make_shared<std::vector<uint8_t>>();
	std::string error;
	if (!m_capture.readMemberBytes(imageRole, *bytes, error))
	{
		MIKAN_LOG_ERROR("FileVideoSourceComponent::resolveActiveMediaSource") << mediaPath << ": " << error;
		return source;
	}

	source.encodedStill= bytes;
	return source;
}

std::string FileVideoSourceComponent::describeActiveMedia() const
{
	std::string description= resolveMediaPath().filename().string();
	if (m_capture.isOpen())
	{
		eCaptureMemberRole role= eCaptureMemberRole::movie;
		if (m_activeMedia == eActiveMedia::markerReference)
			role= eCaptureMemberRole::marker_image;
		else if (m_capture.getKind() == eCaptureKind::stillphoto)
			role= eCaptureMemberRole::plate_image;
		description+= ":" + m_capture.getManifest().getMemberName(role);
	}
	return description;
}

bool FileVideoSourceComponent::openVideoSource()
{
	if (m_bIsOpen || m_bOpenPending)
		return true;

	if (resolveMediaPath().empty())
		return false;

	if (!loadMedia())
	{
		m_bOpenFailed= true;
		return false;
	}
	if (dropStaleOverride())
		return true;

	m_bAutoPlayOnOpen= true;
	startWorker();
	requestOpen(resolveActiveMediaSource());
	return true;
}

// Clears a pose track override loadMedia found to belong to another take.
// The path change reopens the media through onDefinitionMarkedDirty, which
// starts the worker on the capture's own track, so a caller that sees true
// has nothing left to start. False when there was nothing to drop.
bool FileVideoSourceComponent::dropStaleOverride()
{
	if (!m_bDropStaleOverride)
		return false;

	m_bDropStaleOverride= false;
	getFileVideoSourceDefinition()->setPoseTrackPath(std::filesystem::path());
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

	if (resolveMediaPath().empty() || !loadMedia())
	{
		stopWorker();
		m_bIsOpen= false;
		m_bOpenPending= false;
		m_bOpenFailed= !resolveMediaPath().empty();
		return;
	}
	if (dropStaleOverride())
		return;

	m_bAutoPlayOnOpen= true;
	startWorker();
	requestOpen(resolveActiveMediaSource());
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
	m_capture.close();
	m_overridePoseTrack.clear();
	m_bHasAlignmentReference= false;
	m_bMarkerShotAlignmentTried= false;
	cancelHeightSnap();
	m_bHeightSnapTried= false;

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

	outVideoModeName= describeActiveMedia() + " " + std::to_string(m_width) + "x" + std::to_string(m_height) + "@"
					  + std::to_string((int)std::lround(m_fps));
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
			const MediaSource& source= requests.openSource;
			bool bSucceeded= false;
			if (source.encodedStill)
				bSucceeded= decoder.openEncodedStill(*source.encodedStill);
			else if (!source.url.empty())
				bSucceeded= decoder.openMovieUrl(source.url);
			else if (!source.path.empty())
				bSucceeded= decoder.open(source.path);

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

void FileVideoSourceComponent::requestOpen(const MediaSource& source)
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
		m_requests.openSource= source;
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
	pollHeightSnap();

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
			<< "Failed to open media: " << describeActiveMedia();
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
// seeds them the moment it opens: from the movie's pose track when there is
// one, else from the marker track, whose pinhole values are the camera's even
// when the camera is treated as fixed, otherwise a default set from the frame
// size when nothing has ever been calibrated. A calibration the operator ran is
// never overwritten.
void FileVideoSourceComponent::seedIntrinsicsFromOpenMedia()
{
	const PoseTrack& track= getIntrinsicsPoseTrack();

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

void FileVideoSourceComponent::stepFrame(int frameDelta)
{
	// A step holds the frame it lands on, so stopped or playing it becomes paused
	m_playbackState= eFilePlaybackState::paused;
	if (!m_bIsOpen || m_bIsStill || !(m_fps > 0.0) || frameDelta == 0)
		return;

	// From the frame on screen rather than the clock, which runs ahead of it while
	// the next frame decodes. The seek rounds to the nearest frame, so a whole
	// frame period lands on the neighbouring frame, and the frame arrives with
	// its own pose.
	seekTo((double)m_currentPtsUs / 1000000.0 + (double)frameDelta / m_fps);
}

// -- Pose tracks ----
// A trackcamera publishes the poses of its movie track, or of the override, and
// the poses of its marker track while the reference is active. A still kind is
// a fixed camera and publishes nothing, so the align stages place the camera
// from the marker frames directly. A raw file publishes the override's poses
// when one is set.
const PoseTrack& FileVideoSourceComponent::getActivePoseTrack() const
{
	if (!m_capture.isOpen())
	{
		return m_activeMedia == eActiveMedia::main ? m_overridePoseTrack : emptyPoseTrack();
	}

	if (m_capture.getKind() != eCaptureKind::trackcamera)
		return emptyPoseTrack();

	if (m_activeMedia == eActiveMedia::markerReference)
		return m_capture.getPoseTrack(eCaptureMemberRole::marker_pose);

	return m_overridePoseTrack.isLoaded() ? m_overridePoseTrack
										  : m_capture.getPoseTrack(eCaptureMemberRole::movie_pose);
}

const PoseTrack& FileVideoSourceComponent::getIntrinsicsPoseTrack() const
{
	if (m_overridePoseTrack.isLoaded())
		return m_overridePoseTrack;

	if (!m_capture.isOpen())
		return emptyPoseTrack();

	const PoseTrack& movieTrack= m_capture.getPoseTrack(eCaptureMemberRole::movie_pose);
	if (movieTrack.isLoaded())
		return movieTrack;

	return m_capture.getPoseTrack(eCaptureMemberRole::marker_pose);
}

bool FileVideoSourceComponent::loadMedia()
{
	FileVideoSourceDefinitionPtr definition= getFileVideoSourceDefinition();

	m_capture.close();
	m_overridePoseTrack.clear();
	m_matteReader.close();
	m_bHasAlignmentReference= false;
	m_bMarkerShotAlignmentTried= false;
	cancelHeightSnap();
	m_bHeightSnapTried= false;

	const std::filesystem::path mediaPath= resolveMediaPath();
	if (CaptureKindUtils::isCaptureExtension(mediaPath))
	{
		std::string error;
		if (!m_capture.open(mediaPath, error))
		{
			MIKAN_LOG_ERROR("FileVideoSourceComponent::loadMedia") << "Failed to open capture " << mediaPath;
			return false;
		}

		openOcclusionMatte(mediaPath);
	}

	// An override belongs to the take it was refined from. One left behind when
	// the media moved to another take is dropped from the definition rather
	// than carried along, so nothing downstream (the occlusion build reads the
	// override too) can take it for this capture's track. The drop itself
	// happens once the load is through, since changing the path reopens the
	// media.
	const std::filesystem::path overridePath= definition->getPoseTrackPath();
	if (!overridePath.empty())
	{
		if (m_capture.isOpen() && m_capture.getKind() != eCaptureKind::trackcamera)
		{
			MIKAN_LOG_WARNING("FileVideoSourceComponent::loadMedia")
				<< "The pose track override " << overridePath << " is dropped: " << mediaPath
				<< " is not a trackcamera capture";
			m_bDropStaleOverride= true;
		}
		else if (m_overridePoseTrack.loadFromFile(overridePath) && m_capture.isOpen()
				 && m_overridePoseTrack.getSessionId() != m_capture.getManifest().sessionId)
		{
			MIKAN_LOG_WARNING("FileVideoSourceComponent::loadMedia")
				<< "The pose track override " << overridePath << " was recorded in session "
				<< m_overridePoseTrack.getSessionId() << ", not the capture's " << m_capture.getManifest().sessionId
				<< ", so it is dropped";
			m_overridePoseTrack.clear();
			m_bDropStaleOverride= true;
		}
	}

	// The marker image aligns an ARKit-space capture. A stage-space capture
	// needs no alignment, so its marker image, when it has one, is never offered.
	m_bHasAlignmentReference=
		m_capture.isOpen() && m_capture.hasMember(eCaptureMemberRole::marker_image) && !m_capture.isStageSpace();
	return true;
}

void FileVideoSourceComponent::openOcclusionMatte(const std::filesystem::path& mediaPath)
{
	m_matteReader.close();

	// A matte built for this take sits beside it. It keys records by the capture's
	// own track, so an override track of the same session indexes it as well.
	const std::filesystem::path mattePath= MatteTrack::sidecarPathForCapture(mediaPath);
	if (!m_capture.isOpen() || m_capture.getKind() != eCaptureKind::trackcamera || !std::filesystem::exists(mattePath))
		return;

	const PoseTrack& track= m_capture.getPoseTrack(eCaptureMemberRole::movie_pose);
	std::string matteError;
	if (m_matteReader.open(mattePath, track.getSessionId(), track.getFrameCount(), matteError))
	{
		MIKAN_LOG_INFO("FileVideoSourceComponent::openOcclusionMatte")
			<< "Occlusion matte " << mattePath << " (" << m_matteReader.getHeader().matteWidth << "x"
			<< m_matteReader.getHeader().matteHeight << ", " << m_matteReader.getHeader().model << ")";
	}
	else
	{
		MIKAN_LOG_WARNING("FileVideoSourceComponent::openOcclusionMatte")
			<< "Occlusion matte " << mattePath << " is not used: " << matteError;
	}
}

void FileVideoSourceComponent::reloadOcclusionMatte() { openOcclusionMatte(resolveMediaPath()); }

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

	MikanVideoSourceIntrinsics intrinsics;
	MikanMonoIntrinsics monoIntrinsics;
	createMonoIntrinsicsFromPinhole(track.getImageWidth(), track.getImageHeight(), poseFrame.fx, poseFrame.fy,
									poseFrame.cx, poseFrame.cy, monoIntrinsics);
	intrinsics.makeMonoIntrinsics()= monoIntrinsics;

	std::lock_guard<std::mutex> lock(m_latestPoseMutex);
	m_latestPose.sourceWorldTransform= poseFrame.cameraToWorld;
	m_latestPose.intrinsics= intrinsics;
	m_latestPose.frameSeq= m_frameSeq;
	m_latestPose.fileIndex= poseFrame.fileIndex;
	m_latestPose.bValid= true;
}

// -- IOcclusionMatteSource ----
bool FileVideoSourceComponent::tryGetLatestFrameMatte(const std::array<uint8_t, 3>& labelValues, cv::Mat& outLabels,
													  int64_t& outFrameKey) const
{
	if (!m_matteReader.isOpen())
		return false;

	size_t fileIndex= 0;
	{
		std::lock_guard<std::mutex> lock(m_latestPoseMutex);
		if (!m_latestPose.bValid)
			return false;
		fileIndex= m_latestPose.fileIndex;
		outFrameKey= (int64_t)m_latestPose.frameSeq;
	}

	return m_matteReader.readRecord(fileIndex, outLabels, labelValues);
}

// -- IFrameCoupledPoseProvider ----
// A stage-space capture was recorded in stage space, so it reports itself
// aligned with an identity offset and takes no other
void FileVideoSourceComponent::setPoseOffset(const glm::mat4& worldToStageXform)
{
	if (m_capture.isOpen() && m_capture.isStageSpace())
	{
		MIKAN_LOG_WARNING("FileVideoSourceComponent::setPoseOffset")
			<< "Ignoring a pose offset for the stage-space capture " << resolveMediaPath();
		return;
	}

	// A new offset starts unsnapped, and any snap of the previous one is moot
	cancelHeightSnap();
	m_bHeightSnapTried= false;
	getFileVideoSourceDefinition()->setPoseOffset(worldToStageXform, getPoseSessionId());
}

// An offset solved from another recording describes another session's world
// origin, so it reads as no alignment at all until this media is aligned
glm::mat4 FileVideoSourceComponent::getPoseOffset() const
{
	if (m_capture.isOpen() && m_capture.isStageSpace())
		return glm::mat4(1.f);

	return hasPoseOffset() ? getFileVideoSourceDefinition()->getPoseOffset() : glm::mat4(1.f);
}

bool FileVideoSourceComponent::hasPoseOffset() const
{
	if (m_capture.isOpen() && m_capture.isStageSpace())
		return true;

	FileVideoSourceDefinitionPtr definition= getFileVideoSourceDefinition();
	return definition->hasPoseOffset() && definition->getPoseOffsetSessionId() == getPoseSessionId();
}

bool FileVideoSourceComponent::wantsMarkerShotAlignment() const
{
	if (m_bMarkerShotAlignmentTried || !m_capture.isOpen() || m_capture.getKind() != eCaptureKind::trackcamera)
		return false;

	return m_bHasAlignmentReference && m_capture.getPoseTrack(eCaptureMemberRole::marker_pose).isLoaded()
		   && !hasPoseOffset();
}

bool FileVideoSourceComponent::alignFromMarkerShot(MarkerDefinitionConstPtr originMarker,
												   eCharucoDictionaryType dictionaryType)
{
	m_bMarkerShotAlignmentTried= true;

	glm::mat4 worldToStage;
	std::string error;
	if (!CaptureMarkerAlignment::solveWorldToStage(m_capture, originMarker, dictionaryType, worldToStage, error))
	{
		MIKAN_LOG_WARNING("FileVideoSourceComponent::alignFromMarkerShot")
			<< "Could not align " << resolveMediaPath() << " from its marker shot: " << error
			<< ". Align Camera on its camera aligns it by hand.";
		return false;
	}

	setPoseOffset(worldToStage);
	MIKAN_LOG_INFO("FileVideoSourceComponent::alignFromMarkerShot")
		<< "Aligned " << resolveMediaPath() << " from its marker shot (session " << getPoseSessionId() << ")";
	return true;
}

// -- Height snap ----
// Everything the job reads it owns: its own reader on the capture file, a copy
// of the movie track, and the offset it started from. The main thread only
// reads the result once bDone is set.
struct FileVideoSourceComponent::HeightSnapJob
{
	std::thread thread;
	std::atomic<bool> bCancel{false};
	std::atomic<bool> bDone{false};

	std::filesystem::path capturePath;
	CaptureArchiveMember depthMember;
	CaptureArchiveMember confidenceMember;
	PoseTrack track;
	glm::mat4 inputOffset= glm::mat4(1.f);
	std::string sessionId;
	float markerLengthM= 0.f;

	bool bSucceeded= false;
	CaptureMarkerAlignment::TableHeightResult result;
	std::string error;

	void run()
	{
		bSucceeded= CaptureMarkerAlignment::snapToTableHeight(capturePath, depthMember, confidenceMember, track,
															  inputOffset, markerLengthM, &bCancel, result, error);
		bDone= true;
	}
};

bool FileVideoSourceComponent::wantsHeightSnap() const
{
	if (m_heightSnapJob || m_bHeightSnapTried || !m_capture.isOpen() || m_capture.isStageSpace())
		return false;

	if (!hasPoseOffset() || getFileVideoSourceDefinition()->isPoseOffsetHeightSnapped())
		return false;

	return m_capture.getPoseTrack(eCaptureMemberRole::movie_pose).hasDepthTrack()
		   && m_capture.findMember(eCaptureMemberRole::depth) != nullptr
		   && m_capture.findMember(eCaptureMemberRole::confidence) != nullptr;
}

bool FileVideoSourceComponent::isAlignmentSettling() const { return m_heightSnapJob != nullptr || wantsHeightSnap(); }

void FileVideoSourceComponent::startHeightSnap(float markerLengthM)
{
	if (!wantsHeightSnap())
		return;

	auto job= std::make_unique<HeightSnapJob>();
	job->capturePath= m_capture.getPath();
	job->depthMember= *m_capture.findMember(eCaptureMemberRole::depth);
	job->confidenceMember= *m_capture.findMember(eCaptureMemberRole::confidence);
	job->track= m_capture.getPoseTrack(eCaptureMemberRole::movie_pose);
	job->inputOffset= getFileVideoSourceDefinition()->getPoseOffset();
	job->sessionId= getPoseSessionId();
	job->markerLengthM= markerLengthM;

	HeightSnapJob* jobPtr= job.get();
	job->thread= std::thread([jobPtr]() { jobPtr->run(); });
	m_heightSnapJob= std::move(job);
}

void FileVideoSourceComponent::pollHeightSnap()
{
	if (!m_heightSnapJob || !m_heightSnapJob->bDone)
		return;

	std::unique_ptr<HeightSnapJob> job= std::move(m_heightSnapJob);
	job->thread.join();
	m_bHeightSnapTried= true;

	// The offset the job started from must still be the one in effect
	FileVideoSourceDefinitionPtr definition= getFileVideoSourceDefinition();
	if (!hasPoseOffset() || definition->getPoseOffset() != job->inputOffset || getPoseSessionId() != job->sessionId)
		return;

	if (!job->bSucceeded)
	{
		MIKAN_LOG_WARNING("FileVideoSourceComponent::pollHeightSnap")
			<< "Kept the marker shot's height for " << resolveMediaPath() << ": " << job->error << " ("
			<< job->result.framesUsed << " frames)";
		return;
	}

	definition->setPoseOffset(job->result.snappedWorldToStage, job->sessionId, true);
	MIKAN_LOG_INFO("FileVideoSourceComponent::pollHeightSnap")
		<< "Snapped the alignment of " << resolveMediaPath()
		<< " onto the table's averaged height: " << job->result.heightM * 1000.f
		<< " mm along the marker normal, spread " << job->result.spreadM * 1000.f << " mm over "
		<< job->result.framesUsed << " frames";
}

void FileVideoSourceComponent::cancelHeightSnap()
{
	if (!m_heightSnapJob)
		return;

	m_heightSnapJob->bCancel= true;
	m_heightSnapJob->thread.join();
	m_heightSnapJob.reset();
}

std::string FileVideoSourceComponent::getPoseSessionId() const
{
	if (m_capture.isOpen())
		return m_capture.getManifest().sessionId;

	return m_overridePoseTrack.isLoaded() ? m_overridePoseTrack.getSessionId() : std::string();
}

bool FileVideoSourceComponent::getLatestFrameCoupledPose(glm::mat4& outTransform,
														 MikanVideoSourceIntrinsics& outIntrinsics,
														 uint32_t& outFrameSeq) const
{
	std::lock_guard<std::mutex> lock(m_latestPoseMutex);
	if (!m_latestPose.bValid)
		return false;

	// The single point where a recorded pose leaves its own world space, applied
	// as the pose is read so a new alignment takes effect without a new frame.
	// A stage-space capture's poses are already there.
	outTransform= getPoseOffset() * m_latestPose.sourceWorldTransform;
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
	requestOpen(resolveActiveMediaSource());
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
	requestOpen(resolveActiveMediaSource());
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
	else if (propertyName == FileVideoSourceDefinition::k_poseTrackPathPropertyId)
	{
		outValue= definition->getPoseTrackPath().string();
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
	else if (propertyName == FileVideoSourceDefinition::k_poseTrackPathPropertyId)
	{
		definition->setPoseTrackPath(PathUtils::utf8CStrToPathString(inValue.getUtf8Value()));
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
	outDescriptors.push_back(std::make_shared<FunctionDescriptor>(k_stepBackFunctionId, "Step Back"));
	outDescriptors.push_back(std::make_shared<FunctionDescriptor>(k_stepForwardFunctionId, "Step Forward"));
	outDescriptors.push_back(
		std::make_shared<FunctionDescriptor>(k_buildOcclusionMeshFunctionId, "Build Occlusion Mesh"));
	outDescriptors.push_back(
		std::make_shared<FunctionDescriptor>(k_buildOcclusionMatteFunctionId, "Build Occlusion Matte"));
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
	else if (functionName == k_stepBackFunctionId)
	{
		stepFrame(-1);
		return true;
	}
	else if (functionName == k_stepForwardFunctionId)
	{
		stepFrame(1);
		return true;
	}
	else if (functionName == k_buildOcclusionMeshFunctionId)
	{
		// The stage checks the take and explains a refusal itself
		AppStage_OcclusionMeshBuild::tryEnterBuild(getOwnerEditorWindow()->getCurrentAppStage(),
												   getSelfPtr<FileVideoSourceComponent>());
		return true;
	}
	else if (functionName == k_buildOcclusionMatteFunctionId)
	{
		AppStage_OcclusionMatteBuild::tryEnterBuild(getOwnerEditorWindow()->getCurrentAppStage(),
													getSelfPtr<FileVideoSourceComponent>());
		return true;
	}

	return VideoSourceComponent::invokeFunction(functionName);
}

void FileVideoSourceComponent::onDefinitionMarkedDirty(CommonConfigPtr configPtr,
													   const ConfigPropertyChangeSet& changedPropertySet)
{
	// Naming new media reloads with the override still set; loadMedia drops it
	// when the new capture is another take's
	if (changedPropertySet.hasPropertyName(FileVideoSourceDefinition::k_mediaPathPropertyId)
		|| changedPropertySet.hasPropertyName(FileVideoSourceDefinition::k_poseTrackPathPropertyId))
	{
		reopenMedia();
	}
	else
	{
		VideoSourceComponent::onDefinitionMarkedDirty(configPtr, changedPropertySet);
	}
}

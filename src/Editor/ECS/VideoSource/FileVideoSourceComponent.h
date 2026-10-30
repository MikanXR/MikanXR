#pragma once

#include "AssetFwd.h"
#include "Capture.h"
#include "ComponentFwd.h"
#include "IAlignmentReferenceSource.h"
#include "IFrameCoupledPoseProvider.h"
#include "IOcclusionMatteSource.h"
#include "MatteTrack.h"
#include "MikanVideoSourceTypes.h"
#include "PoseTrack.h"
#include "ProjectConfigConstants.h"
#include "VideoSourceComponent.h"

#include "opencv2/core.hpp"

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

enum class eFilePlaybackState : int
{
	stopped= 0,
	playing= 1,
	paused= 2,
};

// -- FileVideoSourceDefinition -----
// A recorded shot standing in for a live camera. The media is a capture
// archive, which carries the shot's movie or plate image, its marker reference,
// and its pose tracks, or a raw movie or image with none of those. The pose
// track path is an override for a trackcamera capture, the refined track the
// occlusion mesh build writes, and is empty otherwise.
class FileVideoSourceDefinition : public VideoSourceDefinition
{
public:
	FileVideoSourceDefinition();
	FileVideoSourceDefinition(MikanVideoSourceID videoSourceId);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	static const std::string k_mediaPathPropertyId;
	std::filesystem::path getMediaPath() const;
	void setMediaPath(const std::filesystem::path& mediaPath);

	static const std::string k_poseTrackPathPropertyId;
	std::filesystem::path getPoseTrackPath() const;
	void setPoseTrackPath(const std::filesystem::path& trackPath);

	static const std::string k_loopPropertyId;
	inline bool getLoop() const { return m_bLoop; }
	void setLoop(bool bLoop);

	// Source-world-to-stage offset solved by marker alignment, persisted so an
	// alignment survives closing the project. Not a reflected property, for the
	// same reason as ARKitVideoSourceDefinition: no client has a use for it and
	// a descriptor would be a wire change. The offset carries the session id of
	// the recording it was solved from, since each ARKit session has its own
	// world origin and an offset says nothing about any other session.
	// The height snap moves a solved offset onto the take's time-averaged table,
	// and is recorded so a project reopening on a snapped offset does not redo it.
	static const std::string k_poseOffsetPropertyId;
	static const std::string k_poseOffsetSessionIdPropertyId;
	static const std::string k_poseOffsetHeightSnappedPropertyId;
	inline const glm::mat4& getPoseOffset() const { return m_poseOffset; }
	inline bool hasPoseOffset() const { return m_bHasPoseOffset; }
	inline const std::string& getPoseOffsetSessionId() const { return m_poseOffsetSessionId; }
	inline bool isPoseOffsetHeightSnapped() const { return m_bPoseOffsetHeightSnapped; }
	void setPoseOffset(const glm::mat4& poseOffset, const std::string& sessionId, bool bHeightSnapped= false);
	void clearPoseOffset();

private:
	static void writePoseOffset(configuru::Config& pt, const glm::mat4& poseOffset);
	bool setAssetPath(AssetReferenceConfigPtr& assetRefConfig, const std::filesystem::path& path);

	AssetReferenceConfigPtr m_mediaAssetRefConfig;
	AssetReferenceConfigPtr m_poseTrackAssetRefConfig;
	bool m_bLoop= true;
	glm::mat4 m_poseOffset;
	bool m_bHasPoseOffset= false;
	std::string m_poseOffsetSessionId;
	bool m_bPoseOffsetHeightSnapped= false;
};

// -- FileVideoSourceComponent -----
// Decodes on a worker thread that owns the MovieDecoder outright; the main
// thread paces playback, delivers frames to the views, and publishes poses. The
// two only meet through a request mailbox and a single decoded-frame slot.
//
// A capture plays by its kind: a stillphoto shows its plate image, a
// stillcamera plays its movie with the camera fixed, and a trackcamera plays
// its movie with the per-frame poses of its track or of the override. The
// marker image is the alignment reference for all three.
class FileVideoSourceComponent : public VideoSourceComponent,
								 public IFrameCoupledPoseProvider,
								 public IAlignmentReferenceSource,
								 public IOcclusionMatteSource
{
public:
	FileVideoSourceComponent(MikanObjectWeakPtr owner);
	virtual ~FileVideoSourceComponent();

	virtual void init() override;
	virtual void dispose() override;

	inline static const std::string k_componentClassName= "FileVideoSourceComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	inline FileVideoSourceDefinitionPtr getFileVideoSourceDefinition() const
	{
		return std::static_pointer_cast<FileVideoSourceDefinition>(m_definition);
	}
	virtual void setDefinition(MikanComponentDefinitionPtr definition) override;

	// -- Video Source Interface ----
	virtual std::string getDevicePath() const override;
	virtual std::string getDeviceAPI() const override;
	virtual bool openVideoSource() override;
	virtual void closeVideoSource() override;
	virtual eVideoStreamingStatus getVideoStreamingStatus() const override;
	virtual bool getVideoPixelDimensions(int& outPixelWidth, int& outPixelHeight) const override;
	virtual bool getVideoModeName(std::string& outVideoModeName) const override;
	virtual bool getFrameRate(float& outFrameRate) const override;

	virtual void update(float deltaSeconds) override;

	// -- Playback ----
	inline eFilePlaybackState getPlaybackState() const { return m_playbackState; }
	inline float getPlaybackTime() const { return (float)m_playbackTime; }
	inline float getDurationSeconds() const { return (float)m_durationSeconds; }
	inline bool isStill() const { return m_bIsStill; }
	inline bool isMarkerReferenceActive() const { return m_bMarkerReferenceActive; }
	// The capture the media path names, not open when the media is a raw file
	// or nothing is loaded
	inline const Capture& getCapture() const { return m_capture; }
	// From stopped, start at zero; from paused, resume
	void play();
	void pause();
	// Return to the first frame and hold it
	void stop();
	// Clamp into the movie and show the frame at that time, in any state
	void seekTo(double timeSeconds);
	// Show the frame frameDelta frame periods after (or before, when negative) the
	// one on screen, and hold it there paused
	void stepFrame(int frameDelta);

	// -- IFrameCoupledPoseProvider ----
	virtual void setPoseOffset(const glm::mat4& worldToStageXform) override;
	virtual glm::mat4 getPoseOffset() const override;
	virtual bool hasPoseOffset() const override;
	virtual bool getLatestFrameCoupledPose(glm::mat4& outTransform, MikanVideoSourceIntrinsics& outIntrinsics,
										   uint32_t& outFrameSeq) const override;
	virtual bool getLatestSourceWorldPose(glm::mat4& outTransform, uint32_t& outFrameSeq) const override;
	// The ARKit session the media's poses were recorded in: the capture's, else
	// the override track's for a raw movie, empty when neither names one
	std::string getPoseSessionId() const;

	// -- Alignment from the marker shot ----
	// A trackcamera capture carries the image of the origin marker and the pose it
	// was taken from, which is everything an alignment needs. True once per media
	// open while the source has no alignment for its session.
	bool wantsMarkerShotAlignment() const;
	// Solves the offset from the marker shot and stores it, logging the outcome.
	// Tried once per media open, whatever the outcome, so a shot without a
	// readable marker is not decoded again every frame.
	bool alignFromMarkerShot(MarkerDefinitionConstPtr originMarker, eCharucoDictionaryType dictionaryType);

	// -- Height snap ----
	// The marker shot catches ARKit's world at one instant, a few millimetres off
	// the world the take averages to. Once an offset is set, a job reads the
	// take's depth on its own thread and moves the offset's origin onto the
	// table's time-averaged height under the marker. The unsnapped offset stays
	// in effect until the job lands. True once per media open and offset while
	// the offset is unsnapped and the capture has depth.
	bool wantsHeightSnap() const;
	void startHeightSnap(float markerLengthM);
	// The offset may still move by a few millimetres: a snap is wanted or running
	bool isAlignmentSettling() const;

	// -- Occlusion matte ----
	// Reopens the matte sidecar beside the capture, so one written while the
	// media is open shows up without reopening the media
	void reloadOcclusionMatte();
	// Lets go of the sidecar so a build can replace it: an open reader holds
	// the file, and the writer's move over it fails while it does
	void closeOcclusionMatte();

	// -- IAlignmentReferenceSource ----
	virtual bool hasAlignmentReference() const override;
	virtual void beginAlignmentReference() override;
	virtual void endAlignmentReference() override;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static const std::string k_playbackStatePropertyId;
	static const std::string k_playbackTimePropertyId;
	static const std::string k_durationSecondsPropertyId;
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static const std::string k_playFunctionId;
	static const std::string k_pauseFunctionId;
	static const std::string k_stopFunctionId;
	static const std::string k_stepBackFunctionId;
	static const std::string k_stepForwardFunctionId;
	static const std::string k_buildOcclusionMeshFunctionId;
	static const std::string k_buildOcclusionMatteFunctionId;
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors);
	virtual bool invokeFunction(const std::string& functionName) override;

protected:
	virtual eVideoStreamingStatus startVideoStreamInternal() override;
	virtual void stopVideoStreamInternal() override;
	virtual void onDefinitionMarkedDirty(CommonConfigPtr configPtr,
										 const ConfigPropertyChangeSet& changedPropertySet) override;

private:
	// The media currently feeding the views
	enum class eActiveMedia
	{
		main,
		markerReference,
	};

	// What the worker opens: a raw file by path, a capture's movie member by
	// its subfile URL, or a capture's image member by its encoded bytes
	struct MediaSource
	{
		std::filesystem::path path;
		std::string url;
		std::shared_ptr<const std::vector<uint8_t>> encodedStill;

		bool isEmpty() const { return path.empty() && url.empty() && !encodedStill; }
	};

	// What the worker decoded, handed over one frame at a time
	struct DecodedFrame
	{
		cv::Mat frame;
		int64_t ptsUs= 0;
		uint32_t seekGeneration= 0;
		bool bEndOfStream= false;
		bool bReady= false;
	};

	// What the worker learned from opening a file
	struct OpenResult
	{
		bool bPending= false;
		bool bSucceeded= false;
		bool bIsStill= false;
		int width= 0;
		int height= 0;
		double fps= 0.0;
		double durationSeconds= 0.0;
	};

	// Everything the main thread asks of the worker. A request stays set until
	// the worker takes it, so the newest of each kind wins.
	struct DecodeRequests
	{
		bool bOpen= false;
		MediaSource openSource;
		bool bSeek= false;
		double seekSeconds= 0.0;
		uint32_t seekGeneration= 0;
		bool bDecodeNext= false;
		bool bQuit= false;
	};

	// -- IOcclusionMatteSource ----
	virtual bool tryGetLatestFrameMatte(cv::Mat& outLabels, int64_t& outFrameKey) const override;

	void reopenMedia();
	void startWorker();
	void stopWorker();
	void workerMain();
	void requestOpen(const MediaSource& source);
	void requestSeek(double timeSeconds);
	void requestDecodeNext();

	void applyOpenResult();
	void seedIntrinsicsFromOpenMedia();
	void tryDeliverDecodedFrame();
	void deliverFrame(cv::Mat&& frame, int64_t ptsUs);
	void reemitCurrentFrame(float deltaSeconds);
	void publishPoseForFrame(int64_t ptsUs);
	void finishPlayback();

	// The media path resolved to a file, empty when none is set or found
	std::filesystem::path resolveMediaPath() const;
	// What the worker should open for the active media
	MediaSource resolveActiveMediaSource();
	// The active media for log lines and the mode name
	std::string describeActiveMedia() const;
	// The track whose poses the active media publishes, unloaded for a fixed camera
	const PoseTrack& getActivePoseTrack() const;
	// The track whose first record seeds the intrinsics, unloaded when none applies
	const PoseTrack& getIntrinsicsPoseTrack() const;
	// Opens the capture when the media names one and loads the override track.
	// Returns false when the capture cannot be opened.
	bool loadMedia();
	// Opens the matte sidecar beside a trackcamera capture when one is there
	// and matches the capture's own track; the reader stays closed otherwise
	void openOcclusionMatte(const std::filesystem::path& mediaPath);
	bool dropStaleOverride();
	void clearRuntimeState();

	// -- main-thread playback state --
	eActiveMedia m_activeMedia= eActiveMedia::main;
	eFilePlaybackState m_playbackState= eFilePlaybackState::stopped;
	double m_playbackTime= 0.0;
	double m_durationSeconds= 0.0;
	double m_fps= 0.0;
	int m_width= 0;
	int m_height= 0;
	bool m_bIsStill= false;
	bool m_bIsOpen= false;
	bool m_bOpenPending= false;
	bool m_bOpenFailed= false;
	bool m_bEverOpened= false;
	bool m_bAutoPlayOnOpen= false;
	bool m_bStreamRequested= false;
	// A seek or a fresh open shows its frame regardless of the clock
	bool m_bDeliverNextFrame= false;
	uint32_t m_seekGeneration= 0;
	cv::Mat m_currentFrame;
	int64_t m_currentPtsUs= 0;
	uint32_t m_frameSeq= 0;
	double m_secondsSinceEmit= 0.0;

	// -- marker reference switch --
	bool m_bMarkerReferenceActive= false;
	eFilePlaybackState m_savedPlaybackState= eFilePlaybackState::stopped;
	double m_savedPlaybackTime= 0.0;

	// -- the capture and the tracks, loaded on the main thread at open --
	Capture m_capture;
	PoseTrack m_overridePoseTrack;
	bool m_bHasAlignmentReference= false;
	bool m_bDropStaleOverride= false; // loadMedia found the override belongs to another take
	bool m_bMarkerShotAlignmentTried= false;

	// -- height snap job, polled from update --
	struct HeightSnapJob;
	void pollHeightSnap();
	void cancelHeightSnap();
	std::unique_ptr<HeightSnapJob> m_heightSnapJob;
	bool m_bHeightSnapTried= false;

	mutable std::mutex m_latestPoseMutex;
	struct LatestPose
	{
		glm::mat4 sourceWorldTransform= glm::mat4(1.f);
		MikanVideoSourceIntrinsics intrinsics;
		uint32_t frameSeq= 0;
		size_t fileIndex= 0; // the track frame's record index, which keys the matte track
		bool bValid= false;
	};
	LatestPose m_latestPose;

	// The take's occlusion matte, found beside the capture by its stem and open
	// while its session and record count match the track's
	MatteTrackReader m_matteReader;

	// -- worker thread --
	std::thread m_workerThread;
	std::mutex m_requestMutex;
	std::condition_variable m_requestCondVar;
	DecodeRequests m_requests;
	std::mutex m_resultMutex;
	OpenResult m_openResult;
	DecodedFrame m_decodedFrame;
};

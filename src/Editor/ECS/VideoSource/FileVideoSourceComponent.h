#pragma once

#include "AssetFwd.h"
#include "IAlignmentReferenceSource.h"
#include "IFrameCoupledPoseProvider.h"
#include "MikanVideoSourceTypes.h"
#include "PoseTrack.h"
#include "VideoSourceComponent.h"

#include "opencv2/core.hpp"

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>

enum class eFilePlaybackState : int
{
	stopped= 0,
	playing= 1,
	paused= 2,
};

// -- FileVideoSourceDefinition -----
// A recorded movie or a still image standing in for a live camera. The main
// media is what the source plays; the marker media is the same shot with the
// origin marker in frame, played only while a marker alignment stage runs. Each
// may carry a pose track, the sidecar a phone writes with the camera pose per
// frame, which makes the source frame-coupled like a live ARKit stream.
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

	static const std::string k_markerMediaPathPropertyId;
	std::filesystem::path getMarkerMediaPath() const;
	void setMarkerMediaPath(const std::filesystem::path& mediaPath);

	static const std::string k_poseTrackPathPropertyId;
	std::filesystem::path getPoseTrackPath() const;
	void setPoseTrackPath(const std::filesystem::path& trackPath);

	static const std::string k_markerPoseTrackPathPropertyId;
	std::filesystem::path getMarkerPoseTrackPath() const;
	void setMarkerPoseTrackPath(const std::filesystem::path& trackPath);

	static const std::string k_loopPropertyId;
	inline bool getLoop() const { return m_bLoop; }
	void setLoop(bool bLoop);

	// Source-world-to-stage offset solved by marker alignment, persisted so an
	// alignment survives closing the project. Not a reflected property, for the
	// same reason as ARKitVideoSourceDefinition: no client has a use for it and
	// a descriptor would be a wire change.
	static const std::string k_poseOffsetPropertyId;
	inline const glm::mat4& getPoseOffset() const { return m_poseOffset; }
	inline bool hasPoseOffset() const { return m_bHasPoseOffset; }
	void setPoseOffset(const glm::mat4& poseOffset);
	void clearPoseOffset();

private:
	static void writePoseOffset(configuru::Config& pt, const glm::mat4& poseOffset);
	bool setAssetPath(AssetReferenceConfigPtr& assetRefConfig, const std::filesystem::path& path);

	AssetReferenceConfigPtr m_mediaAssetRefConfig;
	AssetReferenceConfigPtr m_markerMediaAssetRefConfig;
	AssetReferenceConfigPtr m_poseTrackAssetRefConfig;
	AssetReferenceConfigPtr m_markerPoseTrackAssetRefConfig;
	bool m_bLoop= true;
	glm::mat4 m_poseOffset;
	bool m_bHasPoseOffset= false;
};

// -- FileVideoSourceComponent -----
// Decodes on a worker thread that owns the MovieDecoder outright; the main
// thread paces playback, delivers frames to the views, and publishes poses. The
// two only meet through a request mailbox and a single decoded-frame slot.
class FileVideoSourceComponent : public VideoSourceComponent,
								 public IFrameCoupledPoseProvider,
								 public IAlignmentReferenceSource
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
	// From stopped, start at zero; from paused, resume
	void play();
	void pause();
	// Return to the first frame and hold it
	void stop();
	// Clamp into the movie and show the frame at that time, in any state
	void seekTo(double timeSeconds);

	// -- IFrameCoupledPoseProvider ----
	virtual void setPoseOffset(const glm::mat4& worldToStageXform) override;
	virtual glm::mat4 getPoseOffset() const override;
	virtual bool hasPoseOffset() const override;
	virtual bool getLatestFrameCoupledPose(glm::mat4& outTransform, MikanVideoSourceIntrinsics& outIntrinsics,
										   uint32_t& outFrameSeq) const override;
	virtual bool getLatestSourceWorldPose(glm::mat4& outTransform, uint32_t& outFrameSeq) const override;

	// -- IAlignmentReferenceSource ----
	virtual bool hasAlignmentReference() const override;
	virtual void beginAlignmentReference() override;
	virtual void endAlignmentReference() override;

	// -- IEntityAccessor ----
	virtual rfk::Struct const* getClientAPIValuesStructType() const override;

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
		std::filesystem::path openPath;
		bool bSeek= false;
		double seekSeconds= 0.0;
		uint32_t seekGeneration= 0;
		bool bDecodeNext= false;
		bool bQuit= false;
	};

	void reopenMedia();
	void startWorker();
	void stopWorker();
	void workerMain();
	void requestOpen(const std::filesystem::path& absolutePath);
	void requestSeek(double timeSeconds);
	void requestDecodeNext();

	void applyOpenResult();
	void seedIntrinsicsFromOpenMedia();
	void tryDeliverDecodedFrame();
	void deliverFrame(cv::Mat&& frame, int64_t ptsUs);
	void reemitCurrentFrame(float deltaSeconds);
	void publishPoseForFrame(int64_t ptsUs);
	void finishPlayback();

	std::filesystem::path resolveActiveMediaPath() const;
	const PoseTrack& getActivePoseTrack() const;
	void loadPoseTracks();
	static bool loadPoseTrack(const std::filesystem::path& storedPath, PoseTrack& outTrack);
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

	// -- pose tracks, loaded on the main thread at open --
	PoseTrack m_mainPoseTrack;
	PoseTrack m_markerPoseTrack;
	bool m_bHasAlignmentReference= false;

	mutable std::mutex m_latestPoseMutex;
	struct LatestPose
	{
		glm::mat4 transform= glm::mat4(1.f);
		glm::mat4 sourceWorldTransform= glm::mat4(1.f);
		MikanVideoSourceIntrinsics intrinsics;
		uint32_t frameSeq= 0;
		bool bValid= false;
	};
	LatestPose m_latestPose;

	// -- worker thread --
	std::thread m_workerThread;
	std::mutex m_requestMutex;
	std::condition_variable m_requestCondVar;
	DecodeRequests m_requests;
	std::mutex m_resultMutex;
	OpenResult m_openResult;
	DecodedFrame m_decodedFrame;
};

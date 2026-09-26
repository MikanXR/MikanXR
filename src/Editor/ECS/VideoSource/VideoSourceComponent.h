#pragma once

#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "IVideoDevice.h"
#include "MikanComponent.h"
#include "MikanTypeFwd.h"
#include "MkRendererFwd.h"
#include "MikanVideoSourceTypes.h"
#include "OpenCVFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectFwd.h"
#include "ProjectConfigConstants.h"
#include "VideoDisplayConstants.h"

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>

class VideoSourceDefinition : public MikanComponentDefinition
{
public:
	VideoSourceDefinition();
	VideoSourceDefinition(MikanVideoSourceID videoSourceId);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	inline MikanVideoSourceID getVideoSourceId() const { return getComponentId(); }

	static const std::string k_isFrameMirroredPropertyId;
	inline bool getIsFrameMirrored() const { return m_bIsFrameMirrored; }
	void setIsFrameMirrored(bool isFrameMirrored);

	static const std::string k_isBufferMirroredPropertyId;
	inline bool getIsBufferMirrored() const { return m_bIsBufferMirrored; }
	void setIsBufferMirrored(bool isBufferMirrored);

	static const std::string k_videoFrameQueueSizePropertyId;
	inline int getVideoFrameQueueSize() const { return m_videoFrameQueueSize; }
	void setVideoFrameQueueSize(int videoFrameQueueSize);

	static const std::string k_videoSourceIntrinsicsPropertyId;
	inline const MikanVideoSourceIntrinsics& getCameraIntrinsics() const { return m_intrinsics; }
	void setCameraIntrinsics(const MikanVideoSourceIntrinsics& cameraIntrinsics);

	static const std::string k_intrinsicsTypePropertyId;
	inline MikanIntrinsicsType getCameraIntrinsicsType() const { return m_intrinsics.intrinsics_type; }

private:
	bool m_bIsFrameMirrored= false;
	bool m_bIsBufferMirrored= false;
	int m_videoFrameQueueSize= DEFAULT_VIDEO_FRAME_QUEUE_SIZE;
	MikanVideoSourceIntrinsics m_intrinsics;
};

// Whether the source's stored intrinsics describe the video mode it is running.
// Nothing rescales intrinsics, so a mode at another resolution silently breaks
// the projection rather than failing.
enum class eVideoSourceIntrinsicsStatus
{
	ok,
	uncalibrated,
	resolutionMismatch
};

class VideoSourceComponent : public MikanComponent
{
public:
	static constexpr int k_maxActiveViews= 32;

	VideoSourceComponent(MikanObjectWeakPtr owner);

	inline VideoSourceDefinitionPtr getVideoSourceDefinition() const
	{
		return std::static_pointer_cast<VideoSourceDefinition>(getDefinition());
	}
	virtual void setDefinition(MikanComponentDefinitionPtr definition) override;

	inline static const std::string k_componentClassName= "VideoSourceComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	MikanVideoSourceID getVideoSourceId() const;

	// Video Source Interface
	virtual std::string getDevicePath() const= 0;
	virtual std::string getDeviceAPI() const= 0;
	virtual bool openVideoSource()= 0;
	virtual void closeVideoSource()= 0;
	// Close and reopen the device without dropping stream subscribers, so a stage
	// or compositor watching the source keeps its view across a device swap.
	// update() starts the stream again once the new device is open.
	void reopenVideoSource();
	void startVideoStream(class VideoFrameDistortionView* view);
	void stopVideoStream(class VideoFrameDistortionView* view);
	void forceStopVideoStream();
	virtual eVideoStreamingStatus getVideoStreamingStatus() const= 0;

	virtual void update(float deltaSeconds) override;

	virtual bool getVideoPixelDimensions(int& outPixelWidth, int& outPixelHeight) const;

	// Zero-copy GPU texture access (ticket E3) - only overridden by video sources
	// whose decoded frames live in GPU memory the whole way through (currently
	// just ARKit's CUDA-GL interop pipeline; see ARKitVideoSourceComponent). The
	// default null return means every other video source type (USB/Network) is
	// unaffected - VideoFrameDistortionView's CPU-buffer pipeline (writeVideoFrame
	// et al.) remains the only path for them.
	virtual IMkTexturePtr getDirectColorTexture() const { return IMkTexturePtr(); }

	// Runs any per-tick GPU processing a GPU-direct source needs before its
	// getDirectColorTexture() is safe to read for compositing this tick (e.g.
	// ARKit's NV12->RGBA shader conversion pass - see ARKitVideoSourceComponent::
	// processDirectVideoFrame). Called once per tick by VideoFrameDistortionView::
	// readAndProcessVideoFrame(), before any caller reads getDirectColorTexture()
	// this same tick. Default no-op - every other video source type is unaffected.
	virtual void processDirectVideoFrame() {}

	// Frame index for GPU-direct sources (ticket E4), used by VideoFrameDistortionView
	// as the "has a new frame arrived" change-detection key in place of the
	// CPU-buffer m_lastVideoFrameWriteIndex (which GPU-direct sources never
	// advance, since they never call writeVideoFrame()). Default -1 means "not
	// applicable" - only overridden by GPU-direct sources (currently just ARKit).
	virtual int64_t getDirectFrameIndex() const { return -1; }

	virtual bool getVideoModeName(std::string& outVideoModeName) const;
	virtual bool getFrameRate(float& outFrameRate) const;
	virtual bool getVideoColorimetry(VideoColorimetry& outColorimetry) const;
	virtual bool areCameraIntrinsicsValid() const;
	eVideoSourceIntrinsicsStatus getCameraIntrinsicsStatus() const;
	virtual bool getCameraIntrinsics(MikanVideoSourceIntrinsics& out_camera_intrinsics) const;
	virtual bool setCameraIntrinsics(const MikanVideoSourceIntrinsics& camera_intrinsics);
	virtual glm::mat4 getProjectionMatrix() const;

	virtual bool isVideoSettingSupported(const eVideoSettingType property_type) const;
	virtual bool setVideoSetting(const eVideoSettingType property_type, float desiredFraction);
	virtual bool getVideoSetting(const eVideoSettingType property_type, float& outFractionValue) const;

	// Video Source Events
	MulticastDelegate<void(VideoSourceComponentPtr videoSource)> OnOpened;
	MulticastDelegate<void(VideoSourceComponentPtr videoSource)> OnClosed;
	MulticastDelegate<void(VideoSourceComponentPtr videoSource)> OnStarted;
	MulticastDelegate<void(VideoSourceComponentPtr videoSource)> OnStopped;
	MulticastDelegate<void(VideoSourceComponentPtr videoSource)> OnFrameSizeChanged;
	MulticastDelegate<void(VideoSourceComponentPtr videoSource)> OnIntrinsicsChanged;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static const std::string k_showVideoSourceSettingsFunctionId;
	static const std::string k_calibrateIntrinsicsFunctionId;
	static const std::string k_testIntrinsicsFunctionId;
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outPropertyNames);
	virtual bool invokeFunction(const std::string& functionName) override;

	void showVideoSourceSettings();
	void calibrateIntrinsics();
	void testIntrinsics();

protected:
	virtual eVideoStreamingStatus startVideoStreamInternal()= 0;
	virtual void stopVideoStreamInternal()= 0;

	void recomputeCameraProjectionMatrix();

	// Called by derived classes when raw video source frame is received in video receive thread.
	// The active view set is locked for the whole write, so a view that unsubscribes
	// on the main thread is never written to after stopVideoStream returns.
	void writeVideoFrame(const unsigned char* videoBuffer, const cv::Size& bufferDimensions, const bool bIsFlipped);
	void writeStereoVideoFrameSection(const unsigned char* videoBuffer, const cv::Size& bufferDimensions,
									  const bool bIsFlipped, const VideoFrameSection section,
									  const cv::Rect& bufferBounds);

protected:
	std::mutex m_activeViewMutex;
	std::set<class VideoFrameDistortionView*> m_activeViews;
	std::atomic_bool m_bHasAnyActiveViews{false};

	glm::mat4 m_projectionMatrix;
};
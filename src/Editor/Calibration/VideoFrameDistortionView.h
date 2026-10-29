#pragma once

#include "IEditorWindow.h"
#include "VideoDisplayConstants.h"
#include "OpenCVFwd.h"
#include "MikanRendererFwd.h"

#include <chrono>
#include <memory>
#include <mutex>

class VideoSourceComponent;
typedef std::shared_ptr<VideoSourceComponent> VideoSourceComponentPtr;

class CVVideoFrameProcessor;
class GLVideoFrameProcessor;

class VideoFrameDistortionView
{
public:
	VideoFrameDistortionView(VideoSourceComponentPtr view, eVideoFrameProcessorMode processorMode,
							 unsigned int frameQueueSize= 1,
							 VideoFrameSection videoFramesection= VideoFrameSection::Primary);
	virtual ~VideoFrameDistortionView();

	inline VideoSourceComponentPtr getVideoSourceComponent() const { return m_videoSourceComponent; }
	IMkGraphicsContext* getGraphicsContext() const;

	inline VideoFrameSection getVideoFrameSection() const { return m_videoFrameSection; }
	inline int getFrameWidth() const { return m_frameWidth; }
	inline int getFrameHeight() const { return m_frameHeight; }
	// GPU-direct sources (ticket E3) never advance m_lastVideoFrameWriteIndex - it's
	// only bumped by writeVideoFrame(), which such sources never call - so this also
	// checks for a live direct-color texture, matching hasNewVideoFrame()'s bypass.
	bool isReceivingFrames() const;
	inline float getFPS() const { return m_fps; }

	/// Fold one new-frame arrival into the rolling frame rate estimate.
	void updateFrameRateStatistic();

	inline eVideoDisplayMode getVideoDisplayMode() const { return m_videoDisplayMode; }
	inline void setVideoDisplayMode(eVideoDisplayMode newMode) { m_videoDisplayMode= newMode; }

	bool isColorUndistortDisabled() const;
	void setColorUndistortDisabled(bool bDisabled);

	bool isGrayscaleUndistortDisabled() const;
	void setGrayscaleUndistortDisabled(bool bDisabled);

	inline unsigned int getMaxFrameQueueSize() const { return m_videoFrameQueueSize; }
	inline int64_t getLastVideoFrameWriteIndex() const { return m_lastVideoFrameReadIndex; }
	cv::Mat* getGrayscaleSourceBuffer() const;
	cv::Mat* getGrayscaleUndistortBuffer() const;
	cv::Mat* getBGRUndistortBuffer() const;
	cv::Mat* getBGRGsDisplayBuffer() const;
	inline IMkTexturePtr getDistortionTexture() const { return m_distortionTextureMap; }

	void writeVideoFrame(const unsigned char* videoBuffer, const cv::Size& bufferDimensions, bool bIsFlipped);
	void writeStereoVideoFrameSection(const unsigned char* videoBuffer, const cv::Size& bufferDimensions,
									  const bool bIsFlipped, const cv::Rect& bufferBounds);

	bool hasNewVideoFrame() const;
	int64_t readAndProcessVideoFrame();
	IMkTexturePtr getVideoTexture(int64_t desiredFrameIndex= -1) const;
	void applyMonoCameraIntrinsics(const struct MikanMonoIntrinsics* instrinsics);

	void renderSelectedVideoBuffers();

protected:
	int64_t readNextVideoFrameIndex();
	void processVideoFrame(int64_t newFrameIndex);
	// Claim the next frame queue texture for a frame and record its index
	IMkTexturePtr acquireQueueWriteTexture(int64_t newFrameIndex);
	// Compositor path for a GPU-direct source: undistort its converted frame into the queue
	void undistortDirectVideoFrame(IMkTexturePtr directTexture, int64_t newFrameIndex);
	// Pull a GPU-direct source's converted frame back to the CPU and push it
	// through writeVideoFrame, so calibration has pixels to work on. Returns
	// false when there is no direct texture, no new frame, or the read failed.
	bool readbackDirectColorTexture();
	void ensureFrameBufferSize(int width, int height);
	void rebuildDistortionMap();

	static void copyOpenCVMatIntoGLTexture(const cv::Mat& mat, IMkTexturePtr texture);

protected:
	VideoFrameSection m_videoFrameSection;
	eVideoDisplayMode m_videoDisplayMode;
	VideoSourceComponentPtr m_videoSourceComponent;
	bool m_bVideoIsStreaming;
	eVideoFrameProcessorMode m_processorMode;
	int m_frameWidth;
	int m_frameHeight;
	float m_fps;
	// Smoothed inter-frame interval backing m_fps (see updateFrameRateStatistic)
	float m_frameIntervalSeconds= 0.f;

	// BGR source buffer
	std::mutex m_bgrSourceBufferMutex;
	cv::Mat* m_bgrSourceBuffer;
	int m_bgrSourceBufferWidth;
	int m_bgrSourceBufferHeight;
	std::atomic_int64_t m_lastVideoFrameWriteIndex;
	int64_t m_lastVideoFrameReadIndex;

	// Staging for readbackDirectColorTexture: RGBA pixels straight off the GPU,
	// and the frame index they came from so an unchanged frame is not re-read.
	std::vector<uint8_t> m_directReadbackBuffer;
	int64_t m_lastReadbackFrameIndex= -1;
	bool m_bReadbackFailureLogged= false;
	std::chrono::steady_clock::time_point m_lastFrameTimestamp;

	// Camera Intrinsics / Distortion parameters
	struct OpenCVMonoCameraIntrinsics* m_intrinsics;

	// Distortion maps (shared by both CV and GL paths)
	cv::Mat* m_distortionMapX;
	cv::Mat* m_distortionMapY;
	IMkTexturePtr m_distortionTextureMap;

	// Circular RGB video frame queue
	struct VideoFrameQueueEntry
	{
		IMkTexturePtr videoTexture;
		int64_t frameIndex;
	};
	VideoFrameQueueEntry* m_videoFrameQueue= nullptr;
	unsigned int m_videoFrameQueueSize= 0;
	int m_videoFrameQueueLastWriteIndex= -1;
	int m_videoFrameQueuePendingWriteIndex= 0;

	// FrameBuffer used for fullscreen video rendering
	IMkTriangulatedMeshPtr m_fullscreenRGBVideoQuad;
	MkMaterialConstPtr m_noVideoMaterial;
	MkMaterialInstancePtr m_noVideoMaterialInstance;

	// Processor objects (at most one of each is active, depending on m_bufferBitmask)
	std::unique_ptr<CVVideoFrameProcessor> m_cvProcessor; // null if no CV flags
	std::unique_ptr<GLVideoFrameProcessor> m_glProcessor; // null if no GL undistort flag
};

#include "App.h"
#include "CVVideoFrameProcessor.h"
#include "GLVideoFrameProcessor.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "IMkGraphicsContext.h"
#include "IMkFrameBuffer.h"
#include "IMkTexture.h"
#include "IMkTriangulatedMesh.h"
#include "MikanShaderCache.h"
#include "Logger.h"
#include "MathTypeConversion.h"
#include "MkScopedObjectBinding.h"
#include "MkStateStack.h"
#include "OpenCVManager.h"
#include "VideoFrameDistortionView.h"
#include "VideoSourceComponent.h"

#include "opencv2/opencv.hpp"
#include "opencv2/calib3d/calib3d.hpp"

#include "assert.h"

#include <easy/profiler.h>

struct OpenCVMonoCameraIntrinsics
{
	cv::Matx33d distorted_intrinsic_matrix;
	cv::Matx81d distortion_coeffs;
	cv::Matx33d undistorted_intrinsic_matrix;

	void init(int pixelWidth, int pixelHeight, double focalLength)
	{
		// Camera intrinsic matrix with principal point at image center
		double cx= pixelWidth * 0.5;
		double cy= pixelHeight * 0.5;

		distorted_intrinsic_matrix= cv::Matx33d(focalLength, 0.0, cx, 0.0, focalLength, cy, 0.0, 0.0, 1.0);

		// No distortion coefficients
		distortion_coeffs= cv::Matx81d(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

		// Undistorted intrinsic matrix same as distorted when no distortion
		undistorted_intrinsic_matrix= distorted_intrinsic_matrix;
	}

	void init(int pixelWidth, int pixelHeight)
	{
		// Estimate focal length assuming typical webcam horizontal FOV of 70 degrees
		// focalLength = (pixelWidth / 2) / tan(hfov / 2)
		// For 70 degrees: focalLength ≈ pixelWidth * 1.1917
		double focalLength= pixelWidth * 1.2;

		init(pixelWidth, pixelHeight, focalLength);
	}

	void init(const MikanMonoIntrinsics& monoIntrinsics)
	{
		distorted_intrinsic_matrix= MikanMatrix3d_to_cv_mat33d(monoIntrinsics.distorted_camera_matrix);
		distortion_coeffs= Mikan_distortion_to_cv_vec8(monoIntrinsics.distortion_coefficients);
		undistorted_intrinsic_matrix= MikanMatrix3d_to_cv_mat33d(monoIntrinsics.undistorted_camera_matrix);
	}
};

VideoFrameDistortionView::VideoFrameDistortionView(VideoSourceComponentPtr videoSourceComponent,
												   eVideoFrameProcessorMode processorMode, unsigned int frameQueueSize,
												   VideoFrameSection videoFramesection)
	: m_videoFrameSection(videoFramesection)
	, m_videoDisplayMode(eVideoDisplayMode::mode_bgr)
	, m_videoSourceComponent(videoSourceComponent)
	, m_bVideoIsStreaming(false)
	, m_processorMode(processorMode)
	, m_frameWidth(0)
	, m_frameHeight(0)
	, m_fps(0.f)
	// Video frame source buffer (from video thread)
	, m_bgrSourceBuffer(nullptr)
	, m_bgrSourceBufferWidth(0)
	, m_bgrSourceBufferHeight(0)
	, m_lastVideoFrameWriteIndex{0}
	, m_lastVideoFrameReadIndex(0)
	, m_lastFrameTimestamp()
	// Camera Intrinsics / Distortion parameters
	, m_intrinsics(new OpenCVMonoCameraIntrinsics)
	// Distortion maps
	, m_distortionMapX(nullptr)
	, m_distortionMapY(nullptr)
	, m_distortionTextureMap(nullptr)
	// Video Frame Texture Queue
	, m_videoFrameQueue(nullptr)
	, m_videoFrameQueueSize(frameQueueSize)
	, m_videoFrameQueueLastWriteIndex(-1)
	, m_videoFrameQueuePendingWriteIndex(0)
{
	IMkShaderCache* shaderCache= getGraphicsContext()->getShaderCache();

	// Init Video Frame Texture Queue Entries
	m_videoFrameQueue= new VideoFrameQueueEntry[frameQueueSize];
	for (int queueIndex= 0; queueIndex < frameQueueSize; ++queueIndex)
	{
		VideoFrameQueueEntry& frameEntry= m_videoFrameQueue[queueIndex];

		frameEntry.videoTexture= IMkTexturePtr();
		frameEntry.frameIndex= -1;
	}

	// Create a mesh used to render the video frame
	// (Assigns the INTERNAL_MATERIAL_PT_FULLSCREEN_RGB_TEXTURE material on the mesh)
	m_fullscreenRGBVideoQuad= createFullscreenQuadMesh(getGraphicsContext(), true);

	// Create a special material for to draw when no video is available
	m_noVideoMaterial= shaderCache->getMaterialByName(INTERNAL_MATERIAL_PT_PM5544_TEST_CARD);
	m_noVideoMaterialInstance= createMkMaterialInstance(m_noVideoMaterial);

	// Create CV processor for calibration mode (CPU-based OpenCV undistortion)
	if (processorMode == eVideoFrameProcessorMode::CALIBRATION)
	{
		m_cvProcessor= std::make_unique<CVVideoFrameProcessor>();
	}

	// Create GL processor for compositor mode (GPU-based shader undistortion)
	if (processorMode == eVideoFrameProcessorMode::COMPOSITOR)
	{
		m_glProcessor= std::make_unique<GLVideoFrameProcessor>();
		m_glProcessor->init(shaderCache);
	}
}

VideoFrameDistortionView::~VideoFrameDistortionView()
{
	// The source's receive thread writes into this view by raw pointer, so it must
	// be unsubscribed before any buffer is freed. This waits out a write in progress
	// and is a no-op when the owner already unsubscribed.
	if (m_videoSourceComponent)
	{
		m_videoSourceComponent->stopVideoStream(this);
	}

	// Free any textures in the video frame queue
	if (m_videoFrameQueue != nullptr)
	{
		for (int queueIndex= 0; queueIndex < m_videoFrameQueueSize; ++queueIndex)
		{
			VideoFrameQueueEntry& frameEntry= m_videoFrameQueue[queueIndex];

			if (frameEntry.videoTexture)
			{
				frameEntry.videoTexture->disposeTexture();
				frameEntry.videoTexture= IMkTexturePtr();
			}
		}

		delete[] m_videoFrameQueue;
	}

	// Free the texture we were rendering to, if any
	m_distortionTextureMap= nullptr;

	// Video Frame data
	if (m_bgrSourceBuffer != nullptr)
	{
		delete m_bgrSourceBuffer;
	}

	// Camera Intrinsics
	delete m_intrinsics;

	// Distortion maps
	if (m_distortionMapX != nullptr)
	{
		delete m_distortionMapX;
	}
	if (m_distortionMapY != nullptr)
	{
		delete m_distortionMapY;
	}

	// Processor objects cleaned up automatically via unique_ptr
}

// -- CVVideoFrameProcessor delegation --

cv::Mat* VideoFrameDistortionView::getGrayscaleSourceBuffer() const
{
	return m_cvProcessor ? m_cvProcessor->getGrayscaleSourceBuffer() : nullptr;
}

cv::Mat* VideoFrameDistortionView::getGrayscaleUndistortBuffer() const
{
	return m_cvProcessor ? m_cvProcessor->getGrayscaleUndistortBuffer() : nullptr;
}

cv::Mat* VideoFrameDistortionView::getBGRUndistortBuffer() const
{
	return m_cvProcessor ? m_cvProcessor->getBGRUndistortBuffer() : nullptr;
}

cv::Mat* VideoFrameDistortionView::getBGRGsDisplayBuffer() const
{
	return m_cvProcessor ? m_cvProcessor->getBGRGsDisplayBuffer() : nullptr;
}

bool VideoFrameDistortionView::isColorUndistortDisabled() const
{
	return m_cvProcessor ? m_cvProcessor->isColorUndistortDisabled() : false;
}

void VideoFrameDistortionView::setColorUndistortDisabled(bool bDisabled)
{
	if (m_cvProcessor)
		m_cvProcessor->setColorUndistortDisabled(bDisabled);
}

bool VideoFrameDistortionView::isGrayscaleUndistortDisabled() const
{
	return m_cvProcessor ? m_cvProcessor->isGrayscaleUndistortDisabled() : false;
}

void VideoFrameDistortionView::setGrayscaleUndistortDisabled(bool bDisabled)
{
	if (m_cvProcessor)
		m_cvProcessor->setGrayscaleUndistortDisabled(bDisabled);
}

// -- end delegation --

void VideoFrameDistortionView::ensureFrameBufferSize(int width, int height)
{
	// If the frame size hasn't changed, then bail
	if (m_frameWidth == width && m_frameHeight == height)
	{
		return;
	}

	// Update the frame size
	m_frameWidth= width;
	m_frameHeight= height;

	// Update intrinsics first in case the resolution changed
	MikanVideoSourceIntrinsics mikanIntrinsics;
	if (m_videoSourceComponent->getCameraIntrinsics(mikanIntrinsics))
	{
		if (mikanIntrinsics.intrinsics_type == MikanIntrinsicsType::MONO_CAMERA_INTRINSICS)
		{
			m_intrinsics->init(mikanIntrinsics.getMonoIntrinsics());
		}
		else
		{
			// TODO: Handle stereo intrinsics
			assert(false && "Unsupported Intrinsics type");
		}
	}
	else
	{
		m_intrinsics->init(width, height);
		MIKAN_LOG_WARNING("VideoFrameDistortionView")
			<< "VideoSource " << m_videoSourceComponent->getDevicePath()
			<< " is not distortion calibrated. Using estimated focal length and no distortion.";
	}

	// Both modes require distortion maps (CALIBRATION: for cv::remap, COMPOSITOR: for shader)
	if (m_distortionMapX != nullptr)
		delete m_distortionMapX;
	m_distortionMapX= new cv::Mat(cv::Size(m_frameWidth, m_frameHeight), CV_32FC1);

	if (m_distortionMapY != nullptr)
		delete m_distortionMapY;
	m_distortionMapY= new cv::Mat(cv::Size(m_frameWidth, m_frameHeight), CV_32FC1);

	// Resize CV processor buffers
	if (m_cvProcessor)
	{
		m_cvProcessor->ensureBufferSize(width, height);
	}

	// Resize GL processor buffers
	if (m_glProcessor)
	{
		m_glProcessor->ensureBufferSize(width, height);
	}

	// Both modes render into a GL texture queue
	{
		// Re-init all video frame queue entries for the new frame size
		for (int queueIndex= 0; queueIndex < m_videoFrameQueueSize; ++queueIndex)
		{
			VideoFrameQueueEntry& frameEntry= m_videoFrameQueue[queueIndex];

			// Free any existing texture for this queue entry
			if (frameEntry.videoTexture)
			{
				frameEntry.videoTexture->disposeTexture();
			}

			// Allocate a new texture for this queue entry
			IMkTexturePtr videoTexture= CreateMkTexture(m_frameWidth, m_frameHeight, nullptr,
														MK_RGB,  // texture format
														MK_BGR); // buffer format
			videoTexture->setGenerateMipMap(false);
			// Use PBO upload optimization only for CPU->GPU uploads (CV path).
			// GL path writes to these textures as FBO render targets, so PBO mode is not applicable.
			if (!m_glProcessor)
			{
				videoTexture->setPixelBufferObjectMode(IMkTexture::PixelBufferObjectMode::DoublePBOWrite);
			}
			videoTexture->createTexture();

			// Update the queue entry with the new texture and reset the frame index
			frameEntry.videoTexture= videoTexture;
			frameEntry.frameIndex= -1;
		}

		// Reset the video frame queue write indices since all frames have been invalidated by the size change
		m_videoFrameQueueLastWriteIndex= -1;
		m_videoFrameQueuePendingWriteIndex= 0;
	}

	// Generate the distortion map for the new frame size
	rebuildDistortionMap();
}

IMkGraphicsContext* VideoFrameDistortionView::getGraphicsContext() const
{
	if (m_videoSourceComponent != nullptr)
	{
		return m_videoSourceComponent->getGraphicsContext();
	}

	return nullptr;
}

bool VideoFrameDistortionView::readbackDirectColorTexture()
{
	EASY_FUNCTION();

	IMkTexturePtr directTexture= m_videoSourceComponent->getDirectColorTexture();
	if (directTexture == nullptr)
		return false;

	// Skip a frame already read back, so a render loop faster than the source
	// rate does not re-read identical pixels.
	const int64_t directFrameIndex= m_videoSourceComponent->getDirectFrameIndex();
	if (directFrameIndex >= 0 && directFrameIndex == m_lastReadbackFrameIndex)
		return false;

	const int width= directTexture->getTextureWidth();
	const int height= directTexture->getTextureHeight();
	if (width <= 0 || height <= 0)
		return false;

	// The conversion pass writes RGBA (see ARKitVideoSourceComponent's NV12
	// conversion framebuffer), and the CPU path downstream wants BGR.
	const size_t rgbaSize= (size_t)width * (size_t)height * 4;
	if (m_directReadbackBuffer.size() != rgbaSize)
		m_directReadbackBuffer.resize(rgbaSize);

	if (!directTexture->readTextureIntoBuffer(m_directReadbackBuffer.data(), m_directReadbackBuffer.size()))
	{
		// Logged once rather than every frame: a readback that silently fails
		// leaves calibration staring at a blank view with nothing to explain it.
		if (!m_bReadbackFailureLogged)
		{
			m_bReadbackFailureLogged= true;
			MIKAN_LOG_WARNING("VideoFrameDistortionView::readbackDirectColorTexture")
				<< "Failed to read the direct color texture (" << width << "x" << height
				<< ") back to the CPU, so calibration will see no frames from this source.";
		}
		return false;
	}

	const cv::Mat rgbaMat(height, width, CV_8UC4, m_directReadbackBuffer.data());
	cv::Mat bgrMat;
	cv::cvtColor(rgbaMat, bgrMat, cv::COLOR_RGBA2BGR);

	// Row order needs no correction. readTextureIntoBuffer returns rows in
	// texture order, and a video texture holds image row 0 at v=0 (see
	// conventions.md), so this is already top-down like every other source.
	// Mirroring stays the source definition's business, exactly as it is for a
	// CPU frame, so writeVideoFrame is given the same flag it always gets.
	VideoSourceDefinitionPtr definition= m_videoSourceComponent->getVideoSourceDefinition();
	const bool bIsFrameMirrored= definition ? definition->getIsFrameMirrored() : false;

	writeVideoFrame(bgrMat.data, cv::Size(width, height), bIsFrameMirrored);
	m_lastReadbackFrameIndex= directFrameIndex;

	return true;
}

void VideoFrameDistortionView::writeVideoFrame(const unsigned char* videoBuffer, const cv::Size& bufferDimensions,
											   bool bIsFlipped)
{
	EASY_FUNCTION();
	std::lock_guard<std::mutex> bufferLock(m_bgrSourceBufferMutex);

	const int srcBufferWidth= bufferDimensions.width;
	const int srcBufferHeight= bufferDimensions.height;
	const cv::Mat videoBufferMat(srcBufferHeight, srcBufferWidth, CV_8UC3, const_cast<unsigned char*>(videoBuffer));

	// (Re)create the target buffer to match source size
	if (m_bgrSourceBufferWidth != srcBufferWidth || m_bgrSourceBufferHeight != srcBufferHeight)
	{
		if (m_bgrSourceBuffer != nullptr)
		{
			delete m_bgrSourceBuffer;
		}

		// Allocate a new bgr source buffer
		m_bgrSourceBuffer= new cv::Mat(srcBufferHeight, srcBufferWidth, CV_8UC3);
		m_bgrSourceBufferWidth= srcBufferWidth;
		m_bgrSourceBufferHeight= srcBufferHeight;
	}

	if (bIsFlipped)
	{
		cv::flip(videoBufferMat, *m_bgrSourceBuffer, +1);
	}
	else
	{
		videoBufferMat.copyTo(*m_bgrSourceBuffer);
	}

	// Atomically increment the frame index on the write thread
	m_lastVideoFrameWriteIndex++;
}

void VideoFrameDistortionView::writeStereoVideoFrameSection(const unsigned char* videoBuffer,
															const cv::Size& bufferDimensions, const bool bIsFlipped,
															const cv::Rect& bufferBounds)
{
	EASY_FUNCTION();
	std::lock_guard<std::mutex> bufferLock(m_bgrSourceBufferMutex);

	const int srcBufferWidth= bufferDimensions.width;
	const int srcBufferHeight= bufferDimensions.height;
	const cv::Mat videoBufferMat(srcBufferHeight, srcBufferWidth, CV_8UC3, const_cast<unsigned char*>(videoBuffer));

	// (Re)create the target buffer to match source size
	const int srcSectionWidth= bufferBounds.size().width;
	const int srcSectionHeight= bufferBounds.size().height;
	if (m_bgrSourceBufferWidth != srcSectionWidth || m_bgrSourceBufferHeight != srcSectionHeight)
	{
		if (m_bgrSourceBuffer != nullptr)
		{
			delete m_bgrSourceBuffer;
		}

		// Allocate a new bgr source buffer
		m_bgrSourceBuffer= new cv::Mat(srcBufferHeight, srcBufferWidth, CV_8UC3);
		m_bgrSourceBufferWidth= srcSectionWidth;
		m_bgrSourceBufferHeight= srcSectionHeight;
	}

	if (bIsFlipped)
	{
		cv::flip(videoBufferMat(bufferBounds), *m_bgrSourceBuffer, +1);
	}
	else
	{
		videoBufferMat(bufferBounds).copyTo(*m_bgrSourceBuffer);
	}

	// Atomically increment the frame index on the write thread
	m_lastVideoFrameWriteIndex++;
}

bool VideoFrameDistortionView::hasNewVideoFrame() const
{
	// GPU-direct sources (ticket E3/E4): prefer a real frame index
	// (VideoSourceComponent::getDirectFrameIndex(), e.g. ARKit's wire-protocol
	// frameSeq) when the source provides one, so change-detection reflects actual
	// new-frame arrivals rather than firing every single call.
	const int64_t directFrameIndex= m_videoSourceComponent->getDirectFrameIndex();
	if (directFrameIndex >= 0)
		return m_lastVideoFrameReadIndex != directFrameIndex;

	// Fallback for GPU-direct sources that don't provide a real frame index yet -
	// write straight into their own CUDA-GL-interop texture every decoded frame,
	// with no CPU-side write/read index bookkeeping at all, so just treat "a live
	// direct texture exists" as "always has a new frame."
	if (m_videoSourceComponent->getDirectColorTexture() != nullptr)
		return true;

	return m_lastVideoFrameReadIndex != m_lastVideoFrameWriteIndex;
}

void VideoFrameDistortionView::updateFrameRateStatistic()
{
	const auto now= std::chrono::steady_clock::now();

	// Skip the very first sample: m_lastFrameTimestamp starts at the epoch, so its
	// delta is meaningless and would drag the average for a long time. Deliberately
	// unclamped, unlike the app's per-frame delta - clamping here would make a
	// genuine multi-second stall read as a healthy rate, which is the opposite of
	// what this number exists to show (see debugging.md on the delta clamp).
	if (m_lastFrameTimestamp.time_since_epoch().count() != 0)
	{
		const float deltaSeconds= std::chrono::duration<float>(now - m_lastFrameTimestamp).count();
		if (deltaSeconds > 0.f)
		{
			// Smooth the interval and invert once, rather than averaging 1/delta.
			// Averaging instantaneous rates over-weights short intervals: a 33ms
			// and a 5ms sample average to ~115fps when the true rate across them is
			// ~52, which had this reading 45fps against a measured 30fps stream.
			m_frameIntervalSeconds=
				(m_frameIntervalSeconds > 0.f) ? (m_frameIntervalSeconds * 0.9f) + (deltaSeconds * 0.1f) : deltaSeconds;
			m_fps= (m_frameIntervalSeconds > 0.f) ? (1.0f / m_frameIntervalSeconds) : 0.f;
		}
	}

	m_lastFrameTimestamp= now;
}

bool VideoFrameDistortionView::isReceivingFrames() const
{
	// The direct-texture check alone deadlocks a GPU-direct source. That texture
	// is created by processDirectVideoFrame(), which only runs inside
	// readAndProcessVideoFrame(), which callers gate behind this very function -
	// so a source that has never converted a frame is reported as not receiving
	// any, and never gets the chance to convert one. getDirectFrameIndex() is set
	// as each bundle arrives, independent of conversion, so it answers the
	// question actually being asked. Non-direct sources return -1 and are
	// unaffected.
	return m_lastVideoFrameWriteIndex > 0 || m_videoSourceComponent->getDirectColorTexture() != nullptr
		   || m_videoSourceComponent->getDirectFrameIndex() >= 0;
}

int64_t VideoFrameDistortionView::readNextVideoFrameIndex()
{
	EASY_FUNCTION();

	if (m_videoSourceComponent->getVideoStreamingStatus() == eVideoStreamingStatus::started)
	{
		// Copy the image from the video view
		if (hasNewVideoFrame())
		{
			updateFrameRateStatistic();

			// Read the next video frame into the source buffer and update the last read frame index
			m_lastVideoFrameReadIndex= m_lastVideoFrameWriteIndex;

			// Flag that we are receiving videoframes
			m_bVideoIsStreaming= true;
		}
	}
	else
	{
		m_bVideoIsStreaming= false;
	}

	return m_lastVideoFrameReadIndex;
}

IMkTexturePtr VideoFrameDistortionView::acquireQueueWriteTexture(int64_t newFrameIndex)
{
	if (m_videoFrameQueue == nullptr || m_videoFrameQueueSize == 0)
	{
		return IMkTexturePtr();
	}

	// Get the next video texture in the queue to write to
	IMkTexturePtr writeTexture= m_videoFrameQueue[m_videoFrameQueuePendingWriteIndex].videoTexture;

	// Record the frame index for this texture in the queue
	m_videoFrameQueue[m_videoFrameQueuePendingWriteIndex].frameIndex= newFrameIndex;

	// Update the queue index of the most recently written video frame
	m_videoFrameQueueLastWriteIndex= m_videoFrameQueuePendingWriteIndex;

	// Advance the pending write index
	m_videoFrameQueuePendingWriteIndex= (m_videoFrameQueuePendingWriteIndex + 1) % m_videoFrameQueueSize;

	return writeTexture;
}

void VideoFrameDistortionView::undistortDirectVideoFrame(IMkTexturePtr directTexture, int64_t newFrameIndex)
{
	EASY_FUNCTION();

	ensureFrameBufferSize(directTexture->getTextureWidth(), directTexture->getTextureHeight());

	IMkTexturePtr writeTexture= acquireQueueWriteTexture(newFrameIndex);
	if (writeTexture != nullptr)
	{
		// The same pass a CPU frame takes after its upload, so the composited texture has one
		// orientation and one undistortion whichever path the frame arrived by
		m_glProcessor->computeUndistortion(directTexture, writeTexture, m_distortionTextureMap,
										   m_fullscreenRGBVideoQuad, getGraphicsContext());
	}
}

void VideoFrameDistortionView::processVideoFrame(int64_t newFrameIndex)
{
	EASY_FUNCTION();

	// Lock the source buffer state while we are processing it
	std::lock_guard<std::mutex> bufferLock(m_bgrSourceBufferMutex);

	// Reallocate the frame buffer if the video source has changed resolution
	// (This can happen on streaming video sources)
	ensureFrameBufferSize(m_bgrSourceBufferWidth, m_bgrSourceBufferHeight);

	IMkTexturePtr writeTexture= acquireQueueWriteTexture(newFrameIndex);

	// Update the video frame display texture
	if (writeTexture != nullptr)
	{
		EASY_BLOCK("Copy to Texture");

		switch (m_videoDisplayMode)
		{
		case eVideoDisplayMode::mode_bgr:
			// Copy BGR OpenCV buffer directly to texture with no processing
			copyOpenCVMatIntoGLTexture(*m_bgrSourceBuffer, writeTexture);
			break;
		case eVideoDisplayMode::mode_undistored:
		{
			if (m_glProcessor)
			{
				// Upload the source BGR frame to the GL texture, then run the undistortion shader
				m_glProcessor->uploadSourceBuffer(*m_bgrSourceBuffer);
				m_glProcessor->computeUndistortion(m_glProcessor->getUploadedSourceTexture(), writeTexture,
												   m_distortionTextureMap, m_fullscreenRGBVideoQuad,
												   getGraphicsContext());
			}
			else if (m_cvProcessor && m_cvProcessor->getBGRUndistortBuffer())
			{
				// Apply undistortion to the video frame using OpenCV
				m_cvProcessor->computeUndistortion(*m_bgrSourceBuffer, *m_distortionMapX, *m_distortionMapY);

				// Copy the BGR OpenCV frame buffer to the output texture
				copyOpenCVMatIntoGLTexture(*m_cvProcessor->getBGRUndistortBuffer(), writeTexture);
			}
		}
		break;
		case eVideoDisplayMode::mode_grayscale:
			if (m_cvProcessor && m_cvProcessor->getBGRGsDisplayBuffer())
			{
				// Apply undistortion to the video frame using OpenCV
				m_cvProcessor->computeUndistortion(*m_bgrSourceBuffer, *m_distortionMapX, *m_distortionMapY);

				// Copy the BGR-Grayscale OpenCV frame buffer to the output texture
				copyOpenCVMatIntoGLTexture(*m_cvProcessor->getBGRGsDisplayBuffer(), writeTexture);
			}
			break;
		default:
			assert(0 && "unreachable");
			break;
		}
	}
}

int64_t VideoFrameDistortionView::readAndProcessVideoFrame()
{
	EASY_FUNCTION();

	// GPU-direct sources (ticket E3/E4/"Phase 6") may need an explicit per-tick
	// processing pass before their direct texture is safe to read (e.g. ARKit's
	// NV12->RGBA shader conversion) - run it unconditionally (cheap no-op for
	// every other source type) before checking getDirectColorTexture() below, so
	// the readback or undistortion below never reads a stale/blank target.
	m_videoSourceComponent->processDirectVideoFrame();

	if (IMkTexturePtr directTexture= m_videoSourceComponent->getDirectColorTexture())
	{
		// Calibration needs pixels on the CPU, and a GPU-direct source never
		// delivers any: its frames go decoder to GPU without passing through
		// writeVideoFrame. Pull the converted texture back so the pattern
		// finders downstream see frames like they would from any other source.
		// Only in CALIBRATION mode - the compositor undistorts the texture on the
		// GPU below and a readback there would stall every frame for nothing.
		if (m_processorMode == eVideoFrameProcessorMode::CALIBRATION)
		{
			if (readbackDirectColorTexture())
			{
				// The readback wrote through writeVideoFrame, so the normal CPU
				// path below has a frame waiting and can undistort and convert it
				// exactly as it would for a USB camera.
				if (hasNewVideoFrame())
				{
					processVideoFrame(readNextVideoFrameIndex());
				}

				m_bVideoIsStreaming= true;
				return m_lastVideoFrameReadIndex;
			}
		}

		// Track a real frame index when the source provides one (e.g. ARKit's
		// frameSeq, via getDirectFrameIndex()) so callers that compare it against a
		// previously-seen value (e.g. CompositorComponent::tryEnqueueNewFrame) only
		// see it advance on an actual new frame, not every single call - falls back
		// to a plain increment-per-call counter for any GPU-direct source that
		// doesn't provide one.
		const int64_t directFrameIndex= m_videoSourceComponent->getDirectFrameIndex();
		m_bVideoIsStreaming= true;

		// This branch returns before readNextVideoFrameIndex() is ever reached, so
		// the frame rate statistic has to be sampled here too - otherwise it stays
		// at its initial value for every GPU-direct source and the settings screen
		// reports 0fps against a live stream.
		const int64_t newReadIndex= (directFrameIndex >= 0) ? directFrameIndex : (m_lastVideoFrameReadIndex + 1);
		if (newReadIndex != m_lastVideoFrameReadIndex)
		{
			updateFrameRateStatistic();

			// The compositor undistorts the converted frame on the GPU into its frame
			// queue, as it does an uploaded CPU frame, so the composited texture never
			// depends on the decode path
			if (m_glProcessor)
			{
				undistortDirectVideoFrame(directTexture, newReadIndex);
			}
		}

		m_lastVideoFrameReadIndex= newReadIndex;
		return m_lastVideoFrameReadIndex;
	}

	if (hasNewVideoFrame())
	{
		processVideoFrame(readNextVideoFrameIndex());
	}

	return m_lastVideoFrameReadIndex;
}

IMkTexturePtr VideoFrameDistortionView::getVideoTexture(int64_t desiredFrameIndex) const
{
	// Calibration views show a GPU-direct source's converted frame as is. The
	// compositor reads its own undistorted queue, filled for both kinds of source.
	if (!m_glProcessor)
	{
		if (IMkTexturePtr directTexture= m_videoSourceComponent->getDirectColorTexture())
			return directTexture;
	}

	if (m_videoFrameQueue != nullptr)
	{
		// If a specific frame index was requested, then find it in the queue and return its texture
		if (desiredFrameIndex != -1)
		{
			for (int queueIndex= 0; queueIndex < m_videoFrameQueueSize; queueIndex++)
			{
				const VideoFrameQueueEntry& queueEntry= m_videoFrameQueue[queueIndex];

				if (queueEntry.frameIndex == desiredFrameIndex)
				{
					return queueEntry.videoTexture;
				}
			}
		}

		// If we get here, then either no specific frame index was requested,
		// or the requested index wasn't found in the queue.
		// In either case, return the most recently written video texture in the queue (if valid)
		if (m_videoFrameQueueLastWriteIndex >= 0)
		{
			return m_videoFrameQueue[m_videoFrameQueueLastWriteIndex].videoTexture;
		}
	}

	return IMkTexturePtr();
}

void VideoFrameDistortionView::applyMonoCameraIntrinsics(const struct MikanMonoIntrinsics* instrinsics)
{
	m_intrinsics->init(*instrinsics);
	rebuildDistortionMap();
}

void VideoFrameDistortionView::rebuildDistortionMap()
{
	m_distortionTextureMap= nullptr;

	if (m_distortionMapX != nullptr && m_distortionMapY != nullptr)
	{
		// (Re)create the X and Y undistortion maps used by cv::remap
		cv::initUndistortRectifyMap(m_intrinsics->distorted_intrinsic_matrix, m_intrinsics->distortion_coeffs,
									cv::noArray(), // unneeded rectification transformation computed by stereoRectify()
									m_intrinsics->undistorted_intrinsic_matrix, cv::Size(m_frameWidth, m_frameHeight),
									CV_32FC1, // Distortion map type
									*m_distortionMapX, *m_distortionMapY);

		// Copy the distortion pixel offsets into a texture with normalized float values
		{
			float width= (float)m_frameWidth;
			float height= (float)m_frameHeight;
			float* data= new float[m_frameWidth * m_frameHeight * 2];
			size_t write_index= 0;
			for (int y= 0; y < m_frameHeight; ++y)
			{
				for (int x= 0; x < m_frameWidth; ++x)
				{
					data[write_index + 0]= m_distortionMapX->at<float>(y, x) / width;
					data[write_index + 1]= m_distortionMapY->at<float>(y, x) / height;
					write_index+= 2;
				}
			}

			m_distortionTextureMap= CreateMkTexture(m_frameWidth, m_frameHeight, (uint8_t*)data, MK_RG32F, MK_RG);
			m_distortionTextureMap->createTexture();

			delete[] data;
		}
	}
}

void VideoFrameDistortionView::renderSelectedVideoBuffers()
{
	if (m_fullscreenRGBVideoQuad != nullptr)
	{
		// Draw the undistorted video texture
		IMkTexturePtr videoTexture= getVideoTexture();
		if (videoTexture != nullptr && m_bVideoIsStreaming)
		{
			MkMaterialInstancePtr materialInstance= m_fullscreenRGBVideoQuad->getMaterialInstance();
			MkMaterialConstPtr material= materialInstance->getMaterial();

			if (auto materialBinding= material->bindMaterial())
			{
				// Bind the color texture
				materialInstance->setTextureBySemantic(eUniformSemantic::rgbTexture, videoTexture);

				// Draw the color texture
				if (auto materialInstanceBinding= materialInstance->bindMaterialInstance(materialBinding))
				{
					m_fullscreenRGBVideoQuad->drawElements();
				}
			}
		}
		// Draw the "no-video" shader
		else
		{
			if (auto materialBinding= m_noVideoMaterial->bindMaterial())
			{
				// TODO: "Time" and "ScreenSize" are uniforms that all materials
				//  should have available by default in the graphics context
				IEditorWindow* ownerWindow= m_videoSourceComponent->getOwnerEditorWindow();
				const double currentTimeSeconds= ownerWindow->getOwnerApp()->getSecondsSinceAppStart();
				const float shaderTime= (float)fmodf(currentTimeSeconds, 1000.0);
				const glm::vec2 screenSize(ownerWindow->getWidth(), ownerWindow->getHeight());
				m_noVideoMaterialInstance->setVec2BySemantic(eUniformSemantic::screenSize, screenSize);
				m_noVideoMaterialInstance->setFloatBySemantic(eUniformSemantic::floatConstant0, shaderTime);

				// Draw the no-video shader
				if (auto materialInstanceBinding= m_noVideoMaterialInstance->bindMaterialInstance(materialBinding))
				{
					m_fullscreenRGBVideoQuad->drawElements();
				}
			}
		}
	}
}

void VideoFrameDistortionView::copyOpenCVMatIntoGLTexture(const cv::Mat& mat, IMkTexturePtr texture)
{
	size_t bufferSize= mat.step[0] * mat.rows;

	texture->copyBufferIntoTexture(mat.data, bufferSize);
}

#include "MikanARKitVideoDevice.h"
#include "ARKitRTPHeaderExtension.h"
#include "Cuda/CudaErrorHandling.h"
#include "IMkTexture.h"
#include "MikanARKitVideoDeviceManager.h"
#include "Logger.h"

// gst/cuda/gstcudamemory.h transitively includes <cudaGL.h>/<d3d11.h>/<dxgi.h>
// (via cuda-gst.h) for CUDA-GL/D3D11 interop declarations this file never calls -
// none of that is needed here (Track D owns actual GL interop), but the headers
// still get pulled in. Windows' own <GL/gl.h> (reached via cudaGL.h) expects
// WINGDIAPI/APIENTRY to already be defined consistently by <windows.h>, so
// <windows.h> must be included first or gl.h's declarations fail to parse.
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <gst/app/gstappsink.h>
#include <gst/cuda/gstcudamemory.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include <chrono>
#include <cstring>
#include <sstream>
#include <thread>

// -- GStreamerImpl -----
// Holds the RTP/H.264 receive pipeline for the video channel (basePort+0).
// Two explicit-element pipeline tiers ("Phase 7" - see MikanARKitVideoDevice.h's
// own doc comment for the full rationale), both built from the same
// udpsrc/rtpjitterbuffer/rtph264depay/h264parse prefix:
//   Hardware: ... ! nvh264dec ! video/x-raw(memory:CUDAMemory) ! appsink
//   Software: ... ! openh264dec ! videoconvert ! video/x-raw,format=BGR ! appsink
// Never decodebin for either tier - an explicit element name is resolved
// immediately by gst_parse_launch rather than lazily on first buffer, which is
// what makes synchronous tier-probing at open()-time possible at all. Kept as
// an opaque struct (rather than members directly on MikanARKitVideoDevice) so
// GStreamer headers don't leak into this plugin's public header, matching
// MikanGStreamerVideoDevice.h's m_impl pattern.
struct GStreamerImpl
{
	// Sized to hold several keyframe bursts rather than the OS default (~64KB),
	// which one 1920x1440 IDR overruns on its own. See the pipeline builders.
	static constexpr int k_udpReceiveBufferBytes= 4 * 1024 * 1024;

	// Enough to ride out a burst that arrives faster than the once-per-tick
	// drain. The previous 50ms left almost no slack at 30fps.
	static constexpr int k_jitterBufferLatencyMs= 200;

	uint16_t videoPort;

	GstElement* pipeline= nullptr;
	GstElement* depay= nullptr;
	GstElement* appsink= nullptr;
	GstBus* bus= nullptr;

	explicit GStreamerImpl(uint16_t inVideoPort)
		: videoPort(inVideoPort)
	{
	}

	std::string buildHardwarePipelineString() const
	{
		std::stringstream ss;
		ss << "udpsrc port=" << videoPort << " "
		   << "caps=\"application/x-rtp,media=video,encoding-name=H264,payload=96\" "
		   // A keyframe is an order of magnitude larger than a P-frame and leaves
		   // the sender as one back-to-back burst of hundreds of RTP packets,
		   // while this socket is only drained once per render tick. On the OS
		   // default receive buffer (~64KB) that burst overruns and the keyframe
		   // arrives incomplete, which costs a full keyframe interval of video
		   // because the decoder has no reference until the next IDR. Measured
		   // live at 1920x1440: every stall was a lost keyframe and lasted
		   // exactly one 4s keyframe period.
		   << "buffer-size=" << k_udpReceiveBufferBytes << " "
		   << "! rtpjitterbuffer latency=" << k_jitterBufferLatencyMs << " "
		   << "! rtph264depay name=depay "
		   << "! h264parse "
		   << "! nvh264dec "
		   << "! video/x-raw(memory:CUDAMemory) "
		   << "! appsink name=sink";
		return ss.str();
	}

	// openh264dec is 8-bit 4:2:0-only (see MikanARKitVideoDevice.h's own note) -
	// a reasonable software-only fallback since it's the only vendor-agnostic
	// H.264 decoder confirmed to load in-process on this project's GStreamer
	// distribution (see project memory on the in-process decoder-plugin gap;
	// avdec_h264/gst-libav is not installed).
	std::string buildSoftwarePipelineString() const
	{
		std::stringstream ss;
		ss << "udpsrc port=" << videoPort << " "
		   << "caps=\"application/x-rtp,media=video,encoding-name=H264,payload=96\" "
		   // A keyframe is an order of magnitude larger than a P-frame and leaves
		   // the sender as one back-to-back burst of hundreds of RTP packets,
		   // while this socket is only drained once per render tick. On the OS
		   // default receive buffer (~64KB) that burst overruns and the keyframe
		   // arrives incomplete, which costs a full keyframe interval of video
		   // because the decoder has no reference until the next IDR. Measured
		   // live at 1920x1440: every stall was a lost keyframe and lasted
		   // exactly one 4s keyframe period.
		   << "buffer-size=" << k_udpReceiveBufferBytes << " "
		   << "! rtpjitterbuffer latency=" << k_jitterBufferLatencyMs << " "
		   << "! rtph264depay name=depay "
		   << "! h264parse "
		   << "! openh264dec "
		   << "! videoconvert "
		   << "! video/x-raw,format=BGR "
		   << "! appsink name=sink";
		return ss.str();
	}

	// Attaches ARKitRTPHeaderExtension (ticket C3) to the depayloader so the
	// frameSeq/captureTimestampUs carried in each RTP packet's header extension
	// (RFC 5285, extension id kARKitRTPHeaderExtensionId) is extracted and attached
	// as ARKitFrameSeqMeta onto rtph264depay's output buffers.
	bool attachFrameSeqExtension() const
	{
		GstRTPHeaderExtension* ext= arkit_rtp_header_extension_new();
		if (ext == nullptr)
			return false;

		gst_rtp_header_extension_set_id(ext, kARKitRTPHeaderExtensionId);

		g_signal_emit_by_name(depay, "add-extension", ext);
		gst_object_unref(ext);

		return true;
	}

	static bool extractFrameDimensions(GstCaps* caps, int& outWidth, int& outHeight)
	{
		outWidth= 0;
		outHeight= 0;

		if (caps == nullptr || gst_caps_is_empty(caps))
			return false;

		GstStructure* structure= gst_caps_get_structure(caps, 0);
		if (structure == nullptr)
			return false;

		const bool bHasWidth= gst_structure_get_int(structure, "width", &outWidth) != FALSE;
		const bool bHasHeight= gst_structure_get_int(structure, "height", &outHeight) != FALSE;

		return bHasWidth && bHasHeight && outWidth > 0 && outHeight > 0;
	}

	static gboolean busCallback(GstBus*, GstMessage* msg, gpointer userData)
	{
		auto* videoDevice= reinterpret_cast<MikanARKitVideoDevice*>(userData);

		switch (GST_MESSAGE_TYPE(msg))
		{
		case GST_MESSAGE_EOS:
		{
			MIKAN_LOG_INFO("MikanARKitVideoDevice::busCallback") << "End of video stream";
			videoDevice->close();
		}
		break;
		case GST_MESSAGE_ERROR:
		{
			GError* error= nullptr;
			gchar* debugInfo= nullptr;
			gst_message_parse_error(msg, &error, &debugInfo);

			MIKAN_LOG_ERROR("MikanARKitVideoDevice::busCallback")
				<< "Error from element " << GST_OBJECT_NAME(msg->src) << ": "
				<< (error != nullptr ? error->message : "unknown");
			MIKAN_LOG_ERROR("MikanARKitVideoDevice::busCallback")
				<< "Debug info: " << (debugInfo != nullptr ? debugInfo : "none");

			g_clear_error(&error);
			g_free(debugInfo);

			videoDevice->close();
		}
		break;
		default:
			break;
		}

		return TRUE;
	}
};

MikanARKitVideoDevice::MikanARKitVideoDevice(MikanARKitVideoDeviceManager* ownerDeviceManager,
											 const ARKitVideoConnectionSettings& connectionInfo)
	: m_ownerDeviceManager(ownerDeviceManager)
	, m_connectionInfo(connectionInfo)
	, m_devicePath("arkit://" + std::to_string(connectionInfo.basePort))
	, m_impl(new GStreamerImpl(connectionInfo.basePort))
{
}

MikanARKitVideoDevice::~MikanARKitVideoDevice()
{
	close();
	delete m_impl;
}

// -- Device Listener -----
void MikanARKitVideoDevice::addListener(IARKitVideoDeviceListener* listener)
{
	std::lock_guard<std::mutex> lock(m_listenersMutex);
	m_listeners.insert(listener);
}

void MikanARKitVideoDevice::removeListener(IARKitVideoDeviceListener* listener)
{
	std::lock_guard<std::mutex> lock(m_listenersMutex);
	m_listeners.erase(listener);
}

// -- Device Properties -----
const char* MikanARKitVideoDevice::getDevicePath() const { return m_devicePath.c_str(); }

const char* MikanARKitVideoDevice::getFriendlyName() const { return getDevicePath(); }

// -- Device Activation -----
eVideoOpeningStatus MikanARKitVideoDevice::open()
{
	if (m_openState == eOpenState::open || m_openState == eOpenState::opening)
		return getVideoOpeningStatus();

	// Launch the GStreamer video pipeline setup on a background thread, matching
	// MikanGStreamerVideoDevice's rationale (pipeline construction is slow on first
	// call - loads GStreamer DLLs).
	m_openState= eOpenState::opening;
	auto promise= std::make_shared<std::promise<bool>>();
	m_openFuture= promise->get_future();
	std::thread([promise, this]() mutable { promise->set_value(openOnThread()); }).detach();

	return eVideoOpeningStatus::opening;
}

bool MikanARKitVideoDevice::openOnThread()
{
	// Two-tier decode ("Phase 7" - see this class's own header comment). Try the
	// hardware pipeline first; an explicit element name (unlike decodebin) means
	// gst_parse_launch fails synchronously right here if nvh264dec/the nvcodec
	// plugin isn't available, which is exactly what makes falling back to the
	// software pipeline cleanly possible at open()-time rather than discovering
	// the failure lazily on first buffer.
	const std::string hardwarePipelineString= m_impl->buildHardwarePipelineString();

	GError* error= nullptr;
	m_impl->pipeline= gst_parse_launch(hardwarePipelineString.c_str(), &error);
	if (error != nullptr)
	{
		MIKAN_LOG_WARNING("MikanARKitVideoDevice::openOnThread")
			<< "Hardware decode pipeline unavailable (is an NVIDIA GPU/driver and the GStreamer nvcodec plugin "
			   "installed?): "
			<< error->message << " - falling back to software decode (openh264dec)";
		g_error_free(error);
		error= nullptr;

		if (m_impl->pipeline != nullptr)
		{
			gst_object_unref(m_impl->pipeline);
			m_impl->pipeline= nullptr;
		}

		const std::string softwarePipelineString= m_impl->buildSoftwarePipelineString();
		m_impl->pipeline= gst_parse_launch(softwarePipelineString.c_str(), &error);
		if (error != nullptr)
		{
			MIKAN_LOG_ERROR("MikanARKitVideoDevice::openOnThread")
				<< "Failed to create software decode pipeline (is the GStreamer openh264 plugin installed?): "
				<< error->message;
			g_error_free(error);
			return false;
		}

		m_decodeTier= eDecodeTier::software;
	}
	else
	{
		m_decodeTier= eDecodeTier::hardware;
	}

	m_impl->appsink= gst_bin_get_by_name(GST_BIN(m_impl->pipeline), "sink");
	if (m_impl->appsink == nullptr)
	{
		MIKAN_LOG_ERROR("MikanARKitVideoDevice::openOnThread") << "Failed to find appsink in pipeline";
		return false;
	}

	m_impl->depay= gst_bin_get_by_name(GST_BIN(m_impl->pipeline), "depay");
	if (m_impl->depay == nullptr)
	{
		MIKAN_LOG_ERROR("MikanARKitVideoDevice::openOnThread") << "Failed to find rtph264depay in pipeline";
		return false;
	}

	if (!m_impl->attachFrameSeqExtension())
	{
		MIKAN_LOG_ERROR("MikanARKitVideoDevice::openOnThread")
			<< "Failed to attach ARKit frameSeq RTP header extension";
		return false;
	}

	m_impl->bus= gst_pipeline_get_bus(GST_PIPELINE(m_impl->pipeline));
	if (m_impl->bus == nullptr)
	{
		MIKAN_LOG_ERROR("MikanARKitVideoDevice::openOnThread") << "Failed to get pipeline bus";
		return false;
	}

	return true;
}

eVideoOpeningStatus MikanARKitVideoDevice::getVideoOpeningStatus() const
{
	switch (m_openState)
	{
	case eOpenState::open:
		return eVideoOpeningStatus::open;
	case eOpenState::opening:
		return eVideoOpeningStatus::opening;
	case eOpenState::failed:
		return eVideoOpeningStatus::failed;
	default:
		return eVideoOpeningStatus::closed;
	}
}

void MikanARKitVideoDevice::update(float deltaSeconds)
{
	// Poll the async open future
	if (m_openState == eOpenState::opening)
	{
		if (m_openFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			const bool bSuccess= m_openFuture.get();
			if (bSuccess)
			{
				// Note: NOT gst_bus_add_watch() here - that attaches a GSource to the
				// default GLib main context, but nothing in this codebase ever pumps
				// that context (no g_main_loop_run/g_main_context_iteration anywhere),
				// so a watch-based callback would simply never fire. Bus messages are
				// instead polled explicitly each tick below, alongside the appsink
				// pull.
				gst_app_sink_set_emit_signals(GST_APP_SINK(m_impl->appsink), FALSE);
				gst_app_sink_set_drop(GST_APP_SINK(m_impl->appsink), TRUE);
				// 3 rather than 1. With a single slot any frames that arrive between
				// two polls cost all but the newest, and the hardware decoder delivers
				// burstier than the software one, so the cap bites hardest exactly where
				// throughput matters. Measured at 1920x1440 on nvh264dec, going from 1 to
				// 3 slots:
				//   30fps  27.65 -> 29.97 fps received, 7.66% -> 0.13% lost
				//   60fps  57.42 -> 59.50 fps received, 3.43% -> 0.11% lost
				// The losses were overwhelmingly isolated single frames, not bursts, which
				// is the signature of a queue collision rather than a starved consumer -
				// the render loop had ample headroom at 97.5Hz, and the identical pipeline
				// into a standalone fakesink (no such cap) sustained a full 60fps.
				// Latency stays bounded: the poll runs faster than frames arrive, so the
				// queue drains rather than accumulating, and drop=TRUE caps the worst case
				// at three frames.
				gst_app_sink_set_max_buffers(GST_APP_SINK(m_impl->appsink), 3);

				m_openState= eOpenState::open;

				{
					std::lock_guard<std::mutex> lock(m_listenersMutex);
					for (IARKitVideoDeviceListener* listener : m_listeners)
						listener->notifyDeviceOpened(this);
				}

				if (m_pendingStreamStartAfterOpen)
				{
					m_pendingStreamStartAfterOpen= false;
					startVideoStream();
				}
			}
			else
			{
				m_openState= eOpenState::failed;
			}
		}
		return;
	}

	if (m_openState != eOpenState::open)
		return;

	// Non-blocking bus drain (see the note in the open-completion branch above for
	// why this is polled explicitly rather than via gst_bus_add_watch).
	while (GstMessage* msg=
			   gst_bus_pop_filtered(m_impl->bus, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)))
	{
		GStreamerImpl::busCallback(m_impl->bus, msg, this);
		gst_message_unref(msg);

		if (m_openState != eOpenState::open)
			return; // busCallback triggered close()
	}

	if (m_streamingStatus != eVideoStreamingStatus::started && m_streamingStatus != eVideoStreamingStatus::pendingStart)
	{
		return;
	}

	GstSample* sample= gst_app_sink_try_pull_sample(GST_APP_SINK(m_impl->appsink), 0);
	if (sample != nullptr)
	{
		m_timeSinceLastFrameSeconds= 0.0f;

		GstBuffer* buffer= gst_sample_get_buffer(sample);
		GstCaps* caps= gst_sample_get_caps(sample);

		int width= 0, height= 0;
		if (buffer != nullptr && GStreamerImpl::extractFrameDimensions(caps, width, height))
		{
			if (m_streamingStatus == eVideoStreamingStatus::pendingStart)
			{
				MIKAN_LOG_INFO("MikanARKitVideoDevice::update")
					<< "Video stream started (" << width << "x" << height << ", "
					<< (m_decodeTier == eDecodeTier::hardware ? "hardware" : "software") << " decode)";
				m_streamingStatus= eVideoStreamingStatus::started;
			}

			// Attached by ARKitRTPHeaderExtension (ticket C3) at the rtph264depay
			// stage, from the RTP header extension carried on the encoded packets;
			// must survive h264parse's (and, for the hardware tier, nvh264dec's)
			// internal buffer transforms to still be present here (see
			// ARKitFrameSeqMeta's transform function) - tier-independent, since pose
			// rides in the RTP extension regardless of which decoder produced this
			// buffer.
			const ARKitFrameSeqMeta* frameSeqMeta= arkit_buffer_get_frame_seq_meta(buffer);

			// Bundle is built directly from the RTP extension meta and dispatched
			// immediately - no separate video/pose correlation step anymore now that
			// pose rides inside the same RTP packet that carries the video frame it
			// belongs to. The video-specific fields (hasVideo/videoData/
			// videoWidth/videoHeight) are filled in per-tier below, once the buffer
			// has been mapped the right way for that tier.
			ARKitVideoFrameBundle bundle;
			bool bHaveFrameSeqMeta= false;
			if (frameSeqMeta != nullptr)
			{
				bundle.frameSeq= frameSeqMeta->frameSeq;
				bundle.timestampUs= frameSeqMeta->captureTimestampUs;

				if (frameSeqMeta->hasPose)
				{
					bundle.hasPose= true;
					std::memcpy(bundle.pose.transform, frameSeqMeta->transform, sizeof(bundle.pose.transform));
					bundle.pose.fx= frameSeqMeta->fx;
					bundle.pose.fy= frameSeqMeta->fy;
					bundle.pose.cx= frameSeqMeta->cx;
					bundle.pose.cy= frameSeqMeta->cy;
					bundle.pose.imageWidth= frameSeqMeta->imageWidth;
					bundle.pose.imageHeight= frameSeqMeta->imageHeight;
				}

				bHaveFrameSeqMeta= true;
			}
			else
			{
				// Expected if the sender doesn't attach the extension (e.g. a
				// non-MikanARStreamer RTP source) or if an intervening element
				// dropped the meta - nothing to dispatch since there's no frameSeq
				// to key a bundle by.
				MIKAN_LOG_WARNING("MikanARKitVideoDevice::update")
					<< "Decoded video frame: " << width << "x" << height
					<< ", but no frameSeq RTP header extension meta was present";
			}

			if (m_decodeTier == eDecodeTier::hardware)
			{
				// GST_MAP_CUDA maps the buffer's device pointer itself rather than
				// downloading to a host-readable copy (ticket C4) - map.data is a
				// CUdeviceptr value, not CPU-readable memory; it must never be
				// dereferenced from the CPU, only handed to CUDA APIs (see
				// CudaGLColorTexture::copyFromDevice, which reads it as the source
				// NV12 frame).
				GstMapInfo map;
				if (gst_buffer_map(buffer, &map, static_cast<GstMapFlags>(GST_MAP_READ | GST_MAP_CUDA)))
				{
					const CUdeviceptr devicePtr= reinterpret_cast<CUdeviceptr>(map.data);

					// LoggerStream has no std::hex/std::dec manipulator support (unlike
					// std::ostream) - its operator<<(const void*) already formats as a
					// hex address, so log the pointer value that way instead.
					const void* devicePtrForLogging= reinterpret_cast<const void*>(devicePtr);

					if (bHaveFrameSeqMeta)
					{
						MIKAN_LOG_TRACE("MikanARKitVideoDevice::update")
							<< "Decoded CUDA video frame: " << width << "x" << height
							<< ", devicePtr=" << devicePtrForLogging << ", frameSeq=" << bundle.frameSeq
							<< (bundle.hasPose ? " (pose attached)" : " (no pose)");

						bundle.hasVideo= true;
						// videoData intentionally left null - decode goes straight to CUDA
						// device memory (GST_MAP_CUDA, see this method's own comment
						// above); handing decoded pixel data across the DLL boundary
						// through this bundle only happens for the software tier below.
						bundle.videoData= nullptr;
						bundle.videoWidth= 0;
						bundle.videoHeight= 0;

						notifyFrameBundleReceived(bundle);
					}

					// Zero-copy CUDA-GL texture pipeline (ticket E3; "Phase 6"):
					// NV12 planes for the GLSL display-conversion pass. Runs
					// regardless of whether the frameSeq meta was present above -
					// display shouldn't depend on pose correlation working.
					updateColorTexture(buffer, devicePtr, width, height);

					gst_buffer_unmap(buffer, &map);
				}
				else
				{
					MIKAN_LOG_ERROR("MikanARKitVideoDevice::update")
						<< "Failed to map decoded video buffer as CUDA memory";
				}
			}
			else // eDecodeTier::software
			{
				// Software tier decodes straight to packed BGR host memory
				// (videoconvert ! video/x-raw,format=BGR - see
				// GStreamerImpl::buildSoftwarePipelineString) - a plain GST_MAP_READ
				// is all that's needed, no CUDA involved. bundle.videoData is only
				// valid for the duration of this callback (see
				// ARKitVideoFrameBundle's own doc comment) - notifyFrameBundleReceived
				// must run, and any listener must finish copying it, before
				// gst_buffer_unmap() below.
				GstMapInfo map;
				if (gst_buffer_map(buffer, &map, GST_MAP_READ))
				{
					if (bHaveFrameSeqMeta)
					{
						MIKAN_LOG_TRACE("MikanARKitVideoDevice::update")
							<< "Decoded software video frame: " << width << "x" << height
							<< ", frameSeq=" << bundle.frameSeq << (bundle.hasPose ? " (pose attached)" : " (no pose)");

						bundle.hasVideo= true;
						bundle.videoData= map.data;
						bundle.videoWidth= width;
						bundle.videoHeight= height;

						notifyFrameBundleReceived(bundle);
					}

					gst_buffer_unmap(buffer, &map);
				}
				else
				{
					MIKAN_LOG_ERROR("MikanARKitVideoDevice::update")
						<< "Failed to map decoded video buffer for CPU read";
				}
			}
		}

		gst_sample_unref(sample);
	}
	else if (m_streamingStatus == eVideoStreamingStatus::started)
	{
		m_timeSinceLastFrameSeconds+= deltaSeconds;
		if (m_timeSinceLastFrameSeconds >= k_streamTimeoutSeconds)
		{
			MIKAN_LOG_WARNING("MikanARKitVideoDevice::update")
				<< "No video frame received for " << m_timeSinceLastFrameSeconds << "s - reopening video pipeline";

			m_pendingStreamStartAfterOpen= true;
			close();
			open();
		}
	}
}

void MikanARKitVideoDevice::close()
{
	if (m_openState == eOpenState::opening && m_openFuture.valid())
	{
		// Block until the in-flight async open completes so we can safely clean up
		m_openFuture.get();
		// Don't notify — the device was never fully open from the caller's perspective
	}
	else if (m_openState == eOpenState::open)
	{
		std::lock_guard<std::mutex> lock(m_listenersMutex);
		for (IARKitVideoDeviceListener* listener : m_listeners)
			listener->notifyDeviceClosed(this);

		// Clients need to reregister as listeners when they reopen
		m_listeners.clear();
	}

	m_openState= eOpenState::closed;
	m_streamingStatus= eVideoStreamingStatus::stopped;
	m_timeSinceLastFrameSeconds= 0.0f;

	// Always retry hardware first on the next open() - driver/GPU availability
	// can change between sessions (see eDecodeTier's own comment), so a
	// software-tier session shouldn't permanently downgrade future ones.
	m_decodeTier= eDecodeTier::hardware;

	// Tear down the whole CUDA-GL texture pipeline on close (ticket E3) rather than
	// trying to keep it alive across reopen - the GStreamer pipeline's own
	// GstCudaContext may not survive a close/reopen cycle intact (see project memory
	// on CUDA context corruption after a fault), so the safest contract is "fully
	// rebuilt on next successful decode", matching how m_impl->pipeline itself is
	// unconditionally torn down and rebuilt below.
	if (m_bTexturePipelineInitialized)
	{
		m_colorTexture.shutdown();

		m_bTexturePipelineInitialized= false;
		m_textureWidth= 0;
		m_textureHeight= 0;
	}

	// Independent of m_bTexturePipelineInitialized - ensureCudaContext() can
	// succeed (creating m_cudaContext) even if a later texture-pipeline init step
	// fails, so this must not be skipped just because the pipeline never finished
	// initializing.
	if (m_cudaContext != nullptr)
	{
		cuCtxDestroy(m_cudaContext);
		m_cudaContext= nullptr;
	}

	if (m_impl->pipeline != nullptr)
	{
		gst_element_set_state(m_impl->pipeline, GST_STATE_NULL);
	}

	if (m_impl->bus != nullptr)
	{
		gst_object_unref(m_impl->bus);
		m_impl->bus= nullptr;
	}

	if (m_impl->appsink != nullptr)
	{
		gst_object_unref(m_impl->appsink);
		m_impl->appsink= nullptr;
	}

	if (m_impl->depay != nullptr)
	{
		gst_object_unref(m_impl->depay);
		m_impl->depay= nullptr;
	}

	if (m_impl->pipeline != nullptr)
	{
		gst_object_unref(m_impl->pipeline);
		m_impl->pipeline= nullptr;
	}
}

// -- Zero-copy CUDA-GL texture pipeline (ticket E3) -----
bool MikanARKitVideoDevice::ensureCudaContext()
{
	if (m_cudaContext != nullptr)
	{
		// Re-assert as current every call (cheap no-op if already current) - see
		// this method's declaration comment for why this can't be a one-time thing.
		return checkCudaResult(cuCtxSetCurrent(m_cudaContext), "cuCtxSetCurrent");
	}

	if (!checkCudaResult(cuInit(0), "cuInit"))
		return false;

	// The context must live on the device the GL context runs on, since the decoded frame is
	// copied into GL textures registered with CUDA. updateColorTexture() calls this on the
	// GL context's thread.
	CUdevice device;
	if (!findCudaDeviceForCurrentGLContext(device))
	{
		if (!m_bLoggedNoCudaDeviceForGL)
		{
			MIKAN_LOG_ERROR("MikanARKitVideoDevice::ensureCudaContext")
				<< "OpenGL is running on a GPU with no CUDA device, so ARKit video frames cannot be shared "
				   "with it. On a hybrid-GPU laptop, set Mikan to the discrete GPU in Windows Graphics settings.";
			m_bLoggedNoCudaDeviceForGL= true;
		}
		return false;
	}

	// 4-arg cuCtxCreate (cuCtxCreate_v4, adds a CUctxCreateParams* param as of CUDA
	// 13.1) - nullptr for that param matches every other call site in this codebase
	// (matches every other cuCtxCreate call site in this codebase).
	if (!checkCudaResult(cuCtxCreate(&m_cudaContext, nullptr, 0, device), "cuCtxCreate"))
	{
		m_cudaContext= nullptr;
		return false;
	}

	// cuCtxCreate already makes the new context current on this thread, but
	// asserting it explicitly costs nothing and keeps this function's contract
	// simple ("returns true only if m_cudaContext is current on return").
	return checkCudaResult(cuCtxSetCurrent(m_cudaContext), "cuCtxSetCurrent");
}

bool MikanARKitVideoDevice::ensureTexturePipelineInitialized(int width, int height)
{
	if (m_bTexturePipelineInitialized && width == m_textureWidth && height == m_textureHeight)
		return true;

	if (m_bTexturePipelineInitialized)
	{
		// Dimensions changed mid-stream - shouldn't normally happen (ARKit's video
		// resolution is fixed for a session), but resize cleanly rather than
		// reuse/leak stale buffers if it does.
		m_colorTexture.shutdown();
		m_bTexturePipelineInitialized= false;
	}

	if (!m_colorTexture.init(width, height))
	{
		MIKAN_LOG_ERROR("MikanARKitVideoDevice::ensureTexturePipelineInitialized") << "Failed to init color texture";
		return false;
	}

	m_textureWidth= width;
	m_textureHeight= height;
	m_bTexturePipelineInitialized= true;
	return true;
}

void MikanARKitVideoDevice::updateColorTexture(GstBuffer* buffer, CUdeviceptr devicePtr, int width, int height)
{
	if (!ensureCudaContext())
		return;

	if (!ensureTexturePipelineInitialized(width, height))
		return;

	// NV12 plane layout (Y full-res, UV half-res interleaved) - stride/offset come
	// from the buffer's own GstVideoMeta rather than being assumed, since CUDA
	// memory allocations can be padded/aligned differently than a tightly-packed
	// buffer would be.
	GstVideoMeta* videoMeta= gst_buffer_get_video_meta(buffer);
	if (videoMeta == nullptr || videoMeta->n_planes < 2)
	{
		MIKAN_LOG_ERROR("MikanARKitVideoDevice::updateColorTexture")
			<< "Missing/invalid GstVideoMeta on decoded NV12 buffer";
		return;
	}

	const CUdeviceptr yPlane= devicePtr + videoMeta->offset[0];
	const int yStrideBytes= static_cast<int>(videoMeta->stride[0]);
	const CUdeviceptr uvPlane= devicePtr + videoMeta->offset[1];
	const int uvStrideBytes= static_cast<int>(videoMeta->stride[1]);

	// Copy the decoded NV12 planes straight into the registered luma/chroma GL
	// textures - no kernel launch, just two device-to-array cuMemcpy2D calls (see
	// CudaGLInterop.h). The actual color conversion happens later as a GLSL
	// shader pass on the Editor side.
	m_colorTexture.copyFromDevice(yPlane, yStrideBytes, uvPlane, uvStrideBytes);
}

uint32_t MikanARKitVideoDevice::getLumaTextureGlId() const
{
	IMkTexturePtr texture= m_colorTexture.getLumaTexture();
	return texture != nullptr ? texture->getGlTextureId() : 0;
}

uint32_t MikanARKitVideoDevice::getChromaTextureGlId() const
{
	IMkTexturePtr texture= m_colorTexture.getChromaTexture();
	return texture != nullptr ? texture->getGlTextureId() : 0;
}

// -- Video Settings -----
bool MikanARKitVideoDevice::isVideoSettingSupported(const eVideoSettingType) const { return false; }

bool MikanARKitVideoDevice::getVideoSettingConstraint(const eVideoSettingType, VideoSettingConstraint&) const
{
	return false;
}

void MikanARKitVideoDevice::setVideoSetting(const eVideoSettingType, int)
{
	// Not supported
}

int MikanARKitVideoDevice::getVideoSetting(const eVideoSettingType) const { return -1; }

// -- Video Streaming -----
eVideoStreamingStatus MikanARKitVideoDevice::startVideoStream()
{
	if (m_openState == eOpenState::open)
	{
		if (m_streamingStatus == eVideoStreamingStatus::stopped)
		{
			m_timeSinceLastFrameSeconds= 0.0f;
			gst_element_set_state(m_impl->pipeline, GST_STATE_PLAYING);
			m_streamingStatus= eVideoStreamingStatus::pendingStart;
		}

		return m_streamingStatus;
	}

	return eVideoStreamingStatus::failed;
}

eVideoStreamingStatus MikanARKitVideoDevice::getVideoStreamingStatus() const { return m_streamingStatus; }

void MikanARKitVideoDevice::stopVideoStream()
{
	if (m_openState == eOpenState::open && m_impl->pipeline != nullptr)
	{
		gst_element_set_state(m_impl->pipeline, GST_STATE_PAUSED);
	}

	m_streamingStatus= eVideoStreamingStatus::stopped;
	m_pendingStreamStartAfterOpen= false;
}

// -- Frame bundle dispatch -----
void MikanARKitVideoDevice::notifyFrameBundleReceived(const ARKitVideoFrameBundle& bundle)
{
	std::lock_guard<std::mutex> lock(m_listenersMutex);
	for (IARKitVideoDeviceListener* listener : m_listeners)
		listener->notifyFrameBundleReceived(bundle);
}

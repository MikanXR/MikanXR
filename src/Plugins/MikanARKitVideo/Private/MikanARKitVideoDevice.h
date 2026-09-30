#pragma once

#include "IARKitVideoDevice.h"
#include "Cuda/CudaGLInterop.h"

#include <future>
#include <mutex>
#include <set>
#include <string>

// Forward declaration matching GStreamer's own typedef (gst/gstbuffer.h) - kept
// opaque here so this header doesn't need to pull in gst.h, same rationale as
// GStreamerImpl below.
typedef struct _GstBuffer GstBuffer;

// Implements IARKitVideoDevice (ticket B7). Structurally mirrors
// MikanGStreamerVideoDevice's async open/close/update pattern: the video RTP
// receive pipeline (udpsrc/rtph264depay, on basePort+0) is built in
// openOnThread() and polled in update(). ARKitRTPHeaderExtension (ticket C3)
// extracts the frameSeq/captureTimestampUs (and, when the sender attaches it,
// the frame-coupled camera pose - see ARKitFrameSeqMeta's hasPose) carried by
// each RTP packet's header extension and attaches it to the decoded buffer as
// ARKitFrameSeqMeta; update()'s appsink pull site reads that meta and builds an
// ARKitVideoFrameBundle directly from it.
// (The separate pose UDP channel (basePort+2) and its ARKitPoseReceiver/
// ARKitFrameCorrelator plumbing were removed once pose started riding inside the
// video RTP stream's own header extension instead - see project history for the
// prior architecture. JBU depth/matte upsampling and the human-stencil pipeline
// were removed earlier still - too noisy in practice to be useful for
// compositing; pose tracking is the part that remains valuable.)
//
// Two-tier decode ("Phase 7"): openOnThread() tries an explicit-element
// hardware pipeline (nvh264dec, straight to CUDA device memory - tickets
// C2/C4) first; if gst_parse_launch fails synchronously to build it (no
// NVIDIA GPU/driver, or the nvcodec plugin isn't installed), it falls back to
// an explicit-element software pipeline (openh264dec, straight to packed BGR
// system memory) - see eDecodeTier. Both tiers share the same udpsrc/
// rtpjitterbuffer/rtph264depay/h264parse prefix and RTP-extension pose
// delivery; only the decode+output tail differs. The hardware tier maps the
// decoded buffer as CUDA memory (GST_MAP_CUDA) for the CUDA-GL color-texture
// pipeline; the software tier maps it as plain host memory (GST_MAP_READ) and
// hands the packed BGR bytes to ARKitVideoFrameBundle::videoData, which
// ARKitVideoSourceComponent's existing dormant CPU-buffer guard (writeVideoFrame)
// was already written in anticipation of. getDirectColorTexture() returning
// null vs. non-null (already VideoFrameDistortionView's existing GPU-direct
// discriminator) is what naturally routes between the two tiers downstream -
// no new code needed there. Constraint: openh264dec is 8-bit 4:2:0-only - if
// the sender ever encodes High 10/4:2:2, this fallback tier silently fails to
// decode (same "explicit element, fail synchronously at open()-time" behavior
// as the hardware tier failing, not a crash).
class MikanARKitVideoDevice : public IARKitVideoDevice
{
public:
	MikanARKitVideoDevice(class MikanARKitVideoDeviceManager* ownerDeviceManager,
						  const ARKitVideoConnectionSettings& connectionInfo);
	virtual ~MikanARKitVideoDevice();

	virtual void update(float deltaSeconds) override;

	// -- Device Listener
	virtual void addListener(IARKitVideoDeviceListener* listener) override;
	virtual void removeListener(IARKitVideoDeviceListener* listener) override;

	// -- Device Properties
	virtual const char* getDevicePath() const override;
	virtual const char* getFriendlyName() const override;

	// -- Device Activation
	virtual eVideoOpeningStatus getVideoOpeningStatus() const override;
	virtual eVideoOpeningStatus open() override;
	virtual void close() override;

	// -- Video Settings (not applicable over this transport, matching
	// MikanGStreamerVideoDevice's equivalent stubs)
	virtual bool isVideoSettingSupported(const eVideoSettingType property_type) const override;
	virtual bool getVideoSettingConstraint(const eVideoSettingType property_type,
										   VideoSettingConstraint& outConstraint) const override;
	virtual void setVideoSetting(const eVideoSettingType property_type, int desired_value) override;
	virtual int getVideoSetting(const eVideoSettingType property_type) const override;

	// -- Video Streaming
	virtual eVideoStreamingStatus startVideoStream() override;
	virtual eVideoStreamingStatus getVideoStreamingStatus() const override;
	virtual void stopVideoStream() override;

	// -- Zero-copy CUDA-GL texture access (ticket E3; NV12 planes as of "Phase 6")
	virtual uint32_t getLumaTextureGlId() const override;
	virtual uint32_t getChromaTextureGlId() const override;

protected:
	bool openOnThread();

	// Called from update()'s appsink-pull site once per successfully-decoded CUDA
	// frame (ticket E3). Resizes the CUDA-GL luma/chroma textures on dimension
	// change and copies the decoded NV12 planes into them - no CUDA kernel
	// involved anymore (see "Phase 6"): the actual NV12->RGBA color conversion is
	// now a GLSL shader pass on the Editor side (see ARKitVideoSourceComponent),
	// since this plugin has no GL context/shader-cache access of its own. Must run
	// on the GL-context-owning thread (same as update() itself already does - see
	// CudaGLInterop.h's threading note).
	void updateColorTexture(GstBuffer* buffer, CUdeviceptr devicePtr, int width, int height);
	bool ensureTexturePipelineInitialized(int width, int height);

	// Live-tested finding (ticket E3): no CUDA context is actually current on this
	// thread by the time update() runs cuGraphicsGLRegisterImage/kernel launches
	// here - GST_MAP_CUDA's own context handling (if any) doesn't leave one
	// current for callers afterward (confirmed via a live
	// CUDA_ERROR_INVALID_CONTEXT failure). So this class creates and owns its own
	// CUcontext instead, independent of nvcodec's internal GstCudaContext - safe
	// because CUDA's Unified Virtual Addressing (the default on 64-bit
	// platforms/modern CUDA) makes device pointers allocated under one context on a
	// device valid to dereference from another context on that same device, within
	// the same process - which is exactly what's needed to read a GST_MAP_CUDA
	// pointer from our own context. Lazily created once; cuCtxSetCurrent() is
	// re-asserted defensively at the start of every updateColorTexture() call
	// since anything else that ran on the main thread in between (including
	// GStreamer's own internal context push/pop during gst_buffer_map) could have
	// changed what's current.
	bool ensureCudaContext();

	// Fans an already-built ARKitVideoFrameBundle out to listeners. Called from
	// update()'s appsink-pull site, on the main/GL thread - unlike before pose
	// moved into the video RTP stream, this no longer fires from a separate
	// receiver worker thread, so listeners that assumed a background-thread call
	// (e.g. for locking purposes) should be re-examined.
	void notifyFrameBundleReceived(const ARKitVideoFrameBundle& bundle);

private:
	enum class eOpenState
	{
		closed,
		opening,
		open,
		failed
	};

	// Which pipeline tier openOnThread() actually built ("Phase 7") - determines
	// how update()'s appsink-pull site maps the decoded buffer (CUDA vs. host
	// memory) and which ARKitVideoFrameBundle fields it populates. Reset to
	// hardware on close() so a reopen always retries hardware first (driver/GPU
	// availability can change between sessions - e.g. app started under RDP,
	// or a GPU mode toggle - so a permanent downgrade would be wrong).
	enum class eDecodeTier
	{
		hardware,
		software
	};
	eDecodeTier m_decodeTier= eDecodeTier::hardware;

	class MikanARKitVideoDeviceManager* m_ownerDeviceManager;
	ARKitVideoConnectionSettings m_connectionInfo;
	std::string m_devicePath;

	eOpenState m_openState= eOpenState::closed;
	std::future<bool> m_openFuture;
	eVideoStreamingStatus m_streamingStatus= eVideoStreamingStatus::stopped;

	// Opaque GStreamer pipeline state (pipeline/appsink/bus). Defined in the .cpp so
	// this header stays free of GStreamer includes, matching
	// MikanGStreamerVideoDevice.h's m_impl pattern.
	struct GStreamerImpl* m_impl;

	static constexpr float k_streamTimeoutSeconds= 10.0f;
	float m_timeSinceLastFrameSeconds= 0.0f;
	bool m_pendingStreamStartAfterOpen= false;

	std::mutex m_listenersMutex;
	std::set<IARKitVideoDeviceListener*> m_listeners;

	bool m_bTexturePipelineInitialized= false;
	int m_textureWidth= 0;
	int m_textureHeight= 0;

	// This class's own CUDA context (see ensureCudaContext()'s comment for why) -
	// independent of whatever context nvcodec's pipeline owns internally.
	CUcontext m_cudaContext= nullptr;

	// ensureCudaContext() runs every frame, so a GL context with no CUDA device behind it
	// is reported once rather than per frame
	bool m_bLoggedNoCudaDeviceForGL= false;

	CudaGLColorTexture m_colorTexture;
};

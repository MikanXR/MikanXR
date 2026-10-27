#include "MikanGStreamerRtpVideoSender.h"
#include "Logger.h"

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <atomic>
#include <cctype>
#include <cstdlib>
#include <sstream>

// -- RtpSenderGStreamerImpl -----
struct RtpSenderGStreamerImpl
{
	// Matches the appsrc max-buffers in the pipeline string. A push made while the
	// queue holds this many buffers evicts the oldest one.
	static constexpr guint64 k_maxQueuedBuffers= 2;

	struct TierDesc
	{
		eRtpEncoderTier tier;
		const char* encoderElement;
		const char* rawFormat;
	};

	static constexpr TierDesc k_tiers[]= {
		{eRtpEncoderTier::nvenc, "nvcudah264enc", "NV12"},
		{eRtpEncoderTier::amf, "amfh264enc", "NV12"},
		{eRtpEncoderTier::openh264, "openh264enc", "I420"},
	};
	static constexpr int k_tierCount= static_cast<int>(sizeof(k_tiers) / sizeof(k_tiers[0]));

	GstElement* pipeline= nullptr;
	GstElement* appsrc= nullptr;
	GstBus* bus= nullptr;

	// Buffers that have left the encoder for the current tier, counted on the
	// pipeline's streaming thread. Zero means the encoder has not produced output
	// yet, which is where a negotiation failure shows up.
	std::atomic<uint64_t> encodedFrames{0};

	static bool isEncoderInstalled(const char* elementName)
	{
		GstElementFactory* factory= gst_element_factory_find(elementName);
		if (factory == nullptr)
			return false;

		gst_object_unref(factory);
		return true;
	}

	static std::string buildEncoderSegment(eRtpEncoderTier tier, const RtpVideoSenderSettings& settings)
	{
		std::ostringstream ss;

		switch (tier)
		{
		case eRtpEncoderTier::nvenc:
			ss << "nvcudah264enc name=enc preset=p1 tune=ultra-low-latency rate-control=cbr"
			   << " bitrate=" << settings.bitrateKbps << " gop-size=" << settings.gopFrames
			   << " repeat-sequence-header=true zero-reorder-delay=true b-frames=0";
			break;
		case eRtpEncoderTier::amf:
			ss << "amfh264enc name=enc usage=ultra-low-latency rate-control=cbr"
			   << " bitrate=" << settings.bitrateKbps << " gop-size=" << settings.gopFrames;
			break;
		case eRtpEncoderTier::openh264:
			// openh264enc takes bits per second rather than kbit
			ss << "openh264enc name=enc"
			   << " bitrate=" << static_cast<int64_t>(settings.bitrateKbps) * 1000
			   << " max-bitrate=" << static_cast<int64_t>(settings.bitrateKbps) * 1250
			   << " rate-control=bitrate gop-size=" << settings.gopFrames
			   << " complexity=low usage-type=camera enable-frame-skip=false";
			break;
		default:
			break;
		}

		return ss.str();
	}

	static std::string buildPipelineString(const TierDesc& tierDesc, const RtpVideoSenderSettings& settings)
	{
		std::ostringstream ss;

		// max-bytes=0 so max-buffers alone bounds the queue: the 200KB default is
		// smaller than a single RGBA frame and would leak every queued frame.
		ss << "appsrc name=src is-live=true format=time do-timestamp=true block=false"
		   << " max-buffers=" << k_maxQueuedBuffers << " max-bytes=0 leaky-type=downstream"
		   << " caps=\"video/x-raw,format=RGBA"
		   << ",width=" << settings.width << ",height=" << settings.height << ",framerate=" << settings.fpsNumerator
		   << "/" << settings.fpsDenominator << "\""
		   << " ! videoconvert ! video/x-raw,format=" << tierDesc.rawFormat << " ! "
		   << buildEncoderSegment(tierDesc.tier, settings) << " ! h264parse"
		   << " ! rtph264pay pt=96 mtu=1400 config-interval=-1 aggregate-mode=zero-latency"
		   << " ! udpsink host=" << settings.host << " port=" << settings.port << " sync=false async=false";

		return ss.str();
	}

	static GstPadProbeReturn encodedBufferProbe(GstPad*, GstPadProbeInfo*, gpointer userData)
	{
		auto* counter= static_cast<std::atomic<uint64_t>*>(userData);
		counter->fetch_add(1);

		return GST_PAD_PROBE_OK;
	}

	static std::string parseBusMessage(GstMessage* msg)
	{
		if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS)
			return "unexpected end of stream";

		GError* error= nullptr;
		gchar* debugInfo= nullptr;
		gst_message_parse_error(msg, &error, &debugInfo);

		std::ostringstream ss;
		ss << (error != nullptr ? error->message : "unknown error");
		if (debugInfo != nullptr)
		{
			ss << " (" << debugInfo << ")";
		}

		g_clear_error(&error);
		g_free(debugInfo);

		return ss.str();
	}
};

// -- MikanGStreamerRtpVideoSender -----
MikanGStreamerRtpVideoSender::MikanGStreamerRtpVideoSender()
	: m_impl(new RtpSenderGStreamerImpl())
{
}

MikanGStreamerRtpVideoSender::~MikanGStreamerRtpVideoSender()
{
	close();
	delete m_impl;
}

bool MikanGStreamerRtpVideoSender::open(const RtpVideoSenderSettings& settings)
{
	close();

	m_status= RtpVideoSenderStatus();

	std::string error;
	if (!validateSettings(settings, error))
	{
		m_status.lastError= error;
		MIKAN_LOG_ERROR("MikanGStreamerRtpVideoSender::open") << "Refused settings: " << error;
		return false;
	}

	m_settings= settings;

	if (!openFirstAvailableTier(0))
	{
		MIKAN_LOG_ERROR("MikanGStreamerRtpVideoSender::open")
			<< "No H.264 encoder tier started: " << m_status.lastError;
		return false;
	}

	m_bIsOpen= true;
	return true;
}

bool MikanGStreamerRtpVideoSender::pushFrameRGBA(const uint8_t* pixels, size_t byteCount)
{
	if (!m_bIsOpen || m_impl->appsrc == nullptr || pixels == nullptr)
		return false;

	const size_t expectedByteCount= static_cast<size_t>(m_settings.width) * m_settings.height * 4;
	if (byteCount != expectedByteCount)
		return false;

	// Only needed until the encoder proves it can negotiate, since a tier fallback
	// re-pushes this frame and fallback never happens once frames have flowed
	if (m_impl->encodedFrames.load() == 0)
	{
		m_lastFrame.assign(pixels, pixels + byteCount);
	}
	else if (!m_lastFrame.empty())
	{
		m_lastFrame.clear();
		m_lastFrame.shrink_to_fit();
	}

	return pushFrameInternal(pixels, byteCount);
}

void MikanGStreamerRtpVideoSender::update()
{
	if (!m_bIsOpen || m_impl->bus == nullptr)
		return;

	// Polled rather than using gst_bus_add_watch: a bus watch is dispatched by a GLib
	// main loop, and nothing in this process runs one, so a watch would never fire.
	GstMessage* msg=
		gst_bus_pop_filtered(m_impl->bus, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
	if (msg == nullptr)
		return;

	const std::string errorText= RtpSenderGStreamerImpl::parseBusMessage(msg);
	gst_message_unref(msg);

	const eRtpEncoderTier failedTier= m_status.tier;
	if (m_impl->encodedFrames.load() > 0)
	{
		// The encoder already produced output, so this is a mid-stream failure.
		// Changing encoders here would silently alter the stream the receiver is decoding.
		failAndClose(std::string(rtpEncoderTierName(failedTier)) + " encoder failed: " + errorText);
		return;
	}

	MIKAN_LOG_WARNING("MikanGStreamerRtpVideoSender::update")
		<< "Encoder tier " << rtpEncoderTierName(failedTier)
		<< " failed before producing output, trying the next tier: " << errorText;

	// The torn down pipeline owned the bus, so no further messages are read from it
	m_status.lastError= std::string(rtpEncoderTierName(failedTier)) + " encoder failed: " + errorText;
	teardownPipeline();

	if (!openFirstAvailableTier(m_tierIndex + 1))
	{
		failAndClose(m_status.lastError);
		return;
	}

	if (!m_lastFrame.empty())
	{
		pushFrameInternal(m_lastFrame.data(), m_lastFrame.size());
	}
}

void MikanGStreamerRtpVideoSender::close()
{
	teardownPipeline();

	m_bIsOpen= false;
	m_tierIndex= -1;
	m_status.tier= eRtpEncoderTier::none;
	m_lastFrame.clear();
	m_lastFrame.shrink_to_fit();
}

bool MikanGStreamerRtpVideoSender::getIsOpen() const { return m_bIsOpen; }

const RtpVideoSenderStatus& MikanGStreamerRtpVideoSender::getStatus() const { return m_status; }

bool MikanGStreamerRtpVideoSender::validateSettings(const RtpVideoSenderSettings& settings, std::string& outError)
{
	if (settings.width < 160 || settings.height < 64)
	{
		outError= "frame size below 160x64";
		return false;
	}

	if ((settings.width % 2) != 0 || (settings.height % 2) != 0)
	{
		outError= "frame size must be even";
		return false;
	}

	if (settings.fpsNumerator <= 0 || settings.fpsDenominator <= 0)
	{
		outError= "invalid frame rate";
		return false;
	}

	if (settings.bitrateKbps <= 0 || settings.gopFrames <= 0)
	{
		outError= "invalid bitrate or GOP length";
		return false;
	}

	if (settings.host.empty() || settings.port == 0)
	{
		outError= "missing destination host or port";
		return false;
	}

	// The host is spliced into the pipeline description, so only hostname and
	// address characters are allowed through
	for (char c : settings.host)
	{
		const bool bAllowed= std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == ':';
		if (!bAllowed)
		{
			outError= "invalid destination host";
			return false;
		}
	}

	// IPv4 multicast is 224.0.0.0/4, first octet 224 through 239
	if (std::isdigit(static_cast<unsigned char>(settings.host[0])))
	{
		char* end= nullptr;
		const long firstOctet= std::strtol(settings.host.c_str(), &end, 10);
		if (end != nullptr && *end == '.' && firstOctet >= 224 && firstOctet <= 239)
		{
			outError= "multicast destination refused";
			return false;
		}
	}

	return true;
}

bool MikanGStreamerRtpVideoSender::openFirstAvailableTier(int firstTierIndex)
{
	bool bAnyTierInstalled= false;

	for (int tierIndex= firstTierIndex; tierIndex < RtpSenderGStreamerImpl::k_tierCount; ++tierIndex)
	{
		const RtpSenderGStreamerImpl::TierDesc& tierDesc= RtpSenderGStreamerImpl::k_tiers[tierIndex];
		if (!RtpSenderGStreamerImpl::isEncoderInstalled(tierDesc.encoderElement))
		{
			MIKAN_LOG_INFO("MikanGStreamerRtpVideoSender::openFirstAvailableTier")
				<< "Encoder element " << tierDesc.encoderElement << " not installed, skipping "
				<< rtpEncoderTierName(tierDesc.tier) << " tier";
			continue;
		}

		bAnyTierInstalled= true;
		if (openTier(tierIndex))
			return true;
	}

	if (!bAnyTierInstalled && m_status.lastError.empty())
	{
		m_status.lastError= "no H.264 encoder available";
	}

	return false;
}

bool MikanGStreamerRtpVideoSender::openTier(int tierIndex)
{
	const RtpSenderGStreamerImpl::TierDesc& tierDesc= RtpSenderGStreamerImpl::k_tiers[tierIndex];
	const char* tierName= rtpEncoderTierName(tierDesc.tier);
	const std::string pipelineString= RtpSenderGStreamerImpl::buildPipelineString(tierDesc, m_settings);

	MIKAN_LOG_INFO("MikanGStreamerRtpVideoSender::openTier") << "Opening " << tierName << " tier: " << pipelineString;

	// gst_parse_launch can return a pipeline and an error together (an unknown
	// property, say), so any error fails the tier
	GError* error= nullptr;
	GstElement* pipeline= gst_parse_launch(pipelineString.c_str(), &error);
	if (pipeline == nullptr || error != nullptr)
	{
		m_status.lastError= std::string(tierName)
							+ " pipeline failed to build: " + (error != nullptr ? error->message : "unknown error");
		MIKAN_LOG_WARNING("MikanGStreamerRtpVideoSender::openTier") << m_status.lastError;

		g_clear_error(&error);
		if (pipeline != nullptr)
		{
			gst_object_unref(pipeline);
		}
		return false;
	}

	m_impl->pipeline= pipeline;
	m_impl->appsrc= gst_bin_get_by_name(GST_BIN(pipeline), "src");
	m_impl->bus= gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	m_impl->encodedFrames.store(0);

	GstElement* encoder= gst_bin_get_by_name(GST_BIN(pipeline), "enc");
	if (encoder != nullptr)
	{
		GstPad* encoderSrcPad= gst_element_get_static_pad(encoder, "src");
		if (encoderSrcPad != nullptr)
		{
			gst_pad_add_probe(encoderSrcPad, GST_PAD_PROBE_TYPE_BUFFER, &RtpSenderGStreamerImpl::encodedBufferProbe,
							  &m_impl->encodedFrames, nullptr);
			gst_object_unref(encoderSrcPad);
		}
		gst_object_unref(encoder);
	}

	if (m_impl->appsrc == nullptr || m_impl->bus == nullptr)
	{
		m_status.lastError= std::string(tierName) + " pipeline is missing its appsrc or bus";
		MIKAN_LOG_WARNING("MikanGStreamerRtpVideoSender::openTier") << m_status.lastError;
		teardownPipeline();
		return false;
	}

	if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
	{
		std::string reason= "state change failed";
		if (GstMessage* msg= gst_bus_pop_filtered(m_impl->bus, GST_MESSAGE_ERROR))
		{
			reason= RtpSenderGStreamerImpl::parseBusMessage(msg);
			gst_message_unref(msg);
		}

		m_status.lastError= std::string(tierName) + " pipeline failed to start: " + reason;
		MIKAN_LOG_WARNING("MikanGStreamerRtpVideoSender::openTier") << m_status.lastError;
		teardownPipeline();
		return false;
	}

	m_tierIndex= tierIndex;
	m_status.tier= tierDesc.tier;
	m_status.lastError.clear();

	MIKAN_LOG_INFO("MikanGStreamerRtpVideoSender::openTier")
		<< "Sending " << m_settings.width << "x" << m_settings.height << " H.264 RTP to " << m_settings.host << ":"
		<< m_settings.port << " with the " << tierName << " encoder";

	return true;
}

bool MikanGStreamerRtpVideoSender::pushFrameInternal(const uint8_t* pixels, size_t byteCount)
{
	GstAppSrc* appsrc= GST_APP_SRC(m_impl->appsrc);

	// With leaky-type=downstream a push into a full queue evicts the oldest frame
	bool bCountedDrop= false;
	if (gst_app_src_get_current_level_buffers(appsrc) >= RtpSenderGStreamerImpl::k_maxQueuedBuffers)
	{
		m_status.framesDropped++;
		bCountedDrop= true;
	}

	GstBuffer* buffer= gst_buffer_new_allocate(nullptr, byteCount, nullptr);
	if (buffer == nullptr)
	{
		if (!bCountedDrop)
		{
			m_status.framesDropped++;
		}
		return false;
	}

	gst_buffer_fill(buffer, 0, pixels, byteCount);

	// Takes ownership of the buffer
	if (gst_app_src_push_buffer(appsrc, buffer) != GST_FLOW_OK)
	{
		if (!bCountedDrop)
		{
			m_status.framesDropped++;
		}
		return false;
	}

	m_status.framesPushed++;
	return true;
}

void MikanGStreamerRtpVideoSender::teardownPipeline()
{
	// Going to NULL joins the streaming threads, so the encoder probe has stopped
	// touching encodedFrames before anything is released
	if (m_impl->pipeline != nullptr)
	{
		gst_element_set_state(m_impl->pipeline, GST_STATE_NULL);
	}

	if (m_impl->bus != nullptr)
	{
		gst_object_unref(m_impl->bus);
		m_impl->bus= nullptr;
	}

	if (m_impl->appsrc != nullptr)
	{
		gst_object_unref(m_impl->appsrc);
		m_impl->appsrc= nullptr;
	}

	if (m_impl->pipeline != nullptr)
	{
		gst_object_unref(m_impl->pipeline);
		m_impl->pipeline= nullptr;
	}
}

void MikanGStreamerRtpVideoSender::failAndClose(const std::string& error)
{
	m_status.lastError= error;
	MIKAN_LOG_ERROR("MikanGStreamerRtpVideoSender") << "Closing RTP sender: " << error;

	close();
}

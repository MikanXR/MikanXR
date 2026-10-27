#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

struct RtpVideoSenderSettings
{
	int width= 0;
	int height= 0;
	int fpsNumerator= 30;
	int fpsDenominator= 1;
	int bitrateKbps= 4000;
	int gopFrames= 30;
	std::string host;
	uint16_t port= 5004;
};

// The H.264 encoder backing the sender, in the order open() tries them
enum class eRtpEncoderTier
{
	none,
	nvenc,
	amf,
	openh264
};

inline const char* rtpEncoderTierName(eRtpEncoderTier tier)
{
	switch (tier)
	{
	case eRtpEncoderTier::nvenc:
		return "nvenc";
	case eRtpEncoderTier::amf:
		return "amf";
	case eRtpEncoderTier::openh264:
		return "openh264";
	default:
		return "none";
	}
}

struct RtpVideoSenderStatus
{
	eRtpEncoderTier tier= eRtpEncoderTier::none;
	uint64_t framesPushed= 0;
	uint64_t framesDropped= 0;
	std::string lastError;
};

// Encodes RGBA frames to H.264 and sends them as RTP over UDP.
// Every method is called from the editor main thread.
class IRtpVideoSender
{
public:
	virtual ~IRtpVideoSender()= default;

	// Builds the first available encoder tier's pipeline and starts it
	virtual bool open(const RtpVideoSenderSettings& settings)= 0;

	// byteCount must equal width*height*4. Returns false if not open or on a size mismatch.
	virtual bool pushFrameRGBA(const uint8_t* pixels, size_t byteCount)= 0;

	// Drains pipeline messages and performs encoder tier fallback. Call once per editor tick.
	virtual void update()= 0;

	virtual void close()= 0;

	virtual bool getIsOpen() const= 0;
	virtual const RtpVideoSenderStatus& getStatus() const= 0;
};
using IRtpVideoSenderPtr= std::shared_ptr<IRtpVideoSender>;

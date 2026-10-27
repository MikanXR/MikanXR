#pragma once

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

// One receiver of the RTP stream
struct RtpDestination
{
	std::string host;
	uint16_t port= 0;

	bool operator==(const RtpDestination& other) const { return host == other.host && port == other.port; }
	bool operator!=(const RtpDestination& other) const { return !(*this == other); }
};

// A hostname or an IPv4 or IPv6 literal: letters, digits, '.', '-' and ':' only.
// No IPv4 multicast group, since Wi-Fi multicast is the wrong transport for a phone.
inline bool isValidRtpDestinationHost(const std::string& host)
{
	if (host.empty())
		return false;

	for (char c : host)
	{
		const bool bAllowed= std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == ':';
		if (!bAllowed)
			return false;
	}

	// IPv4 multicast is 224.0.0.0/4, first octet 224 through 239
	if (std::isdigit(static_cast<unsigned char>(host[0])))
	{
		char* end= nullptr;
		const long firstOctet= std::strtol(host.c_str(), &end, 10);
		if (end != nullptr && *end == '.' && firstOctet >= 224 && firstOctet <= 239)
			return false;
	}

	return true;
}

struct RtpVideoSenderSettings
{
	int width= 0;
	int height= 0;
	int fpsNumerator= 30;
	int fpsDenominator= 1;
	int bitrateKbps= 4000;
	int gopFrames= 30;
	// Every receiver of the one encode. open() needs at least one.
	std::vector<RtpDestination> destinations;
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

	// Replaces the receivers of a running stream without restarting the encoder. An empty
	// list leaves the stream with no receivers. Returns false if not open or when any
	// destination is invalid, in which case nothing changes.
	virtual bool setDestinations(const std::vector<RtpDestination>& destinations)= 0;

	// Drains pipeline messages and performs encoder tier fallback. Call once per editor tick.
	virtual void update()= 0;

	virtual void close()= 0;

	virtual bool getIsOpen() const= 0;
	virtual const RtpVideoSenderStatus& getStatus() const= 0;
};
using IRtpVideoSenderPtr= std::shared_ptr<IRtpVideoSender>;

#pragma once

#include "IRtpVideoSender.h"

#include <string>
#include <vector>

// Sends RGBA frames as H.264 RTP over UDP through an appsrc-fed GStreamer pipeline.
// open() builds the first encoder tier whose element is installed and that starts
// cleanly. An encoder can also fail later, at caps negotiation on the first buffer,
// so update() falls through to the next tier when an error arrives before any
// encoded frame has left the encoder. An error after frames have flowed closes the
// sender rather than changing encoders mid-stream.
class MikanGStreamerRtpVideoSender : public IRtpVideoSender
{
public:
	MikanGStreamerRtpVideoSender();
	virtual ~MikanGStreamerRtpVideoSender();

	virtual bool open(const RtpVideoSenderSettings& settings) override;
	virtual bool pushFrameRGBA(const uint8_t* pixels, size_t byteCount) override;
	virtual void update() override;
	virtual void close() override;

	virtual bool getIsOpen() const override;
	virtual const RtpVideoSenderStatus& getStatus() const override;

protected:
	static bool validateSettings(const RtpVideoSenderSettings& settings, std::string& outError);

	// Tries each tier from firstTierIndex onward until one starts. Returns false if none did.
	bool openFirstAvailableTier(int firstTierIndex);
	bool openTier(int tierIndex);
	bool pushFrameInternal(const uint8_t* pixels, size_t byteCount);
	void teardownPipeline();
	void failAndClose(const std::string& error);

private:
	// Opaque GStreamer pipeline state, defined in the .cpp so this header stays free
	// of GStreamer includes
	struct RtpSenderGStreamerImpl* m_impl;

	RtpVideoSenderSettings m_settings;
	RtpVideoSenderStatus m_status;
	bool m_bIsOpen= false;
	int m_tierIndex= -1;

	// The most recent frame, re-pushed into the next tier when a tier fails at negotiation
	std::vector<uint8_t> m_lastFrame;
};

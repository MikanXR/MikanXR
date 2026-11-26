#include "RtpOutputStream.h"
#include "IMkTexture.h"
#include "NetworkVideoSourceSystem.h"
#include "OutputComponent.h"
#include "RtpFrameSender.h"

#include <cmath>

RtpOutputStream::RtpOutputStream(IMkGraphicsContext* graphicsContext, NetworkVideoSourceSystemPtr networkVideoSystem)
	: m_graphicsContext(graphicsContext)
	, m_networkVideoSystem(networkVideoSystem)
{
}

RtpOutputStream::~RtpOutputStream() { close(); }

void RtpOutputStream::writeFrame(const OutputFrame& frame, const Config& config)
{
	if (!frame.texture)
		return;

	m_lastSourceWidth= frame.texture->getTextureWidth();
	m_lastSourceHeight= frame.texture->getTextureHeight();
	m_lastFps= frame.frameRate;
	m_lastRequestedHeight= config.height;

	if (config.destinations.empty())
	{
		close();
		return;
	}

	// The encoder's size, bitrate and rate control are fixed when it opens, so a
	// change to any of them rebuilds the sender. That also clears a failed one.
	if (m_frameSender && (config.needsRebuildFrom(m_openedConfig) || std::fabs(frame.frameRate - m_openedFps) > 0.5f))
	{
		close();
	}

	// A sender that refused its settings or failed mid-stream is left as it is,
	// showing the error, until the settings change and it is rebuilt
	const bool bHasFailed= m_frameSender && m_frameSender->getHasFailed();

	if (!getIsStreaming())
	{
		if (bHasFailed || !start(frame, config))
			return;
	}
	else if (config.destinations != m_frameSender->getDestinations())
	{
		m_frameSender->setDestinations(config.destinations);
	}

	m_frameSender->writeFrame(frame.texture);
}

bool RtpOutputStream::start(const OutputFrame& frame, const Config& config)
{
	if (!m_frameSender)
	{
		m_frameSender= std::make_shared<RtpFrameSender>(m_graphicsContext);
	}
	m_openedConfig= config;
	m_openedFps= frame.frameRate;

	// The sender comes from the GStreamer plugin, which the network video system owns
	NetworkVideoSourceSystemPtr networkVideoSystem= m_networkVideoSystem.lock();
	if (!networkVideoSystem)
	{
		m_frameSender->setUnavailable("no network video system");
		return false;
	}

	std::string unavailableReason;
	if (!networkVideoSystem->isRtpVideoSenderAvailable(unavailableReason))
	{
		m_frameSender->setUnavailable(unavailableReason);
		return false;
	}

	IRtpVideoSenderPtr sender= networkVideoSystem->createRtpVideoSender();
	if (!sender)
	{
		m_frameSender->setUnavailable("the GStreamer plugin provided no RTP sender");
		return false;
	}

	RtpFrameSender::Settings settings;
	settings.destinations= config.destinations;
	settings.height= config.height;
	settings.bitrateKbps= config.bitrateKbps;
	settings.fps= frame.frameRate;

	return m_frameSender->start(sender, settings, frame.texture->getTextureWidth(), frame.texture->getTextureHeight());
}

void RtpOutputStream::update(float deltaSeconds)
{
	if (m_frameSender)
	{
		m_frameSender->update(deltaSeconds);
	}
}

void RtpOutputStream::close() { m_frameSender= nullptr; }

bool RtpOutputStream::getIsStreaming() const { return m_frameSender && m_frameSender->getIsStreaming(); }

std::string RtpOutputStream::getStatusText() const
{
	return m_frameSender ? m_frameSender->getStatusText() : std::string("idle");
}

bool RtpOutputStream::getStreamFormat(int& outWidth, int& outHeight, float& outFps) const
{
	if (m_lastSourceWidth <= 0 || m_lastSourceHeight <= 0)
		return false;

	RtpFrameSender::computeOutputSize(m_lastSourceWidth, m_lastSourceHeight, m_lastRequestedHeight, outWidth,
									  outHeight);
	outFps= m_lastFps;

	return outWidth > 0 && outHeight > 0;
}

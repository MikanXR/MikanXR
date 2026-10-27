#include "CompositorRtpOutput.h"
#include "IMkFrameBuffer.h"
#include "IMkGraphicsContext.h"
#include "IMkState.h"
#include "IMkTexture.h"
#include "IMkTriangulatedMesh.h"
#include "Logger.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkScopedObjectBinding.h"
#include "MkShaderConstants.h"
#include "MkStateStack.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

// The smallest frame the H.264 encoders accept
static constexpr int k_minOutputWidth= 160;
static constexpr int k_minOutputHeight= 64;
static constexpr int k_maxOutputHeight= 4096;

CompositorRtpOutput::CompositorRtpOutput(IMkGraphicsContext* graphicsContext)
	: m_graphicsContext(graphicsContext)
{
	refreshStatusText();
}

CompositorRtpOutput::~CompositorRtpOutput() { stop(); }

void CompositorRtpOutput::computeOutputSize(int sourceWidth, int sourceHeight, int requestedHeight, int& outWidth,
											int& outHeight)
{
	if (sourceWidth <= 0 || sourceHeight <= 0)
	{
		outWidth= 0;
		outHeight= 0;
		return;
	}

	int height= requestedHeight > 0 ? requestedHeight : sourceHeight;
	height= std::clamp(height, k_minOutputHeight, k_maxOutputHeight);

	int width= static_cast<int>(std::lround(static_cast<double>(height) * sourceWidth / sourceHeight));
	width= std::max(width, k_minOutputWidth);

	// The encoders take even dimensions only
	outWidth= width & ~1;
	outHeight= height & ~1;
}

bool CompositorRtpOutput::start(IRtpVideoSenderPtr sender, const Settings& settings, int sourceWidth, int sourceHeight)
{
	stop();

	m_sender= sender;
	m_settings= settings;
	m_bHasFailed= false;
	m_unavailableReason.clear();

	bool bOpened= false;
	if (m_sender)
	{
		bOpened= openSender(sourceWidth, sourceHeight);
	}
	else
	{
		m_unavailableReason= "no RTP sender";
	}

	refreshStatusText();

	return bOpened;
}

void CompositorRtpOutput::stop()
{
	if (m_sender)
	{
		m_sender->close();
		m_sender= nullptr;
	}
	m_bSenderWasOpen= false;

	m_frameBuffer= nullptr;
	m_quadMesh= nullptr;
	m_readbackBuffer.clear();
}

bool CompositorRtpOutput::getIsStreaming() const { return m_sender && m_sender->getIsOpen(); }

void CompositorRtpOutput::setUnavailable(const std::string& reason)
{
	if (m_unavailableReason != reason)
	{
		m_unavailableReason= reason;
		MIKAN_LOG_WARNING("CompositorRtpOutput::setUnavailable") << "RTP output unavailable: " << reason;
		refreshStatusText();
	}
}

bool CompositorRtpOutput::openSender(int sourceWidth, int sourceHeight)
{
	m_sourceWidth= sourceWidth;
	m_sourceHeight= sourceHeight;
	computeOutputSize(sourceWidth, sourceHeight, m_settings.height, m_outputWidth, m_outputHeight);

	if (!ensureFrameBuffer(m_outputWidth, m_outputHeight))
	{
		m_bHasFailed= true;
		m_unavailableReason= "output framebuffer failed";
		return false;
	}

	RtpVideoSenderSettings senderSettings;
	senderSettings.width= m_outputWidth;
	senderSettings.height= m_outputHeight;
	senderSettings.bitrateKbps= m_settings.bitrateKbps;
	senderSettings.host= m_settings.host;
	senderSettings.port= static_cast<uint16_t>(std::clamp(m_settings.port, 0, 65535));

	// The frame rate as a fraction: whole rates as N/1, the broadcast rates as N/1001
	// (29.97 is 30000/1001), which is the form the encoders' rate control expects
	const float fps= m_settings.fps > 0.f ? m_settings.fps : 30.f;
	if (std::fabs(fps - std::round(fps)) < 0.01f)
	{
		senderSettings.fpsNumerator= static_cast<int>(std::lround(fps));
		senderSettings.fpsDenominator= 1;
	}
	else
	{
		senderSettings.fpsNumerator= static_cast<int>(std::lround(fps * 1001.f));
		senderSettings.fpsDenominator= 1001;
	}
	// One second of frames between keyframes
	senderSettings.gopFrames= std::max(1, static_cast<int>(std::lround(fps)));

	if (!m_sender->open(senderSettings))
	{
		m_bHasFailed= true;
		return false;
	}

	m_bSenderWasOpen= true;
	m_fpsWindowSeconds= 0.f;
	m_fpsWindowStartPushed= m_sender->getStatus().framesPushed;
	m_measuredFps= 0.f;

	return true;
}

bool CompositorRtpOutput::ensureFrameBuffer(int width, int height)
{
	if (width <= 0 || height <= 0)
		return false;

	if (!m_frameBuffer)
	{
		// 8-bit RGBA, whose color texture reads back through a double pixel buffer object
		m_frameBuffer= createMkFrameBuffer("Compositor RTP Output Frame Buffer");
		m_frameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
		m_frameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::RGBA);
	}

	// Does nothing when the size is unchanged, otherwise invalidates the resources
	m_frameBuffer->setSize(width, height);
	if (!m_frameBuffer->isValid() && !m_frameBuffer->createResources())
	{
		return false;
	}

	if (!m_quadMesh)
	{
		// Unflipped: the composited texture already reads back top row first
		m_quadMesh= createFullscreenQuadMesh(m_graphicsContext, false);
	}

	m_readbackBuffer.resize(static_cast<size_t>(width) * height * 4);

	return true;
}

void CompositorRtpOutput::writeFrame(IMkTexturePtr compositedTexture)
{
	if (!getIsStreaming() || !compositedTexture || !m_frameBuffer || !m_quadMesh)
		return;

	// The composited frame follows the video source size, so a change reopens the
	// sender with matching caps
	const int sourceWidth= compositedTexture->getTextureWidth();
	const int sourceHeight= compositedTexture->getTextureHeight();
	if (sourceWidth != m_sourceWidth || sourceHeight != m_sourceHeight)
	{
		m_sender->close();
		if (!openSender(sourceWidth, sourceHeight))
		{
			refreshStatusText();
			return;
		}
	}

	// Scale the composite into the output framebuffer
	{
		MkScopedObjectBinding framebufferBinding(m_graphicsContext->getMkStateStack().getCurrentState(),
												 "Compositor RTP Output Framebuffer Scope", m_frameBuffer);
		if (!framebufferBinding)
			return;

		framebufferBinding.getMkState()->disableFlag(eMkStateFlagType::depthTest);

		MkMaterialInstancePtr materialInstance= m_quadMesh->getMaterialInstance();
		MkMaterialConstPtr material= materialInstance->getMaterial();
		if (auto materialBinding= material->bindMaterial())
		{
			materialInstance->setTextureBySemantic(eUniformSemantic::rgbTexture, compositedTexture);

			if (auto materialInstanceBinding= materialInstance->bindMaterialInstance(materialBinding))
			{
				m_quadMesh->drawElements();
			}
		}
	}

	// The pixels handed back are the previous frame's, and the first call after an
	// open has none, which keeps the readback from waiting on the GPU
	IMkTexturePtr colorTexture= m_frameBuffer->getColorTexture();
	if (colorTexture && colorTexture->copyTextureIntoBuffer(m_readbackBuffer.data(), m_readbackBuffer.size()))
	{
		m_sender->pushFrameRGBA(m_readbackBuffer.data(), m_readbackBuffer.size());
	}
}

void CompositorRtpOutput::update(float deltaSeconds)
{
	if (m_sender)
	{
		m_sender->update();

		// The sender closes itself after an encoder error, which is final until the
		// settings change
		if (m_bSenderWasOpen && !m_sender->getIsOpen())
		{
			m_bSenderWasOpen= false;
			m_bHasFailed= true;
		}

		m_fpsWindowSeconds+= deltaSeconds;
		if (m_fpsWindowSeconds >= 1.f)
		{
			const uint64_t framesPushed= m_sender->getStatus().framesPushed;
			m_measuredFps= static_cast<float>(framesPushed - m_fpsWindowStartPushed) / m_fpsWindowSeconds;
			m_fpsWindowStartPushed= framesPushed;
			m_fpsWindowSeconds= 0.f;
		}
	}

	refreshStatusText();
}

void CompositorRtpOutput::refreshStatusText()
{
	std::ostringstream ss;

	if (!m_sender)
	{
		ss << (m_unavailableReason.empty() ? "starting" : m_unavailableReason);
	}
	else if (m_sender->getIsOpen())
	{
		const RtpVideoSenderStatus& status= m_sender->getStatus();
		ss << rtpEncoderTierName(status.tier) << " " << m_outputWidth << "x" << m_outputHeight << " " << std::fixed
		   << std::setprecision(1) << m_measuredFps << " fps sent " << status.framesPushed << " dropped "
		   << status.framesDropped;
	}
	else
	{
		const std::string& lastError= m_sender->getStatus().lastError;
		ss << (lastError.empty() ? "stopped" : lastError);
	}

	m_statusText= ss.str();
}

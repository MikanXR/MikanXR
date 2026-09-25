#include "SharedTextureReader.h"
#include "SharedTextureUtility.h"
#include "IMkTexture.h"
#include "Logger.h"
#include "MikanCoreCAPI.h"
#include "SpoutLibrary.h"

#include <easy/profiler.h>

// Map the descriptor's color/shadow buffer type to the GL internal texture format used for
// the receiving texture. RGBA16F carries HDR half-float values; everything else stays 8-bit RGBA.
static GLint getReceiveColorTextureFormat(MikanColorBufferType colorBufferType)
{
	return (colorBufferType == MikanColorBuffer_RGBA16F) ? (GLint)MK_RGBA16F : GL_RGBA;
}

static GLint getReceiveShadowTextureFormat(MikanShadowBufferType shadowBufferType)
{
	return (shadowBufferType == MikanShadowBuffer_RGBA16F) ? (GLint)MK_RGBA16F : GL_RGBA;
}

class SpoutTextureReader
{
public:
	SpoutTextureReader(SharedTextureReadAccessor* parentAccessor)
		: m_parentAccessor(parentAccessor)
		, m_spoutColorFrame(nullptr)
		, m_spoutDepthFrame(nullptr)
		, m_spoutShadowFrame(nullptr)
	{
	}

	virtual ~SpoutTextureReader() { dispose(); }

	bool init(const MikanRenderTargetDescriptor* descriptor)
	{
		dispose();

		const std::string colorSenderName= m_parentAccessor->getColorSenderName();

		m_spoutColorFrame= GetSpout();
		if (m_spoutColorFrame != nullptr)
		{
			m_spoutColorFrame->SetReceiverName(colorSenderName.c_str());
		}
		else
		{
			MIKAN_LOG_ERROR("SpoutTextureReader") << "Failed to open spout for sender: " << colorSenderName;
			return false;
		}

		if (descriptor->depth_buffer_type != MikanDepthBuffer_NODEPTH)
		{
			const std::string depthSenderName= m_parentAccessor->getDepthSenderName();

			m_spoutDepthFrame= GetSpout();
			if (m_spoutDepthFrame != nullptr)
			{
				m_spoutDepthFrame->SetReceiverName(depthSenderName.c_str());
			}
			else
			{
				MIKAN_LOG_ERROR("SpoutTextureReader") << "Failed to open spout for sender: " << depthSenderName;
				return false;
			}
		}

		if (descriptor->shadow_buffer_type != MikanShadowBuffer_NOSHADOW)
		{
			const std::string shadowSenderName= m_parentAccessor->getShadowSenderName();

			m_spoutShadowFrame= GetSpout();
			if (m_spoutShadowFrame != nullptr)
			{
				m_spoutShadowFrame->SetReceiverName(shadowSenderName.c_str());
			}
			else
			{
				MIKAN_LOG_ERROR("SpoutTextureReader") << "Failed to open spout for sender: " << shadowSenderName;
				return false;
			}
		}

		return true;
	}

	void dispose()
	{
		if (m_spoutColorFrame != nullptr)
		{
			m_spoutColorFrame->Release();
			m_spoutColorFrame= nullptr;
		}

		if (m_spoutDepthFrame != nullptr)
		{
			m_spoutDepthFrame->Release();
			m_spoutDepthFrame= nullptr;
		}

		if (m_spoutShadowFrame != nullptr)
		{
			m_spoutShadowFrame->Release();
			m_spoutShadowFrame= nullptr;
		}
	}

	// Whether a receiving texture has to be rebuilt before the next receive.
	//
	// IsUpdated() reports a sender change exactly once and is cleared by reading it, so on
	// its own it only ever fixes up whichever ring slot happened to be the pending write
	// that frame. Every other slot keeps whatever size it was allocated with, and Spout
	// writes the sender's image into the corner of an oversized texture rather than
	// failing, which surfaces as a composite artifact on the frames those slots serve.
	// Comparing the sizes as well lets every slot converge within one cycle of the ring.
	bool needsTextureRebuild(const std::string& senderName, SPOUTLIBRARY* spoutFrame, IMkTexturePtr texture,
							 int& inout_resizeCount)
	{
		const bool bSenderUpdated= spoutFrame->IsUpdated();

		const int senderWidth= (int)spoutFrame->GetSenderWidth();
		const int senderHeight= (int)spoutFrame->GetSenderHeight();
		if (senderWidth <= 0 || senderHeight <= 0)
			return bSenderUpdated;

		const int textureWidth= (int)texture->getTextureWidth();
		const int textureHeight= (int)texture->getTextureHeight();
		const bool bSizeDiffers= textureWidth != senderWidth || textureHeight != senderHeight;

		if (bSizeDiffers)
		{
			inout_resizeCount++;
			if (inout_resizeCount == 1 || (inout_resizeCount % 100) == 0)
			{
				MIKAN_LOG_INFO("SpoutTextureReader::readRenderTargetTexture")
					<< senderName << " receiving texture was " << textureWidth << "x" << textureHeight
					<< ", resizing to the sender's " << senderWidth << "x" << senderHeight << " (" << inout_resizeCount
					<< " so far)";
			}
		}

		return bSenderUpdated || bSizeDiffers;
	}

	bool readRenderTargetTexture()
	{
		bool bSuccess= false;

		IMkTexturePtr colorTexture= m_parentAccessor->getColorTexture();
		if (colorTexture != nullptr)
		{
			EASY_BLOCK("receive color texture");

			if (needsTextureRebuild(m_parentAccessor->getColorSenderName(), m_spoutColorFrame, colorTexture,
									m_colorResizeCount))
			{
				const GLint colorTexFormat=
					getReceiveColorTextureFormat(m_parentAccessor->getRenderTargetDescriptor().color_buffer_type);

				colorTexture->disposeTexture();
				colorTexture->setSize(m_spoutColorFrame->GetSenderWidth(), m_spoutColorFrame->GetSenderHeight());
				colorTexture->setTextureFormat(colorTexFormat);
				colorTexture->setBufferFormat(GL_RGBA);
				colorTexture->createTexture();
			}

			bSuccess= m_spoutColorFrame->ReceiveTexture(colorTexture->getGlTextureId(), GL_TEXTURE_2D);
		}

		IMkTexturePtr depthTexture= m_parentAccessor->getDepthTexture();
		if (depthTexture != nullptr)
		{
			EASY_BLOCK("receive depth texture");

			if (needsTextureRebuild(m_parentAccessor->getDepthSenderName(), m_spoutDepthFrame, depthTexture,
									m_depthResizeCount))
			{
				depthTexture->disposeTexture();
				depthTexture->setSize(m_spoutDepthFrame->GetSenderWidth(), m_spoutDepthFrame->GetSenderHeight());
				depthTexture->setTextureFormat(GL_RGBA);
				depthTexture->setBufferFormat(GL_RGBA);
				depthTexture->createTexture();
			}

			bSuccess&= m_spoutDepthFrame->ReceiveTexture(depthTexture->getGlTextureId(), GL_TEXTURE_2D);
		}

		IMkTexturePtr shadowTexture= m_parentAccessor->getShadowTexture();
		if (shadowTexture != nullptr && m_spoutShadowFrame != nullptr)
		{
			EASY_BLOCK("receive shadow texture");

			if (needsTextureRebuild(m_parentAccessor->getShadowSenderName(), m_spoutShadowFrame, shadowTexture,
									m_shadowResizeCount))
			{
				const GLint shadowTexFormat=
					getReceiveShadowTextureFormat(m_parentAccessor->getRenderTargetDescriptor().shadow_buffer_type);

				shadowTexture->disposeTexture();
				shadowTexture->setSize(m_spoutShadowFrame->GetSenderWidth(), m_spoutShadowFrame->GetSenderHeight());
				shadowTexture->setTextureFormat(shadowTexFormat);
				shadowTexture->setBufferFormat(GL_RGBA);
				shadowTexture->createTexture();
			}

			bSuccess&= m_spoutShadowFrame->ReceiveTexture(shadowTexture->getGlTextureId(), GL_TEXTURE_2D);
		}

		return bSuccess;
	}

private:
	int m_colorResizeCount= 0;
	int m_depthResizeCount= 0;
	int m_shadowResizeCount= 0;

	SharedTextureReadAccessor* m_parentAccessor;
	SPOUTLIBRARY* m_spoutColorFrame;
	SPOUTLIBRARY* m_spoutDepthFrame;
	SPOUTLIBRARY* m_spoutShadowFrame;
};

//-- SharedTextureReadAccessor -----
struct RenderTargetReaderImpl
{
	union
	{
		SpoutTextureReader* spoutTextureReader;
	} readerApi;
	MikanClientGraphicsApi graphicsAPI;
};

SharedTextureReadAccessor::SharedTextureReadAccessor(const std::string& senderPrefix, MikanCameraID cameraId)
	: m_senderPrefix(senderPrefix)
	, m_cameraId(cameraId)
	, m_colorTexture(nullptr)
	, m_depthTexture(nullptr)
	, m_shadowTexture(nullptr)
	, m_readerImpl(new RenderTargetReaderImpl)
{
	m_descriptor= MikanRenderTargetDescriptor();
	m_readerImpl->readerApi.spoutTextureReader= nullptr;
	m_readerImpl->graphicsAPI= MikanClientGraphicsApi_UNKNOWN;
}

SharedTextureReadAccessor::~SharedTextureReadAccessor()
{
	dispose();
	delete m_readerImpl;
	m_readerImpl= nullptr;
}

// Every client API whose frames arrive as Spout senders. The reader opens a sender by name, so
// what the client rendered with only decides whether its writer could produce one.
static bool isSpoutBackedGraphicsApi(MikanClientGraphicsApi graphicsAPI)
{
	return graphicsAPI == MikanClientGraphicsApi_Direct3D9 || graphicsAPI == MikanClientGraphicsApi_Direct3D11
		   || graphicsAPI == MikanClientGraphicsApi_Direct3D12 || graphicsAPI == MikanClientGraphicsApi_OpenGL
		   || graphicsAPI == MikanClientGraphicsApi_Vulkan;
}

bool SharedTextureReadAccessor::initialize(const MikanRenderTargetDescriptor* descriptor)
{
	bool bSuccess= false;

	dispose();

	m_descriptor= *descriptor;
	m_lastFrameRenderedIndex= 0;

	// Make a spout sender name for the color buffer
	if (!makeSpoutSenderName(m_senderPrefix, m_cameraId, SharedTextureType::COLOR, m_colorSenderName))
	{
		MIKAN_LOG_ERROR("SharedTextureReadAccessor") << "Failed to create spout color texture sender name.";
		return false;
	}

	// Make a spout sender name for the depth buffer
	if (descriptor->depth_buffer_type != MikanDepthBuffer_NODEPTH)
	{
		if (!makeSpoutSenderName(m_senderPrefix, m_cameraId, SharedTextureType::DEPTH, m_depthSenderName))
		{
			MIKAN_LOG_ERROR("SharedTextureReadAccessor") << "Failed to create spout depth texture sender name.";
			return false;
		}
	}
	else
	{
		m_depthSenderName= "";
	}

	// Make a spout sender name for the (optional) shadow buffer
	if (descriptor->shadow_buffer_type != MikanShadowBuffer_NOSHADOW)
	{
		if (!makeSpoutSenderName(m_senderPrefix, m_cameraId, SharedTextureType::SHADOW, m_shadowSenderName))
		{
			MIKAN_LOG_ERROR("SharedTextureReadAccessor") << "Failed to create spout shadow texture sender name.";
			return false;
		}
	}
	else
	{
		m_shadowSenderName= "";
	}

	// Allocate a reader implementation
	if (isSpoutBackedGraphicsApi(descriptor->graphicsAPI))
	{
		m_readerImpl->readerApi.spoutTextureReader= new SpoutTextureReader(this);
		m_readerImpl->graphicsAPI= descriptor->graphicsAPI;

		bSuccess= m_readerImpl->readerApi.spoutTextureReader->init(descriptor);
	}

	return bSuccess;
}

void SharedTextureReadAccessor::dispose()
{
	assert(m_readerImpl != nullptr);
	if (isSpoutBackedGraphicsApi(m_readerImpl->graphicsAPI))
	{
		if (m_readerImpl->readerApi.spoutTextureReader != nullptr)
		{
			m_readerImpl->readerApi.spoutTextureReader->dispose();
			delete m_readerImpl->readerApi.spoutTextureReader;
			m_readerImpl->readerApi.spoutTextureReader= nullptr;
		}
	}

	m_readerImpl->graphicsAPI= MikanClientGraphicsApi_UNKNOWN;
	m_descriptor= {};
}

bool SharedTextureReadAccessor::readRenderTargetTextures(const int64_t newFrameIndex)
{
	bool bSuccess= false;

	if (isSpoutBackedGraphicsApi(m_readerImpl->graphicsAPI))
	{
		m_lastFrameRenderedIndex= newFrameIndex;

		if (m_readerImpl->readerApi.spoutTextureReader != nullptr
			&& m_readerImpl->readerApi.spoutTextureReader->readRenderTargetTexture())
		{
			bSuccess= true;
		}
	}

	return bSuccess;
}
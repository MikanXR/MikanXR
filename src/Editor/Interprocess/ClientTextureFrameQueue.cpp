#include "ClientTextureFrameQueue.h"
#include "IMkTexture.h"
#include "Logger.h"

ClientTextureFrameQueue::ClientTextureFrameQueue(int queueSize)
	: m_queueSize(queueSize)
{
	m_entries= new ClientTextureFrameEntry[queueSize];
}

ClientTextureFrameQueue::~ClientTextureFrameQueue()
{
	dispose();
	delete[] m_entries;
	m_entries= nullptr;
}

bool ClientTextureFrameQueue::initialize(const MikanRenderTargetDescriptor& desc)
{
	bool bSuccess= true;

	for (int i= 0; i < m_queueSize; ++i)
	{
		ClientTextureFrameEntry& entry= m_entries[i];
		entry.frameIndex= -1;

		// Create color texture
		switch (desc.color_buffer_type)
		{
		case MikanColorBuffer_RGB24:
			entry.colorTexture= CreateMkTexture();
			entry.colorTexture->setTextureFormat(MK_RGB);
			entry.colorTexture->setBufferFormat(MK_RGB);
			break;
		case MikanColorBuffer_RGBA32:
			entry.colorTexture= CreateMkTexture();
			entry.colorTexture->setTextureFormat(MK_RGBA);
			entry.colorTexture->setBufferFormat(MK_RGBA);
			break;
		case MikanColorBuffer_BGRA32:
			entry.colorTexture= CreateMkTexture();
			entry.colorTexture->setTextureFormat(MK_RGBA);
			entry.colorTexture->setBufferFormat(MK_BGRA);
			break;
		case MikanColorBuffer_RGBA16F:
			entry.colorTexture= CreateMkTexture();
			entry.colorTexture->setTextureFormat(MK_RGBA16F);
			entry.colorTexture->setBufferFormat(MK_RGBA);
			break;
		}

		if (entry.colorTexture != nullptr)
		{
			entry.colorTexture->setSize(desc.width, desc.height);
			entry.colorTexture->setGenerateMipMap(false);
			entry.colorTexture->setPixelBufferObjectMode(desc.graphicsAPI == MikanClientGraphicsApi_UNKNOWN
															 ? IMkTexture::PixelBufferObjectMode::DoublePBOWrite
															 : IMkTexture::PixelBufferObjectMode::NoPBO);
			bSuccess&= entry.colorTexture->createTexture();
		}

		// Create depth texture
		switch (desc.depth_buffer_type)
		{
		case MikanDepthBuffer_FLOAT_DEVICE_DEPTH:
		case MikanDepthBuffer_FLOAT_SCENE_DEPTH:
			entry.depthTexture= CreateMkTexture();
			entry.depthTexture->setTextureFormat(MK_R32F);
			entry.depthTexture->setBufferFormat(MK_RED);
			break;
		case MikanDepthBuffer_PACK_DEPTH_RGBA:
			entry.depthTexture= CreateMkTexture();
			entry.depthTexture->setTextureFormat(MK_RGBA);
			entry.depthTexture->setBufferFormat(MK_RGBA);
			break;
		}

		if (entry.depthTexture != nullptr)
		{
			entry.depthTexture->setSize(desc.width, desc.height);
			entry.depthTexture->setGenerateMipMap(false);
			entry.depthTexture->setPixelBufferObjectMode(desc.graphicsAPI == MikanClientGraphicsApi_UNKNOWN
															 ? IMkTexture::PixelBufferObjectMode::DoublePBOWrite
															 : IMkTexture::PixelBufferObjectMode::NoPBO);
			bSuccess&= entry.depthTexture->createTexture();
		}

		// Create shadow texture (optional second color-like buffer)
		switch (desc.shadow_buffer_type)
		{
		case MikanShadowBuffer_RGB24:
			entry.shadowTexture= CreateMkTexture();
			entry.shadowTexture->setTextureFormat(MK_RGB);
			entry.shadowTexture->setBufferFormat(MK_RGB);
			break;
		case MikanShadowBuffer_RGBA32:
			entry.shadowTexture= CreateMkTexture();
			entry.shadowTexture->setTextureFormat(MK_RGBA);
			entry.shadowTexture->setBufferFormat(MK_RGBA);
			break;
		case MikanShadowBuffer_BGRA32:
			entry.shadowTexture= CreateMkTexture();
			entry.shadowTexture->setTextureFormat(MK_RGBA);
			entry.shadowTexture->setBufferFormat(MK_BGRA);
			break;
		case MikanShadowBuffer_RGBA16F:
			entry.shadowTexture= CreateMkTexture();
			entry.shadowTexture->setTextureFormat(MK_RGBA16F);
			entry.shadowTexture->setBufferFormat(MK_RGBA);
			break;
		}

		if (entry.shadowTexture != nullptr)
		{
			entry.shadowTexture->setSize(desc.width, desc.height);
			entry.shadowTexture->setGenerateMipMap(false);
			entry.shadowTexture->setPixelBufferObjectMode(desc.graphicsAPI == MikanClientGraphicsApi_UNKNOWN
															  ? IMkTexture::PixelBufferObjectMode::DoublePBOWrite
															  : IMkTexture::PixelBufferObjectMode::NoPBO);
			bSuccess&= entry.shadowTexture->createTexture();
		}

		if (!bSuccess)
			break;
	}

	return bSuccess;
}

void ClientTextureFrameQueue::dispose()
{
	if (m_entries != nullptr)
	{
		for (int i= 0; i < m_queueSize; ++i)
		{
			m_entries[i].colorTexture= nullptr;
			m_entries[i].depthTexture= nullptr;
			m_entries[i].shadowTexture= nullptr;
			m_entries[i].frameIndex= -1;
		}
	}

	m_lastWriteIndex= -1;
	m_pendingWriteIndex= 0;
}

IMkTexturePtr ClientTextureFrameQueue::getPendingWriteColorTexture() const
{
	if (m_entries != nullptr && m_queueSize > 0)
	{
		return m_entries[m_pendingWriteIndex].colorTexture;
	}
	return IMkTexturePtr();
}

IMkTexturePtr ClientTextureFrameQueue::getPendingWriteDepthTexture() const
{
	if (m_entries != nullptr && m_queueSize > 0)
	{
		return m_entries[m_pendingWriteIndex].depthTexture;
	}
	return IMkTexturePtr();
}

IMkTexturePtr ClientTextureFrameQueue::getPendingWriteShadowTexture() const
{
	if (m_entries != nullptr && m_queueSize > 0)
	{
		return m_entries[m_pendingWriteIndex].shadowTexture;
	}
	return IMkTexturePtr();
}

void ClientTextureFrameQueue::advanceWriteIndex(int64_t frameIndex)
{
	if (m_entries != nullptr && m_queueSize > 0)
	{
		// Stamp the frame index on the slot that was just written to
		m_entries[m_pendingWriteIndex].frameIndex= frameIndex;

		// Track the most recently completed write slot
		m_lastWriteIndex= m_pendingWriteIndex;

		// Advance to the next slot for the next frame
		m_pendingWriteIndex= (m_pendingWriteIndex + 1) % m_queueSize;
	}
}

void ClientTextureFrameQueue::noteFrameIndexMiss(int64_t requestedFrameIndex) const
{
	// A client running behind the compositor shows up here and nowhere else, since
	// the fallback composites without complaint. Log the first miss and then every
	// hundredth so a steady miss rate stays visible without flooding.
	m_frameIndexMissCount++;
	if (m_frameIndexMissCount == 1 || (m_frameIndexMissCount % 100) == 0)
	{
		const int64_t servedFrameIndex= m_lastWriteIndex >= 0 ? m_entries[m_lastWriteIndex].frameIndex : -1;
		MIKAN_LOG_WARNING("ClientTextureFrameQueue::getColorTexture")
			<< "Client frame " << requestedFrameIndex << " not in texture ring, serving newest frame "
			<< servedFrameIndex << " (" << m_frameIndexMissCount << " misses so far)";
	}
}

IMkTexturePtr ClientTextureFrameQueue::getColorTexture(int64_t frameIndex) const
{
	if (m_entries != nullptr)
	{
		// Search for a matching frame index
		if (frameIndex != -1)
		{
			for (int i= 0; i < m_queueSize; ++i)
			{
				if (m_entries[i].frameIndex == frameIndex)
				{
					return m_entries[i].colorTexture;
				}
			}

			// A free-running client stamps -1 on every slot and is never a miss
			if (m_lastWriteIndex >= 0 && m_entries[m_lastWriteIndex].frameIndex != -1)
			{
				noteFrameIndexMiss(frameIndex);
			}
		}

		// Fall back to the most recently written slot
		if (m_lastWriteIndex >= 0)
		{
			return m_entries[m_lastWriteIndex].colorTexture;
		}
	}

	return IMkTexturePtr();
}

IMkTexturePtr ClientTextureFrameQueue::getDepthTexture(int64_t frameIndex) const
{
	if (m_entries != nullptr)
	{
		// Search for a matching frame index
		if (frameIndex != -1)
		{
			for (int i= 0; i < m_queueSize; ++i)
			{
				if (m_entries[i].frameIndex == frameIndex)
				{
					return m_entries[i].depthTexture;
				}
			}
		}

		// Fall back to the most recently written slot
		if (m_lastWriteIndex >= 0)
		{
			return m_entries[m_lastWriteIndex].depthTexture;
		}
	}

	return IMkTexturePtr();
}

IMkTexturePtr ClientTextureFrameQueue::getShadowTexture(int64_t frameIndex) const
{
	if (m_entries != nullptr)
	{
		// Search for a matching frame index
		if (frameIndex != -1)
		{
			for (int i= 0; i < m_queueSize; ++i)
			{
				if (m_entries[i].frameIndex == frameIndex)
				{
					return m_entries[i].shadowTexture;
				}
			}
		}

		// Fall back to the most recently written slot
		if (m_lastWriteIndex >= 0)
		{
			return m_entries[m_lastWriteIndex].shadowTexture;
		}
	}

	return IMkTexturePtr();
}

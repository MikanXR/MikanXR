#include "ClientSourceManager.h"
#include "IMkTexture.h"
#include "Logger.h"
#include "MikanClientConnectionState.h"
#include "MikanServer.h"
#include "SharedTextureReader.h"
#include "CameraRequestHandler.h"
#include "StringUtils.h"

#include <algorithm>

ClientSourceManager::ClientSourceManager(int textureQueueSize)
	: m_textureQueueSize(textureQueueSize)
{
}

bool ClientSourceManager::startup()
{
	MikanServer* mikanServer= MikanServer::getInstance();

	// Create layers for all connected clients with allocated render targets
	std::vector<MikanClientConnectionStateConstPtr> clientStateList;
	mikanServer->getConnectedClientStateList(clientStateList);
	for (MikanClientConnectionStateConstPtr clientState : clientStateList)
	{
		RenderTargetClientState* renderTargetClientState= clientState->getRenderTargetClientState();
		const SharedTextureReadAccessorCameraMap& readAccessorMap=
			renderTargetClientState->getRenderTargetAccessorMap();

		for (auto it : readAccessorMap)
		{
			SharedTextureReadAccessorPtr readAccessor= it.second;

			onClientRenderTargetAllocated(clientState->getClientId(), clientState->getMikanClientInfo(),
										  readAccessor.get());
		}
	}

	// Listen for new render target events
	auto* cameraRequestHandler= mikanServer->getCameraRequestHandler();
	cameraRequestHandler->OnClientRenderTargetAllocated+=
		MakeDelegate(this, &ClientSourceManager::onClientRenderTargetAllocated);
	cameraRequestHandler->OnClientRenderTargetReleased+=
		MakeDelegate(this, &ClientSourceManager::onClientRenderTargetReleased);
	cameraRequestHandler->OnClientRenderTargetUpdated+=
		MakeDelegate(this, &ClientSourceManager::onClientRenderTargetUpdated);

	return true;
}

void ClientSourceManager::shutdown()
{
	// Stop listening to render target events
	MikanServer* mikanServer= MikanServer::getInstance();
	auto* cameraRequestHandler= mikanServer->getCameraRequestHandler();

	cameraRequestHandler->OnClientRenderTargetAllocated-=
		MakeDelegate(this, &ClientSourceManager::onClientRenderTargetAllocated);
	cameraRequestHandler->OnClientRenderTargetReleased-=
		MakeDelegate(this, &ClientSourceManager::onClientRenderTargetReleased);
	cameraRequestHandler->OnClientRenderTargetUpdated-=
		MakeDelegate(this, &ClientSourceManager::onClientRenderTargetUpdated);

	// Clean up any allocated clientSources
	while (!m_clientSources.getMap().empty())
	{
		auto iter= m_clientSources.getMap().begin();

		destroyClientSource(iter->first, iter->second);
	}
}

bool ClientSourceManager::hasClientSource(const std::string& clientId, MikanCameraID cameraId) const
{
	return getClientSource(clientId.c_str(), cameraId) != nullptr;
}

bool ClientSourceManager::getClientSourceDimensions(const std::string& clientId, MikanCameraID cameraId, int& outWidth,
													int& outHeight) const
{
	ClientSource* clientSource= getClientSource(clientId.c_str(), cameraId);
	if (clientSource != nullptr)
	{
		outWidth= clientSource->desc.width;
		outHeight= clientSource->desc.height;
		return true;
	}

	return false;
}

IMkTexturePtr ClientSourceManager::getClientColorSourceTexture(const std::string& clientId, MikanCameraID cameraId,
															   eTextureSourceColorType textureSourceColorType,
															   int64_t frameIndex) const
{
	ClientSource* clientSource= getClientSource(clientId.c_str(), cameraId);
	if (clientSource != nullptr && clientSource->textureQueue != nullptr)
	{
		switch (textureSourceColorType)
		{
		case eTextureSourceColorType::colorRGB:
		case eTextureSourceColorType::colorRGBA:
			return clientSource->textureQueue->getColorTexture(frameIndex);
		case eTextureSourceColorType::shadowRGB:
		case eTextureSourceColorType::shadowRGBA:
			return clientSource->textureQueue->getShadowTexture(frameIndex);
		}
	}

	return IMkTexturePtr();
}

IMkTexturePtr ClientSourceManager::getClientDepthSourceTexture(const std::string& clientId, MikanCameraID cameraId,
															   eTextureSourceDepthType textureSourceDepthType,
															   int64_t frameIndex) const
{
	ClientSource* clientSource= getClientSource(clientId.c_str(), cameraId);
	if (clientSource != nullptr && clientSource->textureQueue != nullptr)
	{
		switch (textureSourceDepthType)
		{
		case eTextureSourceDepthType::depthPackRGBA:
			return clientSource->textureQueue->getDepthTexture(frameIndex);
		}
	}

	return IMkTexturePtr();
}

std::string ClientSourceManager::makeClientSourceTableKey(const char* clientId, MikanCameraID cameraId)
{
	return StringUtils::stringify(clientId, "_camera", std::to_string(cameraId));
}

ClientSourceManager::ClientSource* ClientSourceManager::getClientSource(const char* clientId,
																		MikanCameraID cameraId) const
{
	if (cameraId != INVALID_MIKAN_ID)
	{
		ClientSourceManager::ClientSource* clientSource= nullptr;
		const std::string tableKey= makeClientSourceTableKey(clientId, cameraId);
		if (m_clientSources.tryGetValue(tableKey, clientSource))
		{
			return clientSource;
		}
	}
	else
	{
		// If an invalid cameraId was provided, find the newest client source for the clientId regardless of cameraId.
		int64_t newestFrameIndex= -1;
		ClientSourceManager::ClientSource* newestClientSource= nullptr;
		for (auto iter= m_clientSources.getMap().begin(); iter != m_clientSources.getMap().end(); iter++)
		{
			ClientSourceManager::ClientSource* clientSource= iter->second;
			if (clientSource->clientId == clientId && clientSource->frameIndex > newestFrameIndex)
			{
				newestFrameIndex= clientSource->frameIndex;
				newestClientSource= clientSource;
			}
		}

		return newestClientSource;
	}

	return nullptr;
}

bool ClientSourceManager::addClientSource(const char* clientId, const MikanClientInfo& clientInfo,
										  SharedTextureReadAccessor* readAccessor)
{
	MikanCameraID cameraId= readAccessor->getCameraId();
	const std::string tableKey= makeClientSourceTableKey(clientId, cameraId);

	// The previous source for this client and camera is normally gone before a new render target
	// is allocated. One that survived belongs to an accessor that no longer exists, so replace it:
	// refusing here would answer the client's allocation with success while leaving it publishing
	// into a source the compositor never reads. Its accessor may already be freed, so it is not
	// touched here.
	ClientSource* staleClientSource= nullptr;
	if (m_clientSources.tryGetValue(tableKey, staleClientSource))
	{
		MIKAN_LOG_WARNING("ClientSourceManager::addClientSource") << "Replacing stale client source " << tableKey;

		destroyClientSource(tableKey, staleClientSource);

		if (OnClientSourceDisconnected)
		{
			OnClientSourceDisconnected(clientId, cameraId);
		}
	}

	ClientSource* clientSource= new ClientSource();
	bool bSuccess= true;

	const MikanRenderTargetDescriptor& desc= readAccessor->getRenderTargetDescriptor();
	clientSource->clientId= clientId;
	clientSource->clientInfo= clientInfo;
	clientSource->desc= desc;
	clientSource->readAccessor= readAccessor;
	clientSource->frameIndex= 0;

	// Create the circular texture frame queue, at least as deep as the compositor's
	// own frame event queue for this camera
	int textureQueueSize= m_textureQueueSize;
	if (m_queueSizeResolver)
	{
		textureQueueSize= std::max(textureQueueSize, m_queueSizeResolver(cameraId));
	}
	clientSource->textureQueue= new ClientTextureFrameQueue(textureQueueSize);
	bSuccess= clientSource->textureQueue->initialize(desc);

	if (bSuccess)
	{
		// Point the read accessor at the first pending write slot
		readAccessor->setColorTexture(clientSource->textureQueue->getPendingWriteColorTexture());
		readAccessor->setDepthTexture(clientSource->textureQueue->getPendingWriteDepthTexture());
		readAccessor->setShadowTexture(clientSource->textureQueue->getPendingWriteShadowTexture());

		// Add the client source to the data source table
		m_clientSources.setValue(tableKey, clientSource);

		// Notify listeners that a new client source has connected
		if (OnClientSourceConnected)
		{
			OnClientSourceConnected(clientId, cameraId);
		}
	}
	else
	{
		// Clean up on failure
		readAccessor->setColorTexture(nullptr);
		readAccessor->setDepthTexture(nullptr);
		readAccessor->setShadowTexture(nullptr);

		clientSource->textureQueue->dispose();
		delete clientSource->textureQueue;
		clientSource->readAccessor= nullptr;

		delete clientSource;
	}

	return bSuccess;
}

bool ClientSourceManager::removeClientSource(const char* clientId, SharedTextureReadAccessor* readAccessor)
{
	MikanCameraID cameraId= readAccessor->getCameraId();
	ClientSource* clientSource= getClientSource(clientId, cameraId);
	if (clientSource == nullptr)
		return false;

	// The accessor is still alive on this path, so unbind the queue textures it points at
	readAccessor->setColorTexture(nullptr);
	readAccessor->setDepthTexture(nullptr);
	readAccessor->setShadowTexture(nullptr);

	destroyClientSource(makeClientSourceTableKey(clientId, cameraId), clientSource);

	// Notify listeners that a new client source has disconnected
	if (OnClientSourceDisconnected)
	{
		OnClientSourceDisconnected(clientId, cameraId);
	}

	return true;
}

// Release a client source's texture queue and drop it from the table. The source's read accessor
// is only cleared here, never dereferenced, so this is safe to call for a source whose accessor
// has already been destroyed.
void ClientSourceManager::destroyClientSource(const std::string& tableKey, ClientSource* clientSource)
{
	if (clientSource == nullptr)
		return;

	if (clientSource->textureQueue != nullptr)
	{
		clientSource->textureQueue->dispose();
		delete clientSource->textureQueue;
		clientSource->textureQueue= nullptr;
	}
	clientSource->readAccessor= nullptr;

	m_clientSources.removeValue(tableKey);

	delete clientSource;
}

// MikanServer Events
void ClientSourceManager::onClientRenderTargetAllocated(const char* clientId, const MikanClientInfo& clientInfo,
														SharedTextureReadAccessor* readAccessor)
{
	MIKAN_LOG_TRACE("ClientSourceManager::onClientRenderTargetAllocated") << "Adding Source " << clientId;

	addClientSource(clientId, clientInfo, readAccessor);
}

void ClientSourceManager::onClientRenderTargetReleased(const char* clientId, SharedTextureReadAccessor* readAccessor)
{
	MIKAN_LOG_TRACE("ClientSourceManager::onClientRenderTargetAllocated") << "Removing Source " << clientId;

	removeClientSource(clientId, readAccessor);
}

void ClientSourceManager::onClientRenderTargetUpdated(const char* clientId, MikanCameraID cameraId, int64_t frameIndex)
{
	MIKAN_LOG_TRACE("ClientSourceManager::onClientRenderTargetUpdated") << "Recv frame " << frameIndex;

	ClientSource* clientSource= getClientSource(clientId, cameraId);
	if (clientSource != nullptr && clientSource->textureQueue != nullptr)
	{
		// Stamp the frame index on the current slot and advance to the next
		clientSource->textureQueue->advanceWriteIndex(frameIndex);
		clientSource->frameIndex= frameIndex;


		// Re-point the accessor at the new pending write slot
		if (clientSource->readAccessor != nullptr)
		{
			clientSource->readAccessor->setColorTexture(clientSource->textureQueue->getPendingWriteColorTexture());
			clientSource->readAccessor->setDepthTexture(clientSource->textureQueue->getPendingWriteDepthTexture());
			clientSource->readAccessor->setShadowTexture(clientSource->textureQueue->getPendingWriteShadowTexture());
		}
	}
}
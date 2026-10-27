#include "CameraRequestHandler.h"
#include "InterprocessMessageServerInterface.h"
#include "MikanClientConnectionState.h"
#include "MikanServer.h"
#include "MikanPropertyEvents.h"
#include "PropertyNotifyDatabase.h"
#include "ServerResponseHelpers.h"

MikanClientConnectionState::MikanClientConnectionState(MikanServer* ownerServer, const std::string& connectionId)
	: m_ownerServer(ownerServer)
	, m_connectionId(connectionId)
	, m_renderTargetClientState(new RenderTargetClientState(this))
	, m_propertyNotifyDatabase(
		  std::make_shared<PropertyNotifyDatabase>(ownerServer->getProjectManager()->getPropertyDatabaseConst()))
{
}

MikanClientConnectionState::~MikanClientConnectionState() { delete m_renderTargetClientState; }

const char* MikanClientConnectionState::getClientId() const { return m_clientInfo.clientId.getUtf8Value(); }

bool MikanClientConnectionState::isClientInfoValid() const
{
	const char* szClientId= getClientId();

	return szClientId != nullptr && szClientId[0] != '\0';
}

void MikanClientConnectionState::setMikanClientInfo(const MikanClientInfo& clientInfo)
{
	// Free any existing render target
	m_renderTargetClientState->disposeAllRenderTargetAccessors();

	// Set the new client info describing the client render capabilities
	m_clientInfo= clientInfo;
}

void MikanClientConnectionState::clearMikanClientInfo()
{
	// Free any existing render target
	m_renderTargetClientState->disposeAllRenderTargetAccessors();

	// Reset the client info with defaults
	m_clientInfo= MikanClientInfo();
}

void MikanClientConnectionState::publishMikanJsonEvent(const std::string& mikanJsonEvent)
{
	m_ownerServer->getMessageServer()->sendMessageToClient(m_connectionId, mikanJsonEvent);
}

// Property Events
bool MikanClientConnectionState::setPropertyNotifyMode(const std::string& systemFilter,
													   const std::string& componentFilter,
													   const std::string& propertyFilter,
													   MikanPropertyNotifyMode notifyMode)
{
	return m_propertyNotifyDatabase->setPropertyNotifyMode(systemFilter, componentFilter, propertyFilter, notifyMode);
}

void MikanClientConnectionState::publishPropertyChangedEvent(const MikanPropertyValue& propertyValue)
{
	MikanPropertyNotifyMode notifyMode=
		m_propertyNotifyDatabase->getPropertyNotifyMode(propertyValue, m_ownerServer->getProjectManager());

	if (notifyMode != MikanPropertyNotifyMode::NONE)
	{
		MikanPropertyUpdateEvent propertyUpdateEvent= {};
		propertyUpdateEvent.propertyValue= propertyValue;
		propertyUpdateEvent.propertyValue.ownerComponentClass= propertyValue.ownerComponentClass;
		propertyUpdateEvent.propertyValue.componentId= propertyValue.componentId;
		propertyUpdateEvent.propertyValue.fieldName= propertyValue.fieldName;

		if (notifyMode == MikanPropertyNotifyMode::NAME_AND_VALUE)
		{
			propertyUpdateEvent.propertyValue.fieldValue= propertyValue.fieldValue;
		}

		m_ownerServer->getMessageServer()->sendMessageToClient(getConnectionId(),
															   mikanEventToJsonRpc(propertyUpdateEvent));
	}
}
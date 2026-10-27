#include "DMXFixtureComponent.h"
#include "DMXObjectSystem.h"
#include "LightRequestHandler.h"
#include "MikanClientConnectionState.h"
#include "MikanLightEvents.h"
#include "MikanLightRequests.h"
#include "MikanServer.h"
#include "RGBSpotLightSystem.h"
#include "RGBPixelGridSystem.h"
#include "ServerResponseHelpers.h"

#include <functional>

using namespace std::placeholders;

bool LightRequestHandler::startup(MainWindow* mainWindow)
{
	IInterprocessMessageServer* messageServer= m_owner->getMessageServer();

	// Register request handlers
	messageServer->setRequestHandler(SetLightDMXDataSubcription::staticGetArchetype().getName(),
									 std::bind(&LightRequestHandler::setLightDMXDataSubscriptionHandler, this, _1, _2));
	messageServer->setRequestHandler(GetDMXData::staticGetArchetype().getName(),
									 std::bind(&LightRequestHandler::getDMXDataHandler, this, _1, _2));

	// Subscribe to the single DMXObjectSystem delegate for all fixture DMX data changes
	if (auto dmxSystem= getObjectSystemOfType<DMXObjectSystem>())
	{
		dmxSystem->OnDMXDataChanged+= MakeDelegate(this, &LightRequestHandler::onDMXDataChanged);
	}

	return true;
}

void LightRequestHandler::shutdown()
{
	if (auto dmxSystem= getObjectSystemOfType<DMXObjectSystem>())
	{
		dmxSystem->OnDMXDataChanged-= MakeDelegate(this, &LightRequestHandler::onDMXDataChanged);
	}

	m_lightSubscriptions.clear();
}

void LightRequestHandler::onDMXDataChanged()
{
	auto dmxSystem= getObjectSystemOfType<DMXObjectSystem>();

	// Only the universes written this tick belong in a change event. extractUniverseData
	// answers for any universe now, so this is where the change filter lives.
	const std::set<uint16_t> dirtyUniverseIds= dmxSystem->getDirtyDMXUniverseIdSet();
	if (dirtyUniverseIds.empty())
		return;

	std::map<uint16_t, MikanUniverseDMXData> universeDataCache;

	for (const auto subscriptionIt : m_lightSubscriptions)
	{
		const std::string& clientId= subscriptionIt.first;
		ClientLightSubscriptionInfoPtr subscriptionInfo= subscriptionIt.second;

		MikanClientConnectionStatePtr clientState= m_owner->getConnectedClientState(clientId);
		if (clientState)
		{
			publishDMXDataToClient(clientState, subscriptionInfo, &dirtyUniverseIds, universeDataCache);
		}
	}
}

void LightRequestHandler::publishDMXDataToClient(MikanClientConnectionStatePtr clientState,
												 ClientLightSubscriptionInfoPtr subscriptionInfo,
												 const std::set<uint16_t>* universeIdFilter,
												 std::map<uint16_t, MikanUniverseDMXData>& universeDataCache) const
{
	auto dmxSystem= getObjectSystemOfType<DMXObjectSystem>();
	if (!dmxSystem)
		return;

	// Get all the universes IDs this client is subscribed to
	std::set<uint16_t> universeIds;
	computeDMXUniverseIdsForLights(subscriptionInfo, universeIds);

	MikanLightDMXDataChangedEvent dmxChangedEvent;
	dmxChangedEvent.dmx_data.server_time_seconds= getServerTime();

	for (uint16_t universeId : universeIds)
	{
		if (universeIdFilter != nullptr && !universeIdFilter->contains(universeId))
			continue;

		auto cacheIt= universeDataCache.find(universeId);
		if (cacheIt != universeDataCache.end())
		{
			// Add the cached universe data that was already fetched
			dmxChangedEvent.dmx_data.universes.push_back(cacheIt->second);
		}
		else
		{
			// No cached data, extract DMX channel data for the universe
			MikanUniverseDMXData universeData;
			if (dmxSystem->extractUniverseData(universeId, universeData))
			{
				// Add the universe data to the event
				dmxChangedEvent.dmx_data.universes.push_back(universeData);

				// Add to the cache for other clients
				universeDataCache.insert({universeId, universeData});
			}
		}
	}

	// Send all the universes, if any, to the client
	if (dmxChangedEvent.dmx_data.universes.size() > 0)
	{
		// TODO: Make this a binary event
		const std::string eventJson= mikanEventToJsonRpc(dmxChangedEvent);

		clientState->publishMikanJsonEvent(eventJson);
	}
}

void LightRequestHandler::setLightDMXDataSubscriptionHandler(const ClientRequest& request, ClientResponse& response)
{
	SetLightDMXDataSubcription subscriptionRequest;
	if (!readTypedRequest(request.utf8RequestString, subscriptionRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	const std::string& clientId= request.connectionId;
	if (subscriptionRequest.subscribe)
	{
		// Find (or add) light subscription info for the given client
		ClientLightSubscriptionInfoPtr subscriptionInfo;
		auto it= m_lightSubscriptions.find(clientId);
		if (it == m_lightSubscriptions.end())
		{
			subscriptionInfo= std::make_shared<ClientLightSubscriptionInfo>();
			subscriptionInfo->clientId= clientId;

			m_lightSubscriptions.insert({clientId, subscriptionInfo});
		}
		else
		{
			subscriptionInfo= it->second;
		}

		if (subscriptionRequest.light_ids.size() > 0)
		{
			for (MikanLightID lightId : subscriptionRequest.light_ids)
			{
				if (lightId != k_AllLights) // skip reserved value
				{
					subscriptionInfo->subscribedLights.insert(lightId);
				}
			}
		}
		else
		{
			// All lights subscribed
			subscriptionInfo->subscribedLights.clear();
			subscriptionInfo->subscribedLights.insert(k_AllLights);
		}

		// Send the current channel data straight away. The change event only fires on the tick
		// a universe is written, so without this a client that subscribes between writes holds
		// every light at zero until something happens to move it.
		MikanClientConnectionStatePtr clientState= m_owner->getConnectedClientState(clientId);
		if (clientState)
		{
			std::map<uint16_t, MikanUniverseDMXData> universeDataCache;

			publishDMXDataToClient(clientState, subscriptionInfo, nullptr, universeDataCache);
		}
	}
	else
	{
		auto it= m_lightSubscriptions.find(clientId);
		if (it != m_lightSubscriptions.end())
		{
			ClientLightSubscriptionInfoPtr subscriptionInfo= it->second;

			if (subscriptionRequest.light_ids.size() > 0)
			{
				for (MikanLightID lightId : subscriptionRequest.light_ids)
				{
					subscriptionInfo->subscribedLights.erase(lightId);
				}
			}
			else
			{
				// All lights unsubscribed
				subscriptionInfo->subscribedLights.clear();
			}

			// Remove the subscription entry if there are no subscribed lights left
			if (subscriptionInfo->subscribedLights.size() == 0)
			{
				m_lightSubscriptions.erase(it);
			}
		}
	}

	writeSimpleJsonResponse(request.requestId, MikanAPIResult::Success, response);
}

void LightRequestHandler::getDMXDataHandler(const ClientRequest& request, ClientResponse& response)
{
	GetDMXData getDataRequest;
	if (!readTypedRequest(request.utf8RequestString, getDataRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	auto dmxSystem= getObjectSystemOfType<DMXObjectSystem>();
	if (!dmxSystem)
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::RequestFailed, response);
		return;
	}

	// Extract all active universes
	MikanDMXDataResponse dmxResponse;
	dmxResponse.dmx_data.server_time_seconds= getServerTime();

	std::set<uint16_t> activeUniverseIds= dmxSystem->getActiveDMXUniverseIdSet();
	for (const uint16_t& universeId : activeUniverseIds)
	{
		MikanUniverseDMXData universeData= {};

		if (dmxSystem->extractUniverseData(universeId, universeData))
		{
			dmxResponse.dmx_data.universes.push_back(universeData);
		}
	}

	writeTypedJsonResponse(request.requestId, dmxResponse, response);
}

void LightRequestHandler::computeDMXUniverseIdsForLights(ClientLightSubscriptionInfoPtr subscriptionInfo,
														 std::set<uint16_t>& outUniverseIds) const
{
	auto dmxSystem= getObjectSystemOfType<DMXObjectSystem>();

	if (subscriptionInfo->subscribedLights.contains(k_AllLights))
	{
		// Get all active universes
		outUniverseIds= dmxSystem->getActiveDMXUniverseIdSet();
	}
	else
	{
		// Get universes for specifically subscribed lights
		for (const MikanLightID lightId : subscriptionInfo->subscribedLights)
		{
			std::shared_ptr<DMXFixtureComponent> dmxFixture= findLightById(lightId);

			if (dmxFixture)
			{
				DMXFixtureComponentDefinitionConstPtr definition= dmxFixture->getDMXFixtureDefinition();
				const uint16_t dmxUniverseId= definition->getDMXUniverse();

				outUniverseIds.insert(dmxUniverseId);
			}
		}
	}
}

std::shared_ptr<DMXFixtureComponent> LightRequestHandler::findLightById(MikanLightID lightId) const
{
	if (auto spotSystem= getObjectSystemOfType<RGBSpotLightSystem>())
	{
		if (auto light= spotSystem->getLightById(lightId))
			return light;
	}

	if (auto gridSystem= getObjectSystemOfType<RGBPixelGridSystem>())
	{
		if (auto grid= gridSystem->getGridById(lightId))
			return grid;
	}

	return nullptr;
}

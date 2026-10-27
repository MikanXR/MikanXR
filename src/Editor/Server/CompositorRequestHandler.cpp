#include "CompositorRequestHandler.h"
#include "CompositorObjectSystem.h"
#include "IRtpVideoSender.h"
#include "Logger.h"
#include "MikanClientConnectionState.h"
#include "MikanCompositorRequests.h"
#include "MikanServer.h"
#include "ServerResponseHelpers.h"

#include <functional>

using namespace std::placeholders;

// -- CompositorRequestHandler -- //
bool CompositorRequestHandler::startup(MainWindow* mainWindow)
{
	IInterprocessMessageServer* messageServer= m_owner->getMessageServer();

	messageServer->setRequestHandler(
		SetCompositorReturnFeed::staticGetArchetype().getName(),
		std::bind(&CompositorRequestHandler::setCompositorReturnFeedHandler, this, _1, _2));

	return true;
}

void CompositorRequestHandler::setCompositorReturnFeedHandler(const ClientRequest& request, ClientResponse& response)
{
	SetCompositorReturnFeed feedRequest;
	if (!readTypedRequest(request.utf8RequestString, feedRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	MikanClientConnectionStatePtr clientState= m_owner->getConnectedClientState(request.connectionId);
	if (!clientState)
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::UnknownClient, response);
		return;
	}

	const std::string host= feedRequest.host.getUtf8Value();
	if (feedRequest.enable && (!isValidRtpDestinationHost(host) || feedRequest.port < 1 || feedRequest.port > 65535))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	if (feedRequest.compositor_id != INVALID_MIKAN_ID)
	{
		CompositorObjectSystemPtr compositorSystem= getObjectSystemOfType<CompositorObjectSystem>();
		if (!compositorSystem || !compositorSystem->getCompositorById(feedRequest.compositor_id))
		{
			writeSimpleJsonResponse(request.requestId, MikanAPIResult::InvalidParam, response);
			return;
		}
	}

	// The feed lives on the connection state, so it ends with the connection. The
	// compositor picks the change up on its next update.
	if (feedRequest.enable)
	{
		RtpDestination destination;
		destination.host= host;
		destination.port= static_cast<uint16_t>(feedRequest.port);
		clientState->setReturnFeed(feedRequest.compositor_id, destination);

		MIKAN_LOG_INFO("CompositorRequestHandler::setCompositorReturnFeedHandler")
			<< "Connection " << request.connectionId << " set the return feed of compositor "
			<< feedRequest.compositor_id << " to " << host << ":" << feedRequest.port;
	}
	else
	{
		const auto& returnFeeds= clientState->getReturnFeeds();
		auto feed_it= returnFeeds.find(feedRequest.compositor_id);
		if (feed_it != returnFeeds.end())
		{
			MIKAN_LOG_INFO("CompositorRequestHandler::setCompositorReturnFeedHandler")
				<< "Connection " << request.connectionId << " cleared the return feed of compositor "
				<< feedRequest.compositor_id << " to " << feed_it->second.host << ":" << feed_it->second.port;

			clientState->clearReturnFeed(feedRequest.compositor_id);
		}
	}

	writeSimpleJsonResponse(request.requestId, MikanAPIResult::Success, response);
}

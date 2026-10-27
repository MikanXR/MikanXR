#pragma once

#include "IServerRequestHandler.h"

// Compositor requests from clients: the return feed a client receives the
// compositor's RTP output on
class CompositorRequestHandler : public IServerRequestHandler
{
public:
	CompositorRequestHandler(class MikanServer* owner)
		: IServerRequestHandler(owner)
	{
	}

	virtual bool startup(class MainWindow* mainWindow) override;
	virtual void shutdown() override {}

protected:
	void setCompositorReturnFeedHandler(const struct ClientRequest& request, struct ClientResponse& response);
};

#pragma once

#include "InterprocessMessageClientInterface.h"

#include <memory>

class WebsocketConnectionState;
using WebsocketConnectionStatePtr= std::shared_ptr<WebsocketConnectionState>;

class WebsocketInterprocessMessageClient : public IInterprocessMessageClient
{
public:
	WebsocketInterprocessMessageClient(int protocolVersion);
	virtual ~WebsocketInterprocessMessageClient();

	virtual MikanCoreResult initialize() override;
	virtual void dispose() override;

	virtual void setTextResponseHandler(TextResponseHandler handler) override;

	MikanCoreResult connect(const std::string& host, const std::string& port) override;
	void disconnect(uint16_t code, const std::string& reason) override;

	virtual MikanCoreResult fetchNextEvent(size_t utf8BufferSize, char* outUtf8Buffer,
										   size_t* outUtf8BufferSizeNeeded) override;
	virtual MikanCoreResult sendRequest(const std::string& utf8RequestString) override;

	const bool getIsConnected() const override;

private:
	WebsocketConnectionStatePtr m_connectionState;
};
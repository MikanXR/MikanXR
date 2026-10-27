#pragma once

#include "IMikanModule.h"
#include "INetworkVideoDeviceManager.h"
#include "IRtpVideoSender.h"

class INetworkVideoDeviceModule : public IMikanModule
{
public:
	INetworkVideoDeviceModule()= default;

	virtual INetworkVideoDeviceManagerPtr createNetworkVideoDeviceManager()= 0;
	virtual IRtpVideoSenderPtr createRtpVideoSender()= 0;
};

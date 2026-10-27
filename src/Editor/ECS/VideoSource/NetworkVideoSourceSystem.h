#pragma once

#include "ComponentFwd.h"
#include "INetworkVideoDeviceManager.h"
#include "IRtpVideoSender.h"
#include "MikanTypedObjectSystem.h"
#include "MikanVideoSourceTypes.h"
#include "NetworkVideoSourceComponent.h"
#include "ObjectSystemConfigFwd.h"
#include "VideoSourceQueries.h"

#include <future>
#include <string>
#include <vector>

class NetworkVideoSourceSystemDefinition
	: public MikanTypedObjectSystemDefinition<NetworkVideoSourceComponent, NetworkVideoSourceDefinition,
											  MikanVideoSourceID>
{
public:
	using Super=
		MikanTypedObjectSystemDefinition<NetworkVideoSourceComponent, NetworkVideoSourceDefinition, MikanVideoSourceID>;

	NetworkVideoSourceSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
};

class NetworkVideoSourceSystem
	: public MikanTypedObjectSystem<NetworkVideoSourceComponent, NetworkVideoSourceDefinition, MikanVideoSourceID,
									NetworkVideoSourceSystem, NetworkVideoSourceSystemDefinition>
{
public:
	using Super= MikanTypedObjectSystem<NetworkVideoSourceComponent, NetworkVideoSourceDefinition, MikanVideoSourceID,
										NetworkVideoSourceSystem, NetworkVideoSourceSystemDefinition>;

	NetworkVideoSourceSystem(ProjectManagerPtr ownerObjectSystem);

	inline static const std::string k_objectSystemClassName= "NetworkVideoObjectSystem";
	virtual std::string getObjectSystemClassName() const { return k_objectSystemClassName; }

	virtual void update(float deltaTime) override;
	virtual void dispose() override;
	virtual bool isLoading() const override
	{
		return m_networkVideoManagerState == eNetworkVideoManagerState::initializing;
	}

	INetworkVideoDeviceManagerPtr getNetworkVideoDeviceManager() const { return m_networkVideoDeviceManager; }

	enum class eNetworkVideoManagerState
	{
		uninitialized,
		initializing,
		ready,
		failed
	};
	eNetworkVideoManagerState getNetworkVideoManagerState() const { return m_networkVideoManagerState; }

	VideoSourceIdList getVideoSourceIdList() const;

	// RTP video senders come from the module this system owns, so they are created
	// here and closed before the system goes away
	IRtpVideoSenderPtr createRtpVideoSender();
	// False while the module is still loading or failed to load, with the reason
	bool isRtpVideoSenderAvailable(std::string& outReason);

protected:
	bool ensureNetworkDeviceManager();
	void disposeNetworkVideoDeviceManager();

private:
	struct NetworkVideoDeviceManagerInitResult
	{
		class INetworkVideoDeviceModule* module= nullptr;
		INetworkVideoDeviceManagerPtr manager;
	};
	static NetworkVideoDeviceManagerInitResult initNetworkVideoDeviceManagerOnThread(const std::string& moduleName);

	eNetworkVideoManagerState m_networkVideoManagerState= eNetworkVideoManagerState::uninitialized;
	std::future<NetworkVideoDeviceManagerInitResult> m_networkVideoManagerFuture;
	class INetworkVideoDeviceModule* m_networkVideoDeviceModule= nullptr;
	INetworkVideoDeviceManagerPtr m_networkVideoDeviceManager= nullptr;
	std::string m_networkVideoManagerFailReason;
	std::vector<std::weak_ptr<IRtpVideoSender>> m_rtpVideoSenders;
};

using NetworkVideoSourceSystemPtr= std::shared_ptr<NetworkVideoSourceSystem>;

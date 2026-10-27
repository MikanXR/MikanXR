#pragma once

#include "MikanCoreTypes.h"
#include "MikanClientTypes.h"
#include "MikanPropertyTypes.h"
#include "MikanTypeFwd.h"

#include <set>
#include <string>

class PropertyNotifyDatabase;
using PropertyNotifyDatabasePtr= std::shared_ptr<class PropertyNotifyDatabase>;

class MikanClientConnectionState
{
public:
	MikanClientConnectionState(class MikanServer* ownerServer, const std::string& connectionId);
	virtual ~MikanClientConnectionState();

	inline class MikanServer* getOwnerServer() const { return m_ownerServer; }
	inline const std::string& getConnectionId() const { return m_connectionId; }
	inline const struct MikanClientInfo& getMikanClientInfo() const { return m_clientInfo; }
	const char* getClientId() const;
	bool isClientInfoValid() const;
	inline class RenderTargetClientState* getRenderTargetClientState() const { return m_renderTargetClientState; }

	void setMikanClientInfo(const struct MikanClientInfo& clientInfo);
	void clearMikanClientInfo();

	// Event Publishing
	void publishMikanJsonEvent(const std::string& mikanJsonEvent);

	// Property Events
	bool setPropertyNotifyMode(const std::string& systemFilter, const std::string& componentFilter,
							   const std::string& propertyFilter, MikanPropertyNotifyMode notifyMode);
	void publishPropertyChangedEvent(const MikanPropertyValue& propertyValue);

	// Video sources this connection asked the editor to watch for the origin marker
	void addAlignmentWatch(MikanVideoSourceID videoSourceId) { m_alignmentWatches.insert(videoSourceId); }
	void removeAlignmentWatch(MikanVideoSourceID videoSourceId) { m_alignmentWatches.erase(videoSourceId); }
	bool hasAlignmentWatch(MikanVideoSourceID videoSourceId) const
	{
		return m_alignmentWatches.count(videoSourceId) > 0;
	}

private:
	class MikanServer* m_ownerServer;
	std::string m_connectionId;
	MikanClientInfo m_clientInfo;
	class RenderTargetClientState* m_renderTargetClientState= nullptr;
	PropertyNotifyDatabasePtr m_propertyNotifyDatabase;
	std::set<MikanVideoSourceID> m_alignmentWatches;
};

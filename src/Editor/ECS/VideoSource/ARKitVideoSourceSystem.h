#pragma once

#include "ARKitVideoDeviceManagerLoader.h"
#include "ARKitVideoSourceComponent.h"
#include "ComponentFwd.h"
#include "IARKitVideoDeviceManager.h"
#include "MikanTypedObjectSystem.h"
#include "MikanVideoSourceTypes.h"
#include "ObjectSystemConfigFwd.h"
#include "VideoSourceQueries.h"

#include <string>

class ARKitVideoSourceSystemDefinition
	: public MikanTypedObjectSystemDefinition<ARKitVideoSourceComponent, ARKitVideoSourceDefinition, MikanVideoSourceID>
{
public:
	using Super=
		MikanTypedObjectSystemDefinition<ARKitVideoSourceComponent, ARKitVideoSourceDefinition, MikanVideoSourceID>;

	ARKitVideoSourceSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
};

// ECS object system for ARKit video sources (ticket E1). The async MikanARKitVideo
// plugin/device-manager loading itself is delegated to ARKitVideoDeviceManagerLoader
// (formerly this class, before ticket E1 - see that class's header comment) rather
// than reimplemented inline, so its existing test coverage kept working unchanged.
class ARKitVideoSourceSystem
	: public MikanTypedObjectSystem<ARKitVideoSourceComponent, ARKitVideoSourceDefinition, MikanVideoSourceID,
									ARKitVideoSourceSystem, ARKitVideoSourceSystemDefinition>
{
public:
	using Super= MikanTypedObjectSystem<ARKitVideoSourceComponent, ARKitVideoSourceDefinition, MikanVideoSourceID,
										ARKitVideoSourceSystem, ARKitVideoSourceSystemDefinition>;

	ARKitVideoSourceSystem(ProjectManagerPtr ownerObjectSystem);

	inline static const std::string k_objectSystemClassName= "ARKitVideoObjectSystem";
	virtual std::string getObjectSystemClassName() const { return k_objectSystemClassName; }

	virtual void update(float deltaTime) override;
	virtual ~ARKitVideoSourceSystem();

	virtual void dispose() override;
	virtual bool isLoading() const override { return m_deviceManagerLoader.isLoading(); }

	IARKitVideoDeviceManagerPtr getARKitVideoDeviceManager() const
	{
		return m_deviceManagerLoader.getARKitVideoDeviceManager();
	}
	ARKitVideoDeviceManagerLoader::eARKitVideoManagerState getARKitVideoManagerState() const
	{
		return m_deviceManagerLoader.getARKitVideoManagerState();
	}

	VideoSourceIdList getVideoSourceIdList() const;

private:
	ARKitVideoDeviceManagerLoader m_deviceManagerLoader;
};

using ARKitVideoSourceSystemPtr= std::shared_ptr<ARKitVideoSourceSystem>;

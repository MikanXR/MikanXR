#include "ARKitVideoSourceSystem.h"
#include "ARKitVideoSourceComponent.h"

// -- ARKitVideoSourceSystemDefinition -----
ARKitVideoSourceSystemDefinition::ARKitVideoSourceSystemDefinition(const std::string& configName,
																   IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

configuru::Config ARKitVideoSourceSystemDefinition::writeToJSON()
{
	configuru::Config pt= Super::writeToJSON();

	return pt;
}

void ARKitVideoSourceSystemDefinition::readFromJSON(const configuru::Config& pt) { Super::readFromJSON(pt); }

// -- ARKitVideoSourceSystem -----
ARKitVideoSourceSystem::ARKitVideoSourceSystem(ProjectManagerPtr ownerObjectSystem)
	: Super::MikanTypedObjectSystem(ownerObjectSystem)
{
	// Retry openVideoSource() on any components that were waiting once the manager
	// becomes ready - the ECS-component-map-walk equivalent of
	// NetworkVideoSourceSystem's inline retry loop, expressed through
	// ARKitVideoDeviceManagerLoader's manager-ready-callback mechanism since the
	// async load itself lives there now (see that class's header comment).
	m_deviceManagerLoader.addManagerReadyCallback(
		[this]
		{
			for (const auto& [id, weakComp] : Super::getComponentMap())
			{
				if (auto comp= weakComp.lock(); comp && comp->isPendingOpen())
					comp->openVideoSource();
			}
		});
}

void ARKitVideoSourceSystem::update(float deltaTime)
{
	m_deviceManagerLoader.update(deltaTime);

	Super::update(deltaTime);
}

// The device manager and its plugin live for the process, not the project:
// the components close their devices in Super::dispose(), and the loader is
// torn down with the system at app shutdown, after every device is gone
void ARKitVideoSourceSystem::dispose() { Super::dispose(); }

ARKitVideoSourceSystem::~ARKitVideoSourceSystem() { m_deviceManagerLoader.dispose(); }

VideoSourceIdList ARKitVideoSourceSystem::getVideoSourceIdList() const
{
	VideoSourceIdList videoSourceIdList;
	for (const auto& it : Super::getComponentMap())
	{
		ARKitVideoSourceComponentPtr componentPtr= it.second.lock();
		if (componentPtr)
		{
			videoSourceIdList.push_back(componentPtr->getVideoSourceId());
		}
	}
	return videoSourceIdList;
}

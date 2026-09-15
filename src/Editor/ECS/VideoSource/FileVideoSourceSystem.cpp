#include "FileVideoSourceSystem.h"
#include "FileVideoSourceComponent.h"

// -- FileVideoSourceSystemDefinition -----
FileVideoSourceSystemDefinition::FileVideoSourceSystemDefinition(const std::string& configName,
																 IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

configuru::Config FileVideoSourceSystemDefinition::writeToJSON()
{
	configuru::Config pt= Super::writeToJSON();

	return pt;
}

void FileVideoSourceSystemDefinition::readFromJSON(const configuru::Config& pt) { Super::readFromJSON(pt); }

// -- FileVideoSourceSystem -----
FileVideoSourceSystem::FileVideoSourceSystem(ProjectManagerPtr ownerObjectSystem)
	: Super::MikanTypedObjectSystem(ownerObjectSystem)
{
}

VideoSourceIdList FileVideoSourceSystem::getVideoSourceIdList() const
{
	VideoSourceIdList videoSourceIdList;
	for (const auto& it : Super::getComponentMap())
	{
		FileVideoSourceComponentPtr componentPtr= it.second.lock();
		if (componentPtr)
		{
			videoSourceIdList.push_back(componentPtr->getVideoSourceId());
		}
	}
	return videoSourceIdList;
}

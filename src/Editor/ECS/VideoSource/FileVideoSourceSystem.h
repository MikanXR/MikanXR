#pragma once

#include "ComponentFwd.h"
#include "FileVideoSourceComponent.h"
#include "MikanTypedObjectSystem.h"
#include "MikanVideoSourceTypes.h"
#include "ObjectSystemConfigFwd.h"
#include "VideoSourceQueries.h"

#include <string>

class FileVideoSourceSystemDefinition
	: public MikanTypedObjectSystemDefinition<FileVideoSourceComponent, FileVideoSourceDefinition, MikanVideoSourceID>
{
public:
	using Super=
		MikanTypedObjectSystemDefinition<FileVideoSourceComponent, FileVideoSourceDefinition, MikanVideoSourceID>;

	FileVideoSourceSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
};

// File video sources decode in-process, so unlike the other video source
// systems there is no plugin module to load and nothing to wait on
class FileVideoSourceSystem
	: public MikanTypedObjectSystem<FileVideoSourceComponent, FileVideoSourceDefinition, MikanVideoSourceID,
									FileVideoSourceSystem, FileVideoSourceSystemDefinition>
{
public:
	using Super= MikanTypedObjectSystem<FileVideoSourceComponent, FileVideoSourceDefinition, MikanVideoSourceID,
										FileVideoSourceSystem, FileVideoSourceSystemDefinition>;

	FileVideoSourceSystem(ProjectManagerPtr ownerObjectSystem);

	inline static const std::string k_objectSystemClassName= "FileVideoSourceSystem";
	virtual std::string getObjectSystemClassName() const { return k_objectSystemClassName; }

	VideoSourceIdList getVideoSourceIdList() const;
};

using FileVideoSourceSystemPtr= std::shared_ptr<FileVideoSourceSystem>;

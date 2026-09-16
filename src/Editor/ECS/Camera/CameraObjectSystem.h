#pragma once

#include "CameraComponent.h"
#include "ComponentFwd.h"
#include "MikanTypeFwd.h"
#include "MikanTypedObjectSystem.h"
#include "ObjectSystemFwd.h"
#include "ObjectSystemConfigFwd.h"

class CameraObjectSystemDefinition
	: public MikanTypedObjectSystemDefinition<CameraComponent, CameraDefinition, MikanCameraID>
{
public:
	using Super= MikanTypedObjectSystemDefinition<CameraComponent, CameraDefinition, MikanCameraID>;

	CameraObjectSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator);
};

class CameraObjectSystem : public MikanTypedObjectSystem<CameraComponent, CameraDefinition, MikanCameraID,
														 CameraObjectSystem, CameraObjectSystemDefinition>
{
public:
	using Super= MikanTypedObjectSystem<CameraComponent, CameraDefinition, MikanCameraID, CameraObjectSystem,
										CameraObjectSystemDefinition>;

	CameraObjectSystem(ProjectManagerPtr ownerObjectSystem);

	inline static const std::string k_objectSystemClassName= "CameraObjectSystem";
	virtual std::string getObjectSystemClassName() const { return k_objectSystemClassName; }

	inline CameraComponentPtr getCameraById(MikanCameraID cameraId) const
	{
		return Super::getTypedComponentById(cameraId);
	}
	inline CameraComponentPtr getCameraByName(const std::string& cameraName) const
	{
		return Super::getTypedComponentByName(cameraName);
	}
	// The first camera whose definition names the video source, null when none does
	inline CameraComponentPtr findCameraForVideoSource(MikanVideoSourceID videoSourceId) const
	{
		return Super::getTypedComponentByPredicate(
			[videoSourceId](CameraComponentConstPtr camera)
			{ return camera->getCameraDefinition()->getVideoSourceId() == videoSourceId; });
	}

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);
};
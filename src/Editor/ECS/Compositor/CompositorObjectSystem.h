#pragma once

#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "CompositorComponent.h"
#include "MikanTypeFwd.h"
#include "MikanTypedObjectSystem.h"
#include "MulticastDelegate.h"
#include "ObjectSystemFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "SceneFwd.h"

#include <memory>
#include <string>
#include <vector>

class CompositorObjectSystemDefinition
	: public MikanTypedObjectSystemDefinition<CompositorComponent, CompositorDefinition, MikanCompositorID>
{
public:
	using Super= MikanTypedObjectSystemDefinition<CompositorComponent, CompositorDefinition, MikanCompositorID>;

	CompositorObjectSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator);
};

class CompositorObjectSystem
	: public MikanTypedObjectSystem<CompositorComponent, CompositorDefinition, MikanCompositorID,
									CompositorObjectSystem, CompositorObjectSystemDefinition>
{
public:
	using Super= MikanTypedObjectSystem<CompositorComponent, CompositorDefinition, MikanCompositorID,
										CompositorObjectSystem, CompositorObjectSystemDefinition>;

	CompositorObjectSystem(ProjectManagerPtr ownerObjectSystem);

	inline static const std::string k_objectSystemClassName= "CompositorObjectSystem";
	virtual std::string getObjectSystemClassName() const { return k_objectSystemClassName; }

	inline CompositorComponentPtr getCompositorById(MikanCompositorID compositorId) const
	{
		return Super::getTypedComponentById(compositorId);
	}
	inline CompositorComponentPtr getCompositorByName(const std::string& compositorName) const
	{
		return Super::getTypedComponentByName(compositorName);
	}
	std::vector<MikanCompositorID> getCompositorIdListForStage(MikanStageID stageId) const;

	// The scene's display compositors. A compositor runs when it is one of
	// these or when a graph editor window holds it; only the scene's may
	// publish output.
	void setActiveCompositors(const std::vector<MikanCompositorID>& activeCompositorIdList);
	// Starts and stops compositors to match the scene list plus the editor holds
	void refreshRunningCompositors();
	MulticastDelegate<void(CompositorComponentPtr oldCompositor)> OnCompositorDeactivated;
	MulticastDelegate<void(CompositorComponentPtr newCompositor)> OnCompositorActivated;

	inline void setAllCompositorsPaused(bool bIsPaused) { m_bAllCompositorsPaused= bIsPaused; }

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);
	bool getAllCompositorsPaused() const { return m_bAllCompositorsPaused; }

private:
	std::vector<MikanCompositorID> m_sceneActiveCompositorIds;
	bool m_bAllCompositorsPaused= false;
};

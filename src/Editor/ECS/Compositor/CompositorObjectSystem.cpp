#include "App.h"
#include "MikanObject.h"
#include "MikanPropertyDatabase.h"
#include "OutputQueries.h"
#include "CompositorComponent.h"
#include "CompositorObjectSystem.h"
#include "ProjectConfig.h"
#include "SceneComponent.h"
#include "SceneObjectSystem.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

// -- CompositorObjectSystemDefinition -----
CompositorObjectSystemDefinition::CompositorObjectSystemDefinition(const std::string& configName,
																   IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

// -- CompositorObjectSystem -----
CompositorObjectSystem::CompositorObjectSystem(ProjectManagerPtr ownerObjectSystem)
	: Super::MikanTypedObjectSystem(ownerObjectSystem)
{
}

std::vector<MikanCompositorID> CompositorObjectSystem::getCompositorIdListForStage(MikanStageID stageId) const
{
	std::vector<MikanCompositorID> compositorIdList;

	for (const auto& compositorPair : Super::getComponentMap())
	{
		CompositorComponentPtr componentPtr= compositorPair.second.lock();

		if (componentPtr && componentPtr->getOwnerStageId() == stageId)
		{
			compositorIdList.push_back(compositorPair.first);
		}
	}

	return compositorIdList;
}

CompositorComponentPtr CompositorObjectSystem::getCompositorForScene(MikanSceneID sceneId) const
{
	if (sceneId == INVALID_MIKAN_ID)
		return CompositorComponentPtr();

	return getTypedComponentByPredicate(
		[sceneId](ComponentConstPtr compositor)
		{ return compositor->getCompositorDefinition()->getOwnerSceneId() == sceneId; });
}

CompositorComponentPtr CompositorObjectSystem::getCurrentSceneCompositor() const
{
	return getCompositorForScene(getObjectSystemOfType<SceneObjectSystem>()->getCurrentSceneId());
}

MikanComponentPtr CompositorObjectSystem::addNewObjectByUntypedDefinition(
	const std::string& primaryComponentClass, Serialization::PolymorphicObjectPtr initParams)
{
	return MikanComponentPtr();
}

MikanComponentPtr CompositorObjectSystem::addNewObjectWithDefaultDefinition(const std::string& primaryComponentClass)
{
	return MikanComponentPtr();
}

MikanComponentPtr CompositorObjectSystem::recreateObjectFromDefinitionJson(const configuru::Config& definitionConfig)
{
	MikanComponentPtr component= Super::recreateObjectFromDefinitionJson(definitionConfig);

	// An undo that brings back the current scene's compositor has to start it,
	// since the scene was activated before its compositor existed again
	auto compositor= std::static_pointer_cast<CompositorComponent>(component);
	SceneComponentPtr ownerScene= compositor ? compositor->getOwnerSceneComponent() : nullptr;
	if (ownerScene && ownerScene->isCurrentScene())
	{
		ownerScene->refreshActiveCompositors();
	}

	return component;
}

void CompositorObjectSystem::setActiveCompositors(const std::vector<MikanCompositorID>& activeCompositorIdList)
{
	m_sceneActiveCompositorIds= activeCompositorIdList;
	refreshRunningCompositors();
}

void CompositorObjectSystem::refreshRunningCompositors()
{
	// Iterate through all compositor components
	for (const auto& compositorPair : Super::getComponentMap())
	{
		MikanCompositorID compositorId= compositorPair.first;
		CompositorComponentPtr compositor= compositorPair.second.lock();

		if (compositor)
		{
			// The current scene's compositor runs and publishes. A compositor an
			// editor holds runs for its frames and evaluation but keeps its output down.
			const bool bSceneActive=
				std::find(m_sceneActiveCompositorIds.begin(), m_sceneActiveCompositorIds.end(), compositorId)
				!= m_sceneActiveCompositorIds.end();
			bool shouldBeActive= bSceneActive || compositor->getIsEditorHeld();
			bool isCurrentlyRunning= compositor->getIsRunning();
			compositor->setOutputStreamingAllowed(bSceneActive);

			if (shouldBeActive && !isCurrentlyRunning)
			{
				// Start the compositor if it should be active but isn't running
				if (compositor->start())
				{
					OnCompositorActivated(compositor);
				}
			}
			else if (!shouldBeActive && isCurrentlyRunning)
			{
				// Stop the compositor if it shouldn't be active but is running
				compositor->stop();
				OnCompositorDeactivated(compositor);
			}
		}
	}

	closeOutputsIfNothingPublishes();
}

void CompositorObjectSystem::closeOutputsIfNothingPublishes()
{
	for (const auto& compositorPair : Super::getComponentMap())
	{
		CompositorComponentPtr compositor= compositorPair.second.lock();
		if (compositor && compositor->getIsRunning() && compositor->getIsOutputStreamingAllowed())
			return;
	}

	OutputQueries::closeOutputs(getOwnerProjectManager());
}
// -- Lua Binding ----
void CompositorObjectSystem::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.beginClass<CompositorObjectSystem>("CompositorObjectSystem")
		.addFunction("getCompositorById", [](CompositorObjectSystem* s, int id) -> CompositorComponent*
					 { return s->getCompositorById(static_cast<MikanCompositorID>(id)).get(); })
		.addFunction("getCompositorByName",
					 [](CompositorObjectSystem* s, const std::string& name) -> CompositorComponent*
					 { return s->getCompositorByName(name).get(); })
		.addFunction("getCompositorCount",
					 [](CompositorObjectSystem* s) -> int { return static_cast<int>(s->getComponentMap().size()); })
		.addFunction("getCompositorAtIndex",
					 [](CompositorObjectSystem* s, int i) -> CompositorComponent*
					 {
						 int n= 0;
						 for (auto& [id, wp] : s->getComponentMap())
							 if (n++ == i)
								 return wp.lock().get();
						 return nullptr;
					 })
		.addFunction(
			"getCompositorCountForStage", [](CompositorObjectSystem* s, int stageId) -> int
			{ return static_cast<int>(s->getCompositorIdListForStage(static_cast<MikanStageID>(stageId)).size()); })
		.addFunction("getCompositorForStageAtIndex",
					 [](CompositorObjectSystem* s, int stageId, int i) -> CompositorComponent*
					 {
						 auto ids= s->getCompositorIdListForStage(static_cast<MikanStageID>(stageId));
						 if (i >= 0 && i < static_cast<int>(ids.size()))
							 return s->getCompositorById(ids[i]).get();
						 return nullptr;
					 })
		.endClass();
}

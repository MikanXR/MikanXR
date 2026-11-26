#include "App.h"
#include "CompositorComponent.h"
#include "CompositorObjectSystem.h"
#include "IEditorWindow.h"
#include "LightEnvironmentComponent.h"
#include "LightEnvironmentSystem.h"
#include "MikanObject.h"
#include "MikanPropertyDatabase.h"
#include "SceneComponent.h"
#include "SceneObjectSystem.h"
#include "ProjectConfig.h"
#include "TransactionHistory.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

// -- SceneObjectSystemDefinition -----
const std::string SceneObjectSystemDefinition::k_currentSceneIdPropertyId= "current_scene_id";

SceneObjectSystemDefinition::SceneObjectSystemDefinition(const std::string& configName,
														 IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

configuru::Config SceneObjectSystemDefinition::writeToJSON()
{
	configuru::Config pt= Super::writeToJSON();

	pt[k_currentSceneIdPropertyId]= m_currentSceneId;

	return pt;
}

void SceneObjectSystemDefinition::readFromJSON(const configuru::Config& pt)
{
	Super::readFromJSON(pt);

	m_currentSceneId=
		pt.get_or<MikanSceneID>(SceneObjectSystemDefinition::k_currentSceneIdPropertyId, m_currentSceneId);
}

void SceneObjectSystemDefinition::setCurrentSceneId(MikanSceneID sceneId)
{
	if (sceneId != m_currentSceneId)
	{
		m_currentSceneId= sceneId;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_currentSceneIdPropertyId));
	}
}

// -- SceneObjectSystem -----
SceneObjectSystem::SceneObjectSystem(ProjectManagerPtr ownerObjectSystem)
	: Super::MikanTypedObjectSystem(ownerObjectSystem)
{
}

bool SceneObjectSystem::init(MikanObjectSystemDefinitionPtr definitionPtr)
{
	Super::init(definitionPtr);

	// Listen for project load completion to activate the current scene
	getOwnerProjectManager()->OnProjectLoaded+= MakeDelegate(this, &SceneObjectSystem::onProjectLoaded);

	return true;
}

void SceneObjectSystem::onProjectLoaded(ProjectManagerPtr newProject)
{
	SceneObjectSystemDefinitionPtr sceneSystemConfig= getTypedDefinition();
	MikanSceneID currentSceneId= sceneSystemConfig->getCurrentSceneId();

	if (currentSceneId != INVALID_MIKAN_ID)
	{
		SceneComponentPtr currentScene= getSceneById(currentSceneId);
		if (currentScene)
		{
			currentScene->activateScene();

			if (OnSceneActivated)
			{
				OnSceneActivated(currentScene);
			}
		}
	}
	else if (sceneSystemConfig->hasDefinitions())
	{
		SceneComponentDefinitionPtr sceneDefinitionPtr= sceneSystemConfig->getAllDefinitions().front();

		setCurrentSceneById(sceneDefinitionPtr->getSceneId());
	}
}

void SceneObjectSystem::dispose()
{
	SceneComponentPtr sceneComponent= getCurrentScene();
	if (sceneComponent)
	{
		sceneComponent->deactivateScene();
	}

	// Stop listening for project load events
	getOwnerProjectManager()->OnProjectLoaded-= MakeDelegate(this, &SceneObjectSystem::onProjectLoaded);

	Super::dispose();
}

SceneComponentPtr SceneObjectSystem::addNewScene(DefinitionInitFunction definitionInit)
{
	IEditorWindow* ownerWindow= getOwnerWindow();
	TransactionCompositeScope compositeScope(ownerWindow ? ownerWindow->getTransactionHistory() : nullptr);

	SceneComponentPtr scene= addNewObjectByTypedDefinition(definitionInit);
	if (!scene)
		return scene;

	const MikanSceneID sceneId= scene->getSceneId();
	getObjectSystemOfType<CompositorObjectSystem>()->addNewObjectByTypedDefinition(
		[sceneId](auto definition)
		{
			definition->setOwnerSceneId(sceneId);
			return true;
		});

	if (scene->isCurrentScene())
	{
		scene->refreshActiveCompositors();
	}

	return scene;
}

MikanComponentPtr SceneObjectSystem::addNewObjectByUntypedDefinition(const std::string& primaryComponentClass,
																	 Serialization::PolymorphicObjectPtr initParams)
{
	if (primaryComponentClass != SceneComponent::k_componentClassName)
		return MikanComponentPtr();

	MikanObjectSystem* ownerObjectSystem= this;

	return addNewScene([ownerObjectSystem, &initParams](SceneComponentDefinitionPtr definition)
					   { return definition->readFromInitParams(ownerObjectSystem, initParams); });
}

MikanComponentPtr SceneObjectSystem::addNewObjectWithDefaultDefinition(const std::string& primaryComponentClass)
{
	if (primaryComponentClass != SceneComponent::k_componentClassName)
		return MikanComponentPtr();

	return addNewScene();
}

bool SceneObjectSystem::deleteObject(MikanObjectPtr objectPtr)
{
	auto scene= objectPtr->getComponentOfType<SceneComponent>();
	if (!scene)
		return false;

	IEditorWindow* ownerWindow= getOwnerWindow();
	TransactionCompositeScope compositeScope(ownerWindow ? ownerWindow->getTransactionHistory() : nullptr);

	// Never leave current_scene_id pointing at a dead scene
	if (scene->isCurrentScene())
	{
		setCurrentSceneById(INVALID_MIKAN_ID);
	}

	CompositorComponentPtr compositor= scene->getCompositor();
	if (compositor)
	{
		getObjectSystemOfType<CompositorObjectSystem>()->removeObjectByPrimaryComponentId(
			compositor->getCompositorId());
	}

	LightEnvironmentComponentPtr probe= scene->getLightEnvironment();
	if (probe)
	{
		getObjectSystemOfType<LightEnvironmentSystem>()->removeObjectByPrimaryComponentId(probe->getComponentId());
	}

	return Super::deleteObject(objectPtr);
}

MikanSceneID SceneObjectSystem::getCurrentSceneId() const
{
	SceneObjectSystemDefinitionConstPtr sceneSystemConfigPtr= getTypedDefinitionConst();
	if (sceneSystemConfigPtr)
	{
		return sceneSystemConfigPtr->getCurrentSceneId();
	}
	return INVALID_MIKAN_ID;
}

SceneComponentPtr SceneObjectSystem::getCurrentScene() const { return getSceneById(getCurrentSceneId()); }

void SceneObjectSystem::setCurrentScene(SceneComponentPtr newScene) { setCurrentSceneById(newScene->getSceneId()); }

void SceneObjectSystem::setCurrentSceneById(MikanSceneID newSceneId)
{
	SceneObjectSystemDefinitionPtr sceneSystemConfig= getTypedDefinition();
	if (sceneSystemConfig)
	{
		MikanSceneID currentSceneId= sceneSystemConfig->getCurrentSceneId();
		if (currentSceneId != newSceneId)
		{
			SceneComponentPtr currentScene= getSceneById(currentSceneId);
			if (currentScene)
			{
				currentScene->deactivateScene();

				if (OnSceneDeactivated)
				{
					OnSceneDeactivated(currentScene);
				}
			}

			sceneSystemConfig->setCurrentSceneId(newSceneId);
			SceneComponentPtr newScene= getSceneById(newSceneId);
			if (newScene)
			{
				newScene->activateScene();

				if (OnSceneActivated)
				{
					OnSceneActivated(newScene);
				}
			}
			else
			{
				// With no scene current nothing composites or publishes. A switch to
				// another scene skips this, so the outputs' senders stay open across it.
				getObjectSystemOfType<CompositorObjectSystem>()->setActiveCompositors({});
			}
		}
	}
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle SceneObjectSystem::getClientAPIValuesStructType() const
{
	return &MikanSceneSystemValues::staticGetArchetype();
}

// -- IPropertyInterface ----
void SceneObjectSystem::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	Super::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
		SceneObjectSystemDefinition::k_currentSceneIdPropertyId, MikanVariantType::INT));
}

bool SceneObjectSystem::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == SceneObjectSystemDefinition::k_currentSceneIdPropertyId)
	{
		outValue= getCurrentSceneId();
		return true;
	}

	return Super::getPropertyValue(propertyName, outValue);
}

bool SceneObjectSystem::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == SceneObjectSystemDefinition::k_currentSceneIdPropertyId)
	{
		MikanSceneID targetSceneId= inValue.getIntValue();
		setCurrentSceneById(targetSceneId);
		return true;
	}

	return MikanObjectSystem::setPropertyValue(propertyName, inValue);
}
// -- Lua Binding ----
void SceneObjectSystem::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.beginClass<SceneObjectSystem>("SceneObjectSystem")
		.addFunction("getCurrentScene",
					 [](SceneObjectSystem* s) -> SceneComponent* { return s->getCurrentScene().get(); })
		.addFunction("getSceneById", [](SceneObjectSystem* s, int id) -> SceneComponent*
					 { return s->getSceneById(static_cast<MikanSceneID>(id)).get(); })
		.addFunction("getSceneByName", [](SceneObjectSystem* s, const std::string& name) -> SceneComponent*
					 { return s->getSceneByName(name).get(); })
		.addFunction("getSceneCount",
					 [](SceneObjectSystem* s) -> int { return static_cast<int>(s->getComponentMap().size()); })
		.addFunction("getSceneAtIndex",
					 [](SceneObjectSystem* s, int i) -> SceneComponent*
					 {
						 int n= 0;
						 for (auto& [id, wp] : s->getComponentMap())
							 if (n++ == i)
								 return wp.lock().get();
						 return nullptr;
					 })
		// Both setters resolve the scene first. setCurrentSceneById accepts an id that
		// names no scene, which deactivates the current scene and leaves the project
		// with none; a script that mistyped an id should change nothing instead.
		.addFunction("setCurrentSceneById",
					 [](SceneObjectSystem* s, int id)
					 {
						 const MikanSceneID sceneId= static_cast<MikanSceneID>(id);
						 if (s->getSceneById(sceneId))
							 s->setCurrentSceneById(sceneId);
					 })
		// Routed through the id rather than SceneObjectSystem::setCurrentScene, which
		// dereferences its argument
		.addFunction("setCurrentScene",
					 [](SceneObjectSystem* s, SceneComponent* scene)
					 {
						 if (scene != nullptr)
							 s->setCurrentSceneById(scene->getSceneId());
					 })
		.endClass();
}

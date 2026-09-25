#pragma once

#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "MikanTypeFwd.h"
#include "MikanObjectSystem.h"
#include "MikanTypedObjectSystemDefinition.h"
#include "MikanTypedObjectSystem.h"
#include "MulticastDelegate.h"
#include "ObjectSystemFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "SceneComponent.h"
#include "SceneFwd.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/ext/matrix_float4x4.hpp>

class GlmTransform;

class SceneObjectSystemDefinition
	: public MikanTypedObjectSystemDefinition<SceneComponent, SceneComponentDefinition, MikanSceneID>
{
public:
	using Super= MikanTypedObjectSystemDefinition<SceneComponent, SceneComponentDefinition, MikanSceneID>;

	SceneObjectSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);

	static const std::string k_currentSceneIdPropertyId;
	inline MikanSceneID getCurrentSceneId() const { return m_currentSceneId; }
	void setCurrentSceneId(MikanSceneID sceneId);

private:
	MikanSceneID m_currentSceneId= -1;
};

class SceneObjectSystem : public MikanTypedObjectSystem<SceneComponent, SceneComponentDefinition, MikanSceneID,
														SceneObjectSystem, SceneObjectSystemDefinition>
{
public:
	using Super= MikanTypedObjectSystem<SceneComponent, SceneComponentDefinition, MikanSceneID, SceneObjectSystem,
										SceneObjectSystemDefinition>;

	SceneObjectSystem(ProjectManagerPtr ownerObjectSystem);

	inline static const std::string k_objectSystemClassName= "SceneObjectSystem";
	virtual std::string getObjectSystemClassName() const { return k_objectSystemClassName; }

	virtual bool init(MikanObjectSystemDefinitionPtr definitionPtr) override;
	virtual void dispose() override;

	MikanSceneID getCurrentSceneId() const;
	SceneComponentPtr getCurrentScene() const;
	void setCurrentScene(SceneComponentPtr scene);
	void setCurrentSceneById(MikanSceneID sceneId);

	inline SceneComponentPtr getSceneById(MikanSceneID sceneId) const { return Super::getTypedComponentById(sceneId); }
	inline SceneComponentPtr getSceneByName(const std::string& sceneName) const
	{
		return Super::getTypedComponentByName(sceneName);
	}

	MulticastDelegate<void(SceneComponentPtr oldScene)> OnSceneDeactivated;
	MulticastDelegate<void(SceneComponentPtr newScene)> OnSceneActivated;

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

protected:
	void onProjectLoaded(ProjectManagerPtr newProject);
};
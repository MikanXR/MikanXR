#pragma once

#include "ComponentFwd.h"
#include "ColliderQuery.h"
#include "ObjectFwd.h"
#include "MikanSceneTypes.h"
#include "MikanRendererFwd.h"
#include "TransformComponent.h"

#include <functional>

class SceneComponentDefinition : public TransformComponentDefinition
{
public:
	SceneComponentDefinition();
	SceneComponentDefinition(MikanSceneID sceneId);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	MikanSceneID getSceneId() const { return getComponentId(); }

	static const std::string k_displayCompositorIdPropertyId;
	MikanCompositorID getDisplayCompositorId() const { return m_displayCompositorId; }
	void setDisplayCompositorId(MikanCompositorID compositorId);

	static const std::string k_forceRenderPropertyId;
	bool getForceRender() const { return m_bForceRender; }
	void setForceRender(bool bForceRender);

protected:
	MikanCompositorID m_displayCompositorId= INVALID_MIKAN_ID;
	bool m_bForceRender= false;
};

class SceneComponent final : public TransformComponent
{
public:
	SceneComponent(MikanObjectWeakPtr owner);

	inline SceneComponentDefinitionPtr getSceneComponentDefinition() const
	{
		return std::static_pointer_cast<SceneComponentDefinition>(m_definition);
	}

	inline static const std::string k_componentClassName= "SceneComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	// -- TransformComponent ----
	virtual bool canAttachToParent(TransformComponentConstPtr newParentComponent) const override;

	// -- IEntityAccessor ----
	virtual rfk::Struct const* getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static const std::string k_showCompositorOutputFunctionId;
	static const std::string k_activateSceneFunctionId;
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors);
	virtual bool invokeFunction(const std::string& functionName) override;

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

	// -- SceneComponent ----
	inline MikanSceneID getSceneId() const { return getSceneComponentDefinition()->getComponentId(); }
	bool isCurrentScene() const;

	// Whether this scene's subtree draws. Only the current scene does, unless the scene
	// asks to be drawn anyway, which is how two stages get aligned against each other.
	bool shouldRender() const;

	MikanStageID getParentStageId() const;
	StageComponentPtr getParentStage() const;
	std::vector<MikanCompositorID> getOutputCompositorIDs() const;
	std::vector<CompositorComponentPtr> getOutputCompositors() const;
	void attachToStage(MikanStageID newParentId);
	void showCompositorOutput();
	void activateScene();
	void deactivateScene();
	void refreshActiveCompositors();

protected:
	virtual void onDefinitionMarkedDirty(CommonConfigPtr configPtr,
										 const ConfigPropertyChangeSet& changedPropertySet) override;
};

// Whether an object's geometry takes part in the project viewport this frame, from the
// scene that owns it. An object under no scene (stage level, or unparented) always does.
// The render and the collider queries share this so picking never reaches hidden geometry.
bool isSceneGeometryRendered(MikanObjectConstPtr objectPtr);

// The scene an object hangs under, the first scene up its transform chain, or
// INVALID_MIKAN_ID for stage-level and unparented objects
MikanSceneID findOwnerSceneId(MikanObjectConstPtr objectPtr);

// The stage an object hangs under, through its scene when it has one, or INVALID_MIKAN_ID
MikanStageID findOwnerStageId(MikanObjectConstPtr objectPtr);
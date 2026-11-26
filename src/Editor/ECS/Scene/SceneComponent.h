#pragma once

#include "ComponentFwd.h"
#include "ColliderQuery.h"
#include "LightSystemFwd.h"
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

	// The camera the scene is shot through: its compositor takes its video and pose from it
	static const std::string k_cameraIdPropertyId;
	MikanCameraID getCameraId() const { return m_cameraId; }
	void setCameraId(MikanCameraID cameraId);

	// The lighting probe captured for the scene, at most one
	static const std::string k_lightEnvironmentIdPropertyId;
	MikanLightID getLightEnvironmentId() const { return m_lightEnvironmentId; }
	void setLightEnvironmentId(MikanLightID lightEnvironmentId);

	static const std::string k_forceRenderPropertyId;
	bool getForceRender() const { return m_bForceRender; }
	void setForceRender(bool bForceRender);

protected:
	MikanCameraID m_cameraId= INVALID_MIKAN_ID;
	MikanLightID m_lightEnvironmentId= INVALID_MIKAN_ID;
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
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static const std::string k_activateSceneFunctionId;
	static const std::string k_captureSceneLightingFunctionId;
	static const std::string k_captureDepthMeshFunctionId;
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
	CameraComponentPtr getCameraComponent() const;
	LightEnvironmentComponentPtr getLightEnvironment() const;

	// Opens the scene lighting capture tool through the scene's camera. The estimate
	// lands in the scene's probe, which is created on the first capture and
	// overwritten by every later one.
	void captureSceneLighting();
	// Opens the depth mesh capture tool through the scene's camera. The captured
	// stencil is parented to this scene.
	void captureDepthMesh();
	// The scene's one compositor, the compositor whose owner scene this is. Null only
	// between the scene's creation and its compositor's, and while an undo is mid-flight.
	CompositorComponentPtr getCompositor() const;
	void attachToStage(MikanStageID newParentId);
	void activateScene();
	void deactivateScene();
	void refreshActiveCompositors();

private:
	// The scene's camera, or null after telling the user the scene has none
	CameraComponentPtr requireCameraForCapture() const;
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
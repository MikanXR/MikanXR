#include "CameraComponent.h"
#include "CameraObjectSystem.h"
#include "CompositorObjectSystem.h"
#include "CompositorComponent.h"
#include "AppStage.h"
#include "ColliderComponent.h"
#include "DepthMeshCapture/AppStage_DepthMeshCapture.h"
#include "IEditorWindow.h"
#include "LightEnvironmentComponent.h"
#include "LightEnvironmentSystem.h"
#include "LocText.h"
#include "ModalMessageBox/ModalDialog_MessageBox.h"
#include "SceneLightingCapture/AppStage_SceneLightingCapture.h"
#include "TransactionHistory.h"
#include "IMkSceneRenderable.h"
#include "MikanObject.h"
#include "MikanSceneTypes.h"
#include "MkScene.h"
#include "MathTypeConversion.h"
#include "MathUtility.h"
#include "MikanCamera.h"
#include "SceneComponent.h"
#include "SelectionComponent.h"
#include "SceneObjectSystem.h"
#include "StageComponent.h"
#include "StageObjectSystem.h"
#include "TransformComponent.h"
#include "VRObjectSystem.h"
#include "VRTrackingVolumeComponent.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

#include <easy/profiler.h>

#include <queue>

// -- SceneComponentDefinition -----
const std::string SceneComponentDefinition::k_cameraIdPropertyId= "camera_id";
const std::string SceneComponentDefinition::k_lightEnvironmentIdPropertyId= "light_environment_id";
const std::string SceneComponentDefinition::k_forceRenderPropertyId= "force_render";

SceneComponentDefinition::SceneComponentDefinition()
	: TransformComponentDefinition()
{
}

SceneComponentDefinition::SceneComponentDefinition(MikanSceneID sceneId)
	: TransformComponentDefinition(sceneId, "", glm_transform_to_MikanTransform(GlmTransform()))
{
}

configuru::Config SceneComponentDefinition::writeToJSON()
{
	configuru::Config pt= TransformComponentDefinition::writeToJSON();

	pt[k_cameraIdPropertyId]= m_cameraId;
	pt[k_lightEnvironmentIdPropertyId]= m_lightEnvironmentId;
	pt[k_forceRenderPropertyId]= m_bForceRender;

	return pt;
}

void SceneComponentDefinition::readFromJSON(const configuru::Config& pt)
{
	TransformComponentDefinition::readFromJSON(pt);

	m_cameraId= pt.get_or<int>(k_cameraIdPropertyId, INVALID_MIKAN_ID);
	m_lightEnvironmentId= pt.get_or<int>(k_lightEnvironmentIdPropertyId, INVALID_MIKAN_ID);
	m_bForceRender= pt.get_or<bool>(k_forceRenderPropertyId, false);
}

bool SceneComponentDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
												  const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!TransformComponentDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanSceneComponentValues>();
	if (componentValues)
	{
		m_cameraId= componentValues->camera_id;
		m_lightEnvironmentId= componentValues->light_environment_id;
		m_bForceRender= componentValues->force_render;
	}

	return true;
}

void SceneComponentDefinition::setCameraId(MikanCameraID cameraId)
{
	if (m_cameraId != cameraId)
	{
		m_cameraId= cameraId;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_cameraIdPropertyId));
	}
}

void SceneComponentDefinition::setLightEnvironmentId(MikanLightID lightEnvironmentId)
{
	if (m_lightEnvironmentId != lightEnvironmentId)
	{
		m_lightEnvironmentId= lightEnvironmentId;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_lightEnvironmentIdPropertyId));
	}
}

void SceneComponentDefinition::setForceRender(bool bForceRender)
{
	if (m_bForceRender != bForceRender)
	{
		m_bForceRender= bForceRender;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_forceRenderPropertyId));
	}
}

// -- SceneComponent -----
SceneComponent::SceneComponent(MikanObjectWeakPtr owner)
	: TransformComponent(owner)
{
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle SceneComponent::getClientAPIValuesStructType() const
{
	return &MikanSceneComponentValues::staticGetArchetype();
}

bool SceneComponent::isCurrentScene() const
{
	SceneObjectSystemPtr sceneSystem= getObjectSystemOfType<SceneObjectSystem>();

	return sceneSystem && sceneSystem->getCurrentSceneId() == getSceneId();
}

bool SceneComponent::shouldRender() const
{
	return isCurrentScene() || getSceneComponentDefinition()->getForceRender();
}

MikanStageID SceneComponent::getParentStageId() const { return getSceneComponentDefinition()->getParentTransformId(); }

StageComponentPtr SceneComponent::getParentStage() const
{
	MikanStageID parentStageId= getParentStageId();
	if (parentStageId != INVALID_MIKAN_ID)
	{
		return getObjectSystemOfType<StageObjectSystem>()->getStageById(parentStageId);
	}

	return StageComponentPtr();
}

CameraComponentPtr SceneComponent::getCameraComponent() const
{
	return getObjectSystemOfType<CameraObjectSystem>()->getCameraById(getSceneComponentDefinition()->getCameraId());
}

LightEnvironmentComponentPtr SceneComponent::getLightEnvironment() const
{
	return getObjectSystemOfType<LightEnvironmentSystem>()->getTypedComponentById(
		getSceneComponentDefinition()->getLightEnvironmentId());
}

CompositorComponentPtr SceneComponent::getCompositor() const
{
	return getObjectSystemOfType<CompositorObjectSystem>()->getCompositorForScene(getSceneId());
}

// -- TransformComponent ----
bool SceneComponent::canAttachToParent(TransformComponentConstPtr newParentComponent) const
{
	return std::dynamic_pointer_cast<const StageComponent>(newParentComponent) != nullptr;
}

void SceneComponent::attachToStage(MikanStageID newParentId)
{
	auto stageSystem= getObjectSystemOfType<StageObjectSystem>();
	StageComponentPtr oldStage= stageSystem->getStageById(getParentStageId());
	StageComponentPtr newStage= stageSystem->getStageById(newParentId);

	if (newStage != oldStage)
	{
		// onAttachedToNewParent / onDetachedFromParent will update our parent stage ID
		if (newStage)
		{
			attachToComponent(newStage->getOwnerObject()->getRootComponent());
		}
		else if (oldStage)
		{
			detachFromParent(TransformComponent::eDetachReason::detachFromParent);
		}
	}
}

void SceneComponent::activateScene() { refreshActiveCompositors(); }

void SceneComponent::deactivateScene() {}

void SceneComponent::refreshActiveCompositors()
{
	CompositorObjectSystemPtr compositorSystem= getObjectSystemOfType<CompositorObjectSystem>();
	CompositorComponentPtr compositor= getCompositor();

	std::vector<MikanCompositorID> activeCompositorIDs;

	if (compositor)
	{
		activeCompositorIDs.push_back(compositor->getCompositorId());
	}

	// Set active compositors for this scene
	compositorSystem->setActiveCompositors(activeCompositorIDs);
}

void SceneComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	TransformComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(SceneComponentDefinition::k_cameraIdPropertyId, MikanVariantType::INT)
			->setDefaultValue(-1)
			->setUIHidden());

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 SceneComponentDefinition::k_lightEnvironmentIdPropertyId, MikanVariantType::INT)
								 ->setDefaultValue(-1)
								 ->setUIHidden());

	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(SceneComponentDefinition::k_forceRenderPropertyId, MikanVariantType::BOOL)
			->setDefaultValue(false));
}

bool SceneComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == SceneComponentDefinition::k_cameraIdPropertyId)
	{
		outValue= getSceneComponentDefinition()->getCameraId();
		return true;
	}

	if (propertyName == SceneComponentDefinition::k_lightEnvironmentIdPropertyId)
	{
		outValue= getSceneComponentDefinition()->getLightEnvironmentId();
		return true;
	}

	if (propertyName == SceneComponentDefinition::k_forceRenderPropertyId)
	{
		outValue= getSceneComponentDefinition()->getForceRender();
		return true;
	}

	return TransformComponent::getPropertyValue(propertyName, outValue);
}

bool SceneComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == SceneComponentDefinition::k_cameraIdPropertyId)
	{
		getSceneComponentDefinition()->setCameraId(inValue.getIntValue());
		return true;
	}

	if (propertyName == SceneComponentDefinition::k_lightEnvironmentIdPropertyId)
	{
		getSceneComponentDefinition()->setLightEnvironmentId(inValue.getIntValue());
		return true;
	}

	if (propertyName == SceneComponentDefinition::k_forceRenderPropertyId)
	{
		getSceneComponentDefinition()->setForceRender(inValue.getBoolValue());
		return true;
	}

	return TransformComponent::setPropertyValue(propertyName, inValue);
}

// -- IFunctionInterface ----
const std::string SceneComponent::k_activateSceneFunctionId= "activate_scene";
const std::string SceneComponent::k_captureSceneLightingFunctionId= "capture_scene_lighting";
const std::string SceneComponent::k_captureDepthMeshFunctionId= "capture_depth_mesh";

void SceneComponent::getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
{
	TransformComponent::getFunctionDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<FunctionDescriptor>(k_activateSceneFunctionId, "Activate Scene"));
	outDescriptors.push_back(
		std::make_shared<FunctionDescriptor>(k_captureSceneLightingFunctionId, "Capture Scene Lighting"));
	outDescriptors.push_back(std::make_shared<FunctionDescriptor>(k_captureDepthMeshFunctionId, "Capture Depth Mesh"));
}

bool SceneComponent::invokeFunction(const std::string& functionName)
{
	if (functionName == SceneComponent::k_activateSceneFunctionId)
	{
		SceneObjectSystemPtr sceneSystem= getObjectSystemOfType<SceneObjectSystem>();
		if (sceneSystem)
		{
			sceneSystem->setCurrentSceneById(getSceneId());
		}
		return true;
	}

	if (functionName == SceneComponent::k_captureSceneLightingFunctionId)
	{
		captureSceneLighting();
		return true;
	}

	if (functionName == SceneComponent::k_captureDepthMeshFunctionId)
	{
		captureDepthMesh();
		return true;
	}

	return TransformComponent::invokeFunction(functionName);
}

CameraComponentPtr SceneComponent::requireCameraForCapture() const
{
	CameraComponentPtr camera= getCameraComponent();
	if (!camera)
	{
		ModalDialog_MessageBox::showMessageBox(getOwnerEditorWindow()->getCurrentAppStage(),
											   locText("project.sceneCaptureNeedsCamera"));
	}

	return camera;
}

void SceneComponent::captureSceneLighting()
{
	// The estimate is only meaningful in stage space if the frame it was captured
	// from has a known camera pose, so the scene has to name its camera first
	CameraComponentPtr camera= requireCameraForCapture();
	if (!camera)
		return;

	LightEnvironmentComponentPtr probe= getLightEnvironment();
	if (!probe)
	{
		// The probe and the id that names it are one undo step
		TransactionCompositeScope compositeScope(getOwnerEditorWindow()->getTransactionHistory());

		const MikanSceneID sceneId= getSceneId();
		probe= getObjectSystemOfType<LightEnvironmentSystem>()->addNewObjectByTypedDefinition(
			[sceneId](auto definition)
			{
				definition->setParentTransformId(sceneId);
				definition->setRelativeTransform(GlmTransform());
				return true;
			});
		if (!probe)
			return;

		getSceneComponentDefinition()->setLightEnvironmentId(probe->getComponentId());
	}

	auto* captureStage= getOwnerEditorWindow()->pushAppStageOfType<AppStage_SceneLightingCapture>();
	if (captureStage != nullptr)
	{
		captureStage->setSourceCamera(camera);
		captureStage->setTargetProbe(probe);
	}
}

void SceneComponent::captureDepthMesh()
{
	CameraComponentPtr camera= requireCameraForCapture();
	if (!camera)
		return;

	auto* captureStage= getOwnerEditorWindow()->pushAppStageOfType<AppStage_DepthMeshCapture>();
	if (captureStage != nullptr)
	{
		captureStage->setSourceCamera(camera);
		captureStage->setTargetScene(getSelfPtr<SceneComponent>());
	}
}

// -- Lua Binding ----
void SceneComponent::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<SceneComponent, TransformComponent>(SceneComponent::k_componentClassName.c_str())
		.addProperty("parentStageId", [](SceneComponent* component) -> int { return component->getParentStageId(); })
		.addProperty(
			"cameraId",
			[](SceneComponent* component) -> int { return component->getSceneComponentDefinition()->getCameraId(); },
			[](SceneComponent* component, int cameraId)
			{ component->getSceneComponentDefinition()->setCameraId(cameraId); })
		.addProperty("lightEnvironmentId", [](SceneComponent* component) -> int
					 { return component->getSceneComponentDefinition()->getLightEnvironmentId(); })
		.addProperty(
			"forceRender", [](SceneComponent* component) -> bool
			{ return component->getSceneComponentDefinition()->getForceRender(); },
			[](SceneComponent* component, bool bForceRender)
			{ component->getSceneComponentDefinition()->setForceRender(bForceRender); })
		.addProperty("isCurrentScene", [](SceneComponent* component) -> bool { return component->isCurrentScene(); })
		.addFunction("getParentStage", [](SceneComponent* c) -> StageComponent* { return c->getParentStage().get(); })
		.addFunction("captureSceneLighting", [](SceneComponent* c) { c->captureSceneLighting(); })
		.addFunction("captureDepthMesh", [](SceneComponent* c) { c->captureDepthMesh(); })
		.addFunction("getCompositor",
					 [](SceneComponent* c) -> CompositorComponent* { return c->getCompositor().get(); })
		.endClass();
}

// -- Scene geometry render gate -----
bool isSceneGeometryRendered(MikanObjectConstPtr objectPtr)
{
	if (!objectPtr)
		return false;

	TransformComponentPtr transformComponent= objectPtr->getRootComponent();
	while (transformComponent)
	{
		auto sceneComponent= std::dynamic_pointer_cast<SceneComponent>(transformComponent);
		if (sceneComponent)
		{
			return sceneComponent->shouldRender();
		}

		transformComponent= transformComponent->getParentTransformComponent();
	}

	// Stage level and unparented geometry belongs to no scene, so no scene gates it
	return true;
}

MikanSceneID findOwnerSceneId(MikanObjectConstPtr objectPtr)
{
	TransformComponentPtr transformComponent= objectPtr ? objectPtr->getRootComponent() : nullptr;
	while (transformComponent)
	{
		auto sceneComponent= std::dynamic_pointer_cast<SceneComponent>(transformComponent);
		if (sceneComponent)
			return sceneComponent->getSceneId();

		transformComponent= transformComponent->getParentTransformComponent();
	}

	return INVALID_MIKAN_ID;
}

MikanStageID findOwnerStageId(MikanObjectConstPtr objectPtr)
{
	TransformComponentPtr transformComponent= objectPtr ? objectPtr->getRootComponent() : nullptr;
	while (transformComponent)
	{
		auto stageComponent= std::dynamic_pointer_cast<StageComponent>(transformComponent);
		if (stageComponent)
			return stageComponent->getStageId();

		transformComponent= transformComponent->getParentTransformComponent();
	}

	return INVALID_MIKAN_ID;
}
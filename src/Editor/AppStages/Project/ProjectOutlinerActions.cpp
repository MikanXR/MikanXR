#include "ProjectOutlinerActions.h"
#include "AnchorObjectSystem.h"
#include "ARKitVideoSourceSystem.h"
#include "FileVideoSourceSystem.h"
#include "BoxShapeSystem.h"
#include "BoxStencilSystem.h"
#include "CameraComponent.h"
#include "CameraObjectSystem.h"
#include "CEFTextureSourceSystem.h"
#include "ClientTextureSourceSystem.h"
#include "CompositorComponent.h"
#include "CompositorObjectSystem.h"
#include "DMXFixtureGroupComponent.h"
#include "DMXFixtureGroupSystem.h"
#include "DMXPresetSystem.h"
#include "DMXSequenceSystem.h"
#include "LightEnvironmentComponent.h"
#include "LightEnvironmentSystem.h"
#include "MarkerObjectSystem.h"
#include "MarkerTrackingVolumeComponent.h"
#include "MarkerTrackingVolumeSystem.h"
#include "MikanCoreTypes.h"
#include "MikanObject.h"
#include "ModelShapeSystem.h"
#include "ModelStencilSystem.h"
#include "NetworkVideoSourceSystem.h"
#include "ProjectManager.h"
#include "QuadShapeSystem.h"
#include "QuadStencilSystem.h"
#include "RGBPixelGridSystem.h"
#include "RGBSpotLightSystem.h"
#include "SceneObjectSystem.h"
#include "ScriptObjectSystem.h"
#include "SpoutOutputSystem.h"
#include "SpoutTextureSourceSystem.h"
#include "StageObjectSystem.h"
#include "TrackingMountComponent.h"
#include "TrackingMountObjectSystem.h"
#include "Transform.h"
#include "TransformComponent.h"
#include "USBVideoSourceSystem.h"
#include "VRTrackingVolumeComponent.h"
#include "VRTrackingVolumeSystem.h"

namespace ProjectOutlinerActions
{

static int componentIdOrInvalid(MikanComponentPtr component)
{
	return component ? component->getComponentId() : INVALID_MIKAN_ID;
}

int addUSBVideoSource(ProjectManagerPtr projectManager)
{
	auto sys= projectManager->getSystemOfType<USBVideoSourceSystem>();
	return sys ? componentIdOrInvalid(sys->addNewUSBVideoSource()) : INVALID_MIKAN_ID;
}

int addNetworkVideoSource(ProjectManagerPtr projectManager)
{
	auto sys= projectManager->getSystemOfType<NetworkVideoSourceSystem>();
	return sys ? componentIdOrInvalid(sys->addNewObjectByTypedDefinition()) : INVALID_MIKAN_ID;
}

int addARKitVideoSource(ProjectManagerPtr projectManager)
{
	auto sys= projectManager->getSystemOfType<ARKitVideoSourceSystem>();
	return sys ? componentIdOrInvalid(sys->addNewObjectByTypedDefinition()) : INVALID_MIKAN_ID;
}

int addFileVideoSource(ProjectManagerPtr projectManager)
{
	auto sys= projectManager->getSystemOfType<FileVideoSourceSystem>();
	return sys ? componentIdOrInvalid(sys->addNewObjectByTypedDefinition()) : INVALID_MIKAN_ID;
}

int addClientTextureSource(ProjectManagerPtr projectManager)
{
	auto sys= projectManager->getSystemOfType<ClientTextureSourceSystem>();
	return sys ? componentIdOrInvalid(sys->addNewObjectByTypedDefinition()) : INVALID_MIKAN_ID;
}

int addSpoutTextureSource(ProjectManagerPtr projectManager)
{
	auto sys= projectManager->getSystemOfType<SpoutTextureSourceSystem>();
	return sys ? componentIdOrInvalid(sys->addNewObjectByTypedDefinition()) : INVALID_MIKAN_ID;
}

int addCEFTextureSource(ProjectManagerPtr projectManager)
{
	auto sys= projectManager->getSystemOfType<CEFTextureSourceSystem>();
	return sys ? componentIdOrInvalid(sys->addNewObjectByTypedDefinition()) : INVALID_MIKAN_ID;
}

int addMarker(ProjectManagerPtr projectManager)
{
	auto sys= projectManager->getSystemOfType<MarkerObjectSystem>();
	return sys ? componentIdOrInvalid(sys->addNewObjectByTypedDefinition()) : INVALID_MIKAN_ID;
}

int addVRTrackingVolume(ProjectManagerPtr projectManager)
{
	auto sys= projectManager->getSystemOfType<VRTrackingVolumeSystem>();
	return sys ? componentIdOrInvalid(sys->addNewVRTrackingVolume(eTrackingRuntime::SteamVR)) : INVALID_MIKAN_ID;
}

int addMarkerTrackingVolume(ProjectManagerPtr projectManager)
{
	auto sys= projectManager->getSystemOfType<MarkerTrackingVolumeSystem>();
	return sys ? componentIdOrInvalid(sys->addNewObjectByTypedDefinition()) : INVALID_MIKAN_ID;
}

int addTrackingMount(ProjectManagerPtr projectManager, int vrVolumeId)
{
	auto volumeSystem= projectManager->getSystemOfType<VRTrackingVolumeSystem>();
	auto mountSystem= projectManager->getSystemOfType<TrackingMountObjectSystem>();
	if (!volumeSystem || !mountSystem)
		return INVALID_MIKAN_ID;

	VRTrackingVolumeComponentPtr vrVolume= volumeSystem->getTypedComponentById(vrVolumeId);
	if (!vrVolume)
		return INVALID_MIKAN_ID;

	TrackingMountComponentPtr mount= mountSystem->addNewObjectByTypedDefinition();
	if (!mount)
		return INVALID_MIKAN_ID;

	vrVolume->getVRTrackingVolumeDefinition()->addTrackingMountID(mount->getComponentId());

	return mount->getComponentId();
}

int addStage(ProjectManagerPtr projectManager, int trackingVolumeId)
{
	auto sys= projectManager->getSystemOfType<StageObjectSystem>();
	if (!sys)
		return INVALID_MIKAN_ID;

	return componentIdOrInvalid(sys->addNewObjectByTypedDefinition(
		[trackingVolumeId](auto def)
		{
			def->setTrackingVolumeId(trackingVolumeId);
			return true;
		}));
}

int addScene(ProjectManagerPtr projectManager, int stageId)
{
	auto sys= projectManager->getSystemOfType<SceneObjectSystem>();
	if (!sys)
		return INVALID_MIKAN_ID;

	return componentIdOrInvalid(sys->addNewScene(
		[stageId](auto def)
		{
			def->setParentTransformId(stageId);
			return true;
		}));
}

int addCamera(ProjectManagerPtr projectManager, int stageId)
{
	auto sys= projectManager->getSystemOfType<CameraObjectSystem>();
	if (!sys)
		return INVALID_MIKAN_ID;

	return componentIdOrInvalid(sys->addNewObjectByTypedDefinition(
		[stageId](auto def)
		{
			def->setRelativeTransform(GlmTransform());
			def->setOwnerStageId(stageId);
			def->setParentTransformId(stageId);
			return true;
		}));
}

int addSpotLight(ProjectManagerPtr projectManager, int stageId)
{
	auto sys= projectManager->getSystemOfType<RGBSpotLightSystem>();
	if (!sys)
		return INVALID_MIKAN_ID;

	return componentIdOrInvalid(sys->createLight(stageId, ""));
}

int addPixelGrid(ProjectManagerPtr projectManager, int stageId)
{
	auto sys= projectManager->getSystemOfType<RGBPixelGridSystem>();
	if (!sys)
		return INVALID_MIKAN_ID;

	return componentIdOrInvalid(sys->addNewObjectByTypedDefinition(
		[stageId](auto def)
		{
			def->setOwnerStageId(stageId);
			def->setParentTransformId(stageId);
			def->setRelativeTransform(GlmTransform());
			return true;
		}));
}

int addLightGroup(ProjectManagerPtr projectManager, int stageId)
{
	auto sys= projectManager->getSystemOfType<DMXFixtureGroupSystem>();
	if (!sys)
		return INVALID_MIKAN_ID;

	return componentIdOrInvalid(sys->createGroup(stageId, ""));
}

int addPreset(ProjectManagerPtr projectManager, int groupId)
{
	auto sys= projectManager->getSystemOfType<DMXPresetSystem>();
	if (!sys)
		return INVALID_MIKAN_ID;

	return componentIdOrInvalid(sys->createPreset(groupId, ""));
}

int addSequence(ProjectManagerPtr projectManager, int groupId)
{
	auto sys= projectManager->getSystemOfType<DMXSequenceSystem>();
	if (!sys)
		return INVALID_MIKAN_ID;

	return componentIdOrInvalid(sys->createSequence(groupId, ""));
}

int addAnchor(ProjectManagerPtr projectManager, int parentTransformId)
{
	auto sys= projectManager->getSystemOfType<AnchorObjectSystem>();
	if (!sys)
		return INVALID_MIKAN_ID;

	return componentIdOrInvalid(sys->addNewObjectByTypedDefinition(
		[parentTransformId](auto def)
		{
			def->setParentTransformId(parentTransformId);
			return true;
		}));
}

int addStencil(ProjectManagerPtr projectManager, eStencilType stencilType, int parentTransformId)
{
	switch (stencilType)
	{
	case eStencilType::quad:
	{
		auto sys= projectManager->getSystemOfType<QuadStencilSystem>();
		if (!sys)
			return INVALID_MIKAN_ID;

		return componentIdOrInvalid(sys->addNewObjectByTypedDefinition(
			[parentTransformId](auto def)
			{
				def->setQuadWidth(0.25f);
				def->setQuadHeight(0.25f);
				def->setIsDoubleSided(true);
				def->setRelativeTransform(GlmTransform());
				def->setParentTransformId(parentTransformId);
				def->setIsDisabled(false);
				return true;
			}));
	}
	case eStencilType::box:
	{
		auto sys= projectManager->getSystemOfType<BoxStencilSystem>();
		if (!sys)
			return INVALID_MIKAN_ID;

		return componentIdOrInvalid(sys->addNewObjectByTypedDefinition(
			[parentTransformId](auto def)
			{
				def->setBoxXSize(0.25f);
				def->setBoxYSize(0.25f);
				def->setBoxZSize(0.25f);
				def->setRelativeTransform(GlmTransform());
				def->setParentTransformId(parentTransformId);
				def->setIsDisabled(false);
				return true;
			}));
	}
	case eStencilType::model:
	{
		auto sys= projectManager->getSystemOfType<ModelStencilSystem>();
		if (!sys)
			return INVALID_MIKAN_ID;

		return componentIdOrInvalid(sys->addNewObjectByTypedDefinition(
			[parentTransformId](auto def)
			{
				def->setRelativeTransform(GlmTransform());
				def->setParentTransformId(parentTransformId);
				def->setIsDisabled(false);
				return true;
			}));
	}
	default:
		return INVALID_MIKAN_ID;
	}
}

int addShape(ProjectManagerPtr projectManager, eShapeType shapeType, int parentTransformId)
{
	switch (shapeType)
	{
	case eShapeType::quad:
	{
		auto sys= projectManager->getSystemOfType<QuadShapeSystem>();
		if (!sys)
			return INVALID_MIKAN_ID;

		return componentIdOrInvalid(sys->addNewObjectByTypedDefinition(
			[parentTransformId](auto def)
			{
				def->setQuadWidth(0.25f);
				def->setQuadHeight(0.25f);
				def->setIsDoubleSided(true);
				def->setRelativeTransform(GlmTransform());
				def->setParentTransformId(parentTransformId);
				return true;
			}));
	}
	case eShapeType::box:
	{
		auto sys= projectManager->getSystemOfType<BoxShapeSystem>();
		if (!sys)
			return INVALID_MIKAN_ID;

		return componentIdOrInvalid(sys->addNewObjectByTypedDefinition(
			[parentTransformId](auto def)
			{
				def->setBoxXSize(0.25f);
				def->setBoxYSize(0.25f);
				def->setBoxZSize(0.25f);
				def->setRelativeTransform(GlmTransform());
				def->setParentTransformId(parentTransformId);
				return true;
			}));
	}
	case eShapeType::model:
	{
		auto sys= projectManager->getSystemOfType<ModelShapeSystem>();
		if (!sys)
			return INVALID_MIKAN_ID;

		return componentIdOrInvalid(sys->addNewObjectByTypedDefinition(
			[parentTransformId](auto def)
			{
				def->setRelativeTransform(GlmTransform());
				def->setParentTransformId(parentTransformId);
				return true;
			}));
	}
	default:
		return INVALID_MIKAN_ID;
	}
}

int addScript(ProjectManagerPtr projectManager)
{
	auto sys= projectManager->getSystemOfType<ScriptObjectSystem>();
	return sys ? componentIdOrInvalid(sys->addNewScript()) : INVALID_MIKAN_ID;
}

int addSpoutOutput(ProjectManagerPtr projectManager)
{
	auto sys= projectManager->getSystemOfType<SpoutOutputSystem>();
	return sys ? componentIdOrInvalid(sys->addNewObjectByTypedDefinition()) : INVALID_MIKAN_ID;
}

int countSubtreeObjects(ProjectOutlinerNodeConstPtr node)
{
	if (!node)
		return 0;

	int count= (node->componentId != INVALID_MIKAN_ID) ? 1 : 0;
	for (ProjectOutlinerNodePtr child : node->children)
	{
		count+= countSubtreeObjects(child);
	}

	return count;
}

static bool deleteNodeComponent(ProjectManagerPtr projectManager, ProjectOutlinerNodeConstPtr node)
{
	MikanComponentPtr component= node->component.lock();
	if (!component || component->getWasDisposed())
	{
		// Already gone, e.g. an environment probe that died with its scene
		return true;
	}

	// An environment probe and a compositor are owned by their scene, and the
	// scene system takes them down with it, never as their own rows
	if (node->componentClassName == LightEnvironmentComponent::k_componentClassName
		|| node->componentClassName == CompositorComponent::k_componentClassName)
		return true;

	if (node->kind == eOutlinerNodeKind::trackingMount)
	{
		// Remove the back-reference from the owning VR volume
		ProjectOutlinerNodePtr volumeNode= node->parent.lock();
		VRTrackingVolumeComponentPtr vrVolume=
			volumeNode ? std::dynamic_pointer_cast<VRTrackingVolumeComponent>(volumeNode->component.lock()) : nullptr;
		if (vrVolume)
			vrVolume->getVRTrackingVolumeDefinition()->removeTrackingMountID(node->componentId);
	}

	MikanObjectPtr ownerObject= component->getOwnerObject();
	MikanObjectSystemPtr ownerSystem= ownerObject ? ownerObject->getOwnerSystem() : nullptr;

	return ownerSystem ? ownerSystem->deleteObject(ownerObject) : false;
}

bool deleteSubtree(ProjectManagerPtr projectManager, ProjectOutlinerNodeConstPtr node)
{
	if (!node || !projectManager)
		return false;

	// Children go first so reverse-order undo recreates parents before children
	bool bSuccess= true;
	for (ProjectOutlinerNodePtr child : node->children)
	{
		bSuccess&= deleteSubtree(projectManager, child);
	}

	if (node->componentId == INVALID_MIKAN_ID)
	{
		// Group rows (project root, unparented) own no component themselves
		return bSuccess;
	}

	return deleteNodeComponent(projectManager, node) && bSuccess;
}

bool reparentSceneActor(TransformComponentPtr draggedComponent, TransformComponentPtr newParentComponent)
{
	if (!draggedComponent || !newParentComponent || draggedComponent == newParentComponent)
		return false;

	if (draggedComponent->getParentTransformComponent() == newParentComponent)
		return false;

	// Subtree cycles and disallowed parent types are both refused inside
	// TransformComponent::attachToComponent
	return draggedComponent->reparentPreservingWorldTransform(newParentComponent);
}

} // namespace ProjectOutlinerActions

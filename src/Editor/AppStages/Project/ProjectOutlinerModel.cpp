#include "ProjectOutlinerModel.h"
#include "AnchorComponent.h"
#include "AnchorObjectSystem.h"
#include "ARKitVideoSourceComponent.h"
#include "ARKitVideoSourceSystem.h"
#include "FileVideoSourceComponent.h"
#include "FileVideoSourceSystem.h"
#include "BoxShapeComponent.h"
#include "BoxShapeSystem.h"
#include "BoxStencilSystem.h"
#include "CameraComponent.h"
#include "CameraObjectSystem.h"
#include "CEFTextureSourceComponent.h"
#include "CEFTextureSourceSystem.h"
#include "ClientTextureSourceComponent.h"
#include "ClientTextureSourceSystem.h"
#include "CompositorComponent.h"
#include "CompositorObjectSystem.h"
#include "DMXFixtureGroupComponent.h"
#include "DMXFixtureGroupSystem.h"
#include "DMXPresetComponent.h"
#include "DMXPresetSystem.h"
#include "DMXSequenceComponent.h"
#include "DMXSequenceSystem.h"
#include "IconsForkAwesome.h"
#include "LightEnvironmentComponent.h"
#include "LightEnvironmentSystem.h"
#include "LocText.h"
#include "MarkerComponent.h"
#include "MarkerObjectSystem.h"
#include "MarkerTrackingVolumeComponent.h"
#include "MarkerTrackingVolumeSystem.h"
#include "MikanCoreTypes.h"
#include "MikanObject.h"
#include "ModelShapeComponent.h"
#include "ModelShapeSystem.h"
#include "ModelStencilSystem.h"
#include "NetworkVideoSourceComponent.h"
#include "NetworkVideoSourceSystem.h"
#include "ProjectManager.h"
#include "QuadShapeComponent.h"
#include "QuadShapeSystem.h"
#include "QuadStencilSystem.h"
#include "RGBPixelGridComponent.h"
#include "RGBPixelGridSystem.h"
#include "RGBSpotLightComponent.h"
#include "RGBSpotLightSystem.h"
#include "SceneComponent.h"
#include "SceneObjectSystem.h"
#include "ScriptComponent.h"
#include "ScriptObjectSystem.h"
#include "SelectionComponent.h"
#include "ShapeComponent.h"
#include "StageComponent.h"
#include "StageObjectSystem.h"
#include "StencilComponent.h"
#include "TrackingMountComponent.h"
#include "TrackingMountObjectSystem.h"
#include "TransformComponent.h"
#include "USBVideoSourceComponent.h"
#include "USBVideoSourceSystem.h"
#include "VRTrackingVolumeComponent.h"
#include "VRTrackingVolumeSystem.h"

// The row glyph for a component, by class where the class is what the reader
// needs to tell apart (the three light kinds, the source backends) and by tree
// role otherwise. Every stencil reads as a mask rather than as its primitive:
// telling a stencil from a shape matters more than telling the stencil
// primitives apart, and the name already carries the primitive.
static const char* pickNodeIcon(eOutlinerNodeKind kind, const std::string& componentClassName)
{
	// Sources, by backend
	if (componentClassName == USBVideoSourceComponent::k_componentClassName)
		return ICON_FK_USB;
	if (componentClassName == NetworkVideoSourceComponent::k_componentClassName)
		return ICON_FK_WIFI;
	if (componentClassName == ARKitVideoSourceComponent::k_componentClassName)
		return ICON_FK_MOBILE;
	if (componentClassName == FileVideoSourceComponent::k_componentClassName)
		return ICON_FK_FILM;
	if (componentClassName == ClientTextureSourceComponent::k_componentClassName)
		return ICON_FK_PLUG;
	if (componentClassName == SpoutTextureSourceComponent::k_componentClassName)
		return ICON_FK_SHARE_ALT;
	if (componentClassName == CEFTextureSourceComponent::k_componentClassName)
		return ICON_FK_CHROME;

	// Lights, replacing the old [Env]/[Spot]/[Grid] name prefixes
	if (componentClassName == LightEnvironmentComponent::k_componentClassName)
		return ICON_FK_GLOBE;
	if (componentClassName == RGBSpotLightComponent::k_componentClassName)
		return ICON_FK_LIGHTBULB_O;
	if (componentClassName == RGBPixelGridComponent::k_componentClassName)
		return ICON_FK_TH;
	if (componentClassName == DMXFixtureGroupComponent::k_componentClassName)
		return ICON_FK_OBJECT_GROUP;
	if (componentClassName == DMXPresetComponent::k_componentClassName)
		return ICON_FK_SLIDERS;
	if (componentClassName == DMXSequenceComponent::k_componentClassName)
		return ICON_FK_PLAY_CIRCLE;

	// Scene actors: stencils as masks, shapes by primitive
	if (componentClassName == AnchorComponent::k_componentClassName)
		return ICON_FK_ANCHOR;
	if (componentClassName == QuadStencilComponent::k_componentClassName
		|| componentClassName == BoxStencilComponent::k_componentClassName
		|| componentClassName == ModelStencilComponent::k_componentClassName)
		return ICON_FK_CROP;
	if (componentClassName == QuadShapeComponent::k_componentClassName)
		return ICON_FK_SQUARE;
	if (componentClassName == BoxShapeComponent::k_componentClassName)
		return ICON_FK_CUBE;
	if (componentClassName == ModelShapeComponent::k_componentClassName)
		return ICON_FK_CUBES;

	// Project scripts
	if (componentClassName == ScriptComponent::k_componentClassName)
		return ICON_FK_FILE_CODE_O;

	switch (kind)
	{
	case eOutlinerNodeKind::marker:
		return ICON_FK_QRCODE;
	case eOutlinerNodeKind::trackingVolume:
		return ICON_FK_BULLSEYE;
	case eOutlinerNodeKind::trackingMount:
		return ICON_FK_CROSSHAIRS;
	case eOutlinerNodeKind::stage:
		return ICON_FK_MAP_O;
	case eOutlinerNodeKind::camera:
		return ICON_FK_VIDEO_CAMERA;
	case eOutlinerNodeKind::compositor:
		return ICON_FK_CLONE;
	case eOutlinerNodeKind::scene:
		return ICON_FK_SITEMAP;
	default:
		// A type with no mapping yet reads as a neutral dot rather than
		// silently losing its column
		return ICON_FK_CIRCLE_O;
	}
}

std::string ProjectOutlinerModel::getNodeStateKey(const ProjectOutlinerNode& node)
{
	if (node.componentId != INVALID_MIKAN_ID)
		return "component:" + std::to_string(node.componentId);

	const char* folderName= nullptr;
	switch (node.kind)
	{
	case eOutlinerNodeKind::projectRoot:
		return "root";
	case eOutlinerNodeKind::unparentedGroup:
		return "unparented";
	case eOutlinerNodeKind::folderSources:
		folderName= "sources";
		break;
	case eOutlinerNodeKind::folderMarkers:
		folderName= "markers";
		break;
	case eOutlinerNodeKind::folderTrackingVolumes:
		folderName= "tracking_volumes";
		break;
	case eOutlinerNodeKind::folderScripts:
		folderName= "scripts";
		break;
	case eOutlinerNodeKind::folderCameras:
		folderName= "cameras";
		break;
	case eOutlinerNodeKind::folderLights:
		folderName= "lights";
		break;
	case eOutlinerNodeKind::folderLightGroups:
		folderName= "light_groups";
		break;
	case eOutlinerNodeKind::folderScenes:
		folderName= "scenes";
		break;
	default:
		return "";
	}

	return node.ownerId != INVALID_MIKAN_ID ? std::string(folderName) + ":" + std::to_string(node.ownerId)
											: std::string(folderName);
}

bool ProjectOutlinerModel::getNodeDefaultOpen(eOutlinerNodeKind kind)
{
	// Folders and the containers above a scene open so the project reads at a
	// glance. Lights folders stay closed: fixture lists are long and rarely edited.
	switch (kind)
	{
	case eOutlinerNodeKind::projectRoot:
	case eOutlinerNodeKind::folderSources:
	case eOutlinerNodeKind::folderMarkers:
	case eOutlinerNodeKind::folderTrackingVolumes:
	case eOutlinerNodeKind::folderCameras:
	case eOutlinerNodeKind::folderLightGroups:
	case eOutlinerNodeKind::folderScenes:
	case eOutlinerNodeKind::folderScripts:
	case eOutlinerNodeKind::trackingVolume:
	case eOutlinerNodeKind::stage:
	case eOutlinerNodeKind::scene:
		return true;
	default:
		return false;
	}
}

static bool isSceneActorObject(MikanObjectPtr objectPtr)
{
	return objectPtr->getComponentOfType<AnchorComponent>() != nullptr
		   || objectPtr->getComponentOfType<StencilComponent>() != nullptr
		   || objectPtr->getComponentOfType<ShapeComponent>() != nullptr;
}

void ProjectOutlinerModel::clear()
{
	m_root.reset();
	m_unparentedGroup.reset();
	m_nodeIndex.clear();
	m_folderIndex.clear();
}

void ProjectOutlinerModel::rebuild(ProjectManagerPtr projectManager)
{
	clear();

	m_root= std::make_shared<ProjectOutlinerNode>();
	m_root->kind= eOutlinerNodeKind::projectRoot;
	m_root->displayName= locText("project.outlinerRoot");
	m_root->icon= ICON_FK_FOLDER_OPEN;

	if (!projectManager)
		return;

	// The top-level folders always exist: they host the add buttons even when empty
	ProjectOutlinerNodePtr sourcesFolder=
		addFolderNode(m_root, eOutlinerNodeKind::folderSources, "project.outlinerSourcesGroup");
	ProjectOutlinerNodePtr markersFolder=
		addFolderNode(m_root, eOutlinerNodeKind::folderMarkers, "project.outlinerMarkersGroup");
	ProjectOutlinerNodePtr trackingVolumesFolder=
		addFolderNode(m_root, eOutlinerNodeKind::folderTrackingVolumes, "project.outlinerTrackingGroup");
	ProjectOutlinerNodePtr scriptsFolder=
		addFolderNode(m_root, eOutlinerNodeKind::folderScripts, "project.outlinerScriptsGroup");

	// Video sources
	if (auto sys= projectManager->getSystemOfType<USBVideoSourceSystem>())
		sys->visitComponents([&](USBVideoSourceComponentPtr comp)
							 { addComponentNode(sourcesFolder, eOutlinerNodeKind::videoSource, comp); });
	if (auto sys= projectManager->getSystemOfType<NetworkVideoSourceSystem>())
		sys->visitComponents([&](NetworkVideoSourceComponentPtr comp)
							 { addComponentNode(sourcesFolder, eOutlinerNodeKind::videoSource, comp); });
	if (auto sys= projectManager->getSystemOfType<ARKitVideoSourceSystem>())
		sys->visitComponents([&](ARKitVideoSourceComponentPtr comp)
							 { addComponentNode(sourcesFolder, eOutlinerNodeKind::videoSource, comp); });
	if (auto sys= projectManager->getSystemOfType<FileVideoSourceSystem>())
		sys->visitComponents([&](FileVideoSourceComponentPtr comp)
							 { addComponentNode(sourcesFolder, eOutlinerNodeKind::videoSource, comp); });

	// Texture sources
	if (auto sys= projectManager->getSystemOfType<ClientTextureSourceSystem>())
		sys->visitComponents([&](ClientTextureSourceComponentPtr comp)
							 { addComponentNode(sourcesFolder, eOutlinerNodeKind::textureSource, comp); });
	if (auto sys= projectManager->getSystemOfType<SpoutTextureSourceSystem>())
		sys->visitComponents([&](SpoutTextureSourceComponentPtr comp)
							 { addComponentNode(sourcesFolder, eOutlinerNodeKind::textureSource, comp); });
	if (auto sys= projectManager->getSystemOfType<CEFTextureSourceSystem>())
		sys->visitComponents([&](CEFTextureSourceComponentPtr comp)
							 { addComponentNode(sourcesFolder, eOutlinerNodeKind::textureSource, comp); });

	// Markers
	if (auto sys= projectManager->getSystemOfType<MarkerObjectSystem>())
		sys->visitComponents([&](MarkerComponentPtr comp)
							 { addComponentNode(markersFolder, eOutlinerNodeKind::marker, comp); });

	// Tracking volumes, with mounts under the VR volumes that own them
	TrackingMountObjectSystemPtr mountSystem= projectManager->getSystemOfType<TrackingMountObjectSystem>();
	if (auto vrVolumeSystem= projectManager->getSystemOfType<VRTrackingVolumeSystem>())
	{
		vrVolumeSystem->visitComponents(
			[&](VRTrackingVolumeComponentPtr volume)
			{
				ProjectOutlinerNodePtr volumeNode=
					addComponentNode(trackingVolumesFolder, eOutlinerNodeKind::trackingVolume, volume);

				if (mountSystem)
				{
					for (MikanTrackingMountID mountId : volume->getVRTrackingVolumeDefinition()->getTrackingMountIDs())
					{
						if (TrackingMountComponentPtr mount= mountSystem->getTypedComponentById(mountId))
							addComponentNode(volumeNode, eOutlinerNodeKind::trackingMount, mount);
					}
				}
			});
	}
	if (auto markerVolumeSystem= projectManager->getSystemOfType<MarkerTrackingVolumeSystem>())
	{
		markerVolumeSystem->visitComponents(
			[&](MarkerTrackingVolumeComponentPtr volume)
			{ addComponentNode(trackingVolumesFolder, eOutlinerNodeKind::trackingVolume, volume); });
	}

	// Project scripts
	if (auto sys= projectManager->getSystemOfType<ScriptObjectSystem>())
		sys->visitComponents([&](ScriptComponentPtr comp)
							 { addComponentNode(scriptsFolder, eOutlinerNodeKind::script, comp); });

	// Stages parent under their volume; one with a missing volume falls back to the root
	if (auto stageSystem= projectManager->getSystemOfType<StageObjectSystem>())
	{
		stageSystem->visitComponents(
			[&](StageComponentPtr stage)
			{
				const MikanTrackingVolumeID volumeId= stage->getStageComponentDefinitionConst()->getTrackingVolumeId();
				ProjectOutlinerNodePtr volumeNode= findNodeByComponentId(volumeId);
				buildStageSubtree(projectManager, stage, volumeNode ? volumeNode : m_root);
			});
	}

	// A scene whose parent stage is gone still needs a row to be repairable from
	if (auto sceneSystem= projectManager->getSystemOfType<SceneObjectSystem>())
	{
		sceneSystem->visitComponents(
			[&](SceneComponentPtr scene)
			{
				if (!findNodeByComponentId(scene->getSceneId()))
					buildSceneSubtree(scene, m_root);
			});
	}

	// A compositor whose owner scene has no row (missing or unresolvable) still
	// needs one to be repairable from, so it falls back to the root
	if (auto compositorSystem= projectManager->getSystemOfType<CompositorObjectSystem>())
	{
		compositorSystem->visitComponents(
			[&](CompositorComponentPtr compositor)
			{
				if (!findNodeByComponentId(compositor->getComponentId()))
					addComponentNode(m_root, eOutlinerNodeKind::compositor, compositor);
			});
	}

	// Scene actors no scene walk reached (legacy unparented shapes, dangling
	// parents) collect in a synthetic group so they stay visible and can be
	// dragged back into a scene. Subtrees start at their root-most orphan: a
	// child whose parent is itself an unlisted actor is reached through it.
	auto addOrphanActor= [&](TransformComponentPtr actor)
	{
		if (findNodeByComponentId(actor->getComponentId()))
			return;

		TransformComponentPtr parentTransform= actor->getParentTransformComponent();
		if (parentTransform && isSceneActorObject(parentTransform->getOwnerObject()))
			return;

		addSceneActorSubtree(actor, getOrCreateUnparentedGroup());
	};
	if (auto sys= projectManager->getSystemOfType<AnchorObjectSystem>())
		sys->visitComponents([&](AnchorComponentPtr comp) { addOrphanActor(comp); });
	if (auto sys= projectManager->getSystemOfType<QuadStencilSystem>())
		sys->visitComponents([&](QuadStencilComponentPtr comp) { addOrphanActor(comp); });
	if (auto sys= projectManager->getSystemOfType<BoxStencilSystem>())
		sys->visitComponents([&](BoxStencilComponentPtr comp) { addOrphanActor(comp); });
	if (auto sys= projectManager->getSystemOfType<ModelStencilSystem>())
		sys->visitComponents([&](ModelStencilComponentPtr comp) { addOrphanActor(comp); });
	if (auto sys= projectManager->getSystemOfType<QuadShapeSystem>())
		sys->visitComponents([&](QuadShapeComponentPtr comp) { addOrphanActor(comp); });
	if (auto sys= projectManager->getSystemOfType<BoxShapeSystem>())
		sys->visitComponents([&](BoxShapeComponentPtr comp) { addOrphanActor(comp); });
	if (auto sys= projectManager->getSystemOfType<ModelShapeSystem>())
		sys->visitComponents([&](ModelShapeComponentPtr comp) { addOrphanActor(comp); });
}

void ProjectOutlinerModel::buildStageSubtree(ProjectManagerPtr projectManager, StageComponentPtr stageComponent,
											 ProjectOutlinerNodePtr parentNode)
{
	ProjectOutlinerNodePtr stageNode= addComponentNode(parentNode, eOutlinerNodeKind::stage, stageComponent);
	const MikanStageID stageId= stageComponent->getStageId();

	// Per-stage folders, always present so their add buttons are reachable
	ProjectOutlinerNodePtr camerasFolder=
		addFolderNode(stageNode, eOutlinerNodeKind::folderCameras, "project.outlinerCamerasGroup", stageId);
	ProjectOutlinerNodePtr lightsFolder=
		addFolderNode(stageNode, eOutlinerNodeKind::folderLights, "project.outlinerLightsGroup", stageId);
	ProjectOutlinerNodePtr scenesFolder=
		addFolderNode(stageNode, eOutlinerNodeKind::folderScenes, "project.outlinerScenesGroup", stageId);

	// Cameras
	if (auto cameraSystem= projectManager->getSystemOfType<CameraObjectSystem>())
	{
		cameraSystem->visitComponents([&](CameraComponentPtr camera)
									  { addComponentNode(camerasFolder, eOutlinerNodeKind::camera, camera); },
									  [stageId](CameraComponentPtr camera)
									  { return camera->getCameraDefinition()->getOwnerStageId() == stageId; });
	}

	// Stage lights: environment probes, then the authored DMX fixtures
	if (auto lightEnvironmentSystem= projectManager->getSystemOfType<LightEnvironmentSystem>())
	{
		lightEnvironmentSystem->visitComponents(
			[&](LightEnvironmentComponentPtr light)
			{ addComponentNode(lightsFolder, eOutlinerNodeKind::stageLight, light); },
			[stageId](LightEnvironmentComponentPtr light)
			{
				StageComponentConstPtr ownerStage= light->getOwnerStageComponent();
				return ownerStage && ownerStage->getComponentId() == stageId;
			});
	}
	if (auto spotLightSystem= projectManager->getSystemOfType<RGBSpotLightSystem>())
	{
		spotLightSystem->visitComponents([&](RGBSpotLightComponentPtr light)
										 { addComponentNode(lightsFolder, eOutlinerNodeKind::stageLight, light); },
										 [stageId](RGBSpotLightComponentPtr light)
										 { return light->getDMXFixtureDefinition()->getOwnerStageId() == stageId; });
	}
	if (auto pixelGridSystem= projectManager->getSystemOfType<RGBPixelGridSystem>())
	{
		pixelGridSystem->visitComponents([&](RGBPixelGridComponentPtr light)
										 { addComponentNode(lightsFolder, eOutlinerNodeKind::stageLight, light); },
										 [stageId](RGBPixelGridComponentPtr light)
										 { return light->getDMXFixtureDefinition()->getOwnerStageId() == stageId; });
	}

	// Light groups: authored fixture bundles, nested under this stage's Lights
	// folder so the add button is reachable even with no group yet
	ProjectOutlinerNodePtr lightGroupsFolder=
		addFolderNode(lightsFolder, eOutlinerNodeKind::folderLightGroups, "project.outlinerLightGroupsGroup", stageId);
	if (auto lightGroupSystem= projectManager->getSystemOfType<DMXFixtureGroupSystem>())
	{
		DMXPresetSystemPtr presetSystem= projectManager->getSystemOfType<DMXPresetSystem>();
		DMXSequenceSystemPtr sequenceSystem= projectManager->getSystemOfType<DMXSequenceSystem>();

		lightGroupSystem->visitComponents(
			[&](DMXFixtureGroupComponentPtr group)
			{
				ProjectOutlinerNodePtr groupNode=
					addComponentNode(lightGroupsFolder, eOutlinerNodeKind::lightGroup, group);

				if (presetSystem)
				{
					const MikanDMXFixtureGroupID groupId= group->getComponentId();
					presetSystem->visitComponents(
						[&](DMXPresetComponentPtr preset)
						{ addComponentNode(groupNode, eOutlinerNodeKind::dmxPreset, preset); },
						[groupId](DMXPresetComponentPtr preset)
						{ return preset->getDMXPresetDefinition()->getGroupId() == groupId; });
				}

				if (sequenceSystem)
				{
					const MikanDMXFixtureGroupID groupId= group->getComponentId();
					sequenceSystem->visitComponents(
						[&](DMXSequenceComponentPtr sequence)
						{ addComponentNode(groupNode, eOutlinerNodeKind::dmxSequence, sequence); },
						[groupId](DMXSequenceComponentPtr sequence)
						{ return sequence->getDMXSequenceDefinition()->getGroupId() == groupId; });
				}
			},
			[stageId](DMXFixtureGroupComponentPtr group)
			{ return group->getDMXFixtureGroupDefinition()->getOwnerStageId() == stageId; });
	}

	// Scenes attached to this stage
	if (auto sceneSystem= projectManager->getSystemOfType<SceneObjectSystem>())
	{
		sceneSystem->visitComponents([&](SceneComponentPtr scene) { buildSceneSubtree(scene, scenesFolder); },
									 [stageId](SceneComponentPtr scene)
									 { return scene->getParentStageId() == stageId; });
	}
}

void ProjectOutlinerModel::buildSceneSubtree(SceneComponentPtr sceneComponent, ProjectOutlinerNodePtr parentNode)
{
	ProjectOutlinerNodePtr sceneNode= addComponentNode(parentNode, eOutlinerNodeKind::scene, sceneComponent);

	// The scene's compositors sit ahead of its actors: they are scene-level outputs
	for (CompositorComponentPtr compositor : sceneComponent->getOutputCompositors())
	{
		addComponentNode(sceneNode, eOutlinerNodeKind::compositor, compositor);
	}

	// Scene actors via the transform hierarchy
	for (TransformComponentWeakPtr childWeakPtr : sceneComponent->getChildTransformComponents())
	{
		if (TransformComponentPtr child= childWeakPtr.lock())
			addSceneActorSubtree(child, sceneNode);
	}
}

void ProjectOutlinerModel::addSceneActorSubtree(TransformComponentPtr transformComponent,
												ProjectOutlinerNodePtr parentNode)
{
	if (!transformComponent || transformComponent->getWasDisposed())
		return;

	MikanObjectPtr ownerObject= transformComponent->getOwnerObject();
	ProjectOutlinerNodePtr actorNode= parentNode;
	if (ownerObject->getRootComponent() == transformComponent)
	{
		// Only anchor, stencil, and shape objects appear inside a scene subtree
		if (!isSceneActorObject(ownerObject))
			return;

		if (findNodeByComponentId(transformComponent->getComponentId()))
			return;

		actorNode= addComponentNode(parentNode, eOutlinerNodeKind::sceneActor, transformComponent);
	}

	for (TransformComponentWeakPtr childWeakPtr : transformComponent->getChildTransformComponents())
	{
		if (TransformComponentPtr child= childWeakPtr.lock())
			addSceneActorSubtree(child, actorNode);
	}
}

ProjectOutlinerNodePtr ProjectOutlinerModel::addComponentNode(ProjectOutlinerNodePtr parentNode, eOutlinerNodeKind kind,
															  MikanComponentPtr component)
{
	ProjectOutlinerNodePtr node= std::make_shared<ProjectOutlinerNode>();
	node->kind= kind;
	node->componentId= component->getComponentId();
	node->componentClassName= component->getComponentClassName();
	node->component= component;

	if (MikanObjectPtr ownerObject= component->getOwnerObject())
	{
		if (MikanObjectSystemPtr ownerSystem= ownerObject->getOwnerSystem())
			node->systemClassName= ownerSystem->getObjectSystemClassName();
		node->selection= ownerObject->getComponentOfType<SelectionComponent>();
	}

	std::string name= component->getName();
	if (name.empty())
		name= locText("project.noName");
	node->displayName= name;
	node->icon= pickNodeIcon(kind, node->componentClassName);

	node->parent= parentNode;
	parentNode->children.push_back(node);
	m_nodeIndex[node->componentId]= node;

	return node;
}

ProjectOutlinerNodePtr ProjectOutlinerModel::addFolderNode(ProjectOutlinerNodePtr parentNode, eOutlinerNodeKind kind,
														   const char* labelKey, int ownerId)
{
	ProjectOutlinerNodePtr folderNode= std::make_shared<ProjectOutlinerNode>();
	folderNode->kind= kind;
	folderNode->ownerId= ownerId;
	folderNode->displayName= locText(labelKey);
	folderNode->icon= ICON_FK_FOLDER;
	folderNode->parent= parentNode;
	parentNode->children.push_back(folderNode);
	m_folderIndex[{(int)kind, ownerId}]= folderNode;

	return folderNode;
}

ProjectOutlinerNodePtr ProjectOutlinerModel::getOrCreateUnparentedGroup()
{
	if (!m_unparentedGroup)
	{
		m_unparentedGroup= addFolderNode(m_root, eOutlinerNodeKind::unparentedGroup, "project.outlinerUnparented");
	}

	return m_unparentedGroup;
}

ProjectOutlinerNodePtr ProjectOutlinerModel::findFolderNode(eOutlinerNodeKind kind, int ownerId) const
{
	auto it= m_folderIndex.find({(int)kind, ownerId});
	return (it != m_folderIndex.end()) ? it->second.lock() : nullptr;
}

ProjectOutlinerNodePtr ProjectOutlinerModel::findNodeByComponentId(int componentId) const
{
	auto it= m_nodeIndex.find(componentId);
	return (it != m_nodeIndex.end()) ? it->second.lock() : nullptr;
}

ProjectOutlinerNodePtr ProjectOutlinerModel::findNodeBySelection(SelectionComponentConstPtr selection) const
{
	if (!selection)
		return nullptr;

	for (const auto& kvpair : m_nodeIndex)
	{
		ProjectOutlinerNodePtr node= kvpair.second.lock();
		if (node && node->selection.lock().get() == selection.get())
			return node;
	}

	return nullptr;
}

ProjectOutlinerNodePtr ProjectOutlinerModel::findOwningSceneNode(ProjectOutlinerNodePtr node) const
{
	while (node && node->kind != eOutlinerNodeKind::scene)
	{
		node= node->parent.lock();
	}

	return node;
}

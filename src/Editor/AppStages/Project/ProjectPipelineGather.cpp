#include "ProjectPipelineGather.h"
#include "BoxShapeComponent.h"
#include "BoxShapeSystem.h"
#include "BoxStencilComponent.h"
#include "BoxStencilSystem.h"
#include "CEFTextureSourceComponent.h"
#include "CameraComponent.h"
#include "ClientSourceManager.h"
#include "ClientTextureSourceComponent.h"
#include "CompositorComponent.h"
#include "DMXFixtureGroupComponent.h"
#include "DMXFixtureGroupSystem.h"
#include "DMXObjectSystem.h"
#include "DMXPresetComponent.h"
#include "DMXPresetSystem.h"
#include "DMXSequenceComponent.h"
#include "DMXSequenceSystem.h"
#include "FileVideoSourceComponent.h"
#include "Graphs/CompositorNodeGraph.h"
#include "LightEnvironmentComponent.h"
#include "LocText.h"
#include "MarkerComponent.h"
#include "MarkerObjectSystem.h"
#include "MikanObject.h"
#include "ModelShapeComponent.h"
#include "ModelShapeSystem.h"
#include "ModelStencilComponent.h"
#include "ModelStencilSystem.h"
#include "OutputQueries.h"
#include "ProjectManager.h"
#include "QuadShapeComponent.h"
#include "QuadShapeSystem.h"
#include "QuadStencilComponent.h"
#include "QuadStencilSystem.h"
#include "RGBPixelGridComponent.h"
#include "RGBPixelGridSystem.h"
#include "RGBSpotLightComponent.h"
#include "RGBSpotLightSystem.h"
#include "RTPOutputComponent.h"
#include "RTPOutputSystem.h"
#include "SceneComponent.h"
#include "SceneObjectSystem.h"
#include "SharedTextureReader.h"
#include "SpoutOutputComponent.h"
#include "SpoutTextureSourceComponent.h"
#include "StageComponent.h"
#include "TextureSourceQueries.h"
#include "TrackingMountComponent.h"
#include "TrackingMountObjectSystem.h"
#include "TrackingVolumeComponent.h"
#include "VideoSourceComponent.h"
#include "VRObjectSystem.h"
#include "VRTrackingVolumeComponent.h"

#include <set>
#include <string>

namespace
{
int addNode(PipelineSnapshot& snapshot, const std::string& id, ePipelineNodeKind kind, ePipelineBox box,
			ePipelineStage stage, int parentIndex)
{
	PipelineNode node;
	node.id= id;
	node.kind= kind;
	node.box= box;
	node.stage= stage;
	node.parentIndex= parentIndex;
	snapshot.nodes.push_back(node);

	return (int)snapshot.nodes.size() - 1;
}

// A node that stands for a component that exists
void setPresentComponent(PipelineNode& node, const MikanComponent* component)
{
	node.componentId= component->getComponentId();
	node.name= component->getName();
	node.facts.bPresent= true;
}

// The function a double-click on the node runs, on the component it runs on
void setNodeTool(PipelineNode& node, const std::string& functionId, const MikanComponent* component)
{
	if (component == nullptr)
		return;

	node.toolFunctionId= functionId;
	node.toolComponentId= component->getComponentId();
}

std::string makeChildId(const std::string& parentId, int componentId)
{
	return parentId + "." + std::to_string(componentId);
}

// A kind box shows how many members it holds
void countKindBoxMembers(PipelineSnapshot& snapshot, int kindIndex)
{
	snapshot.nodes[kindIndex].facts.count= (int)ProjectPipelineModel::getChildren(snapshot, kindIndex).size();
}

// A mount is only usable through the VR device it names
int addMountNode(PipelineSnapshot& snapshot, const std::string& id, ePipelineNodeKind kind, ePipelineBox box,
				 int parentIndex, TrackingMountComponentConstPtr mount)
{
	const int nodeIndex= addNode(snapshot, id, kind, box, ePipelineStage::volume, parentIndex);
	if (mount)
	{
		PipelineNode& node= snapshot.nodes[nodeIndex];
		setPresentComponent(node, mount.get());
		node.facts.bActive= mount->getVRDeviceComponent() != nullptr;
	}
	return nodeIndex;
}

ePipelineRuntimeVariant toRuntimeVariant(VRObjectSystem::eTrackingRuntimeState state)
{
	switch (state)
	{
	case VRObjectSystem::eTrackingRuntimeState::initializing:
		return ePipelineRuntimeVariant::initializing;
	case VRObjectSystem::eTrackingRuntimeState::ready:
		return ePipelineRuntimeVariant::ready;
	case VRObjectSystem::eTrackingRuntimeState::failed:
		return ePipelineRuntimeVariant::failed;
	case VRObjectSystem::eTrackingRuntimeState::uninitialized:
	default:
		break;
	}
	return ePipelineRuntimeVariant::uninitialized;
}

ePipelineVideoVariant toVideoVariant(eVideoStreamingStatus status)
{
	switch (status)
	{
	case eVideoStreamingStatus::stopped:
		return ePipelineVideoVariant::stopped;
	case eVideoStreamingStatus::pendingStart:
		return ePipelineVideoVariant::pendingStart;
	case eVideoStreamingStatus::started:
		return ePipelineVideoVariant::started;
	case eVideoStreamingStatus::failed:
	default:
		break;
	}
	return ePipelineVideoVariant::failed;
}

ePipelineLensVariant toLensVariant(eVideoSourceIntrinsicsStatus status)
{
	switch (status)
	{
	case eVideoSourceIntrinsicsStatus::ok:
		return ePipelineLensVariant::calibrated;
	case eVideoSourceIntrinsicsStatus::resolutionMismatch:
		return ePipelineLensVariant::resolutionMismatch;
	case eVideoSourceIntrinsicsStatus::uncalibrated:
	default:
		break;
	}
	return ePipelineLensVariant::uncalibrated;
}

void gatherVolume(ProjectManagerPtr projectManager, StageComponentPtr stage, CameraComponentPtr camera, int volumeIndex,
				  PipelineSnapshot& snapshot)
{
	TrackingVolumeComponentConstPtr volume= stage ? stage->getTrackingVolumeConst() : nullptr;
	if (!volume)
		return;

	const bool bIsVR= volume->getTrackingVolumeType() == eTrackingVolumeType::vr;
	{
		PipelineNode& volumeNode= snapshot.nodes[volumeIndex];
		setPresentComponent(volumeNode, volume.get());
		volumeNode.facts.variant= (int)(bIsVR ? ePipelineVolumeVariant::vr : ePipelineVolumeVariant::marker);
	}

	VRTrackingVolumeDefinitionConstPtr vrDefinition;
	if (bIsVR)
	{
		vrDefinition=
			std::static_pointer_cast<const VRTrackingVolumeComponent>(volume)->getVRTrackingVolumeDefinition();

		const int runtimeIndex= addNode(snapshot, "volume.runtime", ePipelineNodeKind::box, ePipelineBox::steamVR,
										ePipelineStage::volume, volumeIndex);
		VRObjectSystemPtr vrObjectSystem= projectManager->getSystemOfType<VRObjectSystem>();
		const VRObjectSystem::eTrackingRuntimeState runtimeState=
			vrObjectSystem ? vrObjectSystem->getTrackingRuntimeState(vrDefinition->getTrackingRuntime())
						   : VRObjectSystem::eTrackingRuntimeState::uninitialized;
		snapshot.nodes[runtimeIndex].facts.bPresent= true;
		snapshot.nodes[runtimeIndex].facts.variant= (int)toRuntimeVariant(runtimeState);
	}

	// A marker volume has no frame without its origin marker, a VR volume only
	// uses it for alignment
	const int originIndex= addNode(snapshot, "volume.origin", ePipelineNodeKind::box, ePipelineBox::originMarker,
								   ePipelineStage::volume, volumeIndex);
	snapshot.nodes[originIndex].facts.bRequired= !bIsVR;
	if (MarkerObjectSystemPtr markerSystem= projectManager->getSystemOfType<MarkerObjectSystem>())
	{
		if (MarkerComponentPtr originMarker= markerSystem->getMarkerById(volume->getOriginMarkerId()))
			setPresentComponent(snapshot.nodes[originIndex], originMarker.get());
	}
	// A marker volume's camera is placed by sighting the origin marker
	if (!bIsVR)
		setNodeTool(snapshot.nodes[originIndex], CameraComponent::k_alignCameraFunctionId, camera.get());

	if (!vrDefinition)
		return;

	TrackingMountObjectSystemPtr mountSystem= projectManager->getSystemOfType<TrackingMountObjectSystem>();
	auto findMount= [&mountSystem](MikanTrackingMountID mountId) -> TrackingMountComponentConstPtr
	{ return mountSystem ? mountSystem->getTypedComponentById(mountId) : nullptr; };

	const int matIndex= addMountNode(snapshot, "volume.mat", ePipelineNodeKind::box, ePipelineBox::matMount,
									 volumeIndex, findMount(vrDefinition->getCharucoTrackingMountId()));
	setNodeTool(snapshot.nodes[matIndex], VRTrackingVolumeComponent::k_alignTrackingVolumeFunctionId, volume.get());

	const int mountsIndex= addNode(snapshot, "volume.mounts", ePipelineNodeKind::kindBox, ePipelineBox::trackingMounts,
								   ePipelineStage::volume, volumeIndex);
	snapshot.nodes[mountsIndex].facts.bPresent= true;
	for (MikanTrackingMountID mountId : vrDefinition->getTrackingMountIDs())
	{
		addMountNode(snapshot, makeChildId("volume.mounts", mountId), ePipelineNodeKind::member,
					 ePipelineBox::trackingMount, mountsIndex, findMount(mountId));
	}
	countKindBoxMembers(snapshot, mountsIndex);
}

// What holds a light group's fixtures: a playing sequence wins over the scene's
// preset, as applying the current scene's preset skips a group with one playing
void gatherLightGroup(ProjectManagerPtr projectManager, DMXFixtureGroupComponentPtr group,
					  DMXPresetComponentPtr scenePreset, int memberIndex, PipelineSnapshot& snapshot)
{
	PipelineNode& member= snapshot.nodes[memberIndex];
	setPresentComponent(member, group.get());
	member.facts.variant= (int)ePipelineLightGroupVariant::dark;

	const MikanDMXFixtureGroupID groupId= group->getComponentId();
	DMXSequenceComponentPtr playingSequence;
	if (DMXSequenceSystemPtr sequenceSystem= projectManager->getSystemOfType<DMXSequenceSystem>())
	{
		sequenceSystem->visitComponents(
			[&playingSequence](DMXSequenceComponentPtr sequence)
			{
				if (!playingSequence)
					playingSequence= sequence;
			},
			[groupId](DMXSequenceComponentPtr sequence)
			{ return sequence->isPlaying() && sequence->getDMXSequenceDefinition()->getGroupId() == groupId; });
	}

	if (playingSequence)
	{
		member.facts.variant= (int)ePipelineLightGroupVariant::sequence;
		member.facts.detailName= playingSequence->getName();
	}
	else if (scenePreset && scenePreset->getDMXPresetDefinition()->getGroupId() == groupId)
	{
		member.facts.variant= (int)ePipelineLightGroupVariant::preset;
		member.facts.detailName= scenePreset->getName();
	}
}

void gatherStage(ProjectManagerPtr projectManager, StageComponentPtr stage, DMXPresetComponentPtr scenePreset,
				 int stageIndex, PipelineSnapshot& snapshot)
{
	if (!stage)
		return;

	setPresentComponent(snapshot.nodes[stageIndex], stage.get());
	const MikanStageID stageId= stage->getStageId();

	const int dmxIndex=
		addNode(snapshot, "stage.dmx", ePipelineNodeKind::box, ePipelineBox::dmx, ePipelineStage::stage, stageIndex);
	snapshot.nodes[dmxIndex].facts.bPresent= true;
	if (DMXObjectSystemPtr dmxSystem= projectManager->getSystemOfType<DMXObjectSystem>())
	{
		PipelineFacts& dmxFacts= snapshot.nodes[dmxIndex].facts;
		dmxFacts.bHasSignal= true;
		dmxFacts.bActive= dmxSystem->getIsSenderRunning();
		dmxFacts.count= dmxSystem->getUniverseCount();
	}

	const int groupsIndex= addNode(snapshot, "stage.groups", ePipelineNodeKind::kindBox, ePipelineBox::lightGroups,
								   ePipelineStage::stage, stageIndex);
	snapshot.nodes[groupsIndex].facts.bPresent= true;

	std::set<MikanLightID> groupedFixtureIds;
	if (DMXFixtureGroupSystemPtr groupSystem= projectManager->getSystemOfType<DMXFixtureGroupSystem>())
	{
		groupSystem->visitComponents(
			[&](DMXFixtureGroupComponentPtr group)
			{
				const int memberIndex=
					addNode(snapshot, makeChildId("stage.groups", group->getComponentId()), ePipelineNodeKind::member,
							ePipelineBox::lightGroup, ePipelineStage::stage, groupsIndex);
				gatherLightGroup(projectManager, group, scenePreset, memberIndex, snapshot);

				for (MikanLightID fixtureId : group->getDMXFixtureGroupDefinition()->getFixtureIds())
					groupedFixtureIds.insert(fixtureId);
			},
			[stageId](DMXFixtureGroupComponentPtr group)
			{ return group->getDMXFixtureGroupDefinition()->getOwnerStageId() == stageId; });
	}

	int ungroupedCount= 0;
	auto countUngrouped= [&](DMXFixtureComponentPtr fixture)
	{
		if (fixture->getDMXFixtureDefinition()->getOwnerStageId() == stageId
			&& groupedFixtureIds.count(fixture->getComponentId()) == 0)
		{
			++ungroupedCount;
		}
	};
	if (RGBSpotLightSystemPtr spotLightSystem= projectManager->getSystemOfType<RGBSpotLightSystem>())
		spotLightSystem->visitComponents([&](RGBSpotLightComponentPtr light) { countUngrouped(light); });
	if (RGBPixelGridSystemPtr pixelGridSystem= projectManager->getSystemOfType<RGBPixelGridSystem>())
		pixelGridSystem->visitComponents([&](RGBPixelGridComponentPtr grid) { countUngrouped(grid); });

	if (ungroupedCount > 0)
	{
		const int ungroupedIndex= addNode(snapshot, "stage.groups.ungrouped", ePipelineNodeKind::member,
										  ePipelineBox::ungroupedLights, ePipelineStage::stage, groupsIndex);
		snapshot.nodes[ungroupedIndex].facts.bPresent= true;
		snapshot.nodes[ungroupedIndex].facts.count= ungroupedCount;
	}
	countKindBoxMembers(snapshot, groupsIndex);
}

// Whether the compositor graph pulls the member, left unknown with no graph
void setGraphUse(PipelineNode& member, const CompositorGraphReferences* references, const std::set<int>* usedIds)
{
	if (references == nullptr)
		return;

	member.facts.bGraphUseKnown= true;
	member.facts.bUsedByGraph= usedIds->count(member.componentId) > 0;
}

// A model path that did not load leaves the stencil masking nothing
bool isModelStencilFailed(ModelStencilComponentPtr stencil)
{
	return stencil->getModelStencilDefinition()->hasModelPath() && !stencil->getRenderModelResource();
}

int addStencilMember(CompositorComponentPtr compositor, const CompositorGraphReferences* references,
					 StencilComponent* stencil, bool bFault, ePipelineBox box, const std::string& parentId,
					 int parentIndex, PipelineSnapshot& snapshot)
{
	if (!compositor->ownsSceneObject(stencil->getOwnerObject()))
		return -1;

	const int memberIndex= addNode(snapshot, makeChildId(parentId, stencil->getComponentId()),
								   ePipelineNodeKind::member, box, snapshot.nodes[parentIndex].stage, parentIndex);
	PipelineNode& member= snapshot.nodes[memberIndex];
	setPresentComponent(member, stencil);
	member.facts.bEnabled= !stencil->getStencilComponentDefinition()->getIsDisabled();
	member.facts.bFault= bFault;
	setGraphUse(member, references, references ? &references->stencilIds : nullptr);
	return memberIndex;
}

// The tool that captures a depth mesh member again: a depth proxy from the scene,
// an occlusion mesh from the recorded take its camera plays
void setDepthMeshTool(PipelineNode& member, eModelStencilCaptureKind captureKind, SceneComponentPtr scene)
{
	if (captureKind == eModelStencilCaptureKind::depthProxy)
	{
		setNodeTool(member, SceneComponent::k_captureDepthMeshFunctionId, scene.get());
		return;
	}

	CameraComponentPtr camera= scene->getCameraComponent();
	FileVideoSourceComponentPtr fileVideoSource=
		camera ? std::dynamic_pointer_cast<FileVideoSourceComponent>(camera->getVideoSourceComponent())
			   : FileVideoSourceComponentPtr();
	if (fileVideoSource)
		setNodeTool(member, FileVideoSourceComponent::k_buildOcclusionMeshFunctionId, fileVideoSource.get());
}

// The model stencils a capture made, which the compositor owns like any stencil
void gatherDepthMeshes(ProjectManagerPtr projectManager, SceneComponentPtr scene, CompositorComponentPtr compositor,
					   const CompositorGraphReferences* references, int depthMeshIndex, PipelineSnapshot& snapshot)
{
	ModelStencilSystemPtr modelSystem= projectManager->getSystemOfType<ModelStencilSystem>();
	if (!compositor || !modelSystem)
		return;

	modelSystem->visitComponents(
		[&](ModelStencilComponentPtr stencil)
		{
			const eModelStencilCaptureKind captureKind= stencil->getModelStencilDefinition()->getCaptureKind();
			const int memberIndex=
				addStencilMember(compositor, references, stencil.get(), isModelStencilFailed(stencil),
								 ePipelineBox::depthMesh, "scene.depthmesh", depthMeshIndex, snapshot);
			if (memberIndex >= 0)
				setDepthMeshTool(snapshot.nodes[memberIndex], captureKind, scene);
		},
		[](ModelStencilComponentPtr stencil)
		{ return stencil->getModelStencilDefinition()->getCaptureKind() != eModelStencilCaptureKind::none; });
}

void gatherScene(ProjectManagerPtr projectManager, SceneComponentPtr scene, CompositorComponentPtr compositor,
				 const CompositorGraphReferences* references, DMXPresetComponentPtr scenePreset, int sceneIndex,
				 PipelineSnapshot& snapshot)
{
	setPresentComponent(snapshot.nodes[sceneIndex], scene.get());

	const int envLightIndex= addNode(snapshot, "scene.envlight", ePipelineNodeKind::box, ePipelineBox::environmentLight,
									 ePipelineStage::scene, sceneIndex);
	{
		PipelineNode& envLightNode= snapshot.nodes[envLightIndex];
		if (LightEnvironmentComponentPtr lightEnvironment= scene->getLightEnvironment())
		{
			setPresentComponent(envLightNode, lightEnvironment.get());
			envLightNode.facts.bHasSignal= true;
			envLightNode.facts.bActive= lightEnvironment->getLightEnvironmentDefinition()->hasCapturedLighting();
			setNodeTool(envLightNode, LightEnvironmentComponent::k_captureSceneLightingFunctionId,
						lightEnvironment.get());
		}
		else
		{
			// The capture creates the probe, so the tool works on an absent one too
			setNodeTool(envLightNode, SceneComponent::k_captureSceneLightingFunctionId, scene.get());
		}
	}

	const int depthMeshIndex= addNode(snapshot, "scene.depthmesh", ePipelineNodeKind::kindBox,
									  ePipelineBox::depthMeshes, ePipelineStage::scene, sceneIndex);
	snapshot.nodes[depthMeshIndex].facts.bPresent= true;
	setNodeTool(snapshot.nodes[depthMeshIndex], SceneComponent::k_captureDepthMeshFunctionId, scene.get());
	gatherDepthMeshes(projectManager, scene, compositor, references, depthMeshIndex, snapshot);
	countKindBoxMembers(snapshot, depthMeshIndex);

	const int presetIndex= addNode(snapshot, "scene.preset", ePipelineNodeKind::box, ePipelineBox::lightingPreset,
								   ePipelineStage::scene, sceneIndex);
	if (scenePreset)
		setPresentComponent(snapshot.nodes[presetIndex], scenePreset.get());
}

void gatherCamera(CameraComponentPtr camera, int cameraIndex, PipelineSnapshot& snapshot)
{
	if (!camera)
		return;

	setPresentComponent(snapshot.nodes[cameraIndex], camera.get());

	const int mountIndex= addNode(snapshot, "camera.mount", ePipelineNodeKind::box, ePipelineBox::cameraMount,
								  ePipelineStage::camera, cameraIndex);
	if (TrackingMountComponentConstPtr mount= camera->getTrackingMountComponent())
	{
		setPresentComponent(snapshot.nodes[mountIndex], mount.get());
		snapshot.nodes[mountIndex].facts.bActive= camera->hasValidTrackingMountPoseView();
	}

	VideoSourceComponentPtr videoSource= camera->getVideoSourceComponent();

	const int videoIndex= addNode(snapshot, "camera.video", ePipelineNodeKind::box, ePipelineBox::videoSource,
								  ePipelineStage::camera, cameraIndex);
	snapshot.nodes[videoIndex].facts.bRequired= true;
	if (videoSource)
	{
		PipelineNode& videoNode= snapshot.nodes[videoIndex];
		setPresentComponent(videoNode, videoSource.get());
		videoNode.facts.variant= (int)toVideoVariant(videoSource->getVideoStreamingStatus());

		float frameRate= 0.f;
		if (videoSource->getFrameRate(frameRate))
			videoNode.facts.rateHz= frameRate;
	}

	// The lens calibration lives on the video source, so selecting it selects the source
	const int lensIndex= addNode(snapshot, "camera.lens", ePipelineNodeKind::box, ePipelineBox::lensCalibration,
								 ePipelineStage::camera, cameraIndex);
	snapshot.nodes[lensIndex].facts.bRequired= true;
	if (videoSource)
	{
		PipelineNode& lensNode= snapshot.nodes[lensIndex];
		lensNode.componentId= videoSource->getComponentId();
		lensNode.facts.bPresent= true;
		lensNode.facts.variant= (int)toLensVariant(videoSource->getCameraIntrinsicsStatus());
		setNodeTool(lensNode, VideoSourceComponent::k_calibrateIntrinsicsFunctionId, videoSource.get());
	}
}

float findClientPublishRate(ClientSourceManager* clientSourceManager, const std::string& clientId,
							MikanCameraID cameraId)
{
	for (const auto& [tableKey, clientSource] : clientSourceManager->getClientSources().getMap())
	{
		if (clientSource->clientId == clientId && clientSource->readAccessor != nullptr
			&& clientSource->readAccessor->getCameraId() == cameraId)
		{
			return clientSource->publishRateHz;
		}
	}
	return 0.f;
}

// One member per texture source the graph reads, then one per reference that no
// longer resolves, then the first client stand-in
void gatherTextureSources(ProjectManagerPtr projectManager, ClientSourceManager* clientSourceManager,
						  CompositorComponentPtr compositor, const CompositorGraphReferences& references,
						  int sourcesIndex, PipelineSnapshot& snapshot)
{
	const MikanCameraID cameraId= compositor->getCameraId();
	int unresolvedCount= references.unresolvedTextureSourceCount;
	for (int textureSourceId : references.textureSourceIds)
	{
		TextureSourceComponentPtr textureSource=
			TextureSourceQueries::getTextureSourceById(projectManager, textureSourceId);
		const std::string memberId= makeChildId("compositor.sources", textureSourceId);

		if (auto clientSource= std::dynamic_pointer_cast<ClientTextureSourceComponent>(textureSource))
		{
			const int memberIndex= addNode(snapshot, memberId, ePipelineNodeKind::member,
										   ePipelineBox::clientTextureSource, ePipelineStage::compositor, sourcesIndex);
			PipelineNode& member= snapshot.nodes[memberIndex];
			setPresentComponent(member, clientSource.get());
			member.facts.bHasSignal= true;
			if (clientSourceManager != nullptr)
			{
				const std::string& clientSourceName= clientSource->getClientSourceName();
				member.facts.bActive= clientSourceManager->hasClientSource(clientSourceName, cameraId);
				member.facts.rateHz= findClientPublishRate(clientSourceManager, clientSourceName, cameraId);
			}
		}
		else if (auto spoutSource= std::dynamic_pointer_cast<SpoutTextureSourceComponent>(textureSource))
		{
			const int memberIndex= addNode(snapshot, memberId, ePipelineNodeKind::member,
										   ePipelineBox::spoutTextureSource, ePipelineStage::compositor, sourcesIndex);
			PipelineNode& member= snapshot.nodes[memberIndex];
			setPresentComponent(member, spoutSource.get());
			member.facts.bHasSignal= true;
			member.facts.bActive= spoutSource->getIsReceiving();
		}
		else if (auto cefSource= std::dynamic_pointer_cast<CEFTextureSourceComponent>(textureSource))
		{
			const int memberIndex= addNode(snapshot, memberId, ePipelineNodeKind::member,
										   ePipelineBox::cefTextureSource, ePipelineStage::compositor, sourcesIndex);
			PipelineNode& member= snapshot.nodes[memberIndex];
			setPresentComponent(member, cefSource.get());
			member.facts.bHasSignal= true;
			member.facts.bActive= cefSource->hasBrowser();
			member.facts.variant= (int)(cefSource->getBrowserIsLoading() ? ePipelineBrowserVariant::loading
																		 : ePipelineBrowserVariant::ready);
			setNodeTool(member, TextureSourceComponent::k_showTextureSourceSettingsFunctionId, cefSource.get());
		}
		else
		{
			// A shape graph keeps its source after the project has dropped it
			++unresolvedCount;
		}
	}

	for (int unresolvedIndex= 0; unresolvedIndex < unresolvedCount; ++unresolvedIndex)
	{
		const int memberIndex= addNode(snapshot, "compositor.sources.unresolved" + std::to_string(unresolvedIndex),
									   ePipelineNodeKind::member, ePipelineBox::unresolvedTextureSource,
									   ePipelineStage::compositor, sourcesIndex);
		snapshot.nodes[memberIndex].facts.bPresent= true;
	}

	if (references.bUsesFirstClientSource)
	{
		const int memberIndex=
			addNode(snapshot, "compositor.sources.firstclient", ePipelineNodeKind::member,
					ePipelineBox::firstClientTextureSource, ePipelineStage::compositor, sourcesIndex);
		PipelineFacts& facts= snapshot.nodes[memberIndex].facts;
		facts.bPresent= true;
		facts.bHasSignal= true;
		std::string firstClientId;
		if (clientSourceManager != nullptr && clientSourceManager->getFirstClientIdForCamera(cameraId, firstClientId))
		{
			facts.bActive= true;
			facts.rateHz= findClientPublishRate(clientSourceManager, firstClientId, cameraId);
		}
	}
	countKindBoxMembers(snapshot, sourcesIndex);
}

void addShapeMember(CompositorComponentPtr compositor, const CompositorGraphReferences* references,
					ShapeComponent* shape, int shapesIndex, PipelineSnapshot& snapshot)
{
	if (!compositor->ownsSceneObject(shape->getOwnerObject()))
		return;

	const int memberIndex=
		addNode(snapshot, makeChildId("compositor.shapes", shape->getComponentId()), ePipelineNodeKind::member,
				ePipelineBox::shape, ePipelineStage::compositor, shapesIndex);
	setPresentComponent(snapshot.nodes[memberIndex], shape);
	setGraphUse(snapshot.nodes[memberIndex], references, references ? &references->shapeIds : nullptr);
}

void gatherCompositor(ProjectManagerPtr projectManager, ClientSourceManager* clientSourceManager,
					  CompositorComponentPtr compositor, const CompositorGraphReferences* references,
					  int compositorIndex, PipelineSnapshot& snapshot)
{
	if (!compositor)
		return;

	{
		PipelineNode& compositorNode= snapshot.nodes[compositorIndex];
		setPresentComponent(compositorNode, compositor.get());
		setNodeTool(compositorNode, CompositorComponent::k_editCompositorGraphFunctionId, compositor.get());
		compositorNode.facts.bEnabled= compositor->hasValidCompositorGraph();
		compositorNode.facts.bActive= compositor->getIsRunning();
		compositorNode.facts.rateHz= compositor->getOutputFrameRate();
		for (const NodeEvaluationError& evalError : compositor->getLastNodeEvalErrors())
		{
			if (evalError.severity == eNodeEvaluationSeverity::error)
				++compositorNode.facts.errorCount;
		}
		if (compositor->getIsEditorHeld() && !compositor->getIsOutputStreamingAllowed())
			compositorNode.facts.variant= (int)ePipelineCompositorVariant::editorHeld;
	}

	const int sourcesIndex= addNode(snapshot, "compositor.sources", ePipelineNodeKind::kindBox,
									ePipelineBox::textureSources, ePipelineStage::compositor, compositorIndex);
	snapshot.nodes[sourcesIndex].facts.bPresent= true;
	gatherTextureSources(projectManager, clientSourceManager, compositor,
						 references ? *references : CompositorGraphReferences(), sourcesIndex, snapshot);

	const int stencilsIndex= addNode(snapshot, "compositor.stencils", ePipelineNodeKind::kindBox,
									 ePipelineBox::stencils, ePipelineStage::compositor, compositorIndex);
	snapshot.nodes[stencilsIndex].facts.bPresent= true;
	auto addStencil= [&](StencilComponent* stencil, bool bFault)
	{
		addStencilMember(compositor, references, stencil, bFault, ePipelineBox::stencil, "compositor.stencils",
						 stencilsIndex, snapshot);
	};
	if (QuadStencilSystemPtr quadSystem= projectManager->getSystemOfType<QuadStencilSystem>())
		quadSystem->visitComponents([&](QuadStencilComponentPtr stencil) { addStencil(stencil.get(), false); });
	if (BoxStencilSystemPtr boxSystem= projectManager->getSystemOfType<BoxStencilSystem>())
		boxSystem->visitComponents([&](BoxStencilComponentPtr stencil) { addStencil(stencil.get(), false); });
	if (ModelStencilSystemPtr modelSystem= projectManager->getSystemOfType<ModelStencilSystem>())
	{
		modelSystem->visitComponents([&](ModelStencilComponentPtr stencil)
									 { addStencil(stencil.get(), isModelStencilFailed(stencil)); });
	}
	countKindBoxMembers(snapshot, stencilsIndex);

	const int shapesIndex= addNode(snapshot, "compositor.shapes", ePipelineNodeKind::kindBox, ePipelineBox::shapes,
								   ePipelineStage::compositor, compositorIndex);
	snapshot.nodes[shapesIndex].facts.bPresent= true;
	auto addShape= [&](ShapeComponent* shape) { addShapeMember(compositor, references, shape, shapesIndex, snapshot); };
	if (QuadShapeSystemPtr quadSystem= projectManager->getSystemOfType<QuadShapeSystem>())
		quadSystem->visitComponents([&](QuadShapeComponentPtr shape) { addShape(shape.get()); });
	if (BoxShapeSystemPtr boxSystem= projectManager->getSystemOfType<BoxShapeSystem>())
		boxSystem->visitComponents([&](BoxShapeComponentPtr shape) { addShape(shape.get()); });
	if (ModelShapeSystemPtr modelSystem= projectManager->getSystemOfType<ModelShapeSystem>())
		modelSystem->visitComponents([&](ModelShapeComponentPtr shape) { addShape(shape.get()); });
	countKindBoxMembers(snapshot, shapesIndex);
}

void gatherOutputs(ProjectManagerPtr projectManager, int outputIndex, PipelineSnapshot& snapshot)
{
	snapshot.nodes[outputIndex].facts.bPresent= true;

	int publishingCount= 0;
	OutputQueries::visitOutputs(
		projectManager,
		[&](OutputComponentPtr output)
		{
			const bool bEnabled= output->getOutputDefinition()->getIsEnabled();

			if (SpoutOutputComponentPtr spoutOutput= std::dynamic_pointer_cast<SpoutOutputComponent>(output))
			{
				const int nodeIndex=
					addNode(snapshot, makeChildId("output", output->getComponentId()), ePipelineNodeKind::box,
							ePipelineBox::spoutOutput, ePipelineStage::output, outputIndex);
				PipelineNode& node= snapshot.nodes[nodeIndex];
				setPresentComponent(node, output.get());
				node.facts.bEnabled= bEnabled;
				node.facts.bActive= spoutOutput->getIsSenderOpen();
				node.facts.detailName= spoutOutput->getSpoutOutputDefinition()->getSpoutOutputName();
				if (bEnabled && node.facts.bActive)
					++publishingCount;
			}
			else if (RTPOutputComponentPtr rtpOutput= std::dynamic_pointer_cast<RTPOutputComponent>(output))
			{
				const int nodeIndex=
					addNode(snapshot, makeChildId("output", output->getComponentId()), ePipelineNodeKind::box,
							ePipelineBox::rtpOutput, ePipelineStage::output, outputIndex);
				PipelineNode& node= snapshot.nodes[nodeIndex];
				setPresentComponent(node, output.get());
				node.facts.bEnabled= bEnabled;
				// Reads true only while enabled, and the disabled rule runs first
				node.facts.bFault= !rtpOutput->getHasDestination();
				node.facts.bHasSignal= true;
				node.facts.bActive= rtpOutput->getIsStreaming();

				int width= 0;
				int height= 0;
				float fps= 0.f;
				if (rtpOutput->getStreamFormat(width, height, fps))
					node.facts.detailName= std::to_string(width) + "x" + std::to_string(height);

				if (!node.facts.bFault && node.facts.bActive)
					++publishingCount;
			}
		});
	snapshot.nodes[outputIndex].facts.count= publishingCount;

	const int returnFeedIndex= addNode(snapshot, "output.returnfeed", ePipelineNodeKind::box, ePipelineBox::returnFeed,
									   ePipelineStage::output, outputIndex);
	snapshot.nodes[returnFeedIndex].facts.bPresent= true;
	// The destinations are collected per frame and not kept, so the count
	// stays unread
	if (RTPOutputSystemPtr rtpSystem= projectManager->getSystemOfType<RTPOutputSystem>())
	{
		snapshot.nodes[returnFeedIndex].facts.bHasSignal= true;
		snapshot.nodes[returnFeedIndex].facts.bActive= rtpSystem->getIsReturnFeedStreaming();
	}
}
} // namespace

PipelineSnapshot gatherProjectPipeline(ProjectManagerPtr projectManager, ClientSourceManager* clientSourceManager)
{
	PipelineSnapshot snapshot;

	static const struct
	{
		const char* id;
		ePipelineBox box;
	} k_stageNodes[(int)ePipelineStage::COUNT]= {
		{"volume", ePipelineBox::volume}, {"stage", ePipelineBox::stage},           {"scene", ePipelineBox::scene},
		{"camera", ePipelineBox::camera}, {"compositor", ePipelineBox::compositor}, {"output", ePipelineBox::output},
	};
	for (int stageIndex= 0; stageIndex < (int)ePipelineStage::COUNT; ++stageIndex)
	{
		addNode(snapshot, k_stageNodes[stageIndex].id, ePipelineNodeKind::stage, k_stageNodes[stageIndex].box,
				(ePipelineStage)stageIndex, -1);
	}

	// With no current scene the scene alone reads as missing, and the stages it
	// would have named read as idle rather than each as its own fault
	const int sceneIndex= (int)ePipelineStage::scene;
	snapshot.nodes[sceneIndex].facts.bRequired= true;

	SceneObjectSystemPtr sceneSystem= projectManager ? projectManager->getSystemOfType<SceneObjectSystem>() : nullptr;
	SceneComponentPtr scene= sceneSystem ? sceneSystem->getCurrentScene() : nullptr;
	if (!scene)
		return snapshot;

	snapshot.bHasCurrentScene= true;
	for (int stageIndex : {(int)ePipelineStage::volume, (int)ePipelineStage::stage, (int)ePipelineStage::camera,
						   (int)ePipelineStage::compositor})
	{
		snapshot.nodes[stageIndex].facts.bRequired= true;
	}

	DMXPresetComponentPtr scenePreset;
	if (DMXPresetSystemPtr presetSystem= projectManager->getSystemOfType<DMXPresetSystem>())
		scenePreset= presetSystem->getPresetById(scene->getSceneComponentDefinition()->getLightingPresetId());

	// One walk of the compositor graph answers every stencil, shape and texture
	// source member, with graph use left unknown when there is no graph
	CompositorComponentPtr compositor= scene->getCompositor();
	CompositorGraphReferences graphReferences;
	const CompositorGraphReferences* references= nullptr;
	CompositorNodeGraphPtr nodeGraph= compositor ? compositor->getActiveCompositorNodeGraph() : nullptr;
	if (nodeGraph)
	{
		nodeGraph->gatherReferencedSceneObjects(graphReferences);
		references= &graphReferences;
	}

	StageComponentPtr stage= scene->getParentStage();
	CameraComponentPtr camera= scene->getCameraComponent();
	gatherVolume(projectManager, stage, camera, (int)ePipelineStage::volume, snapshot);
	gatherStage(projectManager, stage, scenePreset, (int)ePipelineStage::stage, snapshot);
	gatherScene(projectManager, scene, compositor, references, scenePreset, sceneIndex, snapshot);
	gatherCamera(camera, (int)ePipelineStage::camera, snapshot);
	gatherCompositor(projectManager, clientSourceManager, compositor, references, (int)ePipelineStage::compositor,
					 snapshot);
	gatherOutputs(projectManager, (int)ePipelineStage::output, snapshot);

	return snapshot;
}

std::string formatPipelineText(const PipelineText& text)
{
	if (text.key.empty())
		return text.name;

	const std::string& key= text.key;
	const bool bIsFormat= key.size() >= 3 && key.compare(key.size() - 3, 3, "Fmt") == 0;
	if (bIsFormat)
		return locFormat(key.c_str(), text.args[0], text.args[1], text.args[2]);

	return locText(key.c_str());
}

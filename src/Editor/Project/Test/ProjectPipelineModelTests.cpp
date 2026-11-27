#include "ProjectPipelineModelTests.h"
#include "Project/ProjectPipelineModel.h"
#include "unit_test.h"

#include <set>
#include <stdio.h>
#include <string>
#include <vector>

bool run_project_pipeline_model_tests()
{
	UNIT_TEST_MODULE_BEGIN("project_pipeline_model")
	UNIT_TEST_MODULE_CALL_TEST(project_pipeline_model_test_healthy_snapshot);
	UNIT_TEST_MODULE_CALL_TEST(project_pipeline_model_test_absent_required_vs_optional);
	UNIT_TEST_MODULE_CALL_TEST(project_pipeline_model_test_volume_and_runtime);
	UNIT_TEST_MODULE_CALL_TEST(project_pipeline_model_test_mounts);
	UNIT_TEST_MODULE_CALL_TEST(project_pipeline_model_test_stage_and_scene_boxes);
	UNIT_TEST_MODULE_CALL_TEST(project_pipeline_model_test_video_source_variants);
	UNIT_TEST_MODULE_CALL_TEST(project_pipeline_model_test_lens_variants);
	UNIT_TEST_MODULE_CALL_TEST(project_pipeline_model_test_compositor);
	UNIT_TEST_MODULE_CALL_TEST(project_pipeline_model_test_texture_sources);
	UNIT_TEST_MODULE_CALL_TEST(project_pipeline_model_test_stencils);
	UNIT_TEST_MODULE_CALL_TEST(project_pipeline_model_test_outputs);
	UNIT_TEST_MODULE_CALL_TEST(project_pipeline_model_test_roll_up);
	UNIT_TEST_MODULE_CALL_TEST(project_pipeline_model_test_no_current_scene);
	UNIT_TEST_MODULE_CALL_TEST(project_pipeline_model_test_lookups);
	UNIT_TEST_MODULE_CALL_TEST(project_pipeline_model_test_apply_click);
	UNIT_TEST_MODULE_CALL_TEST(project_pipeline_model_test_apply_follow);
	UNIT_TEST_MODULE_CALL_TEST(project_pipeline_model_test_validate_selection);
	UNIT_TEST_MODULE_CALL_TEST(project_pipeline_model_test_box_and_state_keys);
	UNIT_TEST_MODULE_END()
}

namespace
{
// The component a mount node and the camera's mount node share
const int k_sharedMountId= 500;
// The video source the video and lens boxes both stand for
const int k_videoSourceId= 410;

int addTestNode(PipelineSnapshot& snapshot, const char* id, ePipelineNodeKind kind, ePipelineBox box,
				ePipelineStage stage, const char* parentId, int componentId= -1, const char* name= "")
{
	PipelineNode node;
	node.id= id;
	node.kind= kind;
	node.box= box;
	node.stage= stage;
	node.parentIndex= ProjectPipelineModel::findNodeById(snapshot, parentId != nullptr ? parentId : "");
	node.componentId= componentId;
	node.name= name;
	node.facts.bPresent= true;
	snapshot.nodes.push_back(node);

	return (int)snapshot.nodes.size() - 1;
}

PipelineFacts& factsOf(PipelineSnapshot& snapshot, const char* id)
{
	return snapshot.nodes[ProjectPipelineModel::findNodeById(snapshot, id)].facts;
}

// Every stage, every sub-stage box, and a member in every kind box, each
// healthy: nothing reads degraded or not configured
PipelineSnapshot makeHealthySnapshot()
{
	using K= ePipelineNodeKind;
	using B= ePipelineBox;
	using S= ePipelineStage;

	PipelineSnapshot snapshot;
	snapshot.bHasCurrentScene= true;

	addTestNode(snapshot, "volume", K::stage, B::volume, S::volume, nullptr, 100, "VOL_100");
	addTestNode(snapshot, "stage", K::stage, B::stage, S::stage, nullptr, 200, "STAGE_200");
	addTestNode(snapshot, "scene", K::stage, B::scene, S::scene, nullptr, 300, "SCN_300");
	addTestNode(snapshot, "camera", K::stage, B::camera, S::camera, nullptr, 400, "CAM_400");
	addTestNode(snapshot, "compositor", K::stage, B::compositor, S::compositor, nullptr, 600, "COMP_600");
	addTestNode(snapshot, "output", K::stage, B::output, S::output, nullptr);
	for (const char* stageId : {"volume", "stage", "scene", "camera", "compositor"})
		factsOf(snapshot, stageId).bRequired= true;
	factsOf(snapshot, "volume").variant= (int)ePipelineVolumeVariant::vr;

	addTestNode(snapshot, "volume.runtime", K::box, B::steamVR, S::volume, "volume");
	factsOf(snapshot, "volume.runtime").variant= (int)ePipelineRuntimeVariant::ready;
	addTestNode(snapshot, "volume.origin", K::box, B::originMarker, S::volume, "volume", 110, "MARKER_110");
	addTestNode(snapshot, "volume.mat", K::box, B::matMount, S::volume, "volume", 101, "MOUNT_101");
	factsOf(snapshot, "volume.mat").bActive= true;
	addTestNode(snapshot, "volume.mounts", K::kindBox, B::trackingMounts, S::volume, "volume");
	factsOf(snapshot, "volume.mounts").count= 2;
	addTestNode(snapshot, "volume.mounts.500", K::member, B::trackingMount, S::volume, "volume.mounts", k_sharedMountId,
				"MOUNT_500");
	factsOf(snapshot, "volume.mounts.500").bActive= true;
	addTestNode(snapshot, "volume.mounts.501", K::member, B::trackingMount, S::volume, "volume.mounts", 501,
				"MOUNT_501");
	factsOf(snapshot, "volume.mounts.501").bActive= true;

	addTestNode(snapshot, "stage.dmx", K::box, B::dmx, S::stage, "stage");
	factsOf(snapshot, "stage.dmx").bHasSignal= true;
	factsOf(snapshot, "stage.dmx").bActive= true;
	factsOf(snapshot, "stage.dmx").count= 2;
	addTestNode(snapshot, "stage.groups", K::kindBox, B::lightGroups, S::stage, "stage");
	factsOf(snapshot, "stage.groups").count= 2;
	addTestNode(snapshot, "stage.groups.210", K::member, B::lightGroup, S::stage, "stage.groups", 210, "GROUP_210");
	factsOf(snapshot, "stage.groups.210").variant= (int)ePipelineLightGroupVariant::sequence;
	factsOf(snapshot, "stage.groups.210").detailName= "SEQ_211";
	addTestNode(snapshot, "stage.groups.ungrouped", K::member, B::ungroupedLights, S::stage, "stage.groups");
	factsOf(snapshot, "stage.groups.ungrouped").count= 3;

	addTestNode(snapshot, "scene.envlight", K::box, B::environmentLight, S::scene, "scene", 301, "PROBE_301");
	factsOf(snapshot, "scene.envlight").bHasSignal= true;
	factsOf(snapshot, "scene.envlight").bActive= true;
	addTestNode(snapshot, "scene.depthmesh", K::kindBox, B::depthMeshes, S::scene, "scene");
	factsOf(snapshot, "scene.depthmesh").count= 1;
	addTestNode(snapshot, "scene.depthmesh.310", K::member, B::depthMesh, S::scene, "scene.depthmesh", 310,
				"PROXY_310");
	addTestNode(snapshot, "scene.preset", K::box, B::lightingPreset, S::scene, "scene", 302, "PRESET_302");

	addTestNode(snapshot, "camera.mount", K::box, B::cameraMount, S::camera, "camera", k_sharedMountId, "MOUNT_500");
	factsOf(snapshot, "camera.mount").bActive= true;
	addTestNode(snapshot, "camera.video", K::box, B::videoSource, S::camera, "camera", k_videoSourceId, "VID_410");
	factsOf(snapshot, "camera.video").bRequired= true;
	factsOf(snapshot, "camera.video").variant= (int)ePipelineVideoVariant::started;
	factsOf(snapshot, "camera.video").rateHz= 30.f;
	addTestNode(snapshot, "camera.lens", K::box, B::lensCalibration, S::camera, "camera", k_videoSourceId);
	factsOf(snapshot, "camera.lens").bRequired= true;
	factsOf(snapshot, "camera.lens").variant= (int)ePipelineLensVariant::calibrated;

	PipelineFacts& compositorFacts= factsOf(snapshot, "compositor");
	compositorFacts.bEnabled= true;
	compositorFacts.bActive= true;
	compositorFacts.rateHz= 30.f;
	addTestNode(snapshot, "compositor.sources", K::kindBox, B::textureSources, S::compositor, "compositor");
	factsOf(snapshot, "compositor.sources").count= 1;
	addTestNode(snapshot, "compositor.sources.610", K::member, B::clientTextureSource, S::compositor,
				"compositor.sources", 610, "CLIENT_610");
	factsOf(snapshot, "compositor.sources.610").bHasSignal= true;
	factsOf(snapshot, "compositor.sources.610").bActive= true;
	factsOf(snapshot, "compositor.sources.610").rateHz= 60.f;
	addTestNode(snapshot, "compositor.stencils", K::kindBox, B::stencils, S::compositor, "compositor");
	factsOf(snapshot, "compositor.stencils").count= 1;
	addTestNode(snapshot, "compositor.stencils.620", K::member, B::stencil, S::compositor, "compositor.stencils", 620,
				"STENCIL_620");
	addTestNode(snapshot, "compositor.shapes", K::kindBox, B::shapes, S::compositor, "compositor");
	factsOf(snapshot, "compositor.shapes").count= 1;
	addTestNode(snapshot, "compositor.shapes.630", K::member, B::shape, S::compositor, "compositor.shapes", 630,
				"SHAPE_630");

	factsOf(snapshot, "output").count= 1;
	addTestNode(snapshot, "output.700", K::box, B::spoutOutput, S::output, "output", 700, "SPOUT_700");
	factsOf(snapshot, "output.700").bActive= true;
	factsOf(snapshot, "output.700").detailName= "MikanXR";
	addTestNode(snapshot, "output.701", K::box, B::rtpOutput, S::output, "output", 701, "RTP_701");
	addTestNode(snapshot, "output.returnfeed", K::box, B::returnFeed, S::output, "output");

	return snapshot;
}

// The no-current-scene snapshot the gather step emits: six absent stages, the
// scene alone required
PipelineSnapshot makeNoSceneSnapshot()
{
	PipelineSnapshot snapshot;
	const char* stageIds[]= {"volume", "stage", "scene", "camera", "compositor", "output"};
	const ePipelineBox stageBoxes[]= {ePipelineBox::volume, ePipelineBox::stage,      ePipelineBox::scene,
									  ePipelineBox::camera, ePipelineBox::compositor, ePipelineBox::output};
	for (int stageIndex= 0; stageIndex < (int)ePipelineStage::COUNT; ++stageIndex)
	{
		const int nodeIndex= addTestNode(snapshot, stageIds[stageIndex], ePipelineNodeKind::stage,
										 stageBoxes[stageIndex], (ePipelineStage)stageIndex, nullptr);
		snapshot.nodes[nodeIndex].facts.bPresent= false;
	}
	factsOf(snapshot, "scene").bRequired= true;

	return snapshot;
}

int indexOf(const PipelineSnapshot& snapshot, const char* id)
{
	return ProjectPipelineModel::findNodeById(snapshot, id);
}

PipelineStatus ownStatusOf(const PipelineSnapshot& snapshot, const char* id)
{
	return ProjectPipelineModel::computeOwnStatus(snapshot, indexOf(snapshot, id));
}

PipelineStatus statusOf(const PipelineSnapshot& snapshot, const char* id)
{
	return ProjectPipelineModel::computeStatus(snapshot, indexOf(snapshot, id));
}

// The state, the value key and the reason key, with "" for an empty key
bool expectStatus(const PipelineStatus& status, ePipelineState state, const char* valueKey, const char* reasonKey,
				  const char* label)
{
	const bool bMatches= status.state == state && status.value.key == valueKey && status.reason.key == reasonKey;
	if (!bMatches)
	{
		fprintf(stdout, "    FAILED: %s read %s '%s' '%s', expected %s '%s' '%s'\n", label,
				ProjectPipelineModel::getStateName(status.state), status.value.key.c_str(), status.reason.key.c_str(),
				ProjectPipelineModel::getStateName(state), valueKey, reasonKey);
	}
	return bMatches;
}

bool expectOwnStatus(const PipelineSnapshot& snapshot, const char* id, ePipelineState state, const char* valueKey,
					 const char* reasonKey)
{
	return expectStatus(ownStatusOf(snapshot, id), state, valueKey, reasonKey, id);
}

std::vector<std::string> idsOf(const PipelineSnapshot& snapshot, const std::vector<int>& nodeIndices)
{
	std::vector<std::string> ids;
	for (int nodeIndex : nodeIndices)
		ids.push_back(snapshot.nodes[nodeIndex].id);
	return ids;
}

bool expectSelection(const PipelineSelection& selection, const char* selectedId, const char* openStageId,
					 const char* openKindId, const char* label)
{
	const bool bMatches= selection.selectedId == selectedId && selection.openStageId == openStageId
						 && selection.openKindId == openKindId;
	if (!bMatches)
	{
		fprintf(stdout, "    FAILED: %s selected '%s' stage '%s' kind '%s', expected '%s' '%s' '%s'\n", label,
				selection.selectedId.c_str(), selection.openStageId.c_str(), selection.openKindId.c_str(), selectedId,
				openStageId, openKindId);
	}
	return bMatches;
}

// The boxes a member or an output stands for, labeled by the component's own
// name rather than a box name key
bool isComponentLabeledBox(ePipelineBox box)
{
	switch (box)
	{
	case ePipelineBox::trackingMount:
	case ePipelineBox::lightGroup:
	case ePipelineBox::depthMesh:
	case ePipelineBox::clientTextureSource:
	case ePipelineBox::spoutTextureSource:
	case ePipelineBox::cefTextureSource:
	case ePipelineBox::stencil:
	case ePipelineBox::shape:
	case ePipelineBox::spoutOutput:
	case ePipelineBox::rtpOutput:
		return true;
	default:
		break;
	}
	return false;
}
} // namespace

bool project_pipeline_model_test_healthy_snapshot()
{
	UNIT_TEST_BEGIN("healthy snapshot reads nothing degraded or missing")

	const PipelineSnapshot snapshot= makeHealthySnapshot();
	for (int nodeIndex= 0; nodeIndex < (int)snapshot.nodes.size(); ++nodeIndex)
	{
		const PipelineStatus status= ProjectPipelineModel::computeStatus(snapshot, nodeIndex);
		if (status.state == ePipelineState::degraded || status.state == ePipelineState::notConfigured)
		{
			fprintf(stdout, "    FAILED: %s reads %s\n", snapshot.nodes[nodeIndex].id.c_str(),
					ProjectPipelineModel::getStateName(status.state));
			success= false;
		}
		if (status.sourceNodeIndex != nodeIndex)
		{
			fprintf(stdout, "    FAILED: %s status came from another node\n", snapshot.nodes[nodeIndex].id.c_str());
			success= false;
		}
	}

	success&=
		expectStatus(statusOf(snapshot, "volume"), ePipelineState::configured, "pipeline.valueVolumeVR", "", "volume");
	success&= expectStatus(statusOf(snapshot, "scene"), ePipelineState::running, "", "", "scene");
	success&= statusOf(snapshot, "scene").value.name == "SCN_300";
	success&= expectStatus(statusOf(snapshot, "compositor"), ePipelineState::running, "pipeline.valueFpsFmt", "",
						   "compositor");
	success&= expectStatus(statusOf(snapshot, "output"), ePipelineState::running, "pipeline.valuePublishingFmt", "",
						   "output");
	success&= statusOf(snapshot, "output").value.args[0] == 1.0;

	UNIT_TEST_COMPLETE()
}

bool project_pipeline_model_test_absent_required_vs_optional()
{
	UNIT_TEST_BEGIN("every box reads absent-required as not configured, absent-optional as inactive")

	for (int boxIndex= 0; boxIndex < (int)ePipelineBox::COUNT; ++boxIndex)
	{
		const ePipelineBox box= (ePipelineBox)boxIndex;

		PipelineSnapshot snapshot;
		const int nodeIndex= addTestNode(snapshot, "node", ePipelineNodeKind::box, box, ePipelineStage::scene, nullptr);
		snapshot.nodes[nodeIndex].facts.bPresent= false;

		const char* expectedReason= "pipeline.reasonNotAssigned";
		switch (box)
		{
		case ePipelineBox::volume:
			expectedReason= "pipeline.reasonNoVolume";
			break;
		case ePipelineBox::stage:
			expectedReason= "pipeline.reasonNoStage";
			break;
		case ePipelineBox::scene:
			expectedReason= "pipeline.reasonNoCurrentScene";
			break;
		case ePipelineBox::originMarker:
			expectedReason= "pipeline.reasonNoOriginMarker";
			break;
		case ePipelineBox::camera:
			expectedReason= "pipeline.reasonNoCamera";
			break;
		case ePipelineBox::videoSource:
		case ePipelineBox::lensCalibration:
			expectedReason= "pipeline.reasonNoVideoSource";
			break;
		case ePipelineBox::compositor:
			expectedReason= "pipeline.reasonNoCompositor";
			break;
		default:
			break;
		}

		const std::string label= std::string("box ") + std::to_string(boxIndex);
		snapshot.nodes[nodeIndex].facts.bRequired= true;
		success&=
			expectStatus(ProjectPipelineModel::computeOwnStatus(snapshot, nodeIndex), ePipelineState::notConfigured,
						 "pipeline.valueMissing", expectedReason, (label + " required").c_str());

		snapshot.nodes[nodeIndex].facts.bRequired= false;
		success&= expectStatus(ProjectPipelineModel::computeOwnStatus(snapshot, nodeIndex), ePipelineState::inactive,
							   "pipeline.valueNone", "", (label + " optional").c_str());
	}

	// The same reading on the healthy snapshot's own nodes, one at a time
	const PipelineSnapshot healthy= makeHealthySnapshot();
	for (int nodeIndex= 0; nodeIndex < (int)healthy.nodes.size(); ++nodeIndex)
	{
		PipelineSnapshot snapshot= healthy;
		PipelineFacts& facts= snapshot.nodes[nodeIndex].facts;
		facts.bPresent= false;
		const PipelineStatus status= ProjectPipelineModel::computeOwnStatus(snapshot, nodeIndex);
		const ePipelineState expected= facts.bRequired ? ePipelineState::notConfigured : ePipelineState::inactive;
		if (status.state != expected)
		{
			fprintf(stdout, "    FAILED: absent %s reads %s\n", snapshot.nodes[nodeIndex].id.c_str(),
					ProjectPipelineModel::getStateName(status.state));
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

bool project_pipeline_model_test_volume_and_runtime()
{
	UNIT_TEST_BEGIN("volume kind and the VR runtime states")

	PipelineSnapshot snapshot= makeHealthySnapshot();
	factsOf(snapshot, "volume").variant= (int)ePipelineVolumeVariant::marker;
	success&= expectOwnStatus(snapshot, "volume", ePipelineState::configured, "pipeline.valueVolumeMarker", "");

	PipelineFacts& runtime= factsOf(snapshot, "volume.runtime");
	runtime.variant= (int)ePipelineRuntimeVariant::ready;
	success&= expectOwnStatus(snapshot, "volume.runtime", ePipelineState::running, "pipeline.valueReady", "");
	runtime.variant= (int)ePipelineRuntimeVariant::initializing;
	success&= expectOwnStatus(snapshot, "volume.runtime", ePipelineState::degraded, "pipeline.valueStarting",
							  "pipeline.reasonRuntimeStarting");
	runtime.variant= (int)ePipelineRuntimeVariant::failed;
	success&= expectOwnStatus(snapshot, "volume.runtime", ePipelineState::notConfigured, "pipeline.valueFailed",
							  "pipeline.reasonRuntimeFailed");
	runtime.variant= (int)ePipelineRuntimeVariant::uninitialized;
	success&= expectOwnStatus(snapshot, "volume.runtime", ePipelineState::notConfigured, "pipeline.valueNotRunning",
							  "pipeline.reasonRuntimeNotRunning");

	// An origin marker has no live signal, so a present one reads configured by name
	const PipelineStatus origin= ownStatusOf(snapshot, "volume.origin");
	success&= origin.state == ePipelineState::configured && origin.value.name == "MARKER_110";

	UNIT_TEST_COMPLETE()
}

bool project_pipeline_model_test_mounts()
{
	UNIT_TEST_BEGIN("a mount with no device reads degraded")

	for (const char* mountId : {"volume.mat", "volume.mounts.501", "camera.mount"})
	{
		PipelineSnapshot snapshot= makeHealthySnapshot();
		const PipelineStatus healthy= ownStatusOf(snapshot, mountId);
		success&= healthy.state == ePipelineState::configured && !healthy.value.name.empty();

		factsOf(snapshot, mountId).bActive= false;
		success&= expectOwnStatus(snapshot, mountId, ePipelineState::degraded, "pipeline.valueNoDevice",
								  "pipeline.reasonDeviceNotConnected");
	}

	UNIT_TEST_COMPLETE()
}

bool project_pipeline_model_test_stage_and_scene_boxes()
{
	UNIT_TEST_BEGIN("DMX, light groups, environment light, preset")

	PipelineSnapshot snapshot= makeHealthySnapshot();

	PipelineFacts& dmx= factsOf(snapshot, "stage.dmx");
	success&= expectOwnStatus(snapshot, "stage.dmx", ePipelineState::running, "pipeline.valueUniversesFmt", "");
	success&= ownStatusOf(snapshot, "stage.dmx").value.args[0] == 2.0;
	dmx.count= 0;
	success&= expectOwnStatus(snapshot, "stage.dmx", ePipelineState::inactive, "pipeline.valueIdle", "");
	dmx.bActive= false;
	success&= expectOwnStatus(snapshot, "stage.dmx", ePipelineState::degraded, "pipeline.valueNotRunning",
							  "pipeline.reasonDmxNotRunning");
	dmx.bHasSignal= false;
	success&= expectOwnStatus(snapshot, "stage.dmx", ePipelineState::configured, "", "");

	PipelineFacts& group= factsOf(snapshot, "stage.groups.210");
	success&= expectOwnStatus(snapshot, "stage.groups.210", ePipelineState::running, "", "");
	success&= ownStatusOf(snapshot, "stage.groups.210").value.name == "SEQ_211";
	group.variant= (int)ePipelineLightGroupVariant::preset;
	group.detailName= "PRESET_302";
	success&= ownStatusOf(snapshot, "stage.groups.210").value.name == "PRESET_302";
	group.variant= (int)ePipelineLightGroupVariant::dark;
	success&= expectOwnStatus(snapshot, "stage.groups.210", ePipelineState::inactive, "pipeline.valueDark", "");

	success&=
		expectOwnStatus(snapshot, "stage.groups.ungrouped", ePipelineState::configured, "pipeline.valueCountFmt", "");
	success&= ownStatusOf(snapshot, "stage.groups.ungrouped").value.args[0] == 3.0;

	PipelineFacts& envLight= factsOf(snapshot, "scene.envlight");
	success&= ownStatusOf(snapshot, "scene.envlight").value.name == "PROBE_301";
	envLight.bActive= false;
	success&= expectOwnStatus(snapshot, "scene.envlight", ePipelineState::inactive, "pipeline.valueNotCaptured",
							  "pipeline.reasonNotCaptured");
	// With no signal, a present probe is configured whatever bActive says
	envLight.bHasSignal= false;
	success&= ownStatusOf(snapshot, "scene.envlight").state == ePipelineState::configured;

	// An empty kind box is inactive, a full one configured by count
	factsOf(snapshot, "scene.depthmesh").count= 0;
	success&= expectOwnStatus(snapshot, "scene.depthmesh", ePipelineState::inactive, "pipeline.valueCountFmt", "");

	const PipelineStatus preset= ownStatusOf(snapshot, "scene.preset");
	success&= preset.state == ePipelineState::configured && preset.value.name == "PRESET_302";

	UNIT_TEST_COMPLETE()
}

bool project_pipeline_model_test_video_source_variants()
{
	UNIT_TEST_BEGIN("video source streaming states")

	PipelineSnapshot snapshot= makeHealthySnapshot();
	PipelineFacts& video= factsOf(snapshot, "camera.video");

	success&= expectOwnStatus(snapshot, "camera.video", ePipelineState::running, "pipeline.valueFpsFmt", "");
	success&= ownStatusOf(snapshot, "camera.video").value.args[0] == 30.0;
	video.variant= (int)ePipelineVideoVariant::pendingStart;
	success&= expectOwnStatus(snapshot, "camera.video", ePipelineState::degraded, "pipeline.valueStarting",
							  "pipeline.reasonVideoStarting");
	video.variant= (int)ePipelineVideoVariant::stopped;
	success&= expectOwnStatus(snapshot, "camera.video", ePipelineState::degraded, "pipeline.valueStopped",
							  "pipeline.reasonVideoStopped");
	video.variant= (int)ePipelineVideoVariant::failed;
	success&= expectOwnStatus(snapshot, "camera.video", ePipelineState::notConfigured, "pipeline.valueFailed",
							  "pipeline.reasonVideoFailed");

	UNIT_TEST_COMPLETE()
}

bool project_pipeline_model_test_lens_variants()
{
	UNIT_TEST_BEGIN("lens calibration states")

	PipelineSnapshot snapshot= makeHealthySnapshot();
	PipelineFacts& lens= factsOf(snapshot, "camera.lens");

	success&= expectOwnStatus(snapshot, "camera.lens", ePipelineState::configured, "pipeline.valueCalibrated", "");
	lens.variant= (int)ePipelineLensVariant::resolutionMismatch;
	success&= expectOwnStatus(snapshot, "camera.lens", ePipelineState::degraded, "pipeline.valueMismatch",
							  "pipeline.reasonLensResolutionMismatch");
	lens.variant= (int)ePipelineLensVariant::uncalibrated;
	success&= expectOwnStatus(snapshot, "camera.lens", ePipelineState::notConfigured, "pipeline.valueUncalibrated",
							  "pipeline.reasonLensUncalibrated");

	UNIT_TEST_COMPLETE()
}

bool project_pipeline_model_test_compositor()
{
	UNIT_TEST_BEGIN("compositor graph, running, errors, editor hold")

	{
		PipelineSnapshot snapshot= makeHealthySnapshot();
		factsOf(snapshot, "compositor").bEnabled= false;
		// No graph wins over stopped
		factsOf(snapshot, "compositor").bActive= false;
		success&= expectOwnStatus(snapshot, "compositor", ePipelineState::notConfigured, "pipeline.valueNoGraph",
								  "pipeline.reasonNoGraph");
	}
	{
		PipelineSnapshot snapshot= makeHealthySnapshot();
		factsOf(snapshot, "compositor").bActive= false;
		success&= expectOwnStatus(snapshot, "compositor", ePipelineState::inactive, "pipeline.valueStopped",
								  "pipeline.reasonCompositorStopped");
	}
	{
		PipelineSnapshot snapshot= makeHealthySnapshot();
		factsOf(snapshot, "compositor").errorCount= 2;
		success&= expectOwnStatus(snapshot, "compositor", ePipelineState::degraded, "pipeline.valueErrorsFmt",
								  "pipeline.reasonGraphErrorsFmt");
		success&= ownStatusOf(snapshot, "compositor").value.args[0] == 2.0;
		success&= ownStatusOf(snapshot, "compositor").reason.args[0] == 2.0;
	}
	{
		PipelineSnapshot snapshot= makeHealthySnapshot();
		factsOf(snapshot, "compositor").variant= (int)ePipelineCompositorVariant::editorHeld;
		success&= expectOwnStatus(snapshot, "compositor", ePipelineState::degraded, "pipeline.valueFpsFmt",
								  "pipeline.reasonEditorHeld");
	}

	UNIT_TEST_COMPLETE()
}

bool project_pipeline_model_test_texture_sources()
{
	UNIT_TEST_BEGIN("client source waiting, the other source kinds, unresolved")

	PipelineSnapshot snapshot= makeHealthySnapshot();
	success&= expectOwnStatus(snapshot, "compositor.sources.610", ePipelineState::running, "pipeline.valueFpsFmt", "");
	success&= ownStatusOf(snapshot, "compositor.sources.610").value.args[0] == 60.0;

	factsOf(snapshot, "compositor.sources.610").bActive= false;
	success&= expectOwnStatus(snapshot, "compositor.sources.610", ePipelineState::degraded, "pipeline.valueWaiting",
							  "pipeline.reasonWaitingForClient");

	PipelineSnapshot sources;
	const int spoutIndex= addTestNode(sources, "spout", ePipelineNodeKind::member, ePipelineBox::spoutTextureSource,
									  ePipelineStage::compositor, nullptr, 1);
	success&= expectOwnStatus(sources, "spout", ePipelineState::configured, "", "");
	sources.nodes[spoutIndex].facts.bHasSignal= true;
	success&= expectOwnStatus(sources, "spout", ePipelineState::degraded, "pipeline.valueWaiting",
							  "pipeline.reasonWaitingForSender");
	sources.nodes[spoutIndex].facts.bActive= true;
	success&= expectOwnStatus(sources, "spout", ePipelineState::running, "pipeline.valueReceiving", "");

	const int cefIndex= addTestNode(sources, "cef", ePipelineNodeKind::member, ePipelineBox::cefTextureSource,
									ePipelineStage::compositor, nullptr, 2);
	success&= expectOwnStatus(sources, "cef", ePipelineState::degraded, "pipeline.valueNoBrowser",
							  "pipeline.reasonNoBrowser");
	sources.nodes[cefIndex].facts.bActive= true;
	sources.nodes[cefIndex].facts.variant= (int)ePipelineBrowserVariant::loading;
	success&= expectOwnStatus(sources, "cef", ePipelineState::degraded, "pipeline.valueLoading",
							  "pipeline.reasonBrowserLoading");
	sources.nodes[cefIndex].facts.variant= (int)ePipelineBrowserVariant::ready;
	success&= expectOwnStatus(sources, "cef", ePipelineState::running, "pipeline.valueReady", "");

	addTestNode(sources, "firstclient", ePipelineNodeKind::member, ePipelineBox::firstClientTextureSource,
				ePipelineStage::compositor, nullptr);
	success&= expectOwnStatus(sources, "firstclient", ePipelineState::degraded, "pipeline.valueWaiting",
							  "pipeline.reasonWaitingForClient");

	addTestNode(sources, "unresolved", ePipelineNodeKind::member, ePipelineBox::unresolvedTextureSource,
				ePipelineStage::compositor, nullptr);
	success&= expectOwnStatus(sources, "unresolved", ePipelineState::notConfigured, "pipeline.valueMissing",
							  "pipeline.reasonUnresolvedTextureSource");

	UNIT_TEST_COMPLETE()
}

bool project_pipeline_model_test_stencils()
{
	UNIT_TEST_BEGIN("stencil disabled, unused with graph use known or not, fault")

	const char* stencilId= "compositor.stencils.620";
	{
		PipelineSnapshot snapshot= makeHealthySnapshot();
		success&= expectOwnStatus(snapshot, stencilId, ePipelineState::configured, "", "");
		factsOf(snapshot, stencilId).bEnabled= false;
		success&= expectOwnStatus(snapshot, stencilId, ePipelineState::inactive, "pipeline.valueDisabled", "");
	}
	{
		PipelineSnapshot snapshot= makeHealthySnapshot();
		PipelineFacts& stencil= factsOf(snapshot, stencilId);
		stencil.bUsedByGraph= false;
		stencil.bGraphUseKnown= false;
		success&= expectOwnStatus(snapshot, stencilId, ePipelineState::configured, "", "");
		stencil.bGraphUseKnown= true;
		success&= expectOwnStatus(snapshot, stencilId, ePipelineState::inactive, "pipeline.valueUnused",
								  "pipeline.reasonNotUsedByGraph");
		stencil.bUsedByGraph= true;
		success&= expectOwnStatus(snapshot, stencilId, ePipelineState::configured, "", "");
	}
	{
		PipelineSnapshot snapshot= makeHealthySnapshot();
		// A failed model reads failed even while switched off
		factsOf(snapshot, stencilId).bFault= true;
		factsOf(snapshot, stencilId).bEnabled= false;
		success&= expectOwnStatus(snapshot, stencilId, ePipelineState::notConfigured, "pipeline.valueFailed",
								  "pipeline.reasonModelFailed");
	}
	{
		PipelineSnapshot snapshot= makeHealthySnapshot();
		factsOf(snapshot, "compositor.shapes.630").bEnabled= false;
		success&=
			expectOwnStatus(snapshot, "compositor.shapes.630", ePipelineState::inactive, "pipeline.valueDisabled", "");
	}

	UNIT_TEST_COMPLETE()
}

bool project_pipeline_model_test_outputs()
{
	UNIT_TEST_BEGIN("outputs off, idle, no destination, publishing count")

	PipelineSnapshot snapshot= makeHealthySnapshot();

	const PipelineStatus spoutRunning= ownStatusOf(snapshot, "output.700");
	success&= spoutRunning.state == ePipelineState::running && spoutRunning.value.name == "MikanXR";
	factsOf(snapshot, "output.700").bActive= false;
	success&= expectOwnStatus(snapshot, "output.700", ePipelineState::inactive, "pipeline.valueIdle", "");
	factsOf(snapshot, "output.700").bEnabled= false;
	success&= expectOwnStatus(snapshot, "output.700", ePipelineState::inactive, "pipeline.valueOff", "");

	PipelineFacts& rtp= factsOf(snapshot, "output.701");
	success&= ownStatusOf(snapshot, "output.701").state == ePipelineState::configured;
	rtp.bFault= true;
	success&= expectOwnStatus(snapshot, "output.701", ePipelineState::notConfigured, "pipeline.valueNoDestination",
							  "pipeline.reasonNoDestination");
	rtp.bEnabled= false;
	success&= expectOwnStatus(snapshot, "output.701", ePipelineState::inactive, "pipeline.valueOff", "");
	rtp.bEnabled= true;
	rtp.bFault= false;
	rtp.bHasSignal= true;
	success&= expectOwnStatus(snapshot, "output.701", ePipelineState::inactive, "pipeline.valueIdle", "");
	rtp.bActive= true;
	success&= ownStatusOf(snapshot, "output.701").state == ePipelineState::running;

	PipelineFacts& returnFeed= factsOf(snapshot, "output.returnfeed");
	success&= expectOwnStatus(snapshot, "output.returnfeed", ePipelineState::configured, "", "");
	returnFeed.bHasSignal= true;
	success&= expectOwnStatus(snapshot, "output.returnfeed", ePipelineState::inactive, "pipeline.valueIdle", "");
	returnFeed.bActive= true;
	success&= expectOwnStatus(snapshot, "output.returnfeed", ePipelineState::running, "pipeline.valueStreaming", "");

	factsOf(snapshot, "output").count= 2;
	success&= expectOwnStatus(snapshot, "output", ePipelineState::running, "pipeline.valuePublishingFmt", "");
	success&= ownStatusOf(snapshot, "output").value.args[0] == 2.0;
	factsOf(snapshot, "output").count= 0;
	success&= expectOwnStatus(snapshot, "output", ePipelineState::inactive, "pipeline.valuePublishingFmt", "");

	UNIT_TEST_COMPLETE()
}

bool project_pipeline_model_test_roll_up()
{
	UNIT_TEST_BEGIN("roll-up reaches the parent only and names the source")

	{
		// A degraded member turns its kind box and its stage degraded
		PipelineSnapshot snapshot= makeHealthySnapshot();
		factsOf(snapshot, "volume.mounts.501").bActive= false;
		const int memberIndex= indexOf(snapshot, "volume.mounts.501");

		const PipelineStatus kindStatus= statusOf(snapshot, "volume.mounts");
		success&= expectStatus(kindStatus, ePipelineState::degraded, "pipeline.valueCountFmt",
							   "pipeline.reasonDeviceNotConnected", "volume.mounts rolled");
		success&= kindStatus.sourceNodeIndex == memberIndex;

		const PipelineStatus volumeStatus= statusOf(snapshot, "volume");
		success&= expectStatus(volumeStatus, ePipelineState::degraded, "pipeline.valueVolumeVR",
							   "pipeline.reasonDeviceNotConnected", "volume rolled");
		success&= volumeStatus.sourceNodeIndex == memberIndex;

		// The own status is untouched, and nothing reaches a sibling stage
		success&= ownStatusOf(snapshot, "volume").state == ePipelineState::configured;
		success&= statusOf(snapshot, "stage").state == ePipelineState::configured;
		success&= statusOf(snapshot, "scene").state == ePipelineState::running;
		success&= statusOf(snapshot, "camera").state == ePipelineState::configured;
	}
	{
		// A waiting client source degrades the compositor without touching the output
		PipelineSnapshot snapshot= makeHealthySnapshot();
		factsOf(snapshot, "compositor.sources.610").bActive= false;
		const PipelineStatus compositorStatus= statusOf(snapshot, "compositor");
		success&= expectStatus(compositorStatus, ePipelineState::degraded, "pipeline.valueFpsFmt",
							   "pipeline.reasonWaitingForClient", "compositor rolled");
		success&= compositorStatus.sourceNodeIndex == indexOf(snapshot, "compositor.sources.610");
		success&= statusOf(snapshot, "output").state == ePipelineState::running;
	}
	{
		// An inactive or configured child changes nothing
		PipelineSnapshot snapshot= makeHealthySnapshot();
		factsOf(snapshot, "compositor.stencils.620").bEnabled= false;
		factsOf(snapshot, "output.700").bEnabled= false;
		success&= expectStatus(statusOf(snapshot, "compositor.stencils"), ePipelineState::configured,
							   "pipeline.valueCountFmt", "", "compositor.stencils with a disabled member");
		success&= statusOf(snapshot, "compositor").state == ePipelineState::running;
		success&= statusOf(snapshot, "compositor").sourceNodeIndex == indexOf(snapshot, "compositor");
		success&= statusOf(snapshot, "output").state == ePipelineState::running;
	}
	{
		// The worst child wins: a missing video source over a degraded mount
		PipelineSnapshot snapshot= makeHealthySnapshot();
		factsOf(snapshot, "camera.mount").bActive= false;
		factsOf(snapshot, "camera.video").variant= (int)ePipelineVideoVariant::failed;
		const PipelineStatus cameraStatus= statusOf(snapshot, "camera");
		success&= expectStatus(cameraStatus, ePipelineState::notConfigured, "", "pipeline.reasonVideoFailed",
							   "camera rolled");
		success&= cameraStatus.sourceNodeIndex == indexOf(snapshot, "camera.video");
	}

	UNIT_TEST_COMPLETE()
}

bool project_pipeline_model_test_no_current_scene()
{
	UNIT_TEST_BEGIN("no current scene: six greyed stages, the scene missing")

	const PipelineSnapshot snapshot= makeNoSceneSnapshot();
	const std::vector<std::string> expectedIds= {"volume", "stage", "scene", "camera", "compositor", "output"};
	success&= idsOf(snapshot, ProjectPipelineModel::getTopRow(snapshot)) == expectedIds;

	for (const std::string& stageId : expectedIds)
	{
		const int stageIndex= indexOf(snapshot, stageId.c_str());
		success&= ProjectPipelineModel::getChildren(snapshot, stageIndex).empty();
		if (stageId == "scene")
		{
			success&=
				expectStatus(ProjectPipelineModel::computeStatus(snapshot, stageIndex), ePipelineState::notConfigured,
							 "pipeline.valueMissing", "pipeline.reasonNoCurrentScene", "scene");
		}
		else
		{
			success&= expectStatus(ProjectPipelineModel::computeStatus(snapshot, stageIndex), ePipelineState::inactive,
								   "pipeline.valueNone", "", stageId.c_str());
		}
	}

	UNIT_TEST_COMPLETE()
}

bool project_pipeline_model_test_lookups()
{
	UNIT_TEST_BEGIN("findNodeById, findNodesByComponentId, getTopRow, getChildren")

	const PipelineSnapshot snapshot= makeHealthySnapshot();

	const int videoIndex= ProjectPipelineModel::findNodeById(snapshot, "camera.video");
	success&= videoIndex >= 0 && snapshot.nodes[videoIndex].id == "camera.video";
	success&= ProjectPipelineModel::findNodeById(snapshot, "camera.nothing") == -1;
	success&= ProjectPipelineModel::findNodeById(snapshot, "") == -1;

	const std::vector<std::string> mountIds= {"volume.mounts.500", "camera.mount"};
	success&= idsOf(snapshot, ProjectPipelineModel::findNodesByComponentId(snapshot, k_sharedMountId)) == mountIds;
	success&= ProjectPipelineModel::findNodesByComponentId(snapshot, 999).empty();
	success&= ProjectPipelineModel::findNodesByComponentId(snapshot, -1).empty();

	const std::vector<std::string> topRow= {"volume", "stage", "scene", "camera", "compositor", "output"};
	success&= idsOf(snapshot, ProjectPipelineModel::getTopRow(snapshot)) == topRow;

	const std::vector<std::string> volumeChildren= {"volume.runtime", "volume.origin", "volume.mat", "volume.mounts"};
	success&=
		idsOf(snapshot, ProjectPipelineModel::getChildren(snapshot, indexOf(snapshot, "volume"))) == volumeChildren;
	const std::vector<std::string> mountMembers= {"volume.mounts.500", "volume.mounts.501"};
	success&= idsOf(snapshot, ProjectPipelineModel::getChildren(snapshot, indexOf(snapshot, "volume.mounts")))
			  == mountMembers;
	success&= ProjectPipelineModel::getChildren(snapshot, indexOf(snapshot, "camera.video")).empty();
	success&= ProjectPipelineModel::getChildren(snapshot, -1).empty();
	success&= ProjectPipelineModel::getChildren(snapshot, (int)snapshot.nodes.size()).empty();

	UNIT_TEST_COMPLETE()
}

bool project_pipeline_model_test_apply_click()
{
	UNIT_TEST_BEGIN("a click selects and opens the rows that hold the node")

	const PipelineSnapshot snapshot= makeHealthySnapshot();
	PipelineSelection selection;

	ProjectPipelineModel::applyClick(snapshot, indexOf(snapshot, "camera"), selection);
	success&= expectSelection(selection, "camera", "camera", "", "stage click");

	selection.openKindId= "volume.mounts";
	ProjectPipelineModel::applyClick(snapshot, indexOf(snapshot, "camera.video"), selection);
	success&= expectSelection(selection, "camera.video", "camera", "", "box click");

	ProjectPipelineModel::applyClick(snapshot, indexOf(snapshot, "compositor.stencils"), selection);
	success&= expectSelection(selection, "compositor.stencils", "compositor", "compositor.stencils", "kind box click");

	ProjectPipelineModel::applyClick(snapshot, indexOf(snapshot, "volume.mounts.501"), selection);
	success&= expectSelection(selection, "volume.mounts.501", "volume", "volume.mounts", "member click");

	// An index outside the snapshot changes nothing
	ProjectPipelineModel::applyClick(snapshot, -1, selection);
	ProjectPipelineModel::applyClick(snapshot, (int)snapshot.nodes.size(), selection);
	success&= expectSelection(selection, "volume.mounts.501", "volume", "volume.mounts", "invalid click");

	UNIT_TEST_COMPLETE()
}

bool project_pipeline_model_test_apply_follow()
{
	UNIT_TEST_BEGIN("follow selects in chain, keeps rows out of chain, prefers the open row")

	const PipelineSnapshot snapshot= makeHealthySnapshot();
	{
		PipelineSelection selection;
		ProjectPipelineModel::applyFollow(snapshot, 620, selection);
		success&= expectSelection(selection, "compositor.stencils.620", "compositor", "compositor.stencils",
								  "follow in chain");

		// Not in the chain: the highlight drops and the open rows stay
		ProjectPipelineModel::applyFollow(snapshot, 999, selection);
		success&= expectSelection(selection, "", "compositor", "compositor.stencils", "follow out of chain");
	}
	{
		// The shared mount with no row open takes its first node in chain order
		PipelineSelection selection;
		ProjectPipelineModel::applyFollow(snapshot, k_sharedMountId, selection);
		success&= expectSelection(selection, "volume.mounts.500", "volume", "volume.mounts", "follow tie, none open");
	}
	{
		// With the camera's row open, the camera's mount box wins
		PipelineSelection selection;
		ProjectPipelineModel::applyClick(snapshot, indexOf(snapshot, "camera"), selection);
		ProjectPipelineModel::applyFollow(snapshot, k_sharedMountId, selection);
		success&= expectSelection(selection, "camera.mount", "camera", "", "follow tie, camera open");
	}
	{
		// With the mounts member row open, the member wins
		PipelineSelection selection;
		selection.openStageId= "volume";
		selection.openKindId= "volume.mounts";
		ProjectPipelineModel::applyFollow(snapshot, k_sharedMountId, selection);
		success&= expectSelection(selection, "volume.mounts.500", "volume", "volume.mounts", "follow tie, mounts open");
	}
	{
		// A node already selected keeps the selection
		PipelineSelection selection;
		ProjectPipelineModel::applyClick(snapshot, indexOf(snapshot, "camera.mount"), selection);
		selection.openKindId= "volume.mounts";
		ProjectPipelineModel::applyFollow(snapshot, k_sharedMountId, selection);
		success&= expectSelection(selection, "camera.mount", "camera", "", "follow tie, already selected");
	}

	UNIT_TEST_COMPLETE()
}

bool project_pipeline_model_test_validate_selection()
{
	UNIT_TEST_BEGIN("validateSelection clears ids the snapshot no longer holds")

	const PipelineSnapshot snapshot= makeHealthySnapshot();

	PipelineSelection selection;
	selection.selectedId= "compositor.stencils.620";
	selection.openStageId= "compositor";
	selection.openKindId= "compositor.stencils";
	ProjectPipelineModel::validateSelection(snapshot, selection);
	success&= expectSelection(selection, "compositor.stencils.620", "compositor", "compositor.stencils", "all held");

	selection.selectedId= "compositor.stencils.9999";
	selection.openKindId= "compositor.gone";
	ProjectPipelineModel::validateSelection(snapshot, selection);
	success&= expectSelection(selection, "", "compositor", "", "two gone");

	const PipelineSnapshot noScene= makeNoSceneSnapshot();
	selection.selectedId= "camera.video";
	selection.openStageId= "camera";
	selection.openKindId= "";
	ProjectPipelineModel::validateSelection(noScene, selection);
	success&= expectSelection(selection, "", "camera", "", "no scene");

	UNIT_TEST_COMPLETE()
}

bool project_pipeline_model_test_box_and_state_keys()
{
	UNIT_TEST_BEGIN("every box not labeled by a component has a name key, every state a key and a name")

	for (int boxIndex= 0; boxIndex < (int)ePipelineBox::COUNT; ++boxIndex)
	{
		const ePipelineBox box= (ePipelineBox)boxIndex;
		const std::string nameKey= ProjectPipelineModel::getBoxNameKey(box);
		if (isComponentLabeledBox(box))
			continue;
		if (nameKey.rfind("pipeline.box", 0) != 0)
		{
			fprintf(stdout, "    FAILED: box %d has name key '%s'\n", boxIndex, nameKey.c_str());
			success= false;
		}
	}

	std::set<std::string> stateKeys;
	std::set<std::string> stateNames;
	for (int stateIndex= 0; stateIndex < (int)ePipelineState::COUNT; ++stateIndex)
	{
		const ePipelineState state= (ePipelineState)stateIndex;
		stateKeys.insert(ProjectPipelineModel::getStateKey(state));
		stateNames.insert(ProjectPipelineModel::getStateName(state));
	}
	success&= (int)stateKeys.size() == (int)ePipelineState::COUNT;
	success&= (int)stateNames.size() == (int)ePipelineState::COUNT;
	success&= std::string(ProjectPipelineModel::getStateName(ePipelineState::notConfigured)) == "not-configured";

	UNIT_TEST_COMPLETE()
}

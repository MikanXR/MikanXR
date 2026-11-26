#include "ProjectHierarchyMigrationTests.h"
#include "unit_test.h"

#include "ProjectHierarchyMigration.h"

#include <configuru.hpp>

#include <map>
#include <string>

namespace ProjectHierarchyMigrationTestsInternal
{
// Scene 10 displays compositor 21 of its three, scene 11 has two and no display
// choice, scene 12 has none, and compositor 26 names a scene that is gone. The
// saved counter sits below the highest id in the file.
const char* const k_legacyProject= R"({
	"persistentIDAllocator": { "next_id": 20 },
	"SceneObjectSystem": {
		"SceneComponentPool": {
			"component_definitions": [
				{ "component_id": 10, "component_name": "SCN_Desk", "parent_transform_id": 5,
				  "relative_scale": [1.0, 1.0, 1.0], "relative_quaternion": [1.0, 0.0, 0.0, 0.0],
				  "relative_position": [0.5, 0.0, 0.0], "display_compositor_id": 21, "force_render": false },
				{ "component_id": 11, "component_name": "SCN_Stage", "parent_transform_id": 6,
				  "relative_scale": [1.0, 1.0, 1.0], "relative_quaternion": [1.0, 0.0, 0.0, 0.0],
				  "relative_position": [0.0, 0.0, 0.0], "display_compositor_id": -1, "force_render": false },
				{ "component_id": 12, "component_name": "SCN_Empty", "parent_transform_id": 6,
				  "relative_scale": [1.0, 1.0, 1.0], "relative_quaternion": [1.0, 0.0, 0.0, 0.0],
				  "relative_position": [0.0, 0.0, 0.0], "display_compositor_id": -1, "force_render": false }
			]
		},
		"current_scene_id": 10
	},
	"CompositorObjectSystem": {
		"CompositorComponentPool": {
			"component_definitions": [
				{ "component_id": 20, "component_name": "COMP_DeskVideo", "camera_id": 30, "owner_scene_id": 10 },
				{ "component_id": 21, "component_name": "COMP_DeskMovie", "camera_id": 31, "owner_scene_id": 10 },
				{ "component_id": 22, "component_name": "COMP_DeskPhone", "camera_id": 32, "owner_scene_id": 10 },
				{ "component_id": 23, "component_name": "COMP_StageWide", "camera_id": 33, "owner_scene_id": 11 },
				{ "component_id": 24, "component_name": "COMP_StageTight", "camera_id": 34, "owner_scene_id": 11 },
				{ "component_id": 26, "component_name": "Orphan", "camera_id": 35, "owner_scene_id": 99 }
			]
		}
	}
})";

const configuru::Config& sceneDefinitions(const configuru::Config& projectJson)
{
	return projectJson["SceneObjectSystem"]["SceneComponentPool"]["component_definitions"];
}

const configuru::Config& compositorDefinitions(const configuru::Config& projectJson)
{
	return projectJson["CompositorObjectSystem"]["CompositorComponentPool"]["component_definitions"];
}

const configuru::Config* findById(const configuru::Config& definitions, int componentId)
{
	for (const configuru::Config& definition : definitions.as_array())
	{
		if (definition.get_or<int>("component_id", -1) == componentId)
			return &definition;
	}

	return nullptr;
}

const configuru::Config* findByName(const configuru::Config& definitions, const std::string& componentName)
{
	for (const configuru::Config& definition : definitions.as_array())
	{
		if (definition.get_or<std::string>("component_name", "") == componentName)
			return &definition;
	}

	return nullptr;
}

int ownerSceneOf(const configuru::Config& projectJson, int compositorId)
{
	const configuru::Config* compositor= findById(compositorDefinitions(projectJson), compositorId);

	return compositor ? compositor->get_or<int>("owner_scene_id", -1) : -1;
}
} // namespace ProjectHierarchyMigrationTestsInternal

using namespace ProjectHierarchyMigrationTestsInternal;

bool project_hierarchy_migration_test_one_compositor_per_scene()
{
	UNIT_TEST_BEGIN("every scene ends with exactly one compositor")
	configuru::Config projectJson= configuru::parse_string(k_legacyProject, configuru::JSON, "legacy");
	ProjectHierarchyMigration::Report report;
	success&= ProjectHierarchyMigration::migrateProjectJson(projectJson, report);

	// Three scenes, plus one per moved compositor: 20, 22, 24 and the orphan 26
	success&= sceneDefinitions(projectJson).array_size() == 7;
	success&= compositorDefinitions(projectJson).array_size() == 7;

	std::map<int, int> compositorCountBySceneId;
	for (const configuru::Config& compositor : compositorDefinitions(projectJson).as_array())
	{
		const int ownerSceneId= compositor.get_or<int>("owner_scene_id", -1);
		success&= findById(sceneDefinitions(projectJson), ownerSceneId) != nullptr;
		compositorCountBySceneId[ownerSceneId]++;
	}
	success&= compositorCountBySceneId.size() == 7;
	for (const auto& count_pair : compositorCountBySceneId)
	{
		success&= count_pair.second == 1;
	}

	for (const configuru::Config& scene : sceneDefinitions(projectJson).as_array())
	{
		success&= !scene.has_key("display_compositor_id");
	}
	UNIT_TEST_COMPLETE()
}

bool project_hierarchy_migration_test_kept_and_split()
{
	UNIT_TEST_BEGIN("a scene keeps its display compositor and the others split off")
	configuru::Config projectJson= configuru::parse_string(k_legacyProject, configuru::JSON, "legacy");
	ProjectHierarchyMigration::Report report;
	success&= ProjectHierarchyMigration::migrateProjectJson(projectJson, report);

	// The displayed compositor stays, and with no display choice the first does
	success&= ownerSceneOf(projectJson, 21) == 10;
	success&= ownerSceneOf(projectJson, 23) == 11;

	// A split scene is named from its compositor and sits where the old scene did
	const configuru::Config* deskVideoScene= findByName(sceneDefinitions(projectJson), "SCN_DeskVideo");
	success&= deskVideoScene != nullptr;
	if (deskVideoScene)
	{
		success&= ownerSceneOf(projectJson, 20) == deskVideoScene->get_or<int>("component_id", -1);
		success&= deskVideoScene->get_or<int>("parent_transform_id", -1) == 5;
		success&= (float)(*deskVideoScene)["relative_position"][0] == 0.5f;
	}
	success&= findByName(sceneDefinitions(projectJson), "SCN_DeskPhone") != nullptr;
	success&= findByName(sceneDefinitions(projectJson), "SCN_StageTight") != nullptr;

	// The compositor keeps its identity
	const configuru::Config* deskVideoCompositor= findById(compositorDefinitions(projectJson), 20);
	success&= deskVideoCompositor && deskVideoCompositor->get_or<std::string>("component_name", "") == "COMP_DeskVideo";

	// The orphan gets an unparented scene with a whole transform
	const configuru::Config* orphanScene= findById(sceneDefinitions(projectJson), ownerSceneOf(projectJson, 26));
	success&= orphanScene != nullptr;
	if (orphanScene)
	{
		success&= orphanScene->get_or<std::string>("component_name", "") == "SCN_Orphan";
		success&= orphanScene->get_or<int>("parent_transform_id", 0) == -1;
		success&= (*orphanScene)["relative_scale"].array_size() == 3;
		success&= (float)(*orphanScene)["relative_scale"][0] == 1.f;
	}

	// Each move is reported
	success&= report.sceneSplits.size() == 4;
	if (report.sceneSplits.size() == 4)
	{
		success&= report.sceneSplits[0].compositorName == "COMP_DeskVideo";
		success&= report.sceneSplits[0].oldSceneName == "SCN_Desk";
		success&= report.sceneSplits[0].newSceneName == "SCN_DeskVideo";
		success&= report.sceneSplits[3].oldSceneName.empty();
	}
	UNIT_TEST_COMPLETE()
}

bool project_hierarchy_migration_test_empty_scene_gains_compositor()
{
	UNIT_TEST_BEGIN("a scene with no compositor gains one")
	configuru::Config projectJson= configuru::parse_string(k_legacyProject, configuru::JSON, "legacy");
	ProjectHierarchyMigration::Report report;
	success&= ProjectHierarchyMigration::migrateProjectJson(projectJson, report);

	const configuru::Config* newCompositor= findByName(compositorDefinitions(projectJson), "COMP_Empty");
	success&= newCompositor != nullptr;
	if (newCompositor)
	{
		success&= newCompositor->get_or<int>("owner_scene_id", -1) == 12;
	}
	UNIT_TEST_COMPLETE()
}

bool project_hierarchy_migration_test_camera_moves_to_scene()
{
	UNIT_TEST_BEGIN("a compositor's camera moves to the scene that owns it")
	configuru::Config projectJson= configuru::parse_string(k_legacyProject, configuru::JSON, "legacy");
	ProjectHierarchyMigration::Report report;
	success&= ProjectHierarchyMigration::migrateProjectJson(projectJson, report);

	for (const configuru::Config& compositor : compositorDefinitions(projectJson).as_array())
	{
		success&= !compositor.has_key("camera_id");
	}

	// The kept compositor's camera lands on the old scene, a moved one's on its new scene
	const configuru::Config* deskScene= findById(sceneDefinitions(projectJson), 10);
	const configuru::Config* deskVideoScene= findByName(sceneDefinitions(projectJson), "SCN_DeskVideo");
	const configuru::Config* emptyScene= findById(sceneDefinitions(projectJson), 12);
	success&= deskScene && deskScene->get_or<int>("camera_id", -1) == 31;
	success&= deskVideoScene && deskVideoScene->get_or<int>("camera_id", -1) == 30;
	// The scene that gained a compositor has no camera to take
	success&= emptyScene && !emptyScene->has_key("camera_id");

	// A project already one compositor per scene still has its cameras moved
	configuru::Config oneToOneJson= configuru::parse_string(R"({
		"SceneObjectSystem": { "SceneComponentPool": { "component_definitions": [
			{ "component_id": 10, "component_name": "SCN_A", "parent_transform_id": 5,
			  "relative_scale": [1.0, 1.0, 1.0], "relative_quaternion": [1.0, 0.0, 0.0, 0.0],
			  "relative_position": [0.0, 0.0, 0.0], "force_render": false } ] } },
		"CompositorObjectSystem": { "CompositorComponentPool": { "component_definitions": [
			{ "component_id": 20, "component_name": "COMP_A", "camera_id": 30, "owner_scene_id": 10 } ] } },
		"SpoutOutputSystem": { "SpoutOutputComponentPool": { "component_definitions": [] } }
	})",
															configuru::JSON, "oneToOne");
	ProjectHierarchyMigration::Report oneToOneReport;
	success&= ProjectHierarchyMigration::migrateProjectJson(oneToOneJson, oneToOneReport);
	success&= oneToOneReport.isEmpty();
	success&= sceneDefinitions(oneToOneJson)[0].get_or<int>("camera_id", -1) == 30;
	success&= !oneToOneJson.has_key("persistentIDAllocator");
	UNIT_TEST_COMPLETE()
}

bool project_hierarchy_migration_test_probes_move_to_scenes()
{
	UNIT_TEST_BEGIN("a lighting probe moves from its camera to a scene")
	// Camera 30 is shot by scenes 10 and 11, and 11 is current. Camera 31 is shot by
	// no scene but shares stage 5 with them. Camera 32 sits on a stage with no
	// scene. Camera 33 names a probe that is gone. Probe 44 hangs under no camera.
	configuru::Config projectJson= configuru::parse_string(R"({
		"SceneObjectSystem": { "SceneComponentPool": { "component_definitions": [
			{ "component_id": 10, "component_name": "SCN_A", "parent_transform_id": 5, "camera_id": 30,
			  "relative_scale": [1.0, 1.0, 1.0], "relative_quaternion": [1.0, 0.0, 0.0, 0.0],
			  "relative_position": [0.0, 0.0, 0.0], "force_render": false },
			{ "component_id": 11, "component_name": "SCN_B", "parent_transform_id": 5, "camera_id": 30,
			  "relative_scale": [1.0, 1.0, 1.0], "relative_quaternion": [1.0, 0.0, 0.0, 0.0],
			  "relative_position": [0.0, 0.0, 0.0], "force_render": false } ] },
			"current_scene_id": 11 },
		"CompositorObjectSystem": { "CompositorComponentPool": { "component_definitions": [
			{ "component_id": 20, "component_name": "COMP_A", "owner_scene_id": 10 },
			{ "component_id": 21, "component_name": "COMP_B", "owner_scene_id": 11 } ] } },
		"CameraObjectSystem": { "CameraComponentPool": { "component_definitions": [
			{ "component_id": 30, "component_name": "CAM_Shared", "stage_id": 5, "light_environment_id": 40 },
			{ "component_id": 31, "component_name": "CAM_Unused", "stage_id": 5, "light_environment_id": 41 },
			{ "component_id": 32, "component_name": "CAM_Elsewhere", "stage_id": 6, "light_environment_id": 42 },
			{ "component_id": 33, "component_name": "CAM_Stale", "stage_id": 5, "light_environment_id": 99 },
			{ "component_id": 34, "component_name": "CAM_None", "stage_id": 5, "light_environment_id": -1 } ] } },
		"LightEnvironmentSystem": { "LightEnvironmentComponentPool": { "component_definitions": [
			{ "component_id": 40, "component_name": "ENV_Shared", "parent_transform_id": 30 },
			{ "component_id": 41, "component_name": "ENV_Unused", "parent_transform_id": 31 },
			{ "component_id": 42, "component_name": "ENV_Elsewhere", "parent_transform_id": 32 },
			{ "component_id": 44, "component_name": "ENV_Loose", "parent_transform_id": -1 } ] } }
	})",
														   configuru::JSON, "probes");
	ProjectHierarchyMigration::Report report;
	success&= ProjectHierarchyMigration::migrateProjectJson(projectJson, report);

	const configuru::Config& scenes= sceneDefinitions(projectJson);
	const configuru::Config& probes=
		projectJson["LightEnvironmentSystem"]["LightEnvironmentComponentPool"]["component_definitions"];
	const configuru::Config* sceneA= findById(scenes, 10);
	const configuru::Config* sceneB= findById(scenes, 11);
	const configuru::Config* sharedProbe= findById(probes, 40);
	const configuru::Config* unusedProbe= findById(probes, 41);

	// The shared camera's probe goes to the current scene of the two
	success&= sceneB && sceneB->get_or<int>("light_environment_id", -1) == 40;
	success&= sharedProbe && sharedProbe->get_or<int>("parent_transform_id", -1) == 11;
	// The camera no scene uses gives its probe to the scene left on its stage
	success&= sceneA && sceneA->get_or<int>("light_environment_id", -1) == 41;
	success&= unusedProbe && unusedProbe->get_or<int>("parent_transform_id", -1) == 10;

	// The probe on a stage with no scene and the loose probe are dropped
	success&= probes.array_size() == 2;
	success&= report.droppedProbeNames.size() == 2;
	if (report.droppedProbeNames.size() == 2)
	{
		success&= report.droppedProbeNames[0] == "ENV_Elsewhere";
		success&= report.droppedProbeNames[1] == "ENV_Loose";
	}
	success&= report.sceneSplits.empty();

	for (const configuru::Config& camera :
		 projectJson["CameraObjectSystem"]["CameraComponentPool"]["component_definitions"].as_array())
	{
		success&= !camera.has_key("light_environment_id");
	}

	// A second pass moves and drops nothing
	const std::string migratedText= configuru::dump_string(projectJson, configuru::JSON);
	ProjectHierarchyMigration::Report secondReport;
	success&= !ProjectHierarchyMigration::migrateProjectJson(projectJson, secondReport);
	success&= secondReport.isEmpty();
	success&= configuru::dump_string(projectJson, configuru::JSON) == migratedText;
	UNIT_TEST_COMPLETE()
}

bool project_hierarchy_migration_test_spout_outputs()
{
	UNIT_TEST_BEGIN("compositor Spout settings become project outputs")
	// Two compositors share the default sender name, one of them enabled. A third
	// has its own name and is off. A fourth left its name empty.
	configuru::Config projectJson= configuru::parse_string(R"({
		"SceneObjectSystem": { "SceneComponentPool": { "component_definitions": [
			{ "component_id": 10, "component_name": "SCN_A", "parent_transform_id": 5 },
			{ "component_id": 11, "component_name": "SCN_B", "parent_transform_id": 5 },
			{ "component_id": 12, "component_name": "SCN_C", "parent_transform_id": 5 },
			{ "component_id": 13, "component_name": "SCN_D", "parent_transform_id": 5 } ] } },
		"CompositorObjectSystem": { "CompositorComponentPool": { "component_definitions": [
			{ "component_id": 20, "component_name": "COMP_A", "owner_scene_id": 10,
			  "spout_enable_output": false, "spout_output_name": "MikanXR", "rtp_enable_output": false },
			{ "component_id": 21, "component_name": "COMP_B", "owner_scene_id": 11,
			  "spout_enable_output": true, "spout_output_name": "MikanXR" },
			{ "component_id": 22, "component_name": "COMP_C", "owner_scene_id": 12,
			  "spout_enable_output": false, "spout_output_name": "Preview" },
			{ "component_id": 23, "component_name": "COMP_D", "owner_scene_id": 13,
			  "spout_enable_output": false, "spout_output_name": "" } ] } }
	})",
														   configuru::JSON, "spout");
	ProjectHierarchyMigration::Report report;
	success&= ProjectHierarchyMigration::migrateProjectJson(projectJson, report);

	const configuru::Config& outputs=
		projectJson["SpoutOutputSystem"]["SpoutOutputComponentPool"]["component_definitions"];
	const configuru::Config* sharedOutput= findByName(outputs, "OUT_MikanXR");
	const configuru::Config* previewOutput= findByName(outputs, "OUT_Preview");

	success&= outputs.array_size() == 2;
	success&= sharedOutput && sharedOutput->get_or<bool>("enabled", false)
			  && sharedOutput->get_or<std::string>("spout_output_name", "") == "MikanXR";
	success&= previewOutput && !previewOutput->get_or<bool>("enabled", true)
			  && previewOutput->get_or<std::string>("spout_output_name", "") == "Preview";
	success&= sharedOutput && previewOutput
			  && sharedOutput->get_or<int>("component_id", -1) != previewOutput->get_or<int>("component_id", -1);

	// The compositors keep everything but the Spout settings
	for (const configuru::Config& compositor : compositorDefinitions(projectJson).as_array())
	{
		success&= !compositor.has_key("spout_enable_output") && !compositor.has_key("spout_output_name");
	}
	const configuru::Config* compositorA= findById(compositorDefinitions(projectJson), 20);
	success&= compositorA && compositorA->has_key("rtp_enable_output");

	// A second pass adds no output, and a user who deleted every output keeps none
	ProjectHierarchyMigration::Report secondReport;
	success&= !ProjectHierarchyMigration::migrateProjectJson(projectJson, secondReport);
	configuru::Config noOutputsJson= configuru::parse_string(
		R"({ "SpoutOutputSystem": { "SpoutOutputComponentPool": { "component_definitions": [] } } })", configuru::JSON,
		"noOutputs");
	success&= !ProjectHierarchyMigration::migrateProjectJson(noOutputsJson, secondReport);
	success&= noOutputsJson["SpoutOutputSystem"]["SpoutOutputComponentPool"]["component_definitions"].array_size() == 0;

	// A project with no compositor settings to carry gets the one default output
	configuru::Config emptyJson= configuru::parse_string("{}", configuru::JSON, "empty");
	success&= ProjectHierarchyMigration::migrateProjectJson(emptyJson, secondReport);
	const configuru::Config& defaultOutputs=
		emptyJson["SpoutOutputSystem"]["SpoutOutputComponentPool"]["component_definitions"];
	success&= defaultOutputs.array_size() == 1;
	if (defaultOutputs.array_size() == 1)
	{
		success&= defaultOutputs[0].get_or<bool>("enabled", false);
		success&= defaultOutputs[0].get_or<std::string>("spout_output_name", "") == "MikanXR";
	}
	UNIT_TEST_COMPLETE()
}

bool project_hierarchy_migration_test_new_ids()
{
	UNIT_TEST_BEGIN("new ids start past the saved counter and every id in the file")
	configuru::Config projectJson= configuru::parse_string(k_legacyProject, configuru::JSON, "legacy");
	ProjectHierarchyMigration::Report report;
	success&= ProjectHierarchyMigration::migrateProjectJson(projectJson, report);

	// The file's highest id was 26 although the counter said 20. Four scenes, one
	// compositor and the default Spout output were added.
	std::map<int, int> useCountById;
	for (const configuru::Config& scene : sceneDefinitions(projectJson).as_array())
		useCountById[scene.get_or<int>("component_id", -1)]++;
	for (const configuru::Config& compositor : compositorDefinitions(projectJson).as_array())
		useCountById[compositor.get_or<int>("component_id", -1)]++;
	for (const auto& use_pair : useCountById)
	{
		success&= use_pair.second == 1;
	}
	success&= useCountById.count(27) == 1 && useCountById.count(31) == 1;
	success&= projectJson["persistentIDAllocator"].get_or<int>("next_id", -1) == 33;
	UNIT_TEST_COMPLETE()
}

bool project_hierarchy_migration_test_idempotent()
{
	UNIT_TEST_BEGIN("a migrated project is left untouched")
	configuru::Config projectJson= configuru::parse_string(k_legacyProject, configuru::JSON, "legacy");
	ProjectHierarchyMigration::Report firstReport;
	success&= ProjectHierarchyMigration::migrateProjectJson(projectJson, firstReport);

	const std::string migratedText= configuru::dump_string(projectJson, configuru::JSON);
	ProjectHierarchyMigration::Report secondReport;
	success&= !ProjectHierarchyMigration::migrateProjectJson(projectJson, secondReport);
	success&= secondReport.isEmpty();
	success&= configuru::dump_string(projectJson, configuru::JSON) == migratedText;

	// A project with no scenes and no compositors gains nothing but its output
	configuru::Config emptyJson=
		configuru::parse_string(R"({ "persistentIDAllocator": { "next_id": 1 } })", configuru::JSON, "empty");
	ProjectHierarchyMigration::Report emptyReport;
	success&= ProjectHierarchyMigration::migrateProjectJson(emptyJson, emptyReport);
	success&= !emptyJson.has_key("SceneObjectSystem");
	success&= !emptyJson.has_key("CompositorObjectSystem");
	success&= emptyReport.isEmpty();
	UNIT_TEST_COMPLETE()
}

bool run_project_hierarchy_migration_tests()
{
	UNIT_TEST_MODULE_BEGIN("project_hierarchy_migration")
	UNIT_TEST_MODULE_CALL_TEST(project_hierarchy_migration_test_one_compositor_per_scene);
	UNIT_TEST_MODULE_CALL_TEST(project_hierarchy_migration_test_kept_and_split);
	UNIT_TEST_MODULE_CALL_TEST(project_hierarchy_migration_test_empty_scene_gains_compositor);
	UNIT_TEST_MODULE_CALL_TEST(project_hierarchy_migration_test_camera_moves_to_scene);
	UNIT_TEST_MODULE_CALL_TEST(project_hierarchy_migration_test_probes_move_to_scenes);
	UNIT_TEST_MODULE_CALL_TEST(project_hierarchy_migration_test_spout_outputs);
	UNIT_TEST_MODULE_CALL_TEST(project_hierarchy_migration_test_new_ids);
	UNIT_TEST_MODULE_CALL_TEST(project_hierarchy_migration_test_idempotent);
	UNIT_TEST_MODULE_END()
}

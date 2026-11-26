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

	// The compositor keeps everything else it had
	const configuru::Config* deskVideoCompositor= findById(compositorDefinitions(projectJson), 20);
	success&= deskVideoCompositor && deskVideoCompositor->get_or<int>("camera_id", -1) == 30;

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
		success&= newCompositor->get_or<int>("camera_id", 0) == -1;
	}
	UNIT_TEST_COMPLETE()
}

bool project_hierarchy_migration_test_new_ids()
{
	UNIT_TEST_BEGIN("new ids start past the saved counter and every id in the file")
	configuru::Config projectJson= configuru::parse_string(k_legacyProject, configuru::JSON, "legacy");
	ProjectHierarchyMigration::Report report;
	success&= ProjectHierarchyMigration::migrateProjectJson(projectJson, report);

	// The file's highest id was 26 although the counter said 20. Four scenes and
	// one compositor were added.
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
	success&= projectJson["persistentIDAllocator"].get_or<int>("next_id", -1) == 32;
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

	// A project with no scenes and no compositors has nothing to migrate
	configuru::Config emptyJson=
		configuru::parse_string(R"({ "persistentIDAllocator": { "next_id": 1 } })", configuru::JSON, "empty");
	ProjectHierarchyMigration::Report emptyReport;
	success&= !ProjectHierarchyMigration::migrateProjectJson(emptyJson, emptyReport);
	success&= !emptyJson.has_key("SceneObjectSystem");
	UNIT_TEST_COMPLETE()
}

bool run_project_hierarchy_migration_tests()
{
	UNIT_TEST_MODULE_BEGIN("project_hierarchy_migration")
	UNIT_TEST_MODULE_CALL_TEST(project_hierarchy_migration_test_one_compositor_per_scene);
	UNIT_TEST_MODULE_CALL_TEST(project_hierarchy_migration_test_kept_and_split);
	UNIT_TEST_MODULE_CALL_TEST(project_hierarchy_migration_test_empty_scene_gains_compositor);
	UNIT_TEST_MODULE_CALL_TEST(project_hierarchy_migration_test_new_ids);
	UNIT_TEST_MODULE_CALL_TEST(project_hierarchy_migration_test_idempotent);
	UNIT_TEST_MODULE_END()
}

#include "ProjectHierarchyMigration.h"
#include "CompositorComponent.h"
#include "ComponentNaming.h"
#include "Logger.h"
#include "SceneComponent.h"

#include <configuru.hpp>

#include <algorithm>
#include <map>

namespace ProjectHierarchyMigration
{
namespace
{
// The keys of the project file as the migrated versions wrote them. These are
// literals rather than the definitions' property ids because a migration reads a
// format that no longer has to match the code.
const char* const k_idAllocatorKey= "persistentIDAllocator";
const char* const k_nextIdKey= "next_id";
const char* const k_sceneSystemKey= "SceneObjectSystem";
const char* const k_scenePoolKey= "SceneComponentPool";
const char* const k_compositorSystemKey= "CompositorObjectSystem";
const char* const k_compositorPoolKey= "CompositorComponentPool";
const char* const k_definitionsKey= "component_definitions";
const char* const k_componentIdKey= "component_id";
const char* const k_componentNameKey= "component_name";
const char* const k_parentTransformIdKey= "parent_transform_id";
const char* const k_relativeScaleKey= "relative_scale";
const char* const k_relativeQuaternionKey= "relative_quaternion";
const char* const k_relativePositionKey= "relative_position";
const char* const k_displayCompositorIdKey= "display_compositor_id";
const char* const k_ownerSceneIdKey= "owner_scene_id";
const char* const k_cameraIdKey= "camera_id";

const int k_invalidId= -1;

// The highest component id anywhere in the file, so a new id never lands on one
// the allocator's saved counter fell behind
void findMaxComponentId(const configuru::Config& config, int& inOutMaxId)
{
	if (config.is_object())
	{
		for (const auto& pair : config.as_object())
		{
			if (pair.key() == k_componentIdKey && pair.value().is_int())
			{
				inOutMaxId= std::max(inOutMaxId, (int)pair.value());
			}
			else
			{
				findMaxComponentId(pair.value(), inOutMaxId);
			}
		}
	}
	else if (config.is_array())
	{
		for (const configuru::Config& element : config.as_array())
		{
			findMaxComponentId(element, inOutMaxId);
		}
	}
}

// The definition array of a system's pool, created empty when the file lacks it
configuru::Config& ensureDefinitions(configuru::Config& projectJson, const char* systemKey, const char* poolKey)
{
	if (!projectJson.has_key(systemKey))
		projectJson[systemKey]= configuru::Config::object();
	configuru::Config& system= projectJson[systemKey];

	if (!system.has_key(poolKey))
		system[poolKey]= configuru::Config::object();
	configuru::Config& pool= system[poolKey];

	if (!pool.has_key(k_definitionsKey))
		pool[k_definitionsKey]= configuru::Config::array();

	return pool[k_definitionsKey];
}

bool hasDefinitions(const configuru::Config& projectJson, const char* systemKey, const char* poolKey)
{
	return projectJson.has_key(systemKey) && projectJson[systemKey].has_key(poolKey)
		   && projectJson[systemKey][poolKey].has_key(k_definitionsKey)
		   && projectJson[systemKey][poolKey][k_definitionsKey].array_size() > 0;
}

// The part of a component name after its prefix: "DeskMovie" for "COMP_DeskMovie"
std::string nameStem(const std::string& componentName)
{
	const size_t separator= componentName.find('_');

	return separator == std::string::npos ? componentName : componentName.substr(separator + 1);
}

// The prefix of a component name, or the class default when it carries none
std::string namePrefix(const std::string& componentName, const std::string& componentClassName)
{
	const size_t separator= componentName.find('_');

	return separator == std::string::npos ? getDefaultComponentNamePrefix(componentClassName)
										  : componentName.substr(0, separator);
}

configuru::Config makeScene(int sceneId, const std::string& sceneName, const configuru::Config* templateScene)
{
	configuru::Config scene= configuru::Config::object();

	scene[k_componentIdKey]= sceneId;
	scene[k_componentNameKey]= sceneName;

	// The transform keys are written out in full: a missing vector reads as zeroes
	if (templateScene && templateScene->has_key(k_relativeScaleKey) && templateScene->has_key(k_relativeQuaternionKey)
		&& templateScene->has_key(k_relativePositionKey))
	{
		scene[k_parentTransformIdKey]= templateScene->get_or<int>(k_parentTransformIdKey, k_invalidId);
		scene[k_relativeScaleKey]= (*templateScene)[k_relativeScaleKey].deep_clone();
		scene[k_relativeQuaternionKey]= (*templateScene)[k_relativeQuaternionKey].deep_clone();
		scene[k_relativePositionKey]= (*templateScene)[k_relativePositionKey].deep_clone();
	}
	else
	{
		scene[k_parentTransformIdKey]= k_invalidId;
		scene[k_relativeScaleKey]= configuru::Config::array({1.0, 1.0, 1.0});
		scene[k_relativeQuaternionKey]= configuru::Config::array({1.0, 0.0, 0.0, 0.0});
		scene[k_relativePositionKey]= configuru::Config::array({0.0, 0.0, 0.0});
	}

	return scene;
}
} // namespace

bool migrateProjectJson(configuru::Config& projectJson, Report& outReport)
{
	if (!projectJson.is_object())
		return false;

	const bool bHasScenes= hasDefinitions(projectJson, k_sceneSystemKey, k_scenePoolKey);
	const bool bHasCompositors= hasDefinitions(projectJson, k_compositorSystemKey, k_compositorPoolKey);
	if (!bHasScenes && !bHasCompositors)
		return false;

	bool bChanged= false;

	int maxComponentId= k_invalidId;
	findMaxComponentId(projectJson, maxComponentId);
	const int savedNextId= projectJson.has_key(k_idAllocatorKey)
							   ? projectJson[k_idAllocatorKey].get_or<int>(k_nextIdKey, k_invalidId)
							   : k_invalidId;
	const int firstNewId= std::max(savedNextId, maxComponentId + 1);
	int nextId= firstNewId;

	configuru::Config& scenes= ensureDefinitions(projectJson, k_sceneSystemKey, k_scenePoolKey);
	configuru::Config& compositors= ensureDefinitions(projectJson, k_compositorSystemKey, k_compositorPoolKey);

	// Which compositor each scene keeps: the one it displayed, else its first
	std::map<int, int> keptCompositorIdBySceneId;
	for (const configuru::Config& scene : scenes.as_array())
	{
		const int sceneId= scene.get_or<int>(k_componentIdKey, k_invalidId);
		const int displayCompositorId= scene.get_or<int>(k_displayCompositorIdKey, k_invalidId);
		int firstCompositorId= k_invalidId;
		int keptCompositorId= k_invalidId;

		for (const configuru::Config& compositor : compositors.as_array())
		{
			if (compositor.get_or<int>(k_ownerSceneIdKey, k_invalidId) != sceneId)
				continue;

			const int compositorId= compositor.get_or<int>(k_componentIdKey, k_invalidId);
			if (firstCompositorId == k_invalidId)
				firstCompositorId= compositorId;
			if (compositorId == displayCompositorId)
				keptCompositorId= compositorId;
		}

		keptCompositorIdBySceneId[sceneId]= keptCompositorId != k_invalidId ? keptCompositorId : firstCompositorId;
	}

	// Every compositor a scene does not keep, and every compositor whose scene is
	// missing, moves to a new scene of its own
	std::vector<configuru::Config> newScenes;
	for (configuru::Config& compositor : compositors.as_array())
	{
		const int compositorId= compositor.get_or<int>(k_componentIdKey, k_invalidId);
		const int ownerSceneId= compositor.get_or<int>(k_ownerSceneIdKey, k_invalidId);

		auto kept_it= keptCompositorIdBySceneId.find(ownerSceneId);
		if (kept_it != keptCompositorIdBySceneId.end() && kept_it->second == compositorId)
			continue;

		const configuru::Config* ownerScene= nullptr;
		for (const configuru::Config& scene : scenes.as_array())
		{
			if (scene.get_or<int>(k_componentIdKey, k_invalidId) == ownerSceneId)
				ownerScene= &scene;
		}

		const std::string compositorName= compositor.get_or<std::string>(k_componentNameKey, "");
		const std::string ownerSceneName= ownerScene ? ownerScene->get_or<std::string>(k_componentNameKey, "") : "";
		const int newSceneId= nextId++;
		const std::string newSceneName=
			namePrefix(ownerSceneName, SceneComponent::k_componentClassName) + "_" + nameStem(compositorName);

		newScenes.push_back(makeScene(newSceneId, newSceneName, ownerScene));
		compositor[k_ownerSceneIdKey]= newSceneId;

		SceneSplit split;
		split.compositorName= compositorName;
		split.oldSceneName= ownerSceneName;
		split.newSceneName= newSceneName;
		outReport.sceneSplits.push_back(split);

		MIKAN_LOG_INFO("ProjectHierarchyMigration") << "Moved compositor '" << compositorName << "' from scene '"
													<< ownerSceneName << "' to new scene '" << newSceneName << "'";
		bChanged= true;
	}

	// A scene with no compositor gains one
	std::vector<configuru::Config> newCompositors;
	for (const auto& kept_pair : keptCompositorIdBySceneId)
	{
		if (kept_pair.second != k_invalidId)
			continue;

		std::string sceneName;
		for (const configuru::Config& scene : scenes.as_array())
		{
			if (scene.get_or<int>(k_componentIdKey, k_invalidId) == kept_pair.first)
				sceneName= scene.get_or<std::string>(k_componentNameKey, "");
		}

		configuru::Config compositor= configuru::Config::object();
		compositor[k_componentIdKey]= nextId++;
		compositor[k_componentNameKey]=
			getDefaultComponentNamePrefix(CompositorComponent::k_componentClassName) + "_" + nameStem(sceneName);
		compositor[k_ownerSceneIdKey]= kept_pair.first;
		newCompositors.push_back(compositor);

		MIKAN_LOG_INFO("ProjectHierarchyMigration") << "Added a compositor to scene '" << sceneName << "'";
		bChanged= true;
	}

	for (configuru::Config& scene : newScenes)
		scenes.push_back(scene);
	for (configuru::Config& compositor : newCompositors)
		compositors.push_back(compositor);

	// With one compositor per scene there is no display choice left to store
	for (configuru::Config& scene : scenes.as_array())
	{
		bChanged|= scene.erase(k_displayCompositorIdKey);
	}

	// The camera moves from each compositor to the scene that owns it, which the
	// steps above have left with exactly that one compositor
	for (configuru::Config& compositor : compositors.as_array())
	{
		if (!compositor.has_key(k_cameraIdKey))
			continue;

		const int ownerSceneId= compositor.get_or<int>(k_ownerSceneIdKey, k_invalidId);
		for (configuru::Config& scene : scenes.as_array())
		{
			if (scene.get_or<int>(k_componentIdKey, k_invalidId) == ownerSceneId && !scene.has_key(k_cameraIdKey))
			{
				scene[k_cameraIdKey]= compositor.get_or<int>(k_cameraIdKey, k_invalidId);
			}
		}

		compositor.erase(k_cameraIdKey);
		bChanged= true;
	}

	// The saved counter moves only when an id was handed out
	if (nextId != firstNewId)
	{
		if (!projectJson.has_key(k_idAllocatorKey))
			projectJson[k_idAllocatorKey]= configuru::Config::object();
		projectJson[k_idAllocatorKey][k_nextIdKey]= nextId;
	}

	return bChanged;
}

bool migrateProject(const std::filesystem::path& projectFilePath, Report& outReport)
{
	std::error_code ec;
	if (!std::filesystem::is_regular_file(projectFilePath, ec))
		return false;

	try
	{
		configuru::Config projectJson= configuru::parse_file(projectFilePath.string(), configuru::JSON);
		if (!migrateProjectJson(projectJson, outReport))
			return false;

		configuru::dump_file(projectFilePath.string(), projectJson, configuru::JSON);
		MIKAN_LOG_INFO("ProjectHierarchyMigration") << "Rewrote the scene hierarchy in " << projectFilePath.string();
		return true;
	}
	catch (const std::exception& e)
	{
		MIKAN_LOG_ERROR("ProjectHierarchyMigration")
			<< "Failed to migrate the scene hierarchy in " << projectFilePath.string() << ": " << e.what();
		return false;
	}
}
} // namespace ProjectHierarchyMigration

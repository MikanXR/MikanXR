#include "ProjectHierarchyMigration.h"
#include "CompositorComponent.h"
#include "ComponentNaming.h"
#include "Logger.h"
#include "ProjectConfigConstants.h"
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
const char* const k_currentSceneIdKey= "current_scene_id";
const char* const k_cameraSystemKey= "CameraObjectSystem";
const char* const k_cameraPoolKey= "CameraComponentPool";
const char* const k_cameraStageIdKey= "stage_id";
const char* const k_lightEnvironmentSystemKey= "LightEnvironmentSystem";
const char* const k_lightEnvironmentPoolKey= "LightEnvironmentComponentPool";
const char* const k_lightEnvironmentIdKey= "light_environment_id";
const char* const k_spoutOutputSystemKey= "SpoutOutputSystem";
const char* const k_spoutOutputPoolKey= "SpoutOutputComponentPool";
const char* const k_spoutEnableOutputKey= "spout_enable_output";
const char* const k_spoutOutputNameKey= "spout_output_name";
const char* const k_enabledKey= "enabled";
const char* const k_rtpOutputSystemKey= "RTPOutputSystem";
const char* const k_rtpOutputPoolKey= "RTPOutputComponentPool";
const char* const k_rtpEnableOutputKey= "rtp_enable_output";
const char* const k_rtpOutputHostKey= "rtp_output_host";
const char* const k_rtpOutputPortKey= "rtp_output_port";
const char* const k_rtpOutputHeightKey= "rtp_output_height";
const char* const k_rtpOutputBitrateKbpsKey= "rtp_output_bitrate_kbps";

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

configuru::Config* findDefinitionById(configuru::Config& definitions, int componentId)
{
	for (configuru::Config& definition : definitions.as_array())
	{
		if (definition.get_or<int>(k_componentIdKey, k_invalidId) == componentId)
			return &definition;
	}

	return nullptr;
}

// The scene a camera's probe goes to: a probe-less scene shot through the camera,
// the current scene first, else a probe-less scene on the camera's stage
configuru::Config* findSceneForCameraProbe(configuru::Config& scenes, int cameraId, int cameraStageId,
										   int currentSceneId)
{
	configuru::Config* firstUser= nullptr;
	configuru::Config* firstOnStage= nullptr;
	for (configuru::Config& scene : scenes.as_array())
	{
		if (scene.get_or<int>(k_lightEnvironmentIdKey, k_invalidId) != k_invalidId)
			continue;

		if (scene.get_or<int>(k_cameraIdKey, k_invalidId) == cameraId)
		{
			if (scene.get_or<int>(k_componentIdKey, k_invalidId) == currentSceneId)
				return &scene;
			if (!firstUser)
				firstUser= &scene;
		}
		else if (!firstOnStage && cameraStageId != k_invalidId
				 && scene.get_or<int>(k_parentTransformIdKey, k_invalidId) == cameraStageId)
		{
			firstOnStage= &scene;
		}
	}

	return firstUser ? firstUser : firstOnStage;
}

// Moves each camera's lighting probe to a scene. Returns true when anything changed.
bool migrateLightingProbes(configuru::Config& projectJson, configuru::Config& scenes, Report& outReport)
{
	if (!hasDefinitions(projectJson, k_cameraSystemKey, k_cameraPoolKey))
		return false;

	bool bChanged= false;
	const int currentSceneId= projectJson[k_sceneSystemKey].get_or<int>(k_currentSceneIdKey, k_invalidId);
	configuru::Config& cameras= projectJson[k_cameraSystemKey][k_cameraPoolKey][k_definitionsKey];
	configuru::Config* probes=
		hasDefinitions(projectJson, k_lightEnvironmentSystemKey, k_lightEnvironmentPoolKey)
			? &projectJson[k_lightEnvironmentSystemKey][k_lightEnvironmentPoolKey][k_definitionsKey]
			: nullptr;

	for (configuru::Config& camera : cameras.as_array())
	{
		if (!camera.has_key(k_lightEnvironmentIdKey))
			continue;

		const int probeId= camera.get_or<int>(k_lightEnvironmentIdKey, k_invalidId);
		camera.erase(k_lightEnvironmentIdKey);
		bChanged= true;

		configuru::Config* probe= probes ? findDefinitionById(*probes, probeId) : nullptr;
		if (!probe)
			continue;

		configuru::Config* scene=
			findSceneForCameraProbe(scenes, camera.get_or<int>(k_componentIdKey, k_invalidId),
									camera.get_or<int>(k_cameraStageIdKey, k_invalidId), currentSceneId);
		if (scene)
		{
			const int sceneId= scene->get_or<int>(k_componentIdKey, k_invalidId);

			(*scene)[k_lightEnvironmentIdKey]= probeId;
			(*probe)[k_parentTransformIdKey]= sceneId;

			MIKAN_LOG_INFO("ProjectHierarchyMigration")
				<< "Moved lighting probe '" << probe->get_or<std::string>(k_componentNameKey, "") << "' to scene '"
				<< scene->get_or<std::string>(k_componentNameKey, "") << "'";
		}
	}

	// A probe no scene names has nothing to belong to
	if (probes)
	{
		auto& probeArray= probes->as_array();
		for (auto probe_it= probeArray.begin(); probe_it != probeArray.end();)
		{
			const int probeId= probe_it->get_or<int>(k_componentIdKey, k_invalidId);
			bool bOwned= false;
			for (const configuru::Config& scene : scenes.as_array())
			{
				bOwned|= scene.get_or<int>(k_lightEnvironmentIdKey, k_invalidId) == probeId
						 && scene.get_or<int>(k_componentIdKey, k_invalidId)
								== probe_it->get_or<int>(k_parentTransformIdKey, k_invalidId);
			}

			if (bOwned)
			{
				++probe_it;
				continue;
			}

			const std::string probeName= probe_it->get_or<std::string>(k_componentNameKey, "");
			outReport.droppedProbeNames.push_back(probeName);
			MIKAN_LOG_INFO("ProjectHierarchyMigration")
				<< "Dropped lighting probe '" << probeName << "', which no scene could take";
			probe_it= probeArray.erase(probe_it);
			bChanged= true;
		}
	}

	return bChanged;
}
// Moves the Spout output settings each compositor carried onto project outputs,
// one per sender name, and gives a project that has no output system yet its
// default output. Returns true when anything changed.
bool migrateSpoutOutputs(configuru::Config& projectJson, int& inOutNextId)
{
	struct SpoutSettings
	{
		std::string name;
		bool bEnabled;
	};
	std::vector<SpoutSettings> foundSettings;
	bool bChanged= false;

	if (hasDefinitions(projectJson, k_compositorSystemKey, k_compositorPoolKey))
	{
		for (configuru::Config& compositor :
			 projectJson[k_compositorSystemKey][k_compositorPoolKey][k_definitionsKey].as_array())
		{
			if (!compositor.has_key(k_spoutOutputNameKey) && !compositor.has_key(k_spoutEnableOutputKey))
				continue;

			std::string name= compositor.get_or<std::string>(k_spoutOutputNameKey, "");
			if (name.empty())
				name= DEFAULT_SPOUT_OUTPUT_NAME;
			const bool bEnabled= compositor.get_or<bool>(k_spoutEnableOutputKey, false);

			// Compositors that shared a sender name share one output, enabled when
			// any of them published under it
			auto found_it= std::find_if(foundSettings.begin(), foundSettings.end(),
										[&name](const SpoutSettings& settings) { return settings.name == name; });
			if (found_it != foundSettings.end())
				found_it->bEnabled|= bEnabled;
			else
				foundSettings.push_back({name, bEnabled});

			compositor.erase(k_spoutOutputNameKey);
			compositor.erase(k_spoutEnableOutputKey);
			bChanged= true;
		}
	}

	// A project that already has the output system keeps the outputs it has
	if (projectJson.has_key(k_spoutOutputSystemKey))
		return bChanged;

	if (foundSettings.empty())
		foundSettings.push_back({DEFAULT_SPOUT_OUTPUT_NAME, true});

	configuru::Config& outputs= ensureDefinitions(projectJson, k_spoutOutputSystemKey, k_spoutOutputPoolKey);
	for (const SpoutSettings& settings : foundSettings)
	{
		configuru::Config output= configuru::Config::object();
		output[k_componentIdKey]= inOutNextId++;
		output[k_componentNameKey]= "OUT_" + settings.name;
		output[k_enabledKey]= settings.bEnabled;
		output[k_spoutOutputNameKey]= settings.name;
		outputs.push_back(output);

		MIKAN_LOG_INFO("ProjectHierarchyMigration") << "Added Spout output '" << settings.name << "'";
	}

	return true;
}

// Moves the RTP output settings each compositor carried onto project outputs, one
// per destination. A compositor with no host named no destination and leaves
// nothing behind. Returns true when anything changed.
bool migrateRtpOutputs(configuru::Config& projectJson, int& inOutNextId)
{
	struct RtpSettings
	{
		std::string host;
		int port;
		int height;
		int bitrateKbps;
		bool bEnabled;
	};
	std::vector<RtpSettings> foundSettings;
	bool bChanged= false;

	if (hasDefinitions(projectJson, k_compositorSystemKey, k_compositorPoolKey))
	{
		for (configuru::Config& compositor :
			 projectJson[k_compositorSystemKey][k_compositorPoolKey][k_definitionsKey].as_array())
		{
			bool bHadSettings= false;
			for (const char* key : {k_rtpEnableOutputKey, k_rtpOutputHostKey, k_rtpOutputPortKey, k_rtpOutputHeightKey,
									k_rtpOutputBitrateKbpsKey})
			{
				bHadSettings|= compositor.has_key(key);
			}
			if (!bHadSettings)
				continue;

			RtpSettings settings;
			settings.host= compositor.get_or<std::string>(k_rtpOutputHostKey, "");
			settings.port= compositor.get_or<int>(k_rtpOutputPortKey, DEFAULT_RTP_OUTPUT_PORT);
			settings.height= compositor.get_or<int>(k_rtpOutputHeightKey, DEFAULT_RTP_OUTPUT_HEIGHT);
			settings.bitrateKbps= compositor.get_or<int>(k_rtpOutputBitrateKbpsKey, DEFAULT_RTP_OUTPUT_BITRATE_KBPS);
			settings.bEnabled= compositor.get_or<bool>(k_rtpEnableOutputKey, false);

			if (!settings.host.empty())
			{
				// Compositors that sent to one destination share one output, enabled
				// when any of them streamed to it, with the first one's picture settings
				auto found_it=
					std::find_if(foundSettings.begin(), foundSettings.end(), [&settings](const RtpSettings& found)
								 { return found.host == settings.host && found.port == settings.port; });
				if (found_it != foundSettings.end())
					found_it->bEnabled|= settings.bEnabled;
				else
					foundSettings.push_back(settings);
			}

			for (const char* key : {k_rtpEnableOutputKey, k_rtpOutputHostKey, k_rtpOutputPortKey, k_rtpOutputHeightKey,
									k_rtpOutputBitrateKbpsKey})
			{
				compositor.erase(key);
			}
			bChanged= true;
		}
	}

	// A project that already has the output system keeps the outputs it has
	if (foundSettings.empty() || projectJson.has_key(k_rtpOutputSystemKey))
		return bChanged;

	configuru::Config& outputs= ensureDefinitions(projectJson, k_rtpOutputSystemKey, k_rtpOutputPoolKey);
	for (const RtpSettings& settings : foundSettings)
	{
		configuru::Config output= configuru::Config::object();
		output[k_componentIdKey]= inOutNextId++;
		output[k_componentNameKey]= "OUT_" + settings.host;
		output[k_enabledKey]= settings.bEnabled;
		output[k_rtpOutputHostKey]= settings.host;
		output[k_rtpOutputPortKey]= settings.port;
		output[k_rtpOutputHeightKey]= settings.height;
		output[k_rtpOutputBitrateKbpsKey]= settings.bitrateKbps;
		outputs.push_back(output);

		MIKAN_LOG_INFO("ProjectHierarchyMigration") << "Added RTP output to " << settings.host << ":" << settings.port;
	}

	return true;
}

// One compositor per scene, the camera and the lighting probe on the scene.
// Returns true when anything changed.
bool migrateSceneHierarchy(configuru::Config& projectJson, int& nextId, Report& outReport)
{
	bool bChanged= false;

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

	// The lighting probes follow, once every scene knows its camera
	bChanged|= migrateLightingProbes(projectJson, scenes, outReport);

	return bChanged;
}
} // namespace

bool migrateProjectJson(configuru::Config& projectJson, Report& outReport)
{
	if (!projectJson.is_object())
		return false;

	int maxComponentId= k_invalidId;
	findMaxComponentId(projectJson, maxComponentId);
	const int savedNextId= projectJson.has_key(k_idAllocatorKey)
							   ? projectJson[k_idAllocatorKey].get_or<int>(k_nextIdKey, k_invalidId)
							   : k_invalidId;
	const int firstNewId= std::max(savedNextId, maxComponentId + 1);
	int nextId= firstNewId;

	bool bChanged= false;
	if (hasDefinitions(projectJson, k_sceneSystemKey, k_scenePoolKey)
		|| hasDefinitions(projectJson, k_compositorSystemKey, k_compositorPoolKey))
	{
		bChanged|= migrateSceneHierarchy(projectJson, nextId, outReport);
	}
	bChanged|= migrateSpoutOutputs(projectJson, nextId);
	bChanged|= migrateRtpOutputs(projectJson, nextId);

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

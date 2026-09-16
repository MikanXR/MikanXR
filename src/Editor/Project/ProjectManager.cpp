#pragma once

#include "App.h"
#include "AppSettingsConfig.h"
#include "AnchorObjectSystem.h"
#include "ARKitVideoSourceSystem.h"
#include "FileVideoSourceSystem.h"
#include "ClientTextureSourceSystem.h"
#include "CameraObjectSystem.h"
#include "CompositorObjectSystem.h"
#include "DMXObjectSystem.h"
#include "EditorObjectSystem.h"
#include "PropertyInterface.h"
#include "MainWindow.h"
#include "MarkerObjectSystem.h"
#include "MikanPropertyDatabase.h"
#include "MikanFunctionDatabase.h"
#include "NetworkVideoSourceSystem.h"
#include "PathUtils.h"
#include "ProjectConfig.h"
#include "ProjectManager.h"
#include "ProjectAssetCatalog.h"
#include "LegacyContentMigration.h"
#include "SceneObjectSystem.h"
#include "ScriptObjectSystem.h"
#include "SpoutTextureSourceSystem.h"
#include "CEFTextureSourceSystem.h"
#include "BoxShapeSystem.h"
#include "BoxStencilSystem.h"
#include "ModelShapeSystem.h"
#include "ModelStencilSystem.h"
#include "QuadShapeSystem.h"
#include "QuadStencilSystem.h"
#include "RGBSpotLightSystem.h"
#include "RGBPixelGridSystem.h"
#include "DMXFixtureGroupSystem.h"
#include "DMXPresetSystem.h"
#include "DMXSequenceSystem.h"
#include "LightEnvironmentSystem.h"
#include "StageObjectSystem.h"
#include "TrackingMountObjectSystem.h"
#include "MarkerTrackingVolumeSystem.h"
#include "VRTrackingVolumeSystem.h"
#include "USBVideoSourceSystem.h"
#include "VRObjectSystem.h"

#include "Logger.h"

#include <chrono>
#include <fstream>
#include <easy/profiler.h>

#define PROJECT_SAVE_COOLDOWN 3.f

const char* ProjectManager::k_mikanProjectFileExtension= ".mikanproj";

ProjectManager::ProjectManager(IEditorWindow* ownerWindow)
	: m_ownerWindow(ownerWindow)
	, m_propertyDatabase(std::make_shared<MikanPropertyDatabase>())
	, m_functionDatabase(std::make_shared<MikanFunctionDatabase>())
{
}

bool ProjectManager::startup(MainWindow* mainWindow)
{
	// Allocate all systems, in the order we want to perform init and updates
	// Init EditorSystem first so that it get component creation events
	// from Anchor and Stencil Systems triggered during init call
	addSystem<EditorObjectSystem>();
	addSystem<ClientTextureSourceSystem>();
	addSystem<SpoutTextureSourceSystem>();
	addSystem<CEFTextureSourceSystem>();
	addSystem<NetworkVideoSourceSystem>();
	addSystem<USBVideoSourceSystem>();
	addSystem<ARKitVideoSourceSystem>();
	addSystem<FileVideoSourceSystem>();
	addSystem<MarkerObjectSystem>();
	addSystem<StageObjectSystem>();
	addSystem<SceneObjectSystem>();
	addSystem<CompositorObjectSystem>();
	addSystem<CameraObjectSystem>();
	addSystem<AnchorObjectSystem>();
	addSystem<QuadShapeSystem>();
	addSystem<BoxShapeSystem>();
	addSystem<ModelShapeSystem>();
	addSystem<QuadStencilSystem>();
	addSystem<BoxStencilSystem>();
	addSystem<ModelStencilSystem>();
	addSystem<VRObjectSystem>();
	addSystem<TrackingMountObjectSystem>();
	addSystem<MarkerTrackingVolumeSystem>();
	addSystem<VRTrackingVolumeSystem>();
	// Before the DMX system: a sequence frame's fixture writes flush the same frame
	addSystem<DMXSequenceSystem>();
	addSystem<DMXObjectSystem>();
	addSystem<RGBSpotLightSystem>();
	addSystem<RGBPixelGridSystem>();
	// After the fixture systems: groups prune ids of fixtures those systems destroy
	addSystem<DMXFixtureGroupSystem>();
	addSystem<DMXPresetSystem>();
	addSystem<LightEnvironmentSystem>();
	// Last: scripts look up the other systems' objects by name when they load
	addSystem<ScriptObjectSystem>();

	// Gather all property descriptors from all the systems and add them to the database
	for (int i= 0; i < (int)m_systems.size(); i++)
	{
		m_systems[i]->registerPropertyDescriptors(m_propertyDatabase);
	}

	// Gather all function descriptors from all the systems and add them to the database
	for (int i= 0; i < (int)m_systems.size(); i++)
	{
		m_systems[i]->registerFunctionDescriptors(m_functionDatabase);
	}

	// Create a default project if we can't load the last opened one
	AppSettingsConfigPtr appSettings= mainWindow->getOwnerApp()->getAppSettings();
	if (!loadProject(appSettings->getLastProjectPath().string()))
	{
		const std::string defaultProjectFolderName= PathUtils::makeTimestampedFileName("Default", "");
		const std::filesystem::path defaultProjectFilename= defaultProjectFolderName + k_mikanProjectFileExtension;
		const std::filesystem::path defaultProjectPath=
			getDefaultProjectFolder() / defaultProjectFolderName / defaultProjectFilename;

		if (newProject(defaultProjectPath.string()))
		{
			appSettings->setLastProjectPath(defaultProjectPath);
		}
		else
		{
			return false;
		}
	}

	return true;
}

ProjectConfigPtr ProjectManager::createEmptyProjectConfig() const
{
	return std::make_shared<ProjectConfig>("ProfileConfig");
}

void ProjectManager::registerSystem(MikanObjectSystemPtr system)
{
	const int systemIndex= (int)m_systems.size();

	m_systemNameToIndexMap.insert({system->getObjectSystemClassName(), systemIndex});
	m_systems.push_back(system);
}

MikanObjectSystemPtr ProjectManager::getSystemByName(const std::string name) const
{
	auto it= m_systemNameToIndexMap.find(name);
	if (it != m_systemNameToIndexMap.end())
	{
		const int systemIndex= it->second;
		assert(systemIndex >= 0 && systemIndex < (int)m_systems.size());

		return m_systems[systemIndex];
	}

	return MikanObjectSystemPtr();
}

MikanComponentPtr ProjectManager::getComponentById(MikanComponentID componentId) const
{
	for (MikanObjectSystemPtr system : m_systems)
	{
		MikanComponentPtr componentPtr= system->getComponentById(componentId);
		if (componentPtr)
		{
			return componentPtr;
		}
	}
	return MikanComponentPtr();
}

void ProjectManager::shutdown()
{
	unloadProject();
	m_systems.clear();
	m_propertyDatabase->clear();
	m_functionDatabase->clear();
}

void ProjectManager::update(float deltaSeconds)
{
	for (MikanObjectSystemPtr system : m_systems)
	{
		system->update(deltaSeconds);
	}

	if (m_projectConfig)
	{
		m_projectConfig->updateAutoSave(deltaSeconds);
	}
}

std::filesystem::path ProjectManager::getDefaultProjectFolder() { return PathUtils::getProjectsRootDirectory(); }

bool ProjectManager::hasLoadedProject() const { return m_projectConfig != nullptr; }

bool ProjectManager::isAnySystemLoading() const
{
	for (const MikanObjectSystemPtr& system : m_systems)
	{
		if (system->isLoading())
			return true;
	}
	return false;
}

bool ProjectManager::newProject(const std::string& projectFilePath)
{
	// Determine the project directory (parent of the .mikanproj file)
	std::filesystem::path projectDir= std::filesystem::path(projectFilePath).parent_path();

	// Create the project directory if it doesn't exist
	if (!std::filesystem::exists(projectDir))
	{
		std::filesystem::create_directories(projectDir);
	}

	auto newProjectConfig= createEmptyProjectConfig();
	newProjectConfig->save(projectFilePath);

	return loadProject(projectFilePath);
}

// Every asset folder the catalog knows exists in a loaded project, empty when the
// project has nothing of that kind: the bundled resources show through behind it
// as read-only entries, and the Add button's file dialog opens in it, which a
// missing folder silently prevents. A project saved before a folder kind existed
// gains it here.
static void ensureAssetFolders(const std::filesystem::path& projectDir)
{
	for (const ProjectAssetFolderDesc& folderDesc : ProjectAssetCatalog::getFolderDescs())
	{
		if (folderDesc.projectSubfolder.empty())
			continue;

		const std::filesystem::path folderPath= projectDir / folderDesc.projectSubfolder;
		if (std::filesystem::is_directory(folderPath))
			continue;

		std::error_code errorCode;
		if (std::filesystem::create_directories(folderPath, errorCode))
		{
			MIKAN_LOG_INFO("ProjectManager::loadProject") << "Created missing asset folder " << folderPath;
		}
		else
		{
			MIKAN_LOG_WARNING("ProjectManager::loadProject")
				<< "Failed to create asset folder " << folderPath << ": " << errorCode.message();
		}
	}
}

// Writes a file once; a project that already has its own copy keeps it
static void writeProjectFileIfMissing(const std::filesystem::path& filePath, const std::string& content)
{
	if (std::filesystem::exists(filePath))
		return;

	std::filesystem::create_directories(filePath.parent_path());

	std::ofstream file(filePath);
	if (!file.is_open())
	{
		MIKAN_LOG_WARNING("ProjectManager::loadProject") << "Failed to write " << filePath;
		return;
	}

	file << content;
}

// The project folder doubles as the VS Code workspace for its scripts: a
// .luarc.json points the Lua language server at the editor's API stubs, and a
// launch config attaches the vscode-lrdb debugger to the editor with the
// workspace as its source root, matching the chunk names the script context
// assigns (paths relative to the project folder).
static void writeScriptWorkspaceFiles(const std::filesystem::path& projectDir)
{
	// The bundled scripts sit on the module search path behind the project's own,
	// so the language server sees them too
	const std::filesystem::path definitionsDir=
		std::filesystem::absolute(PathUtils::getResourceDirectory() / "lua-definitions");
	const std::filesystem::path bundledScriptsDir=
		std::filesystem::absolute(PathUtils::getResourceDirectory() / "scripts");
	const std::string luarcContent= "{\n"
									"\t\"runtime.version\": \"Lua 5.4\",\n"
									"\t\"workspace.library\": [\""
									+ definitionsDir.generic_string() + "\", \"" + bundledScriptsDir.generic_string()
									+ "\"],\n"
									  "\t\"workspace.checkThirdParty\": false\n"
									  "}\n";
	writeProjectFileIfMissing(projectDir / ".luarc.json", luarcContent);

	writeProjectFileIfMissing(projectDir / ".vscode" / "launch.json",
							  "{\n"
							  "\t\"version\": \"0.2.0\",\n"
							  "\t\"configurations\": [\n"
							  "\t\t{\n"
							  "\t\t\t\"type\": \"lrdb\",\n"
							  "\t\t\t\"request\": \"attach\",\n"
							  "\t\t\t\"name\": \"Attach to MikanXR Lua (port 21110)\",\n"
							  "\t\t\t\"host\": \"localhost\",\n"
							  "\t\t\t\"port\": 21110,\n"
							  "\t\t\t\"sourceRoot\": \"${workspaceFolder}\"\n"
							  "\t\t}\n"
							  "\t]\n"
							  "}\n");
}

bool ProjectManager::loadProject(const std::string& projectFilePath)
{
	EASY_FUNCTION();

	// Early out if the project file path is invalid
	if (projectFilePath.empty() || !std::filesystem::exists(projectFilePath))
	{
		return false;
	}

	// Early out if the requested project path isn't new
	if (hasLoadedProject() && m_projectConfig->getLoadedConfigPath() == projectFilePath)
	{
		return true;
	}

	// Unload the existing project first
	unloadProject();

	// Set the project directory so PathUtils can resolve project-relative resources
	const std::filesystem::path projectDir= std::filesystem::path(projectFilePath).parent_path();
	PathUtils::setProjectDirectory(projectDir);
	writeScriptWorkspaceFiles(projectDir);

	// A project saved before graph and material kinds had their own folders and
	// extensions is moved onto the current layout before its file is read
	LegacyContentMigration::migrateProject(projectDir, projectFilePath);
	ensureAssetFolders(projectDir);

	// create an empty project config
	m_projectConfig= createEmptyProjectConfig();

	// Attempt to load and init the new project
	auto loadStart= std::chrono::high_resolution_clock::now();
	bool bSuccess= m_projectConfig->load(projectFilePath);
	auto loadEnd= std::chrono::high_resolution_clock::now();
	MIKAN_LOG_INFO("ProjectManager::loadProject")
		<< "Config load: " << std::chrono::duration_cast<std::chrono::milliseconds>(loadEnd - loadStart).count()
		<< "ms";

	if (bSuccess)
	{
		// Initialize all systems using the loaded project config
		for (int i= 0; i < (int)m_systems.size(); i++)
		{
			MikanObjectSystemPtr system= m_systems[i];

			auto initStart= std::chrono::high_resolution_clock::now();
			MikanObjectSystemDefinitionPtr systemDefinition= m_projectConfig->getDefinitionForSystem(system);

			if (!system->init(systemDefinition))
			{
				bSuccess= false;
				break;
			}
			auto initEnd= std::chrono::high_resolution_clock::now();
			MIKAN_LOG_INFO("ProjectManager::loadProject")
				<< system->getObjectSystemClassName()
				<< "::init: " << std::chrono::duration_cast<std::chrono::milliseconds>(initEnd - initStart).count()
				<< "ms";
		}

		// Once all systems are successfully initialized,
		// call postInit on all systems to allow them to perform any setup
		// that requires other systems/components to be initialized
		// (e.g. TransformComponent wants to attach to its parent in postInit)
		auto postInitStart= std::chrono::high_resolution_clock::now();
		for (int i= 0; i < (int)m_systems.size(); i++)
		{
			m_systems[i]->postInit();
		}
		auto postInitEnd= std::chrono::high_resolution_clock::now();
		MIKAN_LOG_INFO("ProjectManager::loadProject")
			<< "postInit (all systems): "
			<< std::chrono::duration_cast<std::chrono::milliseconds>(postInitEnd - postInitStart).count() << "ms";
	}

	if (bSuccess)
	{
		// Broadcast that the project has been loaded
		if (OnProjectLoaded)
		{
			OnProjectLoaded(shared_from_this());
		}

		// Enable auto-save
		m_projectConfig->setAutoSaveCooldownDuration(PROJECT_SAVE_COOLDOWN);
	}
	else
	{
		// Unload if we failed to load or init
		unloadProject();
	}

	return bSuccess;
}

bool ProjectManager::saveProject(const std::string& projectFilePath)
{
	if (hasLoadedProject())
	{
		m_projectConfig->save(projectFilePath);
		return true;
	}

	return false;
}

void ProjectManager::unloadProject()
{
	// Broadcast that the project is about to be unloaded
	if (OnProjectPreUnload)
	{
		OnProjectPreUnload(shared_from_this());
	}

	// Call dispose in reverse order
	// so that Editor system gets component destroy events
	// from the Anchor and Stencil Systems triggered during dispose call
	for (int i= (int)m_systems.size() - 1; i >= 0; i--)
	{
		MikanObjectSystemPtr system= m_systems[i];

		system->dispose();
	}

	// Clear the current project directory so PathUtils doesn't resolve project-relative resources
	PathUtils::setProjectDirectory(std::filesystem::path());

	m_projectConfig= nullptr;
}
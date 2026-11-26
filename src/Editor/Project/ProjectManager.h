#pragma once

#include "AssetPathRepair.h"
#include "ProjectHierarchyMigration.h"
#include "CommonConfigFwd.h"
#include "ComponentFwd.h"
#include "MikanTypeFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectSystemFwd.h"
#include "MulticastDelegate.h"

#include <filesystem>
#include <string>
#include <vector>
#include <map>

using MikanPropertyDatabasePtr= std::shared_ptr<class MikanPropertyDatabase>;
using MikanPropertyDatabaseConstPtr= std::shared_ptr<const class MikanPropertyDatabase>;
using MikanFunctionDatabasePtr= std::shared_ptr<class MikanFunctionDatabase>;
using MikanFunctionDatabaseConstPtr= std::shared_ptr<const class MikanFunctionDatabase>;

class ProjectManager : public std::enable_shared_from_this<ProjectManager>
{
public:
	ProjectManager(class IEditorWindow* ownerWindow);

	inline ProjectConfigPtr getProjectConfig() const { return m_projectConfig; }
	inline class IEditorWindow* getOwnerWindow() const { return m_ownerWindow; }
	inline MikanPropertyDatabasePtr getPropertyDatabase() { return m_propertyDatabase; }
	inline MikanPropertyDatabaseConstPtr getPropertyDatabaseConst() const { return m_propertyDatabase; }
	inline MikanFunctionDatabasePtr getFunctionDatabase() { return m_functionDatabase; }
	inline MikanFunctionDatabaseConstPtr getFunctionDatabaseConst() const { return m_functionDatabase; }

	template <class t_system_type>
	std::shared_ptr<t_system_type> addSystem()
	{
		std::shared_ptr<t_system_type> systemPtr= std::make_shared<t_system_type>(shared_from_this());
		registerSystem(systemPtr);

		return systemPtr;
	}

	template <class t_system_type>
	std::shared_ptr<t_system_type> getSystemOfType() const
	{
		for (MikanObjectSystemPtr system : m_systems)
		{
			std::shared_ptr<t_system_type> derivedSystem= std::dynamic_pointer_cast<t_system_type>(system);

			if (derivedSystem != nullptr)
			{
				return derivedSystem;
			}
		}

		return nullptr;
	}

	template <class t_system_type>
	std::shared_ptr<const t_system_type> getSystemOfTypeConst() const
	{
		for (MikanObjectSystemPtr system : m_systems)
		{
			std::shared_ptr<const t_system_type> derivedSystem= std::dynamic_pointer_cast<const t_system_type>(system);

			if (derivedSystem != nullptr)
			{
				return derivedSystem;
			}
		}

		return nullptr;
	}

	MikanComponentPtr getComponentById(MikanComponentID componentId) const;
	template <class t_component_type>
	std::shared_ptr<t_component_type> getTypedComponentById(MikanComponentID componentId) const
	{
		MikanComponentPtr componentPtr= getComponentById(componentId);
		return std::dynamic_pointer_cast<t_component_type>(componentPtr);
	}

	MikanObjectSystemPtr getSystemByName(const std::string name) const;
	const std::vector<MikanObjectSystemPtr>& getSystems() const { return m_systems; }

	bool startup(class MainWindow* mainWindow);
	void shutdown();
	void update(float deltaSeconds);

	static std::filesystem::path getDefaultProjectFolder();
	static const char* k_mikanProjectFileExtension;

	bool hasLoadedProject() const;
	bool isAnySystemLoading() const;
	bool newProject(const std::string& projectFilePath);
	bool loadProject(const std::string& projectFilePath);
	bool saveProject(const std::string& projectFilePath);
	void unloadProject();
	// The asset paths the last load repointed, handed over once so the notice shows
	// once per load
	std::vector<AssetPathRepair> takeAssetPathRepairs();
	// What the last load's hierarchy migration changed that the user has to know
	// about, handed over once the same way
	ProjectHierarchyMigration::Report takeHierarchyMigrationReport();

	MulticastDelegate<void(ProjectManagerPtr oldProject)> OnProjectPreUnload;
	MulticastDelegate<void(ProjectManagerPtr newProject)> OnProjectLoaded;

protected:
	ProjectConfigPtr createEmptyProjectConfig() const;
	void registerSystem(MikanObjectSystemPtr system);

private:
	class IEditorWindow* m_ownerWindow= nullptr;
	std::vector<MikanObjectSystemPtr> m_systems;
	std::map<std::string, int> m_systemNameToIndexMap;
	MikanPropertyDatabasePtr m_propertyDatabase;
	MikanFunctionDatabasePtr m_functionDatabase;

	// Project Config
	ProjectConfigPtr m_projectConfig;
	std::vector<AssetPathRepair> m_assetPathRepairs;
	ProjectHierarchyMigration::Report m_hierarchyMigrationReport;
};
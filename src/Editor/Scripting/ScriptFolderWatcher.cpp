#include "ScriptFolderWatcher.h"
#include "MainWindow.h"
#include "PathUtils.h"
#include "ProjectAssetCatalog.h"
#include "ProjectManager.h"
#include "ScriptObjectSystem.h"

#include <filesystem>

bool ScriptFolderWatcher::startup(MainWindow* mainWindow)
{
	m_mainWindow= mainWindow;

	ProjectManagerPtr projectManager= mainWindow->getProjectManager();
	if (projectManager)
	{
		projectManager->OnProjectLoaded+= MakeDelegate(this, &ScriptFolderWatcher::onProjectLoaded);
		projectManager->OnProjectPreUnload+= MakeDelegate(this, &ScriptFolderWatcher::onProjectPreUnload);

		if (projectManager->hasLoadedProject())
		{
			onProjectLoaded(projectManager);
		}
	}

	return true;
}

void ScriptFolderWatcher::shutdown()
{
	if (m_mainWindow)
	{
		ProjectManagerPtr projectManager= m_mainWindow->getProjectManager();
		if (projectManager)
		{
			projectManager->OnProjectLoaded-= MakeDelegate(this, &ScriptFolderWatcher::onProjectLoaded);
			projectManager->OnProjectPreUnload-= MakeDelegate(this, &ScriptFolderWatcher::onProjectPreUnload);
		}
	}

	m_projectWatcher.stop();
	m_bundledWatcher.stop();
	m_projectManager= nullptr;
	m_mainWindow= nullptr;
}

void ScriptFolderWatcher::update(float deltaSeconds)
{
	DirectoryWatcher::ChangeSet changes;

	if (m_projectWatcher.poll(deltaSeconds, changes) && !changes.empty())
	{
		applyChanges(changes);
	}

	if (m_bundledWatcher.poll(deltaSeconds, changes) && !changes.empty())
	{
		applyChanges(changes);
	}
}

void ScriptFolderWatcher::onProjectLoaded(ProjectManagerPtr projectManager)
{
	m_projectManager= projectManager;

	// The project's scripts folder is optional; a project that never added one is skipped
	const std::filesystem::path projectScriptsDir= PathUtils::getProjectDirectory() / "scripts";
	std::error_code ec;
	if (std::filesystem::is_directory(projectScriptsDir, ec))
	{
		m_projectWatcher.watch(projectScriptsDir, ".lua");
	}

	const std::filesystem::path bundledScriptsDir= PathUtils::getResourceDirectory() / "scripts";
	m_bundledWatcher.watch(bundledScriptsDir, ".lua");
}

void ScriptFolderWatcher::onProjectPreUnload(ProjectManagerPtr projectManager)
{
	m_projectWatcher.stop();
	m_bundledWatcher.stop();
	m_projectManager= nullptr;
}

void ScriptFolderWatcher::applyChanges(const DirectoryWatcher::ChangeSet& changes)
{
	// A modified required module has no component of its own but still needs the
	// whole script pool rebuilt, since it may already be cached in package.loaded
	if (m_projectManager)
	{
		if (ScriptObjectSystemPtr scriptSystem= m_projectManager->getSystemOfType<ScriptObjectSystem>())
		{
			scriptSystem->requestReload();
		}
	}

	// A file appearing or disappearing is the only case the Assets panel's script
	// folder needs to know about; a modification in place keeps the same entry
	if (!changes.added.empty() || !changes.removed.empty())
	{
		if (m_mainWindow && m_mainWindow->getAssetCatalog())
		{
			m_mainWindow->getAssetCatalog()->refresh();
		}
	}
}

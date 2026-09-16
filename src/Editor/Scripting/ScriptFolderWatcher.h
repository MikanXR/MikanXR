#pragma once

#include "DirectoryWatcher.h"
#include "ObjectSystemFwd.h"

// Reloads project scripts when a .lua file under the project's scripts folder
// or the bundled resources/scripts folder changes on disk, and refreshes the
// asset catalog when a file appears or disappears
class ScriptFolderWatcher
{
public:
	bool startup(class MainWindow* mainWindow);
	void shutdown();
	void update(float deltaSeconds);

private:
	void onProjectLoaded(ProjectManagerPtr projectManager);
	void onProjectPreUnload(ProjectManagerPtr projectManager);
	void applyChanges(const DirectoryWatcher::ChangeSet& changes);

	class MainWindow* m_mainWindow= nullptr;
	ProjectManagerPtr m_projectManager;
	DirectoryWatcher m_projectWatcher;
	DirectoryWatcher m_bundledWatcher;
};

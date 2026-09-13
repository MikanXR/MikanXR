#include "ScriptAssetReference.h"
#include "App.h"
#include "AppSettingsConfig.h"
#include "Logger.h"
#include "MainWindow.h"
#include "OSUtils.h"
#include "PathUtils.h"
#include "ProjectAssetCatalog.h"

// -- ScriptAssetReference -----
void ScriptAssetReference::editorOpen()
{
	if (isEmpty())
		return;

	std::filesystem::path scriptPath= getResolvedAssetPath();
	const std::string editorCmd= App::getInstance()->getAppSettings()->getScriptEditorCommand();

	// A read-only bundled script is copied into the project first, at the path
	// that shadows it, so the edit lands in the project and the stored script
	// path keeps resolving (now to the copy)
	ProjectAssetCatalog* catalog= App::getInstance()->getMainWindow()->getAssetCatalog();
	if (catalog != nullptr && catalog->isReadOnlyPath(scriptPath))
	{
		std::string storedPath;
		std::string error;
		if (!catalog->importAsset("scripts", scriptPath, storedPath, error))
		{
			MIKAN_LOG_ERROR("ScriptAssetReference::editorOpen")
				<< "Failed to copy bundled script into the project: " << error;
			return;
		}

		scriptPath= PathUtils::resolveProjectResource(storedPath);
	}

	// A script inside the project opens with the project folder ahead of it, so
	// the editor lands in the project workspace (where the generated .luarc.json
	// and launch config live) with the file focused
	const std::filesystem::path projectDir= PathUtils::getProjectDirectory();
	const std::filesystem::path relPath=
		projectDir.empty() ? std::filesystem::path() : scriptPath.lexically_relative(projectDir);
	const bool isUnderProject=
		!relPath.empty() && relPath.native().substr(0, 2) != L".." && relPath.native().front() != L'/';
	if (isUnderProject)
	{
		OSUtils::openPathsWithApplication({projectDir, scriptPath}, editorCmd);
	}
	else
	{
		OSUtils::openFileWithApplication(scriptPath, editorCmd);
	}
}

// -- ScriptAssetReferenceFactory -----
ScriptAssetReferenceFactory::ScriptAssetReferenceFactory()
	: TypedAssetReferenceFactory<ScriptAssetReference, AssetReferenceConfig>()
{
	m_defaultPath= (PathUtils::getProjectDirectory() / "scripts" / "").string();
}
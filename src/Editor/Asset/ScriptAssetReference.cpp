#include "ScriptAssetReference.h"
#include "App.h"
#include "AppSettingsConfig.h"
#include "Logger.h"
#include "MainWindow.h"
#include "OSUtils.h"
#include "PathUtils.h"
#include "ProjectAssetCatalog.h"

#include <algorithm>

namespace
{
// Replaces every occurrence of placeholder in str with value
void replaceAllPlaceholders(std::string& str, const std::string& placeholder, const std::string& value)
{
	size_t pos= 0;
	while ((pos= str.find(placeholder, pos)) != std::string::npos)
	{
		str.replace(pos, placeholder.length(), value);
		pos+= value.length();
	}
}
} // namespace

// -- ScriptAssetReference -----
std::string ScriptAssetReference::expandEditorCommand(const std::string& command,
													  const std::filesystem::path& projectDir,
													  const std::filesystem::path& scriptPath, int line)
{
	std::string expanded= command;
	replaceAllPlaceholders(expanded, "{project}", "\"" + projectDir.generic_string() + "\"");
	replaceAllPlaceholders(expanded, "{file}", "\"" + scriptPath.generic_string() + "\"");
	replaceAllPlaceholders(expanded, "{line}", std::to_string(std::max(line, 1)));

	return expanded;
}

bool ScriptAssetReference::openScriptInEditor(const std::filesystem::path& scriptPath, int line)
{
	if (scriptPath.empty())
		return false;

	std::filesystem::path resolvedPath= scriptPath;
	const std::string editorCmd= App::getInstance()->getAppSettings()->getScriptEditorCommand();

	// A read-only bundled script is copied into the project first, at the path
	// that shadows it, so the edit lands in the project and the stored script
	// path keeps resolving (now to the copy)
	ProjectAssetCatalog* catalog= App::getInstance()->getMainWindow()->getAssetCatalog();
	if (catalog != nullptr && catalog->isReadOnlyPath(resolvedPath))
	{
		std::string storedPath;
		std::string error;
		if (!catalog->importAsset("scripts", resolvedPath, storedPath, error))
		{
			MIKAN_LOG_ERROR("ScriptAssetReference::openScriptInEditor")
				<< "Failed to copy bundled script into the project: " << error;
			return false;
		}

		resolvedPath= PathUtils::resolveProjectResource(storedPath);
	}

	const std::filesystem::path projectDir= PathUtils::getProjectDirectory();

	// A self-contained command expands its own placeholders rather than
	// receiving paths appended after it
	if (AppSettingsConfig::scriptEditorCommandHasPlaceholders(editorCmd))
	{
		const std::string expandedCmd= expandEditorCommand(editorCmd, projectDir, resolvedPath, line);
		return OSUtils::runCommandLine(expandedCmd);
	}

	// A script inside the project opens with the project folder ahead of it, so
	// the editor lands in the project workspace (where the generated .luarc.json
	// and launch config live) with the file focused
	const std::filesystem::path relPath=
		projectDir.empty() ? std::filesystem::path() : resolvedPath.lexically_relative(projectDir);
	const bool isUnderProject=
		!relPath.empty() && relPath.native().substr(0, 2) != L".." && relPath.native().front() != L'/';
	if (isUnderProject)
	{
		return OSUtils::openPathsWithApplication({projectDir, resolvedPath}, editorCmd);
	}
	else
	{
		return OSUtils::openFileWithApplication(resolvedPath, editorCmd);
	}
}

void ScriptAssetReference::editorOpen()
{
	if (isEmpty())
		return;

	openScriptInEditor(getResolvedAssetPath(), 0);
}

// -- ScriptAssetReferenceFactory -----
ScriptAssetReferenceFactory::ScriptAssetReferenceFactory()
	: TypedAssetReferenceFactory<ScriptAssetReference, AssetReferenceConfig>()
{
	m_defaultPath= (PathUtils::getProjectDirectory() / "scripts" / "").string();
}
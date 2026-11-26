#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace configuru
{
class Config;
}

// Moves a project file written under an older scene hierarchy onto the current one,
// on the raw project JSON, before the project config reads it:
//   - a scene owns exactly one compositor. A scene that held several keeps the one
//     it displayed, or its first, and each other compositor moves to a new scene on
//     the same stage. A scene that held none gains one. A compositor whose owner
//     scene is missing gets a scene of its own
//   - the scene's display compositor choice is dropped, since there is nothing
//     left to choose
//   - the camera a compositor named moves to the scene that owns it
// Every step keys on the shape it repairs, so a current project is left untouched
// and a project migrated once is not migrated again.
namespace ProjectHierarchyMigration
{
// A scene the migration created to hold a compositor its old scene could not keep.
// The new scene starts with no actors, so the compositor draws none of the old
// scene's stencils or shapes until the user moves or rebuilds them.
struct SceneSplit
{
	std::string compositorName;
	// Empty when the compositor's owner scene was missing
	std::string oldSceneName;
	std::string newSceneName;
};

// What a migration did that the user has to know about
struct Report
{
	std::vector<SceneSplit> sceneSplits;

	bool isEmpty() const { return sceneSplits.empty(); }
};

// Migrates the parsed project JSON in place. Returns true when anything changed.
bool migrateProjectJson(configuru::Config& projectJson, Report& outReport);

// Migrates the project file in place. Returns true when anything changed.
bool migrateProject(const std::filesystem::path& projectFilePath, Report& outReport);
} // namespace ProjectHierarchyMigration

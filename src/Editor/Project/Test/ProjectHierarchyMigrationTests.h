#pragma once

// Validates the move of a project file onto one compositor per scene:
//   - a scene with several compositors keeps the one it displayed and each other
//     compositor lands in a new scene on the same stage, reported as a split
//   - a scene with no display choice keeps its first compositor
//   - a scene with no compositor gains one
//   - a compositor whose owner scene is missing gets an unparented scene
//   - the camera each compositor named ends up on the scene that owns it
//   - new ids start past both the saved counter and every id in the file
//   - a migrated project is left untouched by a second pass
bool run_project_hierarchy_migration_tests();

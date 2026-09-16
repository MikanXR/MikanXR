#pragma once

// Validates the script editor command's {project}/{file}/{line} placeholder
// handling:
//   - expandEditorCommand replaces every placeholder occurrence with a quoted
//     forward-slash path (or the 1-based line number)
//   - scriptEditorCommandHasPlaceholders looks for {file}, the placeholder
//     that makes a command self-contained
//   - a stored legacy default command upgrades to the new default on load,
//     any other stored command is kept as-is
bool run_script_editor_command_tests();

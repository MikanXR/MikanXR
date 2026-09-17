#pragma once

// Validates ModelRepository's search order and presence rules:
//   - a model in the working directory's models/ wins over a downloaded copy
//   - a downloaded copy is found when the working directory has none
//   - a directory missing any required file is not installed, and the missing
//     names are reported
//   - an explicit override directory is used as given rather than searched past
bool run_model_repository_tests();

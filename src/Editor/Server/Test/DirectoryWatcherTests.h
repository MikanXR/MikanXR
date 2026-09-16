#pragma once

// Validates DirectoryWatcher:
//   - diff() is a pure function over two snapshots: added, removed, modified,
//     and a rename showing as a removed plus an added entry
//   - scan() filters by extension, case-insensitively, recursing into subfolders
//   - watch()/poll() report a real OS change (create, modify, delete) on a temp folder
//   - a burst of writes within the debounce window collapses into one report
bool run_directory_watcher_tests();

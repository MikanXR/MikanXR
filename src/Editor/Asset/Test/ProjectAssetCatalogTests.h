#pragma once

// Validates the project asset catalog against a temp project on disk:
//   - the scan lists each folder's files by type, one tile per .mat, and the
//     bundled resources as a read-only overlay behind every folder
//   - import copies into the folder with a numeric suffix on collision, leaves a
//     file already inside the folder alone, and refuses unsupported types
//   - material import lands the folder under shaders/<domain>/ and refuses a
//     duplicate or a .mat with no domain
//   - findReferences names the graphs, materials, and texture parameter nodes
//     that use a path, and not a material's own sources
//   - every folder overlays the bundled resources as read-only entries a project
//     file shadows by stored path, copying one in keeps its relative path, and
//     the developer switch makes them writable
//   - delete removes a file or a material folder and refuses bundled entries
//   - AssetReference stores project-relative paths and writes them as UTF-8
//   - a missing absolute path from another project folder relocates to the
//     trailing part the project or the bundled resources hold
bool run_project_asset_catalog_tests();

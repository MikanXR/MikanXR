#include "Constants_OcclusionMeshBuild.h"

// Named per stage: the unity build concatenates the Constants_*.cpp files,
// so file-statics collide by name
static const std::string k_OcclusionMeshBuildMenuStateStringTable[(int)eOcclusionMeshBuildMenuState::COUNT]= {
	"inactive", "verifyTake", "building", "failed", "verifyMesh", "buildComplete",
};
const std::string* k_OcclusionMeshBuildMenuStateStrings= k_OcclusionMeshBuildMenuStateStringTable;

static const std::string k_OcclusionMeshVoxelPresetStringTable[(int)eOcclusionMeshVoxelPreset::COUNT]= {
	"room",
	"tabletop",
};
const std::string* k_OcclusionMeshVoxelPresetStrings= k_OcclusionMeshVoxelPresetStringTable;

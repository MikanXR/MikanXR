#pragma once

#include <string>

enum class eOcclusionMeshBuildMenuState : int
{
	INVALID= -1,

	inactive,
	// The take is loaded and summarised; the operator picks the settings and hits Build
	verifyTake,
	// The worker is fusing; progress is frames of keyframes
	building,
	failed,
	// Mesh built; the summary and the preview are shown so it can be judged
	// before the stencil is created
	verifyMesh,
	buildComplete,

	COUNT
};
extern const std::string* k_OcclusionMeshBuildMenuStateStrings;

// The fusion grid presets. A room wants the coarser grid; a tabletop sweep
// at arm's length resolves small objects at the finer one and keeps more of
// its small islands.
enum class eOcclusionMeshVoxelPreset : int
{
	INVALID= -1,

	room,
	tabletop,

	COUNT
};
extern const std::string* k_OcclusionMeshVoxelPresetStrings;

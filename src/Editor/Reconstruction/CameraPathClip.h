#pragma once

#include "glm/ext/vector_float3.hpp"

#include <cstdint>
#include <vector>

class TsdfVolume;

// The keep mask that trims a fused volume to the neighbourhood of the sweep:
// a voxel is kept when it lies within radius of any camera position the
// fusion visited, so the surfaces a long throw dragged in from the rest of
// the room fall away at extraction. The test runs per block of cellVoxels
// voxels per axis against the block's centre, so the cut is quantised to a
// block (6 cm at the room preset with the default of 4), which is fine for a
// bound a couple of metres out.
class CameraPathClip
{
public:
	static constexpr int k_defaultCellVoxels= 4;

	// outKeep is one byte per voxel in the volume's flat layout, 1 kept. An
	// empty path keeps nothing. Returns the count of voxels dropped.
	static size_t buildKeepMask(const TsdfVolume& volume, const std::vector<glm::vec3>& pathPositions, float radius,
								std::vector<uint8_t>& outKeep, int cellVoxels= k_defaultCellVoxels);
};

#pragma once

#include "ReconstructionTypes.h"

#include "glm/ext/vector_int3.hpp"

#include <cstdint>

// Marching cubes over a dense scalar field laid out like TsdfVolume
// (flat index (z * dimY + y) * dimX + x, sample position
// origin + index * voxelSize).
class MarchingCubes
{
public:
	// Extract the isoLevel surface. A cube is emitted only when all eight of
	// its corner samples are set in mask (CV_8UC1 semantics: nonzero is in).
	// Vertices on shared cube edges are welded so the mesh is connected.
	// Triangles are wound counter-clockwise about the direction of
	// increasing field value, which for a signed distance (negative inside)
	// is the outward normal.
	static void extract(const float* field, const uint8_t* mask, const glm::ivec3& dims, const glm::vec3& origin,
						float voxelSize, float isoLevel, TriangleMesh& outMesh);
};

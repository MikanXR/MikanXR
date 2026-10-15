#include "CameraPathClip.h"

#include "ParallelFor.h"
#include "TsdfVolume.h"

#include "glm/ext/vector_int3.hpp"
#include "glm/geometric.hpp"

#include <algorithm>
#include <atomic>
#include <limits>

size_t CameraPathClip::buildKeepMask(const TsdfVolume& volume, const std::vector<glm::vec3>& pathPositions,
									 float radius, std::vector<uint8_t>& outKeep, int cellVoxels)
{
	const glm::ivec3& dims= volume.getDims();
	const size_t voxelCount= volume.getVoxelCount();
	outKeep.assign(voxelCount, 0);
	if (pathPositions.empty() || radius <= 0.f)
		return voxelCount;

	const int cell= std::max(cellVoxels, 1);
	const glm::ivec3 cellDims((dims.x + cell - 1) / cell, (dims.y + cell - 1) / cell, (dims.z + cell - 1) / cell);
	const float radiusSquared= radius * radius;
	const float voxelSize= volume.getVoxelSize();
	const glm::vec3& origin= volume.getOrigin();

	// Each z range of cells writes only its own voxels
	std::atomic<size_t> keptCount(0);
	parallelFor(
		cellDims.z,
		[&](int cellBegin, int cellEnd)
		{
			size_t chunkKept= 0;
			for (int cz= cellBegin; cz < cellEnd; ++cz)
			{
				for (int cy= 0; cy < cellDims.y; ++cy)
				{
					for (int cx= 0; cx < cellDims.x; ++cx)
					{
						const glm::ivec3 voxelLo(cx * cell, cy * cell, cz * cell);
						const glm::ivec3 voxelHi(std::min(voxelLo.x + cell, dims.x), std::min(voxelLo.y + cell, dims.y),
												 std::min(voxelLo.z + cell, dims.z));
						const glm::vec3 center=
							origin + (glm::vec3(voxelLo) + glm::vec3(voxelHi - glm::ivec3(1))) * (0.5f * voxelSize);

						float nearestSquared= std::numeric_limits<float>::infinity();
						for (const glm::vec3& position : pathPositions)
						{
							const glm::vec3 offset= center - position;
							nearestSquared= std::min(nearestSquared, glm::dot(offset, offset));
							if (nearestSquared <= radiusSquared)
								break;
						}
						if (nearestSquared > radiusSquared)
							continue;

						for (int z= voxelLo.z; z < voxelHi.z; ++z)
						{
							for (int y= voxelLo.y; y < voxelHi.y; ++y)
							{
								uint8_t* row= &outKeep[volume.flatIndex(voxelLo.x, y, z)];
								std::fill(row, row + (voxelHi.x - voxelLo.x), (uint8_t)1);
								chunkKept+= (size_t)(voxelHi.x - voxelLo.x);
							}
						}
					}
				}
			}
			keptCount+= chunkKept;
		},
		1);

	return voxelCount - keptCount.load();
}

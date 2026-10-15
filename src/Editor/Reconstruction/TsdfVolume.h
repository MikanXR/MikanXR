#pragma once

#include "ReconstructionTypes.h"

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_int3.hpp"

#include <opencv2/core.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

struct TsdfVolumeConfig
{
	float voxelSize= 0.015f;    // metres
	float truncation= 0.06f;    // metres, the signed distance band around a surface
	float depthMin= 0.1f;       // metres, nearer samples are ignored
	float depthMax= 5.f;        // metres, farther samples are ignored
	float gradientCull= 0.08f;  // relative depth jump to a 4-neighbour that drops a pixel
	float minWeight= 0.001f;    // per-pixel weight below which a pixel is skipped
	size_t maxVoxels= 60000000; // the voxel grows until the grid fits
};

// A dense truncated signed distance volume over an axis-aligned box, with
// one running weighted average per voxel. Every camera here uses OpenCV
// axes (+X right, +Y down, +Z forward) and depth is distance along +Z; the
// world is whatever space the camera-to-world transforms are given in.
//
// Flat index is (z * dimY + y) * dimX + x, and a voxel's centre is
// origin + index * voxel, so the grid's corner voxel is centred on origin.
class TsdfVolume
{
public:
	// Covers [boundsLo, boundsHi]. If the box needs more than maxVoxels at
	// the configured voxel size the voxel grows by 10 percent until it fits.
	TsdfVolume(const glm::vec3& boundsLo, const glm::vec3& boundsHi, const TsdfVolumeConfig& config);

	const TsdfVolumeConfig& getConfig() const { return m_config; }
	float getVoxelSize() const { return m_voxelSize; }
	const glm::vec3& getOrigin() const { return m_origin; }
	const glm::ivec3& getDims() const { return m_dims; }
	size_t getVoxelCount() const { return m_tsdf.size(); }
	int getFramesIntegrated() const { return m_framesIntegrated; }

	// Read access for extraction and tests. observed is weight > 0.
	const std::vector<float>& getTsdf() const { return m_tsdf; }
	const std::vector<float>& getWeights() const { return m_weights; }
	float getVoxelTsdf(int x, int y, int z) const { return m_tsdf[flatIndex(x, y, z)]; }
	bool isVoxelObserved(int x, int y, int z) const { return m_weights[flatIndex(x, y, z)] > 0.f; }
	size_t flatIndex(int x, int y, int z) const { return ((size_t)z * m_dims.y + y) * m_dims.x + x; }
	glm::vec3 voxelCenter(int x, int y, int z) const;

	// Fold one depth frame into the volume. depthMeters is CV_32FC1 (0 or
	// non-finite where invalid), weight CV_32FC1 per pixel. Pixels failing the
	// gradient cull, the weight floor, or the depth range are skipped. Only
	// voxels inside the frame's frustum between depthMin and depthMax are
	// visited.
	void integrate(const cv::Mat& depthMeters, const cv::Mat& weight, const PinholeIntrinsics& intrinsics,
				   const glm::mat4& cameraToWorld);

	// Trilinear signed distance at a world point. NaN when the point is
	// outside the grid or any of the eight surrounding voxels is unobserved,
	// so the rim of an observed patch never reads as a surface.
	float sample(const glm::vec3& worldPoint) const;

	// Render the volume from a camera into hit points and normals at
	// width x height. Rays step by stepFraction * truncation from depthMin
	// to depthMax and stop at the first observed positive-to-negative zero
	// crossing, refined by linear interpolation. Normals are the gradient by
	// central differences normalStencilVoxels voxels either side of the hit.
	void raycast(const PinholeIntrinsics& intrinsics, const glm::mat4& cameraToWorld, int width, int height,
				 float stepFraction, float normalStencilVoxels, ModelMap& outMap) const;

	// Extract the zero crossing as a mesh in world space. Unobserved voxels
	// first take the sign of the nearest observed voxel so no back shell forms
	// behind a surface; marching cubes then runs over the observed region
	// dilated by one voxel. keepMask, when given, is one byte per voxel in the
	// grid's flat layout: a voxel whose byte is zero is left out of the
	// marching cubes region, so no cube touching it is emitted and the surface
	// ends there with a bare edge. Returns false when too little was observed.
	bool extractMesh(TriangleMesh& outMesh, const std::vector<uint8_t>* keepMask= nullptr) const;

	// Mark pixels whose depth jumps by more than threshold (relative to the
	// smaller of the two) against any 4-neighbour. Both pixels of a bad pair
	// are dropped. outKeep is CV_8UC1, 1 where the pixel survives.
	static void cullFlyingPixels(const cv::Mat& depthMeters, float threshold, cv::Mat& outKeep);

private:
	TsdfVolumeConfig m_config;
	float m_voxelSize= 0.f;
	float m_truncation= 0.f;
	glm::vec3 m_origin= glm::vec3(0.f);
	glm::ivec3 m_dims= glm::ivec3(0);
	std::vector<float> m_tsdf;    // 1 until observed
	std::vector<float> m_weights; // 0 until observed
	int m_framesIntegrated= 0;
};

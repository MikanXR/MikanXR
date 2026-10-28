#include "TsdfVolume.h"
#include "MarchingCubes.h"
#include "ParallelFor.h"

#include "Logger.h"

#include "glm/ext/matrix_double4x4.hpp"
#include "glm/ext/matrix_float3x3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/geometric.hpp"
#include "glm/matrix.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>

namespace TsdfVolumeHelpers
{
// Samples per axis to cover extent at voxelSize, the box's corner on a sample
static glm::ivec3 computeGridDims(const glm::vec3& extent, double voxelSize)
{
	glm::ivec3 dims;
	for (int axis= 0; axis < 3; ++axis)
	{
		const double cells= std::ceil(std::max((double)extent[axis], 0.0) / voxelSize);
		dims[axis]= std::max(1, (int)std::min(cells, 1.0e9) + 1);
	}
	return dims;
}

static double gridVoxelCount(const glm::ivec3& dims) { return (double)dims.x * (double)dims.y * (double)dims.z; }

// Sign of an observed signed distance with zero counted as free space
static float signedDistanceSign(float value) { return value < 0.f ? -1.f : 1.f; }

static cv::Mat toFloatImage(const cv::Mat& image)
{
	if (image.type() == CV_32FC1)
		return image;

	cv::Mat converted;
	image.convertTo(converted, CV_32F);
	return converted;
}
} // namespace TsdfVolumeHelpers

TsdfVolume::TsdfVolume(const glm::vec3& boundsLo, const glm::vec3& boundsHi, const TsdfVolumeConfig& config)
	: m_config(config)
{
	const glm::vec3 extent= boundsHi - boundsLo;
	const double maxVoxels= (double)std::max<size_t>(config.maxVoxels, 1);

	double voxelSize= std::max((double)config.voxelSize, 1e-6);
	glm::ivec3 dims= TsdfVolumeHelpers::computeGridDims(extent, voxelSize);
	while (TsdfVolumeHelpers::gridVoxelCount(dims) > maxVoxels)
	{
		voxelSize*= 1.1;
		dims= TsdfVolumeHelpers::computeGridDims(extent, voxelSize);
	}

	m_voxelSize= (float)voxelSize;
	m_truncation= config.truncation;
	m_origin= boundsLo;
	m_dims= dims;

	const size_t voxelCount= (size_t)dims.x * (size_t)dims.y * (size_t)dims.z;
	m_tsdf.assign(voxelCount, 1.f);
	m_weights.assign(voxelCount, 0.f);
}

glm::vec3 TsdfVolume::voxelCenter(int x, int y, int z) const
{
	return m_origin + glm::vec3((float)x, (float)y, (float)z) * m_voxelSize;
}

void TsdfVolume::cullFlyingPixels(const cv::Mat& depthMeters, float threshold, cv::Mat& outKeep, bool bKeepNear,
								  int keepNearRadius)
{
	if (depthMeters.empty() || depthMeters.channels() != 1)
	{
		outKeep.release();
		return;
	}

	const cv::Mat depth= TsdfVolumeHelpers::toFloatImage(depthMeters);
	const int height= depth.rows;
	const int width= depth.cols;

	outKeep.create(height, width, CV_8UC1);
	for (int y= 0; y < height; ++y)
	{
		const float* depthRow= depth.ptr<float>(y);
		uint8_t* keepRow= outKeep.ptr<uint8_t>(y);
		for (int x= 0; x < width; ++x)
		{
			keepRow[x]= (std::isfinite(depthRow[x]) && depthRow[x] > 0.f) ? 1 : 0;
		}
	}

	// Valid pairs only, judged against the unmodified validity so a dropped
	// pixel still drops its other bad neighbours
	const cv::Mat valid= outKeep.clone();
	auto relativeJump= [](float a, float b) { return std::fabs(a - b) / std::max(std::min(a, b), 1e-6f); };

	for (int y= 0; y < height; ++y)
	{
		const float* depthRow= depth.ptr<float>(y);
		const uint8_t* validRow= valid.ptr<uint8_t>(y);
		uint8_t* keepRow= outKeep.ptr<uint8_t>(y);
		for (int x= 0; x < width; ++x)
		{
			if (!validRow[x])
				continue;

			// Right neighbour
			if (x + 1 < width && validRow[x + 1] && relativeJump(depthRow[x + 1], depthRow[x]) > threshold)
			{
				const bool bThisFarther= depthRow[x] > depthRow[x + 1];
				if (!bKeepNear || bThisFarther)
					keepRow[x]= 0;
				if (!bKeepNear || !bThisFarther)
					keepRow[x + 1]= 0;
			}

			// Lower neighbour
			if (y + 1 < height)
			{
				const float* nextDepthRow= depth.ptr<float>(y + 1);
				const uint8_t* nextValidRow= valid.ptr<uint8_t>(y + 1);
				if (nextValidRow[x] && relativeJump(nextDepthRow[x], depthRow[x]) > threshold)
				{
					const bool bThisFarther= depthRow[x] > nextDepthRow[x];
					if (!bKeepNear || bThisFarther)
						keepRow[x]= 0;
					if (!bKeepNear || !bThisFarther)
						outKeep.ptr<uint8_t>(y + 1)[x]= 0;
				}
			}
		}
	}

	// A wider far band: a pixel farther than the nearest valid depth within the
	// radius by more than the threshold is dropped too
	if (bKeepNear && keepNearRadius > 1)
	{
		cv::Mat nearest= depth.clone();
		nearest.setTo(std::numeric_limits<float>::max(), valid == 0);
		const int size= 2 * keepNearRadius + 1;
		cv::erode(nearest, nearest, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(size, size)));
		for (int y= 0; y < height; ++y)
		{
			const float* depthRow= depth.ptr<float>(y);
			const float* nearestRow= nearest.ptr<float>(y);
			uint8_t* keepRow= outKeep.ptr<uint8_t>(y);
			for (int x= 0; x < width; ++x)
			{
				if (keepRow[x] && relativeJump(depthRow[x], nearestRow[x]) > threshold)
					keepRow[x]= 0;
			}
		}
	}
}

void TsdfVolume::integrate(const cv::Mat& depthMeters, const cv::Mat& weight, const PinholeIntrinsics& intrinsics,
						   const glm::mat4& cameraToWorld)
{
	if (depthMeters.empty() || depthMeters.channels() != 1 || weight.channels() != 1
		|| weight.size() != depthMeters.size())
	{
		MIKAN_LOG_ERROR("TsdfVolume::integrate") << "Depth and weight must be single channel images of one size";
		return;
	}
	if (!(intrinsics.fx > 0.0) || !(intrinsics.fy > 0.0))
	{
		MIKAN_LOG_ERROR("TsdfVolume::integrate") << "Intrinsics have no focal length";
		return;
	}

	const cv::Mat depthIn= TsdfVolumeHelpers::toFloatImage(depthMeters);
	const cv::Mat weightIn= TsdfVolumeHelpers::toFloatImage(weight);
	const int height= depthIn.rows;
	const int width= depthIn.cols;
	const float depthMin= m_config.depthMin;
	const float depthMax= m_config.depthMax;

	// Depth and weight zeroed wherever the pixel is not kept
	cv::Mat keep;
	cullFlyingPixels(depthIn, m_config.gradientCull, keep, m_config.gradientCullKeepsNear, m_config.gradientCullRadius);

	cv::Mat depth(height, width, CV_32FC1, cv::Scalar(0.f));
	cv::Mat pixelWeight(height, width, CV_32FC1, cv::Scalar(0.f));
	bool bAnyKept= false;
	for (int y= 0; y < height; ++y)
	{
		const float* depthInRow= depthIn.ptr<float>(y);
		const float* weightInRow= weightIn.ptr<float>(y);
		const uint8_t* keepRow= keep.ptr<uint8_t>(y);
		float* depthRow= depth.ptr<float>(y);
		float* weightRow= pixelWeight.ptr<float>(y);
		for (int x= 0; x < width; ++x)
		{
			const float pixelDepth= depthInRow[x];
			const float pixelWeightValue= weightInRow[x];
			if (keepRow[x] && pixelWeightValue >= m_config.minWeight && pixelDepth >= depthMin
				&& pixelDepth <= depthMax)
			{
				depthRow[x]= pixelDepth;
				weightRow[x]= pixelWeightValue;
				bAnyKept= true;
			}
		}
	}
	if (!bAnyKept)
		return;

	const glm::mat4 worldToCamera= glm::mat4(glm::inverse(glm::dmat4(cameraToWorld)));
	const float fx= (float)intrinsics.fx;
	const float fy= (float)intrinsics.fy;
	const float cx= (float)intrinsics.cx;
	const float cy= (float)intrinsics.cy;
	const float truncation= m_truncation;

	// World box of the frustum. A voxel can be updated up to one truncation
	// behind the farthest kept depth, and pixel indices round, so the box
	// reaches half a pixel past the image edge.
	const float frustumDepths[2]= {depthMin, depthMax + truncation};
	const float pixelCorners[4][2]= {{-0.5f, -0.5f},
									 {(float)width - 0.5f, -0.5f},
									 {-0.5f, (float)height - 0.5f},
									 {(float)width - 0.5f, (float)height - 0.5f}};
	glm::vec3 boxLo(std::numeric_limits<float>::max());
	glm::vec3 boxHi(-std::numeric_limits<float>::max());
	for (const float frustumDepth : frustumDepths)
	{
		for (const auto& pixelCorner : pixelCorners)
		{
			const glm::vec4 cameraPoint((pixelCorner[0] - cx) / fx * frustumDepth,
										(pixelCorner[1] - cy) / fy * frustumDepth, frustumDepth, 1.f);
			const glm::vec3 worldPoint= glm::vec3(cameraToWorld * cameraPoint);
			for (int axis= 0; axis < 3; ++axis)
			{
				boxLo[axis]= std::min(boxLo[axis], worldPoint[axis]);
				boxHi[axis]= std::max(boxHi[axis], worldPoint[axis]);
			}
		}
	}

	// Voxel index range of the box, clamped to the grid
	glm::ivec3 indexLo;
	glm::ivec3 indexHi;
	for (int axis= 0; axis < 3; ++axis)
	{
		const float lo= std::floor((boxLo[axis] - m_origin[axis]) / m_voxelSize);
		const float hi= std::ceil((boxHi[axis] - m_origin[axis]) / m_voxelSize);
		const float lastIndex= (float)(m_dims[axis] - 1);
		if (!(lo <= lastIndex) || !(hi >= 0.f))
			return;

		indexLo[axis]= (int)std::max(lo, 0.f);
		indexHi[axis]= (int)std::min(hi, lastIndex);
	}

	// Each z slab writes only its own voxels
	std::atomic<bool> bAnyUpdated(false);
	parallelFor(
		indexHi.z - indexLo.z + 1,
		[&](int begin, int end)
		{
			bool bChunkUpdated= false;
			for (int z= indexLo.z + begin; z < indexLo.z + end; ++z)
			{
				for (int y= indexLo.y; y <= indexHi.y; ++y)
				{
					for (int x= indexLo.x; x <= indexHi.x; ++x)
					{
						const glm::vec4 cameraPoint= worldToCamera * glm::vec4(voxelCenter(x, y, z), 1.f);
						const float voxelDepth= cameraPoint.z;
						if (!(voxelDepth > depthMin))
							continue;

						const float u= cameraPoint.x / voxelDepth * fx + cx;
						const float v= cameraPoint.y / voxelDepth * fy + cy;
						if (!(u > -1.f && u < (float)width + 1.f && v > -1.f && v < (float)height + 1.f))
							continue;

						const long ui= std::lround(u);
						const long vi= std::lround(v);
						if (ui < 0 || ui >= width || vi < 0 || vi >= height)
							continue;

						const float pixelDepth= depth.ptr<float>((int)vi)[ui];
						if (!(pixelDepth > 0.f))
							continue;

						const float sdf= pixelDepth - voxelDepth;
						if (!(sdf > -truncation))
							continue;

						// A zero weight carries no information and would divide by
						// zero on a voxel not yet observed
						const float newWeight= pixelWeight.ptr<float>((int)vi)[ui];
						if (!(newWeight > 0.f))
							continue;

						const float newTsdf= std::min(sdf / truncation, 1.f);
						const size_t index= flatIndex(x, y, z);
						const float oldWeight= m_weights[index];
						m_tsdf[index]= (m_tsdf[index] * oldWeight + newTsdf * newWeight) / (oldWeight + newWeight);
						m_weights[index]= oldWeight + newWeight;
						bChunkUpdated= true;
					}
				}
			}

			if (bChunkUpdated)
			{
				bAnyUpdated.store(true);
			}
		},
		1);

	if (bAnyUpdated.load())
	{
		m_framesIntegrated++;
	}
}

float TsdfVolume::sample(const glm::vec3& worldPoint) const
{
	const float k_noSample= std::numeric_limits<float>::quiet_NaN();

	int lower[3];
	int upper[3];
	float fractions[3];
	for (int axis= 0; axis < 3; ++axis)
	{
		const float gridCoordinate= (worldPoint[axis] - m_origin[axis]) / m_voxelSize;
		const int lastIndex= m_dims[axis] - 1;
		if (!(gridCoordinate >= 0.f && gridCoordinate <= (float)lastIndex))
			return k_noSample;

		lower[axis]= std::min((int)gridCoordinate, lastIndex);
		upper[axis]= std::min(lower[axis] + 1, lastIndex);
		fractions[axis]= gridCoordinate - (float)lower[axis];
	}

	// Blend the eight surrounding voxels, none of which may be unobserved
	float result= 0.f;
	for (int corner= 0; corner < 8; ++corner)
	{
		const int x= (corner & 1) ? upper[0] : lower[0];
		const int y= (corner & 2) ? upper[1] : lower[1];
		const int z= (corner & 4) ? upper[2] : lower[2];
		const size_t index= flatIndex(x, y, z);
		if (!(m_weights[index] > 0.f))
			return k_noSample;

		const float cornerWeight= ((corner & 1) ? fractions[0] : 1.f - fractions[0])
								  * ((corner & 2) ? fractions[1] : 1.f - fractions[1])
								  * ((corner & 4) ? fractions[2] : 1.f - fractions[2]);
		result+= cornerWeight * m_tsdf[index];
	}

	return result;
}

void TsdfVolume::raycast(const PinholeIntrinsics& intrinsics, const glm::mat4& cameraToWorld, int width, int height,
						 float stepFraction, float normalStencilVoxels, ModelMap& outMap) const
{
	if (width <= 0 || height <= 0)
	{
		MIKAN_LOG_ERROR("TsdfVolume::raycast") << "Image size must be positive";
		outMap.points.release();
		outMap.normals.release();
		outMap.hit.release();
		return;
	}

	outMap.points.create(height, width, CV_32FC3);
	outMap.points.setTo(cv::Scalar::all(0.0));
	outMap.normals.create(height, width, CV_32FC3);
	outMap.normals.setTo(cv::Scalar::all(0.0));
	outMap.hit.create(height, width, CV_8UC1);
	outMap.hit.setTo(cv::Scalar::all(0.0));

	const float stepLength= m_truncation * stepFraction;
	if (!(stepLength > 0.f) || !(intrinsics.fx > 0.0) || !(intrinsics.fy > 0.0))
	{
		MIKAN_LOG_ERROR("TsdfVolume::raycast") << "Step length and focal length must be positive";
		return;
	}

	// Ray parameters depthMin + i * step for every i keeping t below depthMax
	const float depthMin= m_config.depthMin;
	const int stepCount= (int)std::ceil((m_config.depthMax - depthMin) / stepLength);
	if (stepCount < 2)
		return;

	const float fx= (float)intrinsics.fx;
	const float fy= (float)intrinsics.fy;
	const float cx= (float)intrinsics.cx;
	const float cy= (float)intrinsics.cy;
	const glm::mat3 rotation(cameraToWorld);
	const glm::vec3 rayOrigin(cameraToWorld[3]);
	const float normalEpsilon= normalStencilVoxels * m_voxelSize;

	parallelFor(
		height,
		[&](int rowBegin, int rowEnd)
		{
			for (int v= rowBegin; v < rowEnd; ++v)
			{
				cv::Vec3f* pointRow= outMap.points.ptr<cv::Vec3f>(v);
				cv::Vec3f* normalRow= outMap.normals.ptr<cv::Vec3f>(v);
				uint8_t* hitRow= outMap.hit.ptr<uint8_t>(v);
				for (int u= 0; u < width; ++u)
				{
					// Unnormalized so t is depth along the camera's +Z
					const glm::vec3 direction= rotation * glm::vec3(((float)u - cx) / fx, ((float)v - cy) / fy, 1.f);

					// First observed positive sample followed by an observed
					// non-positive one. NaN (unobserved) fails both comparisons.
					float previousT= depthMin;
					float previousSample= sample(rayOrigin + direction * previousT);
					bool bCrossed= false;
					float hitT= 0.f;
					for (int stepIndex= 1; stepIndex < stepCount; ++stepIndex)
					{
						const float t= depthMin + (float)stepIndex * stepLength;
						const float currentSample= sample(rayOrigin + direction * t);
						if (previousSample > 0.f && currentSample <= 0.f)
						{
							const float fraction= previousSample / std::max(previousSample - currentSample, 1e-6f);
							hitT= previousT + fraction * stepLength;
							bCrossed= true;
							break;
						}
						previousT= t;
						previousSample= currentSample;
					}
					if (!bCrossed)
						continue;

					// Central difference gradient, which points out of the surface
					const glm::vec3 hitPoint= rayOrigin + direction * hitT;
					glm::vec3 gradient(0.f);
					bool bGradientValid= true;
					for (int axis= 0; axis < 3 && bGradientValid; ++axis)
					{
						glm::vec3 offset(0.f);
						offset[axis]= normalEpsilon;
						gradient[axis]= sample(hitPoint + offset) - sample(hitPoint - offset);
						bGradientValid= std::isfinite(gradient[axis]);
					}
					if (!bGradientValid)
						continue;

					const float gradientLength= glm::length(gradient);
					if (!(gradientLength > 1e-6f))
						continue;

					const glm::vec3 normal= gradient / gradientLength;
					pointRow[u]= cv::Vec3f(hitPoint.x, hitPoint.y, hitPoint.z);
					normalRow[u]= cv::Vec3f(normal.x, normal.y, normal.z);
					hitRow[u]= 1;
				}
			}
		},
		4);
}

bool TsdfVolume::extractMesh(TriangleMesh& outMesh, const std::vector<uint8_t>* keepMask) const
{
	outMesh.clear();

	const size_t voxelCount= m_tsdf.size();
	if (keepMask != nullptr && keepMask->size() != voxelCount)
	{
		MIKAN_LOG_ERROR("TsdfVolume::extractMesh") << "The keep mask does not match the grid";
		return false;
	}
	size_t observedCount= 0;
	for (const float voxelWeight : m_weights)
	{
		if (voxelWeight > 0.f)
			observedCount++;
	}
	if (observedCount < 8)
		return false;

	// The 26 neighbour offsets ordered by distance: faces, then edges, then corners
	struct NeighbourOffset
	{
		int dx, dy, dz;
	};
	std::vector<NeighbourOffset> neighbourOffsets;
	for (int distanceSquared= 1; distanceSquared <= 3; ++distanceSquared)
	{
		for (int dz= -1; dz <= 1; ++dz)
		{
			for (int dy= -1; dy <= 1; ++dy)
			{
				for (int dx= -1; dx <= 1; ++dx)
				{
					if (dx * dx + dy * dy + dz * dz == distanceSquared)
					{
						neighbourOffsets.push_back({dx, dy, dz});
					}
				}
			}
		}
	}

	// Unobserved voxels hold tsdf 1. Left alone they form a zero crossing
	// against the observed negative band behind every surface, a back shell
	// behind the real one. Each unobserved voxel next to an observed one takes
	// the sign of its nearest observed neighbour instead, keeping the band
	// negative behind surfaces and positive in free space. Marching cubes reads
	// only the observed region dilated by one voxel, and every voxel there has
	// its nearest observed voxel among its 26 neighbours, so this matches a
	// nearest-observed fill over the whole grid wherever it is read.
	std::vector<float> filled(voxelCount, 1.f);
	std::vector<uint8_t> mask(voxelCount, 0);
	parallelFor(
		m_dims.z,
		[&](int zBegin, int zEnd)
		{
			for (int z= zBegin; z < zEnd; ++z)
			{
				for (int y= 0; y < m_dims.y; ++y)
				{
					for (int x= 0; x < m_dims.x; ++x)
					{
						const size_t index= flatIndex(x, y, z);
						if (keepMask != nullptr && (*keepMask)[index] == 0)
							continue;

						if (m_weights[index] > 0.f)
						{
							filled[index]= m_tsdf[index];
							mask[index]= 1;
							continue;
						}

						for (const NeighbourOffset& offset : neighbourOffsets)
						{
							const int nx= x + offset.dx;
							const int ny= y + offset.dy;
							const int nz= z + offset.dz;
							if (nx < 0 || ny < 0 || nz < 0 || nx >= m_dims.x || ny >= m_dims.y || nz >= m_dims.z)
								continue;

							const size_t neighbourIndex= flatIndex(nx, ny, nz);
							if (m_weights[neighbourIndex] > 0.f)
							{
								filled[index]= TsdfVolumeHelpers::signedDistanceSign(m_tsdf[neighbourIndex]);
								mask[index]= 1;
								break;
							}
						}
					}
				}
			}
		},
		1);

	MarchingCubes::extract(filled.data(), mask.data(), m_dims, m_origin, m_voxelSize, 0.f, outMesh);
	return outMesh.getTriangleCount() > 0;
}

#pragma once

#include "DepthProjection.h"
#include "ReconstructionTypes.h"

#include "glm/ext/matrix_float4x4.hpp"

#include <opencv2/core.hpp>

#include <cstdint>
#include <vector>

// How well a take observed one surface point
enum class eSurfaceCoverage : uint8_t
{
	unseen= 0, // no frame supports it
	weak= 1,   // seen, but failing one of the good thresholds
	good= 2,
};

struct MeshCoverageSettings
{
	// Metres a frame's depth may differ from the point and still support it.
	// The band never drops below bandVoxels voxels of the fused grid.
	float supportBand= 0.015f;
	float bandVoxels= 2.f;
	// A point is good when all four hold
	int minSupport= 8;
	int minDirections= 3;
	float minBestCos= 0.7f;
	float maxCarveShare= 1.f / 3.f; // carve / (support + carve)
	// Supporting directions within this angle of the normal share the
	// frontal bin; the rest fall in eight azimuth bins around the normal
	float frontalDegrees= 30.f;
};

// One fused frame as the coverage pass sees it
struct CoverageView
{
	glm::mat4 cameraToWorld= glm::mat4(1.f); // OpenCV axes, the pose the frame was fused at
	PinholeIntrinsics depthIntrinsics;       // in pixels of depth
	cv::Mat depth;                           // CV_32FC1 metres, 0 where the sensor saw nothing
	cv::Mat confidence;                      // CV_8UC1, 0 to 2
	cv::Mat keep;                            // CV_8UC1, 1 where the fusion's flying pixel cull kept the pixel
	float depthMin= 0.f;                     // the fusion's depth range
	float depthMax= 0.f;
};

// The per-vertex record of the shared coverage metric, one entry per mesh
// vertex in every array
struct MeshCoverageResult
{
	std::vector<uint16_t> support;      // frames whose depth agrees with the point within the band
	std::vector<uint16_t> supportHigh;  // the same at confidence 2
	std::vector<uint16_t> carve;        // frames whose depth lies farther than the point by more than the band
	std::vector<uint8_t> directions;    // distinct direction bins the supporting frames saw it from, 0 to 9
	std::vector<float> bestCos;         // largest cosine between the normal and a supporting camera, 0 when unseen
	std::vector<float> nearest;         // metres to the nearest supporting camera, 0 when unseen
	std::vector<uint8_t> coverageClass; // eSurfaceCoverage
	// Shares of the mesh's surface area in each class, summing to 1
	float areaGood= 0.f;
	float areaWeak= 0.f;
	float areaUnseen= 0.f;

	size_t getVertexCount() const { return coverageClass.size(); }
	bool isEmpty() const { return coverageClass.empty(); }
	void clear() { *this= MeshCoverageResult(); }
	// Re-index onto a mesh whose vertex i was split from vertex sourceVertex[i]
	// of the mesh the record was measured on
	void remap(const std::vector<uint32_t>& sourceVertex);
};

// Measures how well a set of fused frames observed each vertex of a mesh.
// Every vertex is projected into each frame's depth at the pose the frame
// was fused at. A usable pixel (kept by the cull, confidence at least 1,
// inside the depth range) supports the vertex when its depth agrees within
// the band, carves it when it lies farther (the frame says the space is
// empty), and is hidden from it when it lies nearer. A frame behind the
// vertex's surface, its camera on the far side of the normal, counts
// nothing. Frames are folded in one at a time, so only one depth image is
// held at once.
class MeshCoverage
{
public:
	// mesh must outlive the accumulation and carry one normal per vertex.
	// voxelSize is the fused grid's, which widens the band on a coarse grid.
	MeshCoverage(const TriangleMesh& mesh, float voxelSize, const MeshCoverageSettings& settings);

	float getSupportBand() const { return m_band; }

	void accumulateView(const CoverageView& view);

	// Classify every vertex and apportion the surface area, a third of each
	// triangle to each corner
	void finish(MeshCoverageResult& outResult) const;

	static eSurfaceCoverage classify(const MeshCoverageSettings& settings, int support, int carve, int directions,
									 float bestCos);

private:
	const TriangleMesh& m_mesh;
	MeshCoverageSettings m_settings;
	float m_band= 0.f;
	float m_frontalCos= 0.f;
	std::vector<uint16_t> m_support;
	std::vector<uint16_t> m_supportHigh;
	std::vector<uint16_t> m_carve;
	std::vector<uint16_t> m_directionMask; // bit 0 frontal, bits 1 to 8 the azimuth bins
	std::vector<float> m_bestCos;
	std::vector<float> m_nearest;
};

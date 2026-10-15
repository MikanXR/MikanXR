#pragma once

#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"

#include <opencv2/core.hpp>

#include <cstdint>
#include <vector>

// Pinhole intrinsics in pixels of the image they describe
struct PinholeIntrinsics
{
	double fx= 0.0;
	double fy= 0.0;
	double cx= 0.0;
	double cy= 0.0;
};

// Rescale intrinsics given for a fromWidth x fromHeight image to a
// toWidth x toHeight image of the same field of view, which is how the
// colour intrinsics of a take describe its depth frames
PinholeIntrinsics scaleIntrinsics(const PinholeIntrinsics& intrinsics, int fromWidth, int fromHeight, int toWidth,
								  int toHeight);

// A triangle mesh with one position and one normal per vertex. Positions are
// metres in the space the producer names, normals point out of the surface,
// and every triangle is wound counter-clockwise about its outward normal.
// Texcoords are V-up (the OBJ convention) and present only once the mesh has
// been textured.
struct TriangleMesh
{
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec2> texCoords;
	std::vector<uint32_t> indices;

	size_t getTriangleCount() const { return indices.size() / 3; }
	bool hasTexCoords() const { return !vertices.empty() && texCoords.size() == vertices.size(); }
	void clear()
	{
		vertices.clear();
		normals.clear();
		texCoords.clear();
		indices.clear();
	}
};

// The model as seen from one camera: a ray cast of the fused volume.
// All three images share the camera's depth resolution.
struct ModelMap
{
	cv::Mat points;  // CV_32FC3 world-space hit points, undefined where hit is 0
	cv::Mat normals; // CV_32FC3 unit normals out of the surface, undefined where hit is 0
	cv::Mat hit;     // CV_8UC1, 1 where the ray met an observed surface
};

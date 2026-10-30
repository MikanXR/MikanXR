#pragma once

#include "ReconstructionTypes.h"

#include "glm/ext/matrix_float4x4.hpp"

#include <opencv2/core.hpp>

#include <vector>

// One mesh to draw: its vertices are in the mesh's own space and meshToWorld
// carries them into the world the camera pose is expressed in
struct DepthRasterizerMesh
{
	const TriangleMesh* mesh= nullptr;
	glm::mat4 meshToWorld= glm::mat4(1.f);
};

// A CPU z-buffer rasterizer of triangle meshes into an eye-depth map, for
// passes that need the depth a camera would see without a GL context
class DepthRasterizer
{
public:
	// Rasterizes the meshes into an eye-depth map of width x height: CV_32FC1
	// metres along the camera's +Z, +inf where nothing drew. The camera is
	// cameraToWorld in OpenCV axes (+X right, +Y down, +Z forward). intrinsics
	// are in pixels of this image (fx, fy, cx, cy; the caller scales them to
	// the output size). Pixel (u, v) samples the ray through the integer
	// position (u, v), so a vertex lands at x = fx * X / Z + cx.
	//
	// Both windings draw. There is no near clipping: a triangle with any vertex
	// at or behind k_minDepth metres is dropped whole, which suits meshes that
	// sit in front of the camera. A pixel exactly on an edge shared by two
	// triangles is drawn by both and the nearer wins.
	static void render(const std::vector<DepthRasterizerMesh>& meshes, const glm::mat4& cameraToWorld,
					   const PinholeIntrinsics& intrinsics, int width, int height, cv::Mat& outDepth);

	// Camera-space Z at or below which a triangle is skipped
	static constexpr float k_minDepth= 1e-4f;
};

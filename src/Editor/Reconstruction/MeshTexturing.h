#pragma once

#include "ReconstructionTypes.h"

#include "glm/ext/matrix_float4x4.hpp"

#include <opencv2/core.hpp>

#include <cstdint>
#include <vector>

// One camera the texturing can project from: the fused pose, the frame's own
// depth for the visibility test, and its colour frame shrunk to a tile of
// the atlas. Every view's tile must be the same size.
struct TextureView
{
	glm::mat4 cameraToWorld= glm::mat4(1.f); // OpenCV axes (+Y down, +Z forward)
	PinholeIntrinsics depthIntrinsics;       // in pixels of depth
	cv::Mat depth;                           // CV_32FC1 metres, 0 where the sensor saw nothing
	PinholeIntrinsics tileIntrinsics;        // in pixels of tile
	cv::Mat tile;                            // CV_8UC3 BGR
};

struct MeshTexturingSettings
{
	float depthTolerance= 0.05f; // metres a vertex may sit behind the view's depth and still count as seen
	float minFacing= 0.2f;       // floor on the cosine between the face normal and the direction to the camera
	// The floor a triangle no view clears minFacing for falls back to: a
	// stretched glancing view reads better than grey, and only a face turned
	// away from every camera stays grey
	float fallbackMinFacing= 0.05f;
};

struct MeshTexturingStats
{
	int viewsUsed= 0;           // views at least one triangle took
	int trianglesFallback= 0;   // triangles textured from a glancing view under fallbackMinFacing
	int trianglesUntextured= 0; // triangles no view saw, mapped to the grey tile
	int atlasWidth= 0;
	int atlasHeight= 0;
};

// Gives a mesh a rough colour by projecting a handful of a take's video
// frames onto it: the frames become the tiles of one atlas, each triangle
// takes the view that sees it best, and its vertices' texcoords are their
// projections into that tile. A vertex shared by triangles of different
// views is split, so the output has more vertices than the input. Seams
// between views are visible and intended: the goal is a mesh that reads,
// not photogrammetry.
class MeshTexturing
{
public:
	// A view sees a triangle when every vertex projects inside its depth
	// image onto a measured pixel (the farthest of the 3x3 around it, the
	// depth being coarse) no farther than depthTolerance in front of the
	// vertex, and the face normal turns toward the camera by at least
	// minFacing. Among the views that see it, the one most frontal per metre
	// of distance wins. A triangle no view sees is tried again under
	// fallbackMinFacing, and takes the atlas's last tile, flat mid-grey, when
	// that fails too. outMesh may alias mesh. Returns false when the mesh is
	// empty or the tiles disagree in size.
	static bool textureMesh(const TriangleMesh& mesh, const std::vector<TextureView>& views,
							const MeshTexturingSettings& settings, TriangleMesh& outMesh, cv::Mat& outAtlas,
							MeshTexturingStats* outStats= nullptr);

	// The triangles one view sees under the rule above, 1 per triangle in
	// outSeen. Reads only the view's pose, depth intrinsics, and depth, so a
	// candidate view can be scored before its colour frame is decoded.
	static void markSeenTriangles(const TriangleMesh& mesh, const TextureView& view,
								  const MeshTexturingSettings& settings, std::vector<uint8_t>& outSeen);

	// The atlas is a square-ish grid of tiles with one more than the views for
	// the grey tile. Tile i sits at column i % tilesPerRow, row i / tilesPerRow.
	static int tilesPerRow(int viewCount);
};

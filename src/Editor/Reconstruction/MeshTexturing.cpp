#include "MeshTexturing.h"

#include "Logger.h"
#include "ParallelFor.h"

#include "glm/ext/matrix_double4x4.hpp"
#include "glm/geometric.hpp"
#include "glm/matrix.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace MeshTexturingInternal
{
struct ViewFrame
{
	glm::mat4 worldToCamera;
	glm::vec3 cameraPosition;
};

// A vertex's depth along the view's axis and its pixel in the view's depth
// image, false when it falls behind the camera or off the image
bool projectToDepth(const TextureView& view, const ViewFrame& frame, const glm::vec3& worldPoint, float& outDepth,
					int& outU, int& outV)
{
	const glm::vec3 cameraPoint= glm::vec3(frame.worldToCamera * glm::vec4(worldPoint, 1.f));
	if (!(cameraPoint.z > 0.f))
		return false;

	const double u= view.depthIntrinsics.fx * cameraPoint.x / cameraPoint.z + view.depthIntrinsics.cx;
	const double v= view.depthIntrinsics.fy * cameraPoint.y / cameraPoint.z + view.depthIntrinsics.cy;
	outU= (int)std::lround(u);
	outV= (int)std::lround(v);
	outDepth= cameraPoint.z;
	return outU >= 0 && outV >= 0 && outU < view.depth.cols && outV < view.depth.rows;
}

// Whether the view's depth image saw this vertex: a measured pixel under it
// that is not nearer than the vertex by more than the tolerance. The
// farthest of the pixel's 3x3 neighbourhood stands in for the pixel, since
// the depth is coarse and a vertex on a concave edge or a silhouette rounds
// onto the nearer surface beside it.
bool isVertexSeen(const TextureView& view, const ViewFrame& frame, const glm::vec3& worldPoint, float depthTolerance)
{
	float depth= 0.f;
	int u= 0;
	int v= 0;
	if (!projectToDepth(view, frame, worldPoint, depth, u, v))
		return false;

	float farthest= 0.f;
	for (int dv= -1; dv <= 1; ++dv)
	{
		const int row= v + dv;
		if (row < 0 || row >= view.depth.rows)
			continue;
		const float* depthRow= view.depth.ptr<float>(row);
		for (int du= -1; du <= 1; ++du)
		{
			const int column= u + du;
			if (column >= 0 && column < view.depth.cols)
				farthest= std::max(farthest, depthRow[column]);
		}
	}
	return farthest > 0.f && depth <= farthest + depthTolerance;
}

// The vertex's pixel in the view's tile, clamped to the tile
glm::vec2 projectToTile(const TextureView& view, const ViewFrame& frame, const glm::vec3& worldPoint)
{
	const glm::vec3 cameraPoint= glm::vec3(frame.worldToCamera * glm::vec4(worldPoint, 1.f));
	const float z= std::max(cameraPoint.z, 1e-6f);
	const float u= (float)(view.tileIntrinsics.fx * cameraPoint.x / z + view.tileIntrinsics.cx);
	const float v= (float)(view.tileIntrinsics.fy * cameraPoint.y / z + view.tileIntrinsics.cy);
	return glm::vec2(std::clamp(u, 0.f, (float)view.tile.cols - 1.f), std::clamp(v, 0.f, (float)view.tile.rows - 1.f));
}

ViewFrame makeFrame(const TextureView& view)
{
	ViewFrame frame;
	frame.worldToCamera= glm::mat4(glm::inverse(glm::dmat4(view.cameraToWorld)));
	frame.cameraPosition= glm::vec3(view.cameraToWorld[3]);
	return frame;
}

// The face normal and centroid of a triangle, false when it is degenerate
bool triangleFrame(const TriangleMesh& mesh, size_t triangle, glm::vec3& outNormal, glm::vec3& outCentroid)
{
	const glm::vec3& a= mesh.vertices[mesh.indices[triangle * 3 + 0]];
	const glm::vec3& b= mesh.vertices[mesh.indices[triangle * 3 + 1]];
	const glm::vec3& c= mesh.vertices[mesh.indices[triangle * 3 + 2]];
	const glm::vec3 faceNormal= glm::cross(b - a, c - a);
	const float faceLength= glm::length(faceNormal);
	if (!(faceLength > 0.f))
		return false;

	outNormal= faceNormal / faceLength;
	outCentroid= (a + b + c) / 3.f;
	return true;
}

// Whether the view sees the triangle under the facing floor given and the
// visibility rule, with its score (how frontal per metre of distance) when
// it does
bool seesTriangle(const TextureView& view, const ViewFrame& frame, const TriangleMesh& mesh, size_t triangle,
				  const glm::vec3& normal, const glm::vec3& centroid, float minFacing,
				  const MeshTexturingSettings& settings, float& outScore)
{
	const glm::vec3 toCamera= frame.cameraPosition - centroid;
	const float distance= glm::length(toCamera);
	if (!(distance > 0.f))
		return false;
	const float facing= glm::dot(normal, toCamera / distance);
	if (facing < minFacing)
		return false;

	for (int corner= 0; corner < 3; ++corner)
	{
		if (!isVertexSeen(view, frame, mesh.vertices[mesh.indices[triangle * 3 + corner]], settings.depthTolerance))
			return false;
	}

	outScore= facing / std::max(distance, 0.1f);
	return true;
}
} // namespace MeshTexturingInternal

void MeshTexturing::markSeenTriangles(const TriangleMesh& mesh, const TextureView& view,
									  const MeshTexturingSettings& settings, std::vector<uint8_t>& outSeen)
{
	using namespace MeshTexturingInternal;

	const size_t triangleCount= mesh.getTriangleCount();
	outSeen.assign(triangleCount, 0);
	if (view.depth.empty() || view.depth.type() != CV_32FC1)
		return;

	const ViewFrame frame= makeFrame(view);
	parallelFor((int)triangleCount,
				[&](int begin, int end)
				{
					for (int triangle= begin; triangle < end; ++triangle)
					{
						glm::vec3 normal, centroid;
						float score= 0.f;
						if (triangleFrame(mesh, (size_t)triangle, normal, centroid)
							&& seesTriangle(view, frame, mesh, (size_t)triangle, normal, centroid, settings.minFacing,
											settings, score))
						{
							outSeen[triangle]= 1;
						}
					}
				},
				1024);
}

int MeshTexturing::tilesPerRow(int viewCount) { return (int)std::ceil(std::sqrt((double)viewCount + 1.0)); }

bool MeshTexturing::textureMesh(const TriangleMesh& mesh, const std::vector<TextureView>& views,
								const MeshTexturingSettings& settings, TriangleMesh& outMesh, cv::Mat& outAtlas,
								MeshTexturingStats* outStats)
{
	using namespace MeshTexturingInternal;

	const size_t triangleCount= mesh.getTriangleCount();
	const size_t vertexCount= mesh.vertices.size();
	if (triangleCount == 0 || vertexCount == 0)
	{
		MIKAN_LOG_ERROR("MeshTexturing::textureMesh") << "The mesh is empty";
		return false;
	}

	int tileWidth= 0;
	int tileHeight= 0;
	for (const TextureView& view : views)
	{
		if (view.tile.empty() || view.tile.type() != CV_8UC3 || view.depth.empty() || view.depth.type() != CV_32FC1)
		{
			MIKAN_LOG_ERROR("MeshTexturing::textureMesh") << "A view needs a BGR tile and a float depth image";
			return false;
		}
		if (tileWidth == 0)
		{
			tileWidth= view.tile.cols;
			tileHeight= view.tile.rows;
		}
		else if (view.tile.cols != tileWidth || view.tile.rows != tileHeight)
		{
			MIKAN_LOG_ERROR("MeshTexturing::textureMesh") << "The views' tiles differ in size";
			return false;
		}
	}
	if (views.empty())
	{
		// Only the grey tile; sized so the atlas still reads as an image
		tileWidth= 64;
		tileHeight= 64;
	}

	const int viewCount= (int)views.size();
	const int greyTile= viewCount;
	const int columns= tilesPerRow(viewCount);
	const int rows= (viewCount + 1 + columns - 1) / columns;
	const int atlasWidth= columns * tileWidth;
	const int atlasHeight= rows * tileHeight;

	std::vector<ViewFrame> frames(views.size());
	for (size_t viewIndex= 0; viewIndex < views.size(); ++viewIndex)
		frames[viewIndex]= makeFrame(views[viewIndex]);

	// Each triangle picks its view independently, so the triangles split
	// across the threads with no shared writes. The strict facing floor goes
	// first; a triangle it leaves grey takes the best glancing view instead.
	std::vector<int> triangleView(triangleCount, greyTile);
	std::vector<uint8_t> triangleFallback(triangleCount, 0);
	const float facingFloors[2]= {settings.minFacing, std::min(settings.fallbackMinFacing, settings.minFacing)};
	parallelFor((int)triangleCount,
				[&](int begin, int end)
				{
					for (int triangle= begin; triangle < end; ++triangle)
					{
						glm::vec3 normal, centroid;
						if (!triangleFrame(mesh, (size_t)triangle, normal, centroid))
							continue;

						for (int tier= 0; tier < 2 && triangleView[triangle] == greyTile; ++tier)
						{
							float bestScore= 0.f;
							int bestView= greyTile;
							for (int viewIndex= 0; viewIndex < viewCount; ++viewIndex)
							{
								float score= 0.f;
								if (seesTriangle(views[viewIndex], frames[viewIndex], mesh, (size_t)triangle, normal,
												 centroid, facingFloors[tier], settings, score)
									&& score > bestScore)
								{
									bestScore= score;
									bestView= viewIndex;
								}
							}
							triangleView[triangle]= bestView;
							triangleFallback[triangle]= (uint8_t)(tier == 1 && bestView != greyTile);
						}
					}
				},
				1024);

	// Split vertices per (vertex, tile): a vertex seen through two views
	// lands in two places of the atlas
	const bool bHasNormals= mesh.normals.size() == vertexCount;
	TriangleMesh result;
	result.indices.resize(mesh.indices.size());
	std::unordered_map<uint64_t, uint32_t> splitIndex;
	splitIndex.reserve(vertexCount + vertexCount / 4);
	std::vector<int> viewTriangles(views.size() + 1, 0);

	for (size_t triangle= 0; triangle < triangleCount; ++triangle)
	{
		const int tile= triangleView[triangle];
		++viewTriangles[tile];
		const int tileColumn= tile % columns;
		const int tileRow= tile / columns;
		const glm::vec2 tileOrigin((float)(tileColumn * tileWidth), (float)(tileRow * tileHeight));

		for (int corner= 0; corner < 3; ++corner)
		{
			const uint32_t vertex= mesh.indices[triangle * 3 + corner];
			const uint64_t key= ((uint64_t)tile << 32) | vertex;
			auto found= splitIndex.find(key);
			if (found == splitIndex.end())
			{
				const uint32_t newIndex= (uint32_t)result.vertices.size();
				found= splitIndex.emplace(key, newIndex).first;

				result.vertices.push_back(mesh.vertices[vertex]);
				if (bHasNormals)
					result.normals.push_back(mesh.normals[vertex]);

				// The grey tile's centre for an unseen triangle, else the
				// projection; V runs up while image rows run down
				glm::vec2 pixel= tile == greyTile ? glm::vec2((float)tileWidth * 0.5f, (float)tileHeight * 0.5f)
												  : projectToTile(views[tile], frames[tile], mesh.vertices[vertex]);
				pixel+= tileOrigin + glm::vec2(0.5f);
				result.texCoords.push_back(glm::vec2(pixel.x / (float)atlasWidth, 1.f - pixel.y / (float)atlasHeight));
			}
			result.indices[triangle * 3 + corner]= found->second;
		}
	}

	outAtlas= cv::Mat(atlasHeight, atlasWidth, CV_8UC3, cv::Scalar(128, 128, 128));
	for (int viewIndex= 0; viewIndex < viewCount; ++viewIndex)
	{
		const cv::Rect tileRect((viewIndex % columns) * tileWidth, (viewIndex / columns) * tileHeight, tileWidth,
								tileHeight);
		views[viewIndex].tile.copyTo(outAtlas(tileRect));
	}

	if (outStats != nullptr)
	{
		*outStats= MeshTexturingStats();
		for (int viewIndex= 0; viewIndex < viewCount; ++viewIndex)
		{
			if (viewTriangles[viewIndex] > 0)
				++outStats->viewsUsed;
		}
		outStats->trianglesFallback= (int)std::count(triangleFallback.begin(), triangleFallback.end(), (uint8_t)1);
		outStats->trianglesUntextured= viewTriangles[greyTile];
		outStats->atlasWidth= atlasWidth;
		outStats->atlasHeight= atlasHeight;
	}

	outMesh= std::move(result);
	return true;
}

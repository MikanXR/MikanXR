#include "DepthRasterizer.h"

#include "ParallelFor.h"

#include "glm/ext/matrix_double4x4.hpp"
#include "glm/ext/vector_double4.hpp"
#include "glm/matrix.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace DepthRasterizerInternal
{
// Rows below this are not worth a thread of their own
const int k_minRowsPerChunk= 16;
// Slack on the inside test, in square pixels of edge function. The two
// triangles sharing an edge evaluate it with different rounding, and a pixel
// exactly on the edge must not fall through both.
const double k_edgeEpsilon= 1e-6;

// A mesh vertex in camera space and on the image
struct ProjectedVertex
{
	float x= 0.f;
	float y= 0.f;
	float z= 0.f;
	bool bValid= false; // in front of the camera with a finite image position
};

// A triangle ready to scan: image positions, 1 / Z at each corner, and the
// pixel range its bounding box covers inside the image
struct ProjectedTriangle
{
	float x[3];
	float y[3];
	float invZ[3];
	int minX;
	int maxX;
	int minY;
	int maxY;
};

// Edge function of (a, b) at the pixel p, whose sign says which side of a -> b
// the pixel is on and whose magnitude is the edge length times the distance
struct EdgeSetup
{
	double ax;
	double ay;
	double dx;
	double dy;

	EdgeSetup(const ProjectedTriangle& triangle, int from, int to)
		: ax(triangle.x[from])
		, ay(triangle.y[from])
		, dx((double)triangle.x[to] - (double)triangle.x[from])
		, dy((double)triangle.y[to] - (double)triangle.y[from])
	{
	}

	// The part of the edge function that depends on the row only
	double rowTerm(int pixelY) const { return dx * ((double)pixelY - ay); }
	double evaluate(double rowTermValue, int pixelX) const { return rowTermValue - dy * ((double)pixelX - ax); }
};

void projectVertices(const TriangleMesh& mesh, const glm::dmat4& meshToCamera, const PinholeIntrinsics& intrinsics,
					 std::vector<ProjectedVertex>& outVertices)
{
	outVertices.resize(mesh.vertices.size());
	parallelFor((int)mesh.vertices.size(),
				[&](int begin, int end)
				{
					for (int vertex= begin; vertex < end; ++vertex)
					{
						const glm::vec3& position= mesh.vertices[vertex];
						const glm::dvec4 camera= meshToCamera * glm::dvec4(position.x, position.y, position.z, 1.0);
						ProjectedVertex& projected= outVertices[vertex];
						projected.bValid= false;
						if (!(camera.z > (double)DepthRasterizer::k_minDepth))
							continue;

						const double x= intrinsics.fx * camera.x / camera.z + intrinsics.cx;
						const double y= intrinsics.fy * camera.y / camera.z + intrinsics.cy;
						if (!std::isfinite(x) || !std::isfinite(y))
							continue;

						projected.x= (float)x;
						projected.y= (float)y;
						projected.z= (float)camera.z;
						projected.bValid= std::isfinite(projected.x) && std::isfinite(projected.y);
					}
				},
				4096);
}

// Appends the mesh's triangles that can reach the image
void appendProjectedTriangles(const TriangleMesh& mesh, const std::vector<ProjectedVertex>& vertices, int width,
							  int height, std::vector<ProjectedTriangle>& outTriangles)
{
	const size_t vertexCount= vertices.size();
	const size_t triangleCount= mesh.getTriangleCount();
	const float lastColumn= (float)(width - 1);
	const float lastRow= (float)(height - 1);

	for (size_t triangle= 0; triangle < triangleCount; ++triangle)
	{
		const uint32_t* corners= &mesh.indices[triangle * 3];
		if (corners[0] >= vertexCount || corners[1] >= vertexCount || corners[2] >= vertexCount)
			continue;

		const ProjectedVertex* corner[3]= {&vertices[corners[0]], &vertices[corners[1]], &vertices[corners[2]]};
		if (!corner[0]->bValid || !corner[1]->bValid || !corner[2]->bValid)
			continue;

		// The pixels whose positions the bounding box can contain
		const float minX= std::min({corner[0]->x, corner[1]->x, corner[2]->x});
		const float maxX= std::max({corner[0]->x, corner[1]->x, corner[2]->x});
		const float minY= std::min({corner[0]->y, corner[1]->y, corner[2]->y});
		const float maxY= std::max({corner[0]->y, corner[1]->y, corner[2]->y});
		if (maxX < 0.f || minX > lastColumn || maxY < 0.f || minY > lastRow)
			continue;

		ProjectedTriangle projected;
		for (int i= 0; i < 3; ++i)
		{
			projected.x[i]= corner[i]->x;
			projected.y[i]= corner[i]->y;
			projected.invZ[i]= 1.f / corner[i]->z;
		}
		projected.minX= (int)std::ceil(std::max(minX, 0.f));
		projected.maxX= (int)std::floor(std::min(maxX, lastColumn));
		projected.minY= (int)std::ceil(std::max(minY, 0.f));
		projected.maxY= (int)std::floor(std::min(maxY, lastRow));
		if (projected.minX > projected.maxX || projected.minY > projected.maxY)
			continue;

		outTriangles.push_back(projected);
	}
}

// Scan converts one triangle into the rows [rowBegin, rowEnd) of the depth map
void rasterizeTriangle(const ProjectedTriangle& triangle, int rowBegin, int rowEnd, cv::Mat& depth)
{
	// Edge i is opposite corner i, so edge i evaluated at a pixel is the
	// barycentric weight of corner i up to the triangle's area
	const EdgeSetup edge0(triangle, 1, 2);
	const EdgeSetup edge1(triangle, 2, 0);
	const EdgeSetup edge2(triangle, 0, 1);

	// The area's sign is the winding, and a pixel is inside when every weight
	// has that sign or is zero, so both windings draw and edges are inclusive
	const double area= edge2.dx * ((double)triangle.y[2] - edge2.ay) - edge2.dy * ((double)triangle.x[2] - edge2.ax);
	if (!(std::fabs(area) > 0.0))
		return;
	const double winding= area > 0.0 ? 1.0 : -1.0;

	const double invZ0= triangle.invZ[0];
	const double invZ1= triangle.invZ[1];
	const double invZ2= triangle.invZ[2];
	// The interpolated depth stays between the corners' depths
	const float nearDepth= (float)(1.0 / std::max({invZ0, invZ1, invZ2}));
	const float farDepth= (float)(1.0 / std::min({invZ0, invZ1, invZ2}));

	const int firstRow= std::max(triangle.minY, rowBegin);
	const int lastRow= std::min(triangle.maxY, rowEnd - 1);
	for (int y= firstRow; y <= lastRow; ++y)
	{
		const double rowTerm0= edge0.rowTerm(y);
		const double rowTerm1= edge1.rowTerm(y);
		const double rowTerm2= edge2.rowTerm(y);
		float* depthRow= depth.ptr<float>(y);

		for (int x= triangle.minX; x <= triangle.maxX; ++x)
		{
			const double weight0= edge0.evaluate(rowTerm0, x);
			const double weight1= edge1.evaluate(rowTerm1, x);
			const double weight2= edge2.evaluate(rowTerm2, x);
			if (weight0 * winding < -k_edgeEpsilon || weight1 * winding < -k_edgeEpsilon
				|| weight2 * winding < -k_edgeEpsilon)
			{
				continue;
			}

			// 1 / Z is affine in image space, so it interpolates with the
			// barycentric weights and the depth is its reciprocal. The weights
			// share a sign up to the edge slack.
			const double weightSum= weight0 + weight1 + weight2;
			const double weightedInvZ= weight0 * invZ0 + weight1 * invZ1 + weight2 * invZ2;
			if (!(weightSum * winding > 0.0) || !(weightedInvZ * winding > 0.0))
				continue;

			const float pixelDepth= std::clamp((float)(weightSum / weightedInvZ), nearDepth, farDepth);
			if (pixelDepth < depthRow[x])
				depthRow[x]= pixelDepth;
		}
	}
}
} // namespace DepthRasterizerInternal

void DepthRasterizer::render(const std::vector<DepthRasterizerMesh>& meshes, const glm::mat4& cameraToWorld,
							 const PinholeIntrinsics& intrinsics, int width, int height, cv::Mat& outDepth)
{
	using namespace DepthRasterizerInternal;

	if (width <= 0 || height <= 0)
	{
		outDepth.release();
		return;
	}
	outDepth.create(height, width, CV_32FC1);
	outDepth.setTo(cv::Scalar(std::numeric_limits<float>::infinity()));

	// Project every vertex once, then collect the triangles that reach the
	// image. Both buffers are sized per mesh, never per triangle.
	const glm::dmat4 worldToCamera= glm::inverse(glm::dmat4(cameraToWorld));
	std::vector<ProjectedVertex> projectedVertices;
	std::vector<ProjectedTriangle> triangles;
	for (const DepthRasterizerMesh& entry : meshes)
	{
		if (entry.mesh == nullptr || entry.mesh->getTriangleCount() == 0)
			continue;

		const glm::dmat4 meshToCamera= worldToCamera * glm::dmat4(entry.meshToWorld);
		projectVertices(*entry.mesh, meshToCamera, intrinsics, projectedVertices);
		triangles.reserve(triangles.size() + entry.mesh->getTriangleCount());
		appendProjectedTriangles(*entry.mesh, projectedVertices, width, height, triangles);
	}
	if (triangles.empty())
		return;

	// Each chunk is a band of rows. It scans every triangle whose bounding
	// box reaches the band, clipped to the band, and writes only its own rows.
	cv::Mat& depth= outDepth;
	parallelFor(
		height,
		[&](int rowBegin, int rowEnd)
		{
			for (const ProjectedTriangle& triangle : triangles)
			{
				if (triangle.maxY < rowBegin || triangle.minY >= rowEnd)
					continue;

				rasterizeTriangle(triangle, rowBegin, rowEnd, depth);
			}
		},
		k_minRowsPerChunk);
}

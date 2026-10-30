#include "DepthRasterizerTests.h"
#include "DepthRasterizer.h"
#include "OcclusionMeshBuilder.h"
#include "unit_test.h"

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/geometric.hpp"

#include <opencv2/core.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace DepthRasterizerTestsInternal
{
const int k_imageWidth= 64;
const int k_imageHeight= 48;
const double k_focalLength= 50.0;
const double k_centerX= 32.0;
const double k_centerY= 24.0;
const float k_infinity= std::numeric_limits<float>::infinity();
const float k_pi= 3.14159265358979f;

PinholeIntrinsics makeIntrinsics()
{
	PinholeIntrinsics intrinsics;
	intrinsics.fx= k_focalLength;
	intrinsics.fy= k_focalLength;
	intrinsics.cx= k_centerX;
	intrinsics.cy= k_centerY;
	return intrinsics;
}

glm::mat4 makeTranslation(const glm::vec3& offset)
{
	glm::mat4 transform(1.f);
	transform[3]= glm::vec4(offset, 1.f);
	return transform;
}

// Camera-to-world in OpenCV axes (+X right, +Y down, +Z forward) from its
// three axes and position
glm::mat4 makeCameraToWorld(const glm::vec3& right, const glm::vec3& down, const glm::vec3& forward,
							const glm::vec3& position)
{
	glm::mat4 cameraToWorld(1.f);
	cameraToWorld[0]= glm::vec4(right, 0.f);
	cameraToWorld[1]= glm::vec4(down, 0.f);
	cameraToWorld[2]= glm::vec4(forward, 0.f);
	cameraToWorld[3]= glm::vec4(position, 1.f);
	return cameraToWorld;
}

// Appends the quad a, b, c, d (in loop order) as two triangles
void appendQuad(TriangleMesh& mesh, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d)
{
	const uint32_t first= (uint32_t)mesh.vertices.size();
	mesh.vertices.push_back(a);
	mesh.vertices.push_back(b);
	mesh.vertices.push_back(c);
	mesh.vertices.push_back(d);
	mesh.indices.insert(mesh.indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
}

// A quad of half extents halfX x halfY about center, in the plane of center's Z
TriangleMesh makeAxisQuad(const glm::vec3& center, float halfX, float halfY)
{
	TriangleMesh mesh;
	appendQuad(mesh, center + glm::vec3(-halfX, -halfY, 0.f), center + glm::vec3(halfX, -halfY, 0.f),
			   center + glm::vec3(halfX, halfY, 0.f), center + glm::vec3(-halfX, halfY, 0.f));
	return mesh;
}

// The same triangles wound the other way
TriangleMesh reverseWinding(const TriangleMesh& mesh)
{
	TriangleMesh reversed= mesh;
	for (size_t i= 0; i + 2 < reversed.indices.size(); i+= 3)
		std::swap(reversed.indices[i + 1], reversed.indices[i + 2]);
	return reversed;
}

cv::Mat renderMeshes(const std::vector<DepthRasterizerMesh>& meshes, const glm::mat4& cameraToWorld)
{
	cv::Mat depth;
	DepthRasterizer::render(meshes, cameraToWorld, makeIntrinsics(), k_imageWidth, k_imageHeight, depth);
	return depth;
}

cv::Mat renderMesh(const TriangleMesh& mesh, const glm::mat4& cameraToWorld= glm::mat4(1.f),
				   const glm::mat4& meshToWorld= glm::mat4(1.f))
{
	DepthRasterizerMesh entry;
	entry.mesh= &mesh;
	entry.meshToWorld= meshToWorld;
	return renderMeshes(std::vector<DepthRasterizerMesh>{entry}, cameraToWorld);
}

bool hasExpectedLayout(const cv::Mat& depth)
{
	return depth.type() == CV_32FC1 && depth.cols == k_imageWidth && depth.rows == k_imageHeight;
}

// Pixels at which two depth maps of the same layout differ
int countDifferences(const cv::Mat& a, const cv::Mat& b)
{
	int differences= 0;
	for (int v= 0; v < a.rows; ++v)
	{
		for (int u= 0; u < a.cols; ++u)
		{
			if (a.at<float>(v, u) != b.at<float>(v, u))
				++differences;
		}
	}
	return differences;
}

int countDrawn(const cv::Mat& depth)
{
	int drawn= 0;
	for (int v= 0; v < depth.rows; ++v)
	{
		for (int u= 0; u < depth.cols; ++u)
		{
			if (std::isfinite(depth.at<float>(v, u)))
				++drawn;
		}
	}
	return drawn;
}

struct PixelBounds
{
	int minU= k_imageWidth;
	int maxU= -1;
	int minV= k_imageHeight;
	int maxV= -1;
};

PixelBounds getDrawnBounds(const cv::Mat& depth)
{
	PixelBounds bounds;
	for (int v= 0; v < depth.rows; ++v)
	{
		for (int u= 0; u < depth.cols; ++u)
		{
			if (std::isfinite(depth.at<float>(v, u)))
			{
				bounds.minU= std::min(bounds.minU, u);
				bounds.maxU= std::max(bounds.maxU, u);
				bounds.minV= std::min(bounds.minV, v);
				bounds.maxV= std::max(bounds.maxV, v);
			}
		}
	}
	return bounds;
}

// Whether the drawn bounds sit within one pixel of the expected pixel range,
// printing the miss
bool boundsMatch(const PixelBounds& bounds, int minU, int maxU, int minV, int maxV)
{
	const bool bMatch= std::abs(bounds.minU - minU) <= 1 && std::abs(bounds.maxU - maxU) <= 1
					   && std::abs(bounds.minV - minV) <= 1 && std::abs(bounds.maxV - maxV) <= 1;
	if (!bMatch)
	{
		fprintf(stdout, "    drawn pixels u %d..%d v %d..%d, expected u %d..%d v %d..%d\n", bounds.minU, bounds.maxU,
				bounds.minV, bounds.maxV, minU, maxU, minV, maxV);
	}
	return bMatch;
}

// A square at a constant depth facing the camera, with a test of whether a
// pixel's ray meets it
struct FrontoQuad
{
	float depth;
	float centerX;
	float centerY;
	float halfExtent;

	TriangleMesh makeMesh() const { return makeAxisQuad(glm::vec3(centerX, centerY, depth), halfExtent, halfExtent); }

	// 1 when the pixel's ray is inside the square by margin metres, -1 when
	// it is outside by margin, 0 near the border
	int classify(int u, int v, float margin) const
	{
		const float x= (float)(((double)u - k_centerX) / k_focalLength) * depth;
		const float y= (float)(((double)v - k_centerY) / k_focalLength) * depth;
		const float distance= std::max(std::fabs(x - centerX), std::fabs(y - centerY));
		if (distance < halfExtent - margin)
			return 1;
		if (distance > halfExtent + margin)
			return -1;
		return 0;
	}
};
} // namespace DepthRasterizerTestsInternal

// ---- Test cases ----

// A centred quad of two triangles draws its depth across the interior with no
// crack along the diagonal, nothing outside, and lands its edges on
// x = fx * X / Z + cx. Reversing the winding changes nothing.
bool depth_rasterizer_test_fronto_quad()
{
	UNIT_TEST_BEGIN("fronto-parallel quad draws its depth inside its projected edges")

	using namespace DepthRasterizerTestsInternal;

	const float quadDepth= 2.f;
	const float halfExtent= 0.4f;
	const int edgePixels= (int)std::lround(k_focalLength * halfExtent / quadDepth); // 10
	const int minU= (int)k_centerX - edgePixels;
	const int maxU= (int)k_centerX + edgePixels;
	const int minV= (int)k_centerY - edgePixels;
	const int maxV= (int)k_centerY + edgePixels;

	const TriangleMesh quad= makeAxisQuad(glm::vec3(0.f, 0.f, quadDepth), halfExtent, halfExtent);
	const cv::Mat depth= renderMesh(quad);
	if (!hasExpectedLayout(depth))
	{
		fprintf(stdout, "    depth map is %dx%d type %d\n", depth.cols, depth.rows, depth.type());
		success= false;
	}
	else
	{
		const float centerDepth= depth.at<float>((int)k_centerY, (int)k_centerX);
		if (!(std::fabs(centerDepth - quadDepth) <= 1e-4f))
		{
			fprintf(stdout, "    centre pixel depth %.5f, expected %.5f\n", centerDepth, quadDepth);
			success= false;
		}

		int interiorMisses= 0;
		int outsideDrawn= 0;
		for (int v= 0; v < k_imageHeight; ++v)
		{
			for (int u= 0; u < k_imageWidth; ++u)
			{
				const float pixelDepth= depth.at<float>(v, u);
				const bool bInterior= u > minU && u < maxU && v > minV && v < maxV;
				const bool bOutside= u < minU || u > maxU || v < minV || v > maxV;
				if (bInterior && !(std::fabs(pixelDepth - quadDepth) <= 1e-4f))
					++interiorMisses;
				if (bOutside && pixelDepth != k_infinity)
					++outsideDrawn;
			}
		}
		if (interiorMisses > 0 || outsideDrawn > 0)
		{
			fprintf(stdout, "    %d interior pixels missed the quad, %d outside pixels drew\n", interiorMisses,
					outsideDrawn);
			success= false;
		}

		success&= boundsMatch(getDrawnBounds(depth), minU, maxU, minV, maxV);

		const cv::Mat reversedDepth= renderMesh(reverseWinding(quad));
		if (!hasExpectedLayout(reversedDepth) || countDifferences(depth, reversedDepth) != 0)
		{
			fprintf(stdout, "    the reversed winding drew a different depth map\n");
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

// Where two fronto-parallel quads overlap the nearer one wins, and a pixel
// only the far one covers reads the far depth.
bool depth_rasterizer_test_nearer_wins()
{
	UNIT_TEST_BEGIN("the nearer of two overlapping quads wins")

	using namespace DepthRasterizerTestsInternal;

	const FrontoQuad nearQuad= {1.5f, 0.f, 0.f, 0.3f};
	const FrontoQuad farQuad= {2.5f, 0.25f, 0.f, 0.5f};
	const TriangleMesh nearMesh= nearQuad.makeMesh();
	const TriangleMesh farMesh= farQuad.makeMesh();

	// Both orders, so the near quad must overwrite the far one and not merely
	// be drawn first
	DepthRasterizerMesh farEntry;
	farEntry.mesh= &farMesh;
	DepthRasterizerMesh nearEntry;
	nearEntry.mesh= &nearMesh;
	const cv::Mat depth= renderMeshes(std::vector<DepthRasterizerMesh>{farEntry, nearEntry}, glm::mat4(1.f));
	const cv::Mat swappedDepth= renderMeshes(std::vector<DepthRasterizerMesh>{nearEntry, farEntry}, glm::mat4(1.f));
	if (!hasExpectedLayout(depth) || !hasExpectedLayout(swappedDepth))
	{
		fprintf(stdout, "    depth map has the wrong layout\n");
		success= false;
	}
	else
	{
		const float margin= 0.02f;
		int wrongPixels= 0;
		int overlapPixels= 0;
		int farOnlyPixels= 0;
		for (int v= 0; v < k_imageHeight; ++v)
		{
			for (int u= 0; u < k_imageWidth; ++u)
			{
				const int nearSide= nearQuad.classify(u, v, margin);
				const int farSide= farQuad.classify(u, v, margin);
				float expected= 0.f;
				if (nearSide > 0)
				{
					expected= nearQuad.depth;
					if (farSide > 0)
						++overlapPixels;
				}
				else if (nearSide < 0 && farSide > 0)
				{
					expected= farQuad.depth;
					++farOnlyPixels;
				}
				else if (nearSide < 0 && farSide < 0)
				{
					expected= k_infinity;
				}
				else
				{
					continue; // on a border
				}

				const float actual= depth.at<float>(v, u);
				if (actual != expected && !(std::fabs(actual - expected) <= 1e-4f))
					++wrongPixels;
			}
		}
		if (wrongPixels > 0 || overlapPixels == 0 || farOnlyPixels == 0)
		{
			fprintf(stdout, "    %d wrong pixels (%d overlapped, %d far only)\n", wrongPixels, overlapPixels,
					farOnlyPixels);
			success= false;
		}

		if (countDifferences(depth, swappedDepth) != 0)
		{
			fprintf(stdout, "    the result depends on the order of the meshes\n");
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

// A quad tilted 45 degrees about the camera's X axis reads the depth of the
// plane at each pixel, which perspective-correct interpolation reproduces.
bool depth_rasterizer_test_tilted_quad()
{
	UNIT_TEST_BEGIN("tilted quad matches the ray-plane intersection")

	using namespace DepthRasterizerTestsInternal;

	const float angle= k_pi * 0.25f;
	const float sine= std::sin(angle);
	const float cosine= std::cos(angle);
	const glm::vec3 planeOrigin(0.f, 0.f, 2.f);
	const glm::vec3 planeNormal(0.f, -sine, cosine);
	const float halfExtent= 0.5f;

	// The quad's local (x, y, 0) turned about X by the angle, about planeOrigin
	auto tilted= [&](float x, float y) { return planeOrigin + glm::vec3(x, y * cosine, y * sine); };
	TriangleMesh quad;
	appendQuad(quad, tilted(-halfExtent, -halfExtent), tilted(halfExtent, -halfExtent), tilted(halfExtent, halfExtent),
			   tilted(-halfExtent, halfExtent));

	const cv::Mat depth= renderMesh(quad);
	if (!hasExpectedLayout(depth))
	{
		fprintf(stdout, "    depth map has the wrong layout\n");
		success= false;
	}
	else
	{
		const int pixels[5][2]= {{32, 24}, {32, 19}, {32, 28}, {27, 25}, {37, 22}};
		for (const auto& pixel : pixels)
		{
			const float dx= (float)((pixel[0] - k_centerX) / k_focalLength);
			const float dy= (float)((pixel[1] - k_centerY) / k_focalLength);
			const glm::vec3 ray(dx, dy, 1.f);
			const float expected= glm::dot(planeNormal, planeOrigin) / glm::dot(planeNormal, ray);

			// The pixel must lie on the quad for the comparison to mean anything
			const glm::vec3 hit= ray * expected - planeOrigin;
			if (std::fabs(hit.x) > halfExtent * 0.9f || std::fabs(hit.y / cosine) > halfExtent * 0.9f)
			{
				fprintf(stdout, "    test pixel (%d, %d) is not on the quad\n", pixel[0], pixel[1]);
				success= false;
				continue;
			}

			const float actual= depth.at<float>(pixel[1], pixel[0]);
			if (!(std::fabs(actual - expected) <= 1e-3f))
			{
				fprintf(stdout, "    pixel (%d, %d) depth %.5f, expected %.5f\n", pixel[0], pixel[1], actual, expected);
				success= false;
			}
		}
	}

	UNIT_TEST_COMPLETE()
}

// A recorded ARKit pose looks down -Z with +Y up. Through the single
// conversion to OpenCV axes a quad ahead of it draws at its distance, with
// world +Y up the image and +X to the right, and a quad behind draws nothing.
bool depth_rasterizer_test_arkit_pose()
{
	UNIT_TEST_BEGIN("an ARKit pose sees what is ahead of it and not what is behind")

	using namespace DepthRasterizerTestsInternal;

	const glm::mat4 cameraToWorld= OcclusionMeshBuilder::toOpenCvCameraToWorld(glm::mat4(1.f));

	// 0.4 m up and right at 2 m ahead: 10 pixels up and right of the centre
	const TriangleMesh ahead= makeAxisQuad(glm::vec3(0.4f, 0.4f, -2.f), 0.1f, 0.1f);
	const cv::Mat aheadDepth= renderMesh(ahead, cameraToWorld);
	if (!hasExpectedLayout(aheadDepth))
	{
		fprintf(stdout, "    depth map has the wrong layout\n");
		success= false;
	}
	else
	{
		const float drawn= aheadDepth.at<float>(14, 42);
		if (!(std::fabs(drawn - 2.f) <= 1e-3f))
		{
			fprintf(stdout, "    pixel (42, 14) depth %.5f, expected 2\n", drawn);
			success= false;
		}
		const int mirroredPixels[3][2]= {{22, 14}, {42, 34}, {22, 34}};
		for (const auto& pixel : mirroredPixels)
		{
			if (aheadDepth.at<float>(pixel[1], pixel[0]) != k_infinity)
			{
				fprintf(stdout, "    pixel (%d, %d) drew, the quad is up and right only\n", pixel[0], pixel[1]);
				success= false;
			}
		}
	}

	const TriangleMesh behind= makeAxisQuad(glm::vec3(0.f, 0.f, 2.f), 0.5f, 0.5f);
	const cv::Mat behindDepth= renderMesh(behind, cameraToWorld);
	if (!hasExpectedLayout(behindDepth) || countDrawn(behindDepth) != 0)
	{
		fprintf(stdout, "    the quad behind the camera drew %d pixels\n", countDrawn(behindDepth));
		success= false;
	}

	UNIT_TEST_COMPLETE()
}

// A camera moved and yawed 30 degrees sees a world-space quad where the
// analytic projection of its corners says, at the depth the ray through each
// sampled pixel meets its plane.
bool depth_rasterizer_test_moved_camera()
{
	UNIT_TEST_BEGIN("translated and yawed camera agrees with the analytic projection")

	using namespace DepthRasterizerTestsInternal;

	const float yaw= k_pi / 6.f;
	const glm::vec3 right(std::cos(yaw), 0.f, -std::sin(yaw));
	const glm::vec3 down(0.f, 1.f, 0.f);
	const glm::vec3 forward(std::sin(yaw), 0.f, std::cos(yaw));
	const glm::vec3 position(1.f, 0.5f, 0.f);
	const glm::mat4 cameraToWorld= makeCameraToWorld(right, down, forward, position);

	// A quad in a world XY plane, 3 m along the optical axis from the camera
	const glm::vec3 center= position + 3.f * forward;
	const float halfExtent= 0.4f;
	const TriangleMesh quad= makeAxisQuad(center, halfExtent, halfExtent);
	const cv::Mat depth= renderMesh(quad, cameraToWorld);
	if (!hasExpectedLayout(depth))
	{
		fprintf(stdout, "    depth map has the wrong layout\n");
		success= false;
	}
	else
	{
		double minX= 1e9;
		double maxX= -1e9;
		double minY= 1e9;
		double maxY= -1e9;
		for (const glm::vec3& corner : quad.vertices)
		{
			const glm::vec3 relative= corner - position;
			const double z= glm::dot(relative, forward);
			const double x= k_focalLength * glm::dot(relative, right) / z + k_centerX;
			const double y= k_focalLength * glm::dot(relative, down) / z + k_centerY;
			minX= std::min(minX, x);
			maxX= std::max(maxX, x);
			minY= std::min(minY, y);
			maxY= std::max(maxY, y);
		}
		success&= boundsMatch(getDrawnBounds(depth), (int)std::ceil(minX), (int)std::floor(maxX), (int)std::ceil(minY),
							  (int)std::floor(maxY));

		const int pixels[5][2]= {{32, 24}, {30, 22}, {35, 26}, {29, 27}, {34, 21}};
		for (const auto& pixel : pixels)
		{
			const float dx= (float)((pixel[0] - k_centerX) / k_focalLength);
			const float dy= (float)((pixel[1] - k_centerY) / k_focalLength);
			const glm::vec3 worldRay= right * dx + down * dy + forward;
			const float expected= (center.z - position.z) / worldRay.z;

			const glm::vec3 hit= position + worldRay * expected;
			if (std::fabs(hit.x - center.x) > halfExtent * 0.9f || std::fabs(hit.y - center.y) > halfExtent * 0.9f)
			{
				fprintf(stdout, "    test pixel (%d, %d) is not on the quad\n", pixel[0], pixel[1]);
				success= false;
				continue;
			}

			const float actual= depth.at<float>(pixel[1], pixel[0]);
			if (!(std::fabs(actual - expected) <= 1e-3f))
			{
				fprintf(stdout, "    pixel (%d, %d) depth %.5f, expected %.5f\n", pixel[0], pixel[1], actual, expected);
				success= false;
			}
		}

		const float centerDepth= depth.at<float>((int)k_centerY, (int)k_centerX);
		if (!(std::fabs(centerDepth - 3.f) <= 1e-3f))
		{
			fprintf(stdout, "    centre pixel depth %.5f, expected 3\n", centerDepth);
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

// meshToWorld moves a mesh built about its own origin, turns it, and one mesh
// listed twice with different transforms draws twice.
bool depth_rasterizer_test_mesh_to_world()
{
	UNIT_TEST_BEGIN("meshToWorld places the mesh where its transformed vertices predict")

	using namespace DepthRasterizerTestsInternal;

	const float halfExtent= 0.3f;
	const TriangleMesh quad= makeAxisQuad(glm::vec3(0.f), halfExtent, halfExtent);

	// 2.5 m ahead, 0.4 m right and 0.2 m up of the optical axis: centred on
	// pixel (40, 20) with a half extent of 6 pixels
	const glm::mat4 firstTransform= makeTranslation(glm::vec3(0.4f, -0.2f, 2.5f));
	const cv::Mat singleDepth= renderMesh(quad, glm::mat4(1.f), firstTransform);
	if (!hasExpectedLayout(singleDepth))
	{
		fprintf(stdout, "    depth map has the wrong layout\n");
		success= false;
	}
	else
	{
		const float centerDepth= singleDepth.at<float>(20, 40);
		if (!(std::fabs(centerDepth - 2.5f) <= 1e-3f))
		{
			fprintf(stdout, "    pixel (40, 20) depth %.5f, expected 2.5\n", centerDepth);
			success= false;
		}
		if (singleDepth.at<float>((int)k_centerY, (int)k_centerX) != k_infinity)
		{
			fprintf(stdout, "    the untransformed position drew\n");
			success= false;
		}
		success&= boundsMatch(getDrawnBounds(singleDepth), 34, 46, 14, 26);
	}

	// A second instance 2 m ahead, 0.4 m left and 0.2 m below the axis:
	// centred on pixel (22, 29) with a half extent of 7.5 pixels
	DepthRasterizerMesh first;
	first.mesh= &quad;
	first.meshToWorld= firstTransform;
	DepthRasterizerMesh second;
	second.mesh= &quad;
	second.meshToWorld= makeTranslation(glm::vec3(-0.4f, 0.2f, 2.f));
	const cv::Mat pairDepth= renderMeshes(std::vector<DepthRasterizerMesh>{first, second}, glm::mat4(1.f));
	if (!hasExpectedLayout(pairDepth))
	{
		fprintf(stdout, "    depth map has the wrong layout\n");
		success= false;
	}
	else
	{
		const float firstDepth= pairDepth.at<float>(20, 40);
		const float secondDepth= pairDepth.at<float>(29, 22);
		if (!(std::fabs(firstDepth - 2.5f) <= 1e-3f) || !(std::fabs(secondDepth - 2.f) <= 1e-3f))
		{
			fprintf(stdout, "    instance depths %.5f and %.5f, expected 2.5 and 2\n", firstDepth, secondDepth);
			success= false;
		}
	}

	// A half turn about Z then a 2 m move ahead carries the mesh point
	// (0.2, 0.1, 0) to (-0.2, -0.1, 2), which lands on pixel (27, 21.5)
	glm::mat4 halfTurn(1.f);
	halfTurn[0]= glm::vec4(-1.f, 0.f, 0.f, 0.f);
	halfTurn[1]= glm::vec4(0.f, -1.f, 0.f, 0.f);
	halfTurn[3]= glm::vec4(0.f, 0.f, 2.f, 1.f);
	const TriangleMesh smallQuad= makeAxisQuad(glm::vec3(0.2f, 0.1f, 0.f), 0.1f, 0.1f);
	const cv::Mat turnedDepth= renderMesh(smallQuad, glm::mat4(1.f), halfTurn);
	if (!hasExpectedLayout(turnedDepth))
	{
		fprintf(stdout, "    depth map has the wrong layout\n");
		success= false;
	}
	else if (!(std::fabs(turnedDepth.at<float>(22, 27) - 2.f) <= 1e-3f) || turnedDepth.at<float>(26, 37) != k_infinity)
	{
		fprintf(stdout, "    the turned mesh is not at pixel (27, 22)\n");
		success= false;
	}

	UNIT_TEST_COMPLETE()
}

// A grid of irregular cells with alternating diagonals and windings leaves no
// crack along any shared edge, and triangles that reach behind the camera or
// miss the image draw nothing.
bool depth_rasterizer_test_grid_and_culling()
{
	UNIT_TEST_BEGIN("a shared-edge grid has no cracks and culled triangles draw nothing")

	using namespace DepthRasterizerTestsInternal;

	const int cells= 16;
	const float half= 0.5f;
	const float quadDepth= 2.f;
	TriangleMesh grid;
	for (int j= 0; j <= cells; ++j)
	{
		for (int i= 0; i <= cells; ++i)
		{
			// Interior vertices shifted off the lattice, the border kept straight
			const bool bInterior= i > 0 && i < cells && j > 0 && j < cells;
			const float shiftX= bInterior ? 0.011f * std::sin(1.7f * (float)(i * 7 + j * 3)) : 0.f;
			const float shiftY= bInterior ? 0.011f * std::cos(2.3f * (float)(i * 5 + j * 11)) : 0.f;
			grid.vertices.push_back(glm::vec3(-half + 2.f * half * (float)i / (float)cells + shiftX,
											  -half + 2.f * half * (float)j / (float)cells + shiftY, quadDepth));
		}
	}
	const uint32_t rowLength= (uint32_t)cells + 1;
	for (int j= 0; j < cells; ++j)
	{
		for (int i= 0; i < cells; ++i)
		{
			const uint32_t i00= (uint32_t)j * rowLength + (uint32_t)i;
			const uint32_t i10= i00 + 1;
			const uint32_t i01= i00 + rowLength;
			const uint32_t i11= i01 + 1;
			if (((i + j) & 1) == 0)
				grid.indices.insert(grid.indices.end(), {i00, i10, i11, i00, i11, i01});
			else
				grid.indices.insert(grid.indices.end(), {i00, i11, i10, i00, i01, i11});
		}
	}

	const cv::Mat gridDepth= renderMesh(grid);
	if (!hasExpectedLayout(gridDepth))
	{
		fprintf(stdout, "    depth map has the wrong layout\n");
		success= false;
	}
	else
	{
		// The grid's straight border sits 12.5 pixels from the centre
		int missed= 0;
		int outside= 0;
		for (int v= 0; v < k_imageHeight; ++v)
		{
			for (int u= 0; u < k_imageWidth; ++u)
			{
				const bool bInterior= std::abs(u - (int)k_centerX) <= 12 && std::abs(v - (int)k_centerY) <= 12;
				const bool bOutside= std::abs(u - (int)k_centerX) >= 14 || std::abs(v - (int)k_centerY) >= 14;
				const float pixelDepth= gridDepth.at<float>(v, u);
				if (bInterior && !(std::fabs(pixelDepth - quadDepth) <= 1e-4f))
					++missed;
				if (bOutside && pixelDepth != k_infinity)
					++outside;
			}
		}
		if (missed > 0 || outside > 0)
		{
			fprintf(stdout, "    %d interior pixels not drawn, %d exterior pixels drawn\n", missed, outside);
			success= false;
		}
	}

	// One with a corner behind the camera, one with a corner on the camera
	// plane, and one wholly off the image
	TriangleMesh culled;
	culled.vertices.push_back(glm::vec3(0.f, 0.f, 2.f));
	culled.vertices.push_back(glm::vec3(0.5f, 0.f, 2.f));
	culled.vertices.push_back(glm::vec3(0.f, 0.5f, -1.f));
	culled.vertices.push_back(glm::vec3(0.f, 0.f, 1.f));
	culled.vertices.push_back(glm::vec3(0.2f, 0.f, 1.f));
	culled.vertices.push_back(glm::vec3(0.f, 0.2f, 0.f));
	culled.vertices.push_back(glm::vec3(5.f, 5.f, 1.f));
	culled.vertices.push_back(glm::vec3(5.2f, 5.f, 1.f));
	culled.vertices.push_back(glm::vec3(5.f, 5.2f, 1.f));
	for (uint32_t index= 0; index < 9; ++index)
		culled.indices.push_back(index);
	const cv::Mat culledDepth= renderMesh(culled);
	if (!hasExpectedLayout(culledDepth) || countDrawn(culledDepth) != 0)
	{
		fprintf(stdout, "    culled triangles drew %d pixels\n", countDrawn(culledDepth));
		success= false;
	}

	UNIT_TEST_COMPLETE()
}

// ---- Test Suite ----

bool run_depth_rasterizer_unit_tests()
{
	UNIT_TEST_MODULE_BEGIN("depth_rasterizer")
	UNIT_TEST_MODULE_CALL_TEST(depth_rasterizer_test_fronto_quad);
	UNIT_TEST_MODULE_CALL_TEST(depth_rasterizer_test_nearer_wins);
	UNIT_TEST_MODULE_CALL_TEST(depth_rasterizer_test_tilted_quad);
	UNIT_TEST_MODULE_CALL_TEST(depth_rasterizer_test_arkit_pose);
	UNIT_TEST_MODULE_CALL_TEST(depth_rasterizer_test_moved_camera);
	UNIT_TEST_MODULE_CALL_TEST(depth_rasterizer_test_mesh_to_world);
	UNIT_TEST_MODULE_CALL_TEST(depth_rasterizer_test_grid_and_culling);
	UNIT_TEST_MODULE_END()
}

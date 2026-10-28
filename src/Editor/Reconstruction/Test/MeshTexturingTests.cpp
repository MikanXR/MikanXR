#include "MeshTexturingTests.h"
#include "MeshCleanup.h"
#include "MeshTexturing.h"
#include "unit_test.h"

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/geometric.hpp"
#include "glm/matrix.hpp"

#include <opencv2/core.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace MeshTexturingTestsInternal
{
const int k_depthWidth= 64;
const int k_depthHeight= 48;
const int k_tileWidth= 32;
const int k_tileHeight= 24;
const double k_depthFocal= 40.0;
// The rendered surfaces reach this far past the mesh quads, as a real
// surface reaches past the patch of it a mesh covers, so a corner's ray
// still lands on it after pixel rounding
const float k_surfaceMargin= 0.1f;

// Camera-to-world in OpenCV axes for a camera at eye looking at target
glm::mat4 lookAtOpenCV(const glm::vec3& eye, const glm::vec3& target)
{
	const glm::vec3 forward= glm::normalize(target - eye);
	glm::vec3 up(0.f, 1.f, 0.f);
	if (std::fabs(glm::dot(forward, up)) > 0.99f)
		up= glm::vec3(0.f, 0.f, 1.f);
	const glm::vec3 right= glm::normalize(glm::cross(forward, up));
	const glm::vec3 down= glm::cross(forward, right);

	glm::mat4 cameraToWorld(1.f);
	cameraToWorld[0]= glm::vec4(right, 0.f);
	cameraToWorld[1]= glm::vec4(down, 0.f);
	cameraToWorld[2]= glm::vec4(forward, 0.f);
	cameraToWorld[3]= glm::vec4(eye, 1.f);
	return cameraToWorld;
}

PinholeIntrinsics depthIntrinsics()
{
	PinholeIntrinsics intrinsics;
	intrinsics.fx= k_depthFocal;
	intrinsics.fy= k_depthFocal;
	intrinsics.cx= (k_depthWidth - 1) * 0.5;
	intrinsics.cy= (k_depthHeight - 1) * 0.5;
	return intrinsics;
}

// An axis-aligned rectangle in the plane axis = value, spanning lo to hi on
// the other two axes, as the depth renderer sees it
struct Surface
{
	int axis;
	float value;
	glm::vec3 lo;
	glm::vec3 hi;
};

Surface makeSurface(int axis, float value, const glm::vec3& lo, const glm::vec3& hi)
{
	Surface surface;
	surface.axis= axis;
	surface.value= value;
	surface.lo= lo - glm::vec3(k_surfaceMargin);
	surface.hi= hi + glm::vec3(k_surfaceMargin);
	return surface;
}

// An axis-aligned quad as two triangles over four fresh vertices, with the
// winding that puts its normal along outwardNormal
void appendQuad(TriangleMesh& mesh, const glm::vec3& corner, const glm::vec3& edgeA, const glm::vec3& edgeB,
				const glm::vec3& outwardNormal)
{
	const uint32_t first= (uint32_t)mesh.vertices.size();
	mesh.vertices.push_back(corner);
	mesh.vertices.push_back(corner + edgeA);
	mesh.vertices.push_back(corner + edgeA + edgeB);
	mesh.vertices.push_back(corner + edgeB);
	for (int i= 0; i < 4; ++i)
		mesh.normals.push_back(outwardNormal);

	if (glm::dot(glm::cross(edgeA, edgeB), outwardNormal) > 0.f)
		mesh.indices.insert(mesh.indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
	else
		mesh.indices.insert(mesh.indices.end(), {first, first + 2, first + 1, first, first + 3, first + 2});
}

// Exact depth of the surfaces from a camera: the nearest hit along each
// pixel's ray, as the camera-Z distance a sensor reports, 0 where nothing
// is hit
void renderDepth(const glm::mat4& cameraToWorld, const PinholeIntrinsics& intrinsics,
				 const std::vector<Surface>& surfaces, cv::Mat& outDepth)
{
	outDepth= cv::Mat(k_depthHeight, k_depthWidth, CV_32FC1, cv::Scalar(0.f));
	const glm::vec3 eye(cameraToWorld[3]);
	for (int v= 0; v < k_depthHeight; ++v)
	{
		for (int u= 0; u < k_depthWidth; ++u)
		{
			const glm::vec3 cameraDirection((float)((u - intrinsics.cx) / intrinsics.fx),
											(float)((v - intrinsics.cy) / intrinsics.fy), 1.f);
			const glm::vec3 direction= glm::mat3(cameraToWorld) * cameraDirection;

			float nearest= 0.f;
			for (const Surface& surface : surfaces)
			{
				if (std::fabs(direction[surface.axis]) < 1e-6f)
					continue;
				const float t= (surface.value - eye[surface.axis]) / direction[surface.axis];
				if (t <= 0.f || (nearest > 0.f && t >= nearest))
					continue;
				const glm::vec3 point= eye + direction * t;
				bool bInside= true;
				for (int axis= 0; axis < 3; ++axis)
				{
					if (axis != surface.axis && (point[axis] < surface.lo[axis] || point[axis] > surface.hi[axis]))
						bInside= false;
				}
				if (bInside)
					nearest= t;
			}
			// cameraDirection.z is 1, so t is the camera-Z depth
			if (nearest > 0.f)
				outDepth.at<float>(v, u)= nearest;
		}
	}
}

TextureView makeView(const glm::vec3& eye, const glm::vec3& target, const std::vector<Surface>& surfaces,
					 const cv::Scalar& colour)
{
	TextureView view;
	view.cameraToWorld= lookAtOpenCV(eye, target);
	view.depthIntrinsics= depthIntrinsics();
	renderDepth(view.cameraToWorld, view.depthIntrinsics, surfaces, view.depth);
	view.tileIntrinsics= scaleIntrinsics(view.depthIntrinsics, k_depthWidth, k_depthHeight, k_tileWidth, k_tileHeight);
	view.tile= cv::Mat(k_tileHeight, k_tileWidth, CV_8UC3, colour);
	return view;
}

// The tile a triangle's texcoords land in, by its first corner's U and V
int tileOfTriangle(const TriangleMesh& mesh, size_t triangle, int columns, int atlasWidth, int atlasHeight)
{
	const glm::vec2& uv= mesh.texCoords[mesh.indices[triangle * 3]];
	const int column= (int)std::floor(uv.x * (float)atlasWidth / (float)k_tileWidth);
	const int row= (int)std::floor((1.f - uv.y) * (float)atlasHeight / (float)k_tileWidth * (float)k_tileWidth
								   / (float)k_tileHeight);
	return row * columns + column;
}
} // namespace MeshTexturingTestsInternal

// ---- Test cases ----

// A wall in the plane z = 2 facing -Z and a small panel in the plane x = 0.5
// facing -X in front of it. A camera on the axis looking down +Z sees the
// wall head on and the panel obliquely; a camera off to -X looking +X sees
// the panel head on and the wall glancing, its near corners behind it. The
// wall takes the axis view, the panel the side view (the more frontal per
// metre), and a quad behind every camera lands on the grey tile.
bool mesh_texturing_test_views_assigned_by_visibility()
{
	UNIT_TEST_BEGIN("triangles take the view that sees them, grey when none does")

	using namespace MeshTexturingTestsInternal;

	TriangleMesh mesh;
	appendQuad(mesh, glm::vec3(-1.f, -1.f, 2.f), glm::vec3(2.f, 0.f, 0.f), glm::vec3(0.f, 2.f, 0.f),
			   glm::vec3(0.f, 0.f, -1.f));
	appendQuad(mesh, glm::vec3(0.5f, -0.25f, 1.f), glm::vec3(0.f, 0.f, 0.5f), glm::vec3(0.f, 0.5f, 0.f),
			   glm::vec3(-1.f, 0.f, 0.f));
	appendQuad(mesh, glm::vec3(-0.5f, -0.5f, -3.f), glm::vec3(1.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f),
			   glm::vec3(0.f, 0.f, 1.f));
	const size_t wallTriangles= 2;
	const size_t panelTriangles= 2;
	const size_t hiddenTriangles= 2;

	const std::vector<Surface> surfaces= {
		makeSurface(2, 2.f, glm::vec3(-1.f, -1.f, 2.f), glm::vec3(1.f, 1.f, 2.f)),
		makeSurface(0, 0.5f, glm::vec3(0.5f, -0.25f, 1.f), glm::vec3(0.5f, 0.25f, 1.5f)),
		makeSurface(2, -3.f, glm::vec3(-0.5f, -0.5f, -3.f), glm::vec3(0.5f, 0.5f, -3.f)),
	};
	std::vector<TextureView> views;
	views.push_back(makeView(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 2.f), surfaces, cv::Scalar(255, 0, 0)));
	views.push_back(
		makeView(glm::vec3(-1.f, 0.f, 1.25f), glm::vec3(0.5f, 0.f, 1.25f), surfaces, cv::Scalar(0, 255, 0)));
	const int axisView= 0;
	const int sideView= 1;
	const int greyTile= 2;

	MeshTexturingSettings settings;
	TriangleMesh textured;
	cv::Mat atlas;
	MeshTexturingStats stats;
	if (!MeshTexturing::textureMesh(mesh, views, settings, textured, atlas, &stats))
	{
		fprintf(stdout, "    textureMesh failed\n");
		success= false;
	}

	if (success)
	{
		const int columns= MeshTexturing::tilesPerRow((int)views.size());
		if (columns != 2 || stats.atlasWidth != 2 * k_tileWidth || stats.atlasHeight != 2 * k_tileHeight
			|| atlas.cols != stats.atlasWidth || atlas.rows != stats.atlasHeight)
		{
			fprintf(stdout, "    atlas %dx%d in %d columns, expected %dx%d in 2\n", stats.atlasWidth, stats.atlasHeight,
					columns, 2 * k_tileWidth, 2 * k_tileHeight);
			success= false;
		}

		if (textured.getTriangleCount() != mesh.getTriangleCount() || !textured.hasTexCoords()
			|| textured.normals.size() != textured.vertices.size())
		{
			fprintf(stdout, "    textured mesh has %zu triangles, %zu vertices, %zu texcoords, %zu normals\n",
					textured.getTriangleCount(), textured.vertices.size(), textured.texCoords.size(),
					textured.normals.size());
			success= false;
		}
	}

	if (success)
	{
		const int columns= MeshTexturing::tilesPerRow((int)views.size());
		int wallOnAxis= 0;
		int panelOnSide= 0;
		int hiddenOnGrey= 0;
		for (size_t triangle= 0; triangle < textured.getTriangleCount(); ++triangle)
		{
			const int tile= tileOfTriangle(textured, triangle, columns, stats.atlasWidth, stats.atlasHeight);
			if (triangle < wallTriangles && tile == axisView)
				++wallOnAxis;
			else if (triangle >= wallTriangles && triangle < wallTriangles + panelTriangles && tile == sideView)
				++panelOnSide;
			else if (triangle >= wallTriangles + panelTriangles && tile == greyTile)
				++hiddenOnGrey;
		}

		if (wallOnAxis != (int)wallTriangles)
		{
			fprintf(stdout, "    %d of %zu wall triangles took the axis view\n", wallOnAxis, wallTriangles);
			success= false;
		}
		if (panelOnSide != (int)panelTriangles)
		{
			fprintf(stdout, "    %d of %zu panel triangles took the side view\n", panelOnSide, panelTriangles);
			success= false;
		}
		if (hiddenOnGrey != (int)hiddenTriangles)
		{
			fprintf(stdout, "    %d of %zu unseen triangles landed on the grey tile\n", hiddenOnGrey, hiddenTriangles);
			success= false;
		}
		if (stats.viewsUsed != 2 || stats.trianglesUntextured != (int)hiddenTriangles)
		{
			fprintf(stdout, "    stats: %d views used, %d untextured\n", stats.viewsUsed, stats.trianglesUntextured);
			success= false;
		}

		// Every texcoord within the atlas, and the colour under a wall
		// texcoord is the axis view's
		int outsideAtlas= 0;
		for (const glm::vec2& uv : textured.texCoords)
		{
			if (uv.x < 0.f || uv.x > 1.f || uv.y < 0.f || uv.y > 1.f)
				++outsideAtlas;
		}
		if (outsideAtlas > 0)
		{
			fprintf(stdout, "    %d texcoords outside the atlas\n", outsideAtlas);
			success= false;
		}

		const glm::vec2& wallUv= textured.texCoords[textured.indices[0]];
		const int px= std::clamp((int)(wallUv.x * (float)atlas.cols), 0, atlas.cols - 1);
		const int py= std::clamp((int)((1.f - wallUv.y) * (float)atlas.rows), 0, atlas.rows - 1);
		const cv::Vec3b colour= atlas.at<cv::Vec3b>(py, px);
		if (colour != cv::Vec3b(255, 0, 0))
		{
			fprintf(stdout, "    wall texcoord samples (%d, %d, %d), expected the axis view's blue\n", colour[0],
					colour[1], colour[2]);
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

// An L of two quads sharing an edge: one in the plane z = 2 facing -Z, one
// in the plane x = 0 facing +X. A camera in front of the first sees it head
// on and the second obliquely; a camera out at +X sees the second head on
// and the first outside its image. Each quad takes its own view, and the
// two vertices of the shared edge are split, so six vertices come out as
// eight.
bool mesh_texturing_test_shared_vertices_split()
{
	UNIT_TEST_BEGIN("a vertex shared across views is split per view")

	using namespace MeshTexturingTestsInternal;

	TriangleMesh mesh;
	mesh.vertices= {glm::vec3(0.f, -0.5f, 2.f), glm::vec3(1.f, -0.5f, 2.f), glm::vec3(1.f, 0.5f, 2.f),
					glm::vec3(0.f, 0.5f, 2.f),  glm::vec3(0.f, -0.5f, 1.f), glm::vec3(0.f, 0.5f, 1.f)};
	mesh.normals= {glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, 0.f, -1.f),
				   glm::vec3(0.f, 0.f, -1.f), glm::vec3(1.f, 0.f, 0.f),  glm::vec3(1.f, 0.f, 0.f)};
	// Wound so the first quad's normal is -Z and the second's +X
	mesh.indices= {0, 2, 1, 0, 3, 2, 4, 3, 0, 4, 5, 3};

	const std::vector<Surface> surfaces= {
		makeSurface(2, 2.f, glm::vec3(0.f, -0.5f, 2.f), glm::vec3(1.f, 0.5f, 2.f)),
		makeSurface(0, 0.f, glm::vec3(0.f, -0.5f, 1.f), glm::vec3(0.f, 0.5f, 2.f)),
	};
	std::vector<TextureView> views;
	views.push_back(makeView(glm::vec3(0.5f, 0.f, 0.f), glm::vec3(0.5f, 0.f, 2.f), surfaces, cv::Scalar(255, 0, 0)));
	views.push_back(makeView(glm::vec3(1.5f, 0.f, 1.5f), glm::vec3(0.f, 0.f, 1.5f), surfaces, cv::Scalar(0, 255, 0)));

	MeshTexturingSettings settings;
	TriangleMesh textured;
	cv::Mat atlas;
	MeshTexturingStats stats;
	std::vector<uint32_t> sourceVertex;
	if (!MeshTexturing::textureMesh(mesh, views, settings, textured, atlas, &stats, &sourceVertex))
	{
		fprintf(stdout, "    textureMesh failed\n");
		success= false;
	}
	else
	{
		// Every split vertex names the input vertex it came from
		bool bRemapMatches= sourceVertex.size() == textured.vertices.size();
		for (size_t vertex= 0; bRemapMatches && vertex < sourceVertex.size(); ++vertex)
		{
			bRemapMatches= sourceVertex[vertex] < mesh.vertices.size()
						   && textured.vertices[vertex] == mesh.vertices[sourceVertex[vertex]];
		}
		if (!bRemapMatches)
		{
			fprintf(stdout, "    the source vertex remap does not match the split\n");
			success= false;
		}

		// The candidate scoring sees the same thing textureMesh does
		std::vector<uint8_t> seenByA, seenByB;
		MeshTexturing::markSeenTriangles(mesh, views[0], settings, seenByA);
		MeshTexturing::markSeenTriangles(mesh, views[1], settings, seenByB);
		if (seenByA != std::vector<uint8_t>{1, 1, 1, 1} || seenByB != std::vector<uint8_t>{0, 0, 1, 1})
		{
			fprintf(stdout, "    seen by the front camera %d %d %d %d, by the side camera %d %d %d %d\n", seenByA[0],
					seenByA[1], seenByA[2], seenByA[3], seenByB[0], seenByB[1], seenByB[2], seenByB[3]);
			success= false;
		}

		const int columns= MeshTexturing::tilesPerRow((int)views.size());
		int tiles[4];
		for (size_t triangle= 0; triangle < 4; ++triangle)
			tiles[triangle]= tileOfTriangle(textured, triangle, columns, stats.atlasWidth, stats.atlasHeight);

		if (tiles[0] != 0 || tiles[1] != 0 || tiles[2] != 1 || tiles[3] != 1)
		{
			fprintf(stdout, "    triangles took tiles %d %d %d %d, expected 0 0 1 1\n", tiles[0], tiles[1], tiles[2],
					tiles[3]);
			success= false;
		}
		else if (textured.vertices.size() != 8)
		{
			fprintf(stdout, "    %zu vertices after the split, expected 8\n", textured.vertices.size());
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

// Texcoords ride through the vertex compaction with their vertices
bool mesh_texturing_test_cleanup_keeps_texcoords()
{
	UNIT_TEST_BEGIN("unreferenced vertex removal carries texcoords")

	TriangleMesh mesh;
	for (int i= 0; i < 6; ++i)
	{
		mesh.vertices.push_back(glm::vec3((float)i, 0.f, 0.f));
		mesh.texCoords.push_back(glm::vec2((float)i * 0.1f, 1.f - (float)i * 0.1f));
	}
	// Vertices 1 and 4 go unreferenced
	mesh.indices= {0, 2, 3, 3, 5, 0};

	const int removed= MeshCleanup::removeUnreferencedVertices(mesh);
	if (removed != 2 || mesh.vertices.size() != 4 || mesh.texCoords.size() != 4)
	{
		fprintf(stdout, "    removed %d leaving %zu vertices and %zu texcoords\n", removed, mesh.vertices.size(),
				mesh.texCoords.size());
		success= false;
	}
	else
	{
		for (size_t vertex= 0; vertex < mesh.vertices.size(); ++vertex)
		{
			const float expectedU= mesh.vertices[vertex].x * 0.1f;
			if (std::fabs(mesh.texCoords[vertex].x - expectedU) > 1e-6f)
			{
				fprintf(stdout, "    vertex %zu at x %.1f carries u %.2f\n", vertex, mesh.vertices[vertex].x,
						mesh.texCoords[vertex].x);
				success= false;
			}
		}
	}

	UNIT_TEST_COMPLETE()
}

// ---- Test Suite ----

bool run_mesh_texturing_unit_tests()
{
	UNIT_TEST_MODULE_BEGIN("mesh_texturing")
	UNIT_TEST_MODULE_CALL_TEST(mesh_texturing_test_views_assigned_by_visibility);
	UNIT_TEST_MODULE_CALL_TEST(mesh_texturing_test_shared_vertices_split);
	UNIT_TEST_MODULE_CALL_TEST(mesh_texturing_test_cleanup_keeps_texcoords);
	UNIT_TEST_MODULE_END()
}

#include "TsdfVolumeTests.h"
#include "CameraPathClip.h"
#include "MarchingCubes.h"
#include "MeshCleanup.h"
#include "TsdfVolume.h"
#include "unit_test.h"

#include "glm/ext/matrix_float3x3.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/geometric.hpp"

#include <opencv2/core.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace TsdfVolumeTestsInternal
{
const int k_imageWidth= 64;
const int k_imageHeight= 48;
const double k_focalLength= 50.0;
const float k_voxelSize= 0.02f;
const float k_truncation= 0.08f;
const float k_planeZ= 1.f;
const float k_unboundedExtent= 1.0e6f;
const float k_pi= 3.14159265f;

// The plane scenes' grid. Voxel centres sit at z = 0.99 and 1.01, either
// side of the plane, so no sample lands exactly on the surface.
const glm::vec3 k_planeBoundsLo(-0.3f, -0.2f, 0.81f);
const glm::vec3 k_planeBoundsHi(0.3f, 0.2f, 1.21f);

const glm::vec3 k_sphereCenter(0.f, 0.f, 0.f);
const float k_sphereRadius= 0.25f;

PinholeIntrinsics makeIntrinsics()
{
	PinholeIntrinsics intrinsics;
	intrinsics.fx= k_focalLength;
	intrinsics.fy= k_focalLength;
	intrinsics.cx= (k_imageWidth - 1) * 0.5;
	intrinsics.cy= (k_imageHeight - 1) * 0.5;
	return intrinsics;
}

TsdfVolumeConfig makeConfig()
{
	TsdfVolumeConfig config;
	config.voxelSize= k_voxelSize;
	config.truncation= k_truncation;
	return config;
}

// Camera-to-world for a camera at eye looking at target in OpenCV axes
// (+X right, +Y down, +Z forward), with +Y as close to worldDown as it gets
glm::mat4 lookAtOpenCV(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& worldDown)
{
	const glm::vec3 forward= glm::normalize(target - eye);
	const glm::vec3 right= glm::normalize(glm::cross(worldDown, forward));
	const glm::vec3 down= glm::cross(forward, right);

	glm::mat4 cameraToWorld(1.f);
	cameraToWorld[0]= glm::vec4(right, 0.f);
	cameraToWorld[1]= glm::vec4(down, 0.f);
	cameraToWorld[2]= glm::vec4(forward, 0.f);
	cameraToWorld[3]= glm::vec4(eye, 1.f);
	return cameraToWorld;
}

// World direction of the ray through pixel (u, v), scaled so its camera Z is 1
glm::vec3 pixelRay(const PinholeIntrinsics& intrinsics, const glm::mat4& cameraToWorld, int u, int v)
{
	const glm::vec3 cameraDirection((float)((u - intrinsics.cx) / intrinsics.fx),
									(float)((v - intrinsics.cy) / intrinsics.fy), 1.f);
	return glm::mat3(cameraToWorld) * cameraDirection;
}

// Exact depth of the plane z = planeZ within |x| <= halfExtentX and
// |y| <= halfExtentY, with weight 1 where hit and depth 0 elsewhere
void renderPlane(const PinholeIntrinsics& intrinsics, const glm::mat4& cameraToWorld, float planeZ, float halfExtentX,
				 float halfExtentY, cv::Mat& outDepth, cv::Mat& outWeight)
{
	outDepth= cv::Mat(k_imageHeight, k_imageWidth, CV_32FC1, cv::Scalar(0.f));
	outWeight= cv::Mat(k_imageHeight, k_imageWidth, CV_32FC1, cv::Scalar(0.f));

	const glm::vec3 eye(cameraToWorld[3]);
	for (int v= 0; v < k_imageHeight; ++v)
	{
		for (int u= 0; u < k_imageWidth; ++u)
		{
			const glm::vec3 direction= pixelRay(intrinsics, cameraToWorld, u, v);
			if (std::fabs(direction.z) < 1e-9f)
				continue;

			const float t= (planeZ - eye.z) / direction.z;
			const glm::vec3 point= eye + direction * t;
			if (t <= 0.f || std::fabs(point.x) > halfExtentX || std::fabs(point.y) > halfExtentY)
				continue;

			outDepth.at<float>(v, u)= t;
			outWeight.at<float>(v, u)= 1.f;
		}
	}
}

// Exact depth of a sphere's near side, with weight 1 where hit and depth 0 elsewhere
void renderSphere(const PinholeIntrinsics& intrinsics, const glm::mat4& cameraToWorld, const glm::vec3& center,
				  float radius, cv::Mat& outDepth, cv::Mat& outWeight)
{
	outDepth= cv::Mat(k_imageHeight, k_imageWidth, CV_32FC1, cv::Scalar(0.f));
	outWeight= cv::Mat(k_imageHeight, k_imageWidth, CV_32FC1, cv::Scalar(0.f));

	const glm::vec3 eye(cameraToWorld[3]);
	const glm::vec3 eyeToCenter= eye - center;
	for (int v= 0; v < k_imageHeight; ++v)
	{
		for (int u= 0; u < k_imageWidth; ++u)
		{
			const glm::vec3 direction= pixelRay(intrinsics, cameraToWorld, u, v);
			const float a= glm::dot(direction, direction);
			const float b= 2.f * glm::dot(direction, eyeToCenter);
			const float c= glm::dot(eyeToCenter, eyeToCenter) - radius * radius;
			const float discriminant= b * b - 4.f * a * c;
			if (discriminant < 0.f)
				continue;

			const float t= (-b - std::sqrt(discriminant)) / (2.f * a);
			if (t <= 0.f)
				continue;

			outDepth.at<float>(v, u)= t;
			outWeight.at<float>(v, u)= 1.f;
		}
	}
}

// The unbounded plane z = 1 fused from three cameras near the origin
// looking along +Z, one of them slightly tilted
TsdfVolume buildFusedPlaneVolume()
{
	TsdfVolume volume(k_planeBoundsLo, k_planeBoundsHi, makeConfig());
	const PinholeIntrinsics intrinsics= makeIntrinsics();
	const glm::vec3 worldDown(0.f, 1.f, 0.f);
	const glm::mat4 poses[3]= {lookAtOpenCV(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f), worldDown),
							   lookAtOpenCV(glm::vec3(0.15f, 0.05f, 0.05f), glm::vec3(0.15f, 0.05f, 1.f), worldDown),
							   lookAtOpenCV(glm::vec3(-0.1f, 0.08f, -0.05f), glm::vec3(-0.08f, 0.06f, 1.f), worldDown)};

	for (const glm::mat4& pose : poses)
	{
		cv::Mat depth, weight;
		renderPlane(intrinsics, pose, k_planeZ, k_unboundedExtent, k_unboundedExtent, depth, weight);
		volume.integrate(depth, weight, intrinsics, pose);
	}

	return volume;
}

// The cleanup chain applied to an extracted mesh
void cleanMesh(TriangleMesh& mesh, int minComponentTriangles)
{
	MeshCleanup::removeDegenerateTriangles(mesh);
	MeshCleanup::removeDuplicateTriangles(mesh);
	MeshCleanup::removeSmallComponents(mesh, minComponentTriangles);
	MeshCleanup::removeUnreferencedVertices(mesh);
	MeshCleanup::computeVertexNormals(mesh);
}

// A width x height quad grid in the z = 0 plane, appended to mesh
void appendQuadGrid(TriangleMesh& mesh, int quadsX, int quadsY, const glm::vec3& offset)
{
	const uint32_t firstVertex= (uint32_t)mesh.vertices.size();
	for (int y= 0; y <= quadsY; ++y)
	{
		for (int x= 0; x <= quadsX; ++x)
		{
			mesh.vertices.push_back(offset + glm::vec3((float)x * 0.01f, (float)y * 0.01f, 0.f));
		}
	}

	const uint32_t rowLength= (uint32_t)quadsX + 1;
	for (int y= 0; y < quadsY; ++y)
	{
		for (int x= 0; x < quadsX; ++x)
		{
			const uint32_t i00= firstVertex + (uint32_t)y * rowLength + (uint32_t)x;
			const uint32_t i10= i00 + 1;
			const uint32_t i01= i00 + rowLength;
			const uint32_t i11= i01 + 1;
			mesh.indices.insert(mesh.indices.end(), {i00, i10, i11, i00, i11, i01});
		}
	}
}
} // namespace TsdfVolumeTestsInternal

// ---- Test cases ----

// An analytic sphere distance (negative inside) meshes onto the sphere, every
// triangle wound counter-clockwise about the outward direction, with shared
// edge vertices welded.
bool tsdf_volume_test_marching_cubes_sphere()
{
	UNIT_TEST_BEGIN("marching cubes meshes an analytic sphere outward and welded")

	const int size= 40;
	const float voxelSize= 0.02f;
	const float radius= 0.3f;
	const glm::vec3 center(0.f);
	const glm::vec3 origin(-0.39f);
	const glm::ivec3 dims(size, size, size);

	std::vector<float> field((size_t)size * size * size);
	const std::vector<uint8_t> mask(field.size(), 1);
	for (int z= 0; z < size; ++z)
	{
		for (int y= 0; y < size; ++y)
		{
			for (int x= 0; x < size; ++x)
			{
				const glm::vec3 position= origin + glm::vec3((float)x, (float)y, (float)z) * voxelSize;
				field[((size_t)z * size + y) * size + x]= glm::length(position - center) - radius;
			}
		}
	}

	TriangleMesh mesh;
	MarchingCubes::extract(field.data(), mask.data(), dims, origin, voxelSize, 0.f, mesh);
	if (mesh.getTriangleCount() == 0)
	{
		fprintf(stdout, "    no triangles extracted\n");
		success= false;
	}
	else
	{
		int offSurfaceVertices= 0;
		float maxRadiusError= 0.f;
		for (const glm::vec3& vertex : mesh.vertices)
		{
			const float radiusError= std::fabs(glm::length(vertex - center) - radius);
			maxRadiusError= std::max(maxRadiusError, radiusError);
			if (radiusError > voxelSize * 0.5f)
				offSurfaceVertices++;
		}
		if (offSurfaceVertices > 0)
		{
			fprintf(stdout, "    %d vertices off the sphere, max radius error %.5f m\n", offSurfaceVertices,
					maxRadiusError);
			success= false;
		}

		int inwardTriangles= 0;
		for (size_t i= 0; i + 2 < mesh.indices.size(); i+= 3)
		{
			const glm::vec3& a= mesh.vertices[mesh.indices[i]];
			const glm::vec3& b= mesh.vertices[mesh.indices[i + 1]];
			const glm::vec3& c= mesh.vertices[mesh.indices[i + 2]];
			const glm::vec3 centroid= (a + b + c) / 3.f;
			if (glm::dot(glm::cross(b - a, c - a), centroid - center) <= 0.f)
				inwardTriangles++;
		}
		if (inwardTriangles > 0)
		{
			fprintf(stdout, "    %d of %zu triangles wound inward\n", inwardTriangles, mesh.getTriangleCount());
			success= false;
		}

		if ((double)mesh.vertices.size() >= 1.5 * (double)mesh.getTriangleCount())
		{
			fprintf(stdout, "    %zu vertices for %zu triangles, edge vertices are not welded\n", mesh.vertices.size(),
					mesh.getTriangleCount());
			success= false;
		}

		if (mesh.normals.size() != mesh.vertices.size())
		{
			fprintf(stdout, "    %zu normals for %zu vertices\n", mesh.normals.size(), mesh.vertices.size());
			success= false;
		}

		MeshCleanup::removeDegenerateTriangles(mesh);
		MeshCleanup::removeDuplicateTriangles(mesh);
		MeshCleanup::removeUnreferencedVertices(mesh);

		std::vector<uint8_t> referenced(mesh.vertices.size(), 0);
		for (const uint32_t index : mesh.indices)
		{
			referenced[index]= 1;
		}
		const size_t unreferencedVertices= std::count(referenced.begin(), referenced.end(), (uint8_t)0);
		if (unreferencedVertices > 0)
		{
			fprintf(stdout, "    %zu unreferenced vertices after cleanup\n", unreferencedVertices);
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

// One pixel 20 percent deeper than a flat field drops itself and its four
// 4-neighbours, and nothing else.
bool tsdf_volume_test_cull_flying_pixels()
{
	UNIT_TEST_BEGIN("flying pixel cull drops both pixels of every bad pair")

	const int size= 8;
	const int spikeX= 3;
	const int spikeY= 4;
	cv::Mat depth(size, size, CV_32FC1, cv::Scalar(1.f));
	depth.at<float>(spikeY, spikeX)= 1.2f;

	cv::Mat keep;
	TsdfVolume::cullFlyingPixels(depth, 0.08f, keep);
	if (keep.type() != CV_8UC1 || keep.rows != size || keep.cols != size)
	{
		fprintf(stdout, "    keep mask is not an 8x8 CV_8UC1 image\n");
		success= false;
	}
	else
	{
		int mismatches= 0;
		for (int y= 0; y < size; ++y)
		{
			for (int x= 0; x < size; ++x)
			{
				const int manhattan= std::abs(x - spikeX) + std::abs(y - spikeY);
				const bool bExpectDropped= manhattan <= 1;
				const bool bDropped= keep.at<uint8_t>(y, x) == 0;
				if (bDropped != bExpectDropped)
				{
					fprintf(stdout, "    pixel (%d, %d) %s\n", x, y, bDropped ? "dropped" : "kept");
					mismatches++;
				}
			}
		}
		success&= mismatches == 0;
	}

	UNIT_TEST_COMPLETE()
}

// A near block on a far field: keeping the near pixel drops only the far
// pixels across the block's edge, so the block keeps its outermost ring.
bool tsdf_volume_test_cull_keeps_near()
{
	UNIT_TEST_BEGIN("flying pixel cull keeping the near pixel drops only the far side of an edge")

	const int size= 8;
	cv::Mat depth(size, size, CV_32FC1, cv::Scalar(2.f));
	const cv::Rect block(2, 2, 3, 3);
	depth(block).setTo(cv::Scalar(1.f));

	cv::Mat keep;
	TsdfVolume::cullFlyingPixels(depth, 0.08f, keep, true);
	int mismatches= 0;
	for (int y= 0; y < size; ++y)
	{
		for (int x= 0; x < size; ++x)
		{
			const bool bInBlock= block.contains(cv::Point(x, y));
			bool bNextToBlock= false;
			const int offsets[4][2]= {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
			for (const auto& offset : offsets)
				bNextToBlock|= block.contains(cv::Point(x + offset[0], y + offset[1]));
			const bool bExpectDropped= !bInBlock && bNextToBlock;
			const bool bDropped= keep.at<uint8_t>(y, x) == 0;
			if (bDropped != bExpectDropped)
			{
				fprintf(stdout, "    pixel (%d, %d) %s\n", x, y, bDropped ? "dropped" : "kept");
				mismatches++;
			}
		}
	}
	success&= mismatches == 0;

	UNIT_TEST_COMPLETE()
}

// A plane fused from three poses: the field crosses zero on the plane, and the
// cleaned mesh lies on it with normals toward the cameras.
bool tsdf_volume_test_fused_plane()
{
	UNIT_TEST_BEGIN("plane fused from three poses meshes onto the plane")

	using namespace TsdfVolumeTestsInternal;

	const TsdfVolume volume= buildFusedPlaneVolume();
	if (volume.getFramesIntegrated() != 3)
	{
		fprintf(stdout, "    %d frames integrated, expected 3\n", volume.getFramesIntegrated());
		success= false;
	}

	// March the field along world +Z lines through the plane
	const float lineXY[][2]= {{0.f, 0.f}, {-0.1f, -0.05f}, {0.12f, 0.07f}, {0.033f, -0.071f}, {0.2f, 0.1f}};
	const float marchStep= 0.001f;
	for (const auto& line : lineXY)
	{
		bool bCrossed= false;
		float crossingZ= 0.f;
		float previousZ= 0.9f;
		float previousSample= volume.sample(glm::vec3(line[0], line[1], previousZ));
		for (int stepIndex= 1; stepIndex <= 200 && !bCrossed; ++stepIndex)
		{
			const float z= 0.9f + (float)stepIndex * marchStep;
			const float currentSample= volume.sample(glm::vec3(line[0], line[1], z));
			if (std::isfinite(previousSample) && std::isfinite(currentSample) && previousSample > 0.f
				&& currentSample <= 0.f)
			{
				crossingZ= previousZ + marchStep * previousSample / (previousSample - currentSample);
				bCrossed= true;
			}
			previousZ= z;
			previousSample= currentSample;
		}

		if (!bCrossed)
		{
			fprintf(stdout, "    no zero crossing along x=%.3f y=%.3f\n", line[0], line[1]);
			success= false;
		}
		else if (std::fabs(crossingZ - k_planeZ) > 0.002f)
		{
			fprintf(stdout, "    zero crossing along x=%.3f y=%.3f at z=%.4f\n", line[0], line[1], crossingZ);
			success= false;
		}
	}

	TriangleMesh mesh;
	if (!volume.extractMesh(mesh))
	{
		fprintf(stdout, "    extractMesh found no surface\n");
		success= false;
	}
	else
	{
		cleanMesh(mesh, 50);

		int offPlaneVertices= 0;
		float maxPlaneError= 0.f;
		for (const glm::vec3& vertex : mesh.vertices)
		{
			const float planeError= std::fabs(vertex.z - k_planeZ);
			maxPlaneError= std::max(maxPlaneError, planeError);
			if (planeError > volume.getVoxelSize() * 0.25f)
				offPlaneVertices++;
		}
		if (mesh.vertices.empty() || offPlaneVertices > 0)
		{
			fprintf(stdout, "    %d of %zu vertices off the plane, max error %.5f m\n", offPlaneVertices,
					mesh.vertices.size(), maxPlaneError);
			success= false;
		}

		glm::vec3 normalSum(0.f);
		for (const glm::vec3& normal : mesh.normals)
		{
			normalSum+= normal;
		}
		const float normalSumLength= glm::length(normalSum);
		const float towardCameras= normalSumLength > 0.f ? -normalSum.z / normalSumLength : 0.f;
		if (towardCameras < 0.99f)
		{
			fprintf(stdout, "    mean normal (%.3f, %.3f, %.3f) does not face the cameras\n", normalSum.x, normalSum.y,
					normalSum.z);
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

// A sphere fused from six axis-aligned poses meshes near its radius. The
// projective distance thickens the surface where a camera sees it at a
// grazing angle, so single vertices stand up to half a voxel proud; the mean
// stays well under a quarter voxel.
bool tsdf_volume_test_fused_sphere()
{
	UNIT_TEST_BEGIN("sphere fused from six poses meshes onto the sphere")

	using namespace TsdfVolumeTestsInternal;

	TsdfVolume volume(glm::vec3(-0.35f), glm::vec3(0.35f), makeConfig());
	const PinholeIntrinsics intrinsics= makeIntrinsics();
	const float cameraDistance= 0.9f;
	const glm::vec3 yDown(0.f, 1.f, 0.f);
	const glm::vec3 zDown(0.f, 0.f, 1.f);
	const glm::mat4 poses[6]= {
		lookAtOpenCV(k_sphereCenter + glm::vec3(cameraDistance, 0.f, 0.f), k_sphereCenter, yDown),
		lookAtOpenCV(k_sphereCenter + glm::vec3(-cameraDistance, 0.f, 0.f), k_sphereCenter, yDown),
		lookAtOpenCV(k_sphereCenter + glm::vec3(0.f, 0.f, cameraDistance), k_sphereCenter, yDown),
		lookAtOpenCV(k_sphereCenter + glm::vec3(0.f, 0.f, -cameraDistance), k_sphereCenter, yDown),
		lookAtOpenCV(k_sphereCenter + glm::vec3(0.f, cameraDistance, 0.f), k_sphereCenter, zDown),
		lookAtOpenCV(k_sphereCenter + glm::vec3(0.f, -cameraDistance, 0.f), k_sphereCenter, zDown)};
	for (const glm::mat4& pose : poses)
	{
		cv::Mat depth, weight;
		renderSphere(intrinsics, pose, k_sphereCenter, k_sphereRadius, depth, weight);
		volume.integrate(depth, weight, intrinsics, pose);
	}

	TriangleMesh mesh;
	if (!volume.extractMesh(mesh))
	{
		fprintf(stdout, "    extractMesh found no surface\n");
		success= false;
	}
	else
	{
		cleanMesh(mesh, 50);

		const float voxelSize= volume.getVoxelSize();
		float maxRadiusError= 0.f;
		double radiusErrorSum= 0.0;
		double outwardNormalSum= 0.0;
		for (size_t i= 0; i < mesh.vertices.size(); ++i)
		{
			const glm::vec3 radial= mesh.vertices[i] - k_sphereCenter;
			const float radiusError= std::fabs(glm::length(radial) - k_sphereRadius);
			maxRadiusError= std::max(maxRadiusError, radiusError);
			radiusErrorSum+= radiusError;
			outwardNormalSum+= glm::dot(mesh.normals[i], glm::normalize(radial));
		}
		const size_t vertexCount= std::max<size_t>(mesh.vertices.size(), 1);
		const double meanRadiusError= radiusErrorSum / (double)vertexCount;
		const double meanOutwardNormal= outwardNormalSum / (double)vertexCount;

		if (meanRadiusError > voxelSize * 0.25f || maxRadiusError > voxelSize)
		{
			fprintf(stdout, "    radius error mean %.5f m, max %.5f m\n", meanRadiusError, maxRadiusError);
			success= false;
		}
		if (meanOutwardNormal < 0.9)
		{
			fprintf(stdout, "    mean outward normal component %.3f\n", meanOutwardNormal);
			success= false;
		}

		// About two triangles per voxel face of the sphere's area
		const double surfaceArea= 4.0 * k_pi * k_sphereRadius * k_sphereRadius;
		const double voxelFaces= surfaceArea / ((double)voxelSize * voxelSize);
		const double triangleCount= (double)mesh.getTriangleCount();
		if (triangleCount < voxelFaces || triangleCount > 5.0 * voxelFaces)
		{
			fprintf(stdout, "    %zu triangles, expected between %.0f and %.0f\n", mesh.getTriangleCount(), voxelFaces,
					5.0 * voxelFaces);
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

// Behind a surface the observed negative band meets unobserved voxels. The
// sign fill must keep that from extracting as a second shell.
bool tsdf_volume_test_no_back_shell()
{
	UNIT_TEST_BEGIN("no back shell forms behind a fused plane")

	using namespace TsdfVolumeTestsInternal;

	const TsdfVolume volume= buildFusedPlaneVolume();
	TriangleMesh mesh;
	if (!volume.extractMesh(mesh))
	{
		fprintf(stdout, "    extractMesh found no surface\n");
		success= false;
	}
	else
	{
		// Raw extraction, so cleanup cannot hide a shell
		int behindVertices= 0;
		float deepestZ= -1.f;
		for (const glm::vec3& vertex : mesh.vertices)
		{
			deepestZ= std::max(deepestZ, vertex.z);
			if (vertex.z > k_planeZ + volume.getVoxelSize())
				behindVertices++;
		}
		if (behindVertices > 0)
		{
			fprintf(stdout, "    %d vertices behind the plane, deepest at z=%.4f\n", behindVertices, deepestZ);
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

// Sampling must not blend an observed patch with unobserved voxels at its rim:
// NaN as soon as one of the eight voxels is unobserved, finite a voxel inside.
bool tsdf_volume_test_sample_rim_is_unobserved()
{
	UNIT_TEST_BEGIN("sample is NaN at the rim of an observed patch")

	using namespace TsdfVolumeTestsInternal;

	// A plane patch |x| <= 0.155, |y| <= 0.105 seen by one camera at the origin
	TsdfVolume volume(k_planeBoundsLo, k_planeBoundsHi, makeConfig());
	const PinholeIntrinsics intrinsics= makeIntrinsics();
	const glm::mat4 pose= lookAtOpenCV(glm::vec3(0.f), glm::vec3(0.f, 0.f, 1.f), glm::vec3(0.f, 1.f, 0.f));
	cv::Mat depth, weight;
	renderPlane(intrinsics, pose, k_planeZ, 0.155f, 0.105f, depth, weight);
	volume.integrate(depth, weight, intrinsics, pose);

	// The voxel layers either side of the plane and the rows either side of y = 0
	const glm::vec3& origin= volume.getOrigin();
	const float voxelSize= volume.getVoxelSize();
	const glm::ivec3& dims= volume.getDims();
	const int zLayer= (int)std::floor((k_planeZ - origin.z) / voxelSize);
	const int yRow= (int)std::floor((0.f - origin.y) / voxelSize);
	const int xCenter= (int)std::floor((0.f - origin.x) / voxelSize);
	auto isColumnObserved= [&](int x)
	{
		for (int dz= 0; dz <= 1; ++dz)
		{
			for (int dy= 0; dy <= 1; ++dy)
			{
				if (!volume.isVoxelObserved(x, yRow + dy, zLayer + dz))
					return false;
			}
		}
		return true;
	};

	// Walk +X from the centre to the last column observed on all four lines
	int lastObservedX= -1;
	for (int x= xCenter; x < dims.x - 1 && isColumnObserved(x); ++x)
	{
		lastObservedX= x;
	}

	const float lastObservedWorldX= origin.x + (float)lastObservedX * voxelSize;
	if (lastObservedX <= xCenter || lastObservedWorldX < 0.1f || lastObservedWorldX > 0.2f)
	{
		fprintf(stdout, "    patch rim not found where expected (last observed column %d at x=%.3f)\n", lastObservedX,
				lastObservedWorldX);
		success= false;
	}
	else
	{
		const float sampleY= origin.y + ((float)yRow + 0.5f) * voxelSize;
		const float sampleZ= origin.z + ((float)zLayer + 0.5f) * voxelSize;
		const glm::vec3 rimPoint(origin.x + ((float)lastObservedX + 0.5f) * voxelSize, sampleY, sampleZ);
		const glm::vec3 insidePoint(origin.x + ((float)lastObservedX - 0.5f) * voxelSize, sampleY, sampleZ);

		const float rimSample= volume.sample(rimPoint);
		if (!std::isnan(rimSample))
		{
			fprintf(stdout, "    sample at the rim x=%.3f is %.4f, expected NaN\n", rimPoint.x, rimSample);
			success= false;
		}

		const float insideSample= volume.sample(insidePoint);
		if (!std::isfinite(insideSample) || std::fabs(insideSample) > 0.5f)
		{
			fprintf(stdout, "    sample one voxel inside x=%.3f is %.4f, expected near zero\n", insidePoint.x,
					insideSample);
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

// A 1000-triangle grid survives the island pass while a separate 20-triangle
// strip is dropped, and compaction keeps positions and normals together.
bool tsdf_volume_test_mesh_cleanup_components()
{
	UNIT_TEST_BEGIN("small components are dropped and vertices compacted")

	using namespace TsdfVolumeTestsInternal;

	// The strip first so compaction has to move every grid vertex
	TriangleMesh mesh;
	appendQuadGrid(mesh, 10, 1, glm::vec3(0.f, 0.f, 1.f));
	const size_t stripVertexCount= mesh.vertices.size();
	const size_t stripIndexCount= mesh.indices.size();
	appendQuadGrid(mesh, 25, 20, glm::vec3(0.f));
	const size_t gridVertexCount= mesh.vertices.size() - stripVertexCount;

	// Normals tagged with the vertex position so their pairing can be checked
	for (const glm::vec3& vertex : mesh.vertices)
	{
		mesh.normals.push_back(vertex);
	}

	// Triangles of the grid by position, to compare after remapping
	std::vector<glm::vec3> gridTriangleCorners;
	for (size_t i= stripIndexCount; i < mesh.indices.size(); ++i)
	{
		gridTriangleCorners.push_back(mesh.vertices[mesh.indices[i]]);
	}

	const int removedTriangles= MeshCleanup::removeSmallComponents(mesh, 100);
	if (removedTriangles != 20 || mesh.getTriangleCount() != 1000)
	{
		fprintf(stdout, "    removed %d triangles leaving %zu, expected 20 leaving 1000\n", removedTriangles,
				mesh.getTriangleCount());
		success= false;
	}

	const int removedVertices= MeshCleanup::removeUnreferencedVertices(mesh);
	if (removedVertices != (int)stripVertexCount || mesh.vertices.size() != gridVertexCount
		|| mesh.normals.size() != gridVertexCount)
	{
		fprintf(stdout, "    removed %d vertices leaving %zu (%zu normals), expected %zu leaving %zu\n",
				removedVertices, mesh.vertices.size(), mesh.normals.size(), stripVertexCount, gridVertexCount);
		success= false;
	}
	else
	{
		int mismatchedCorners= 0;
		for (size_t i= 0; i < mesh.indices.size(); ++i)
		{
			const uint32_t index= mesh.indices[i];
			if (index >= mesh.vertices.size() || i >= gridTriangleCorners.size()
				|| mesh.vertices[index] != gridTriangleCorners[i] || mesh.normals[index] != mesh.vertices[index])
			{
				mismatchedCorners++;
			}
		}
		if (mismatchedCorners > 0)
		{
			fprintf(stdout, "    %d triangle corners changed position or lost their normal\n", mismatchedCorners);
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

// A keep mask built from a camera path trims the extraction to the path's
// neighbourhood: every vertex of the trimmed mesh lies within the radius plus
// the mask's cell tolerance of the one path position, while the untrimmed
// mesh reaches the far side of the plane
bool tsdf_volume_test_camera_path_trim()
{
	UNIT_TEST_BEGIN("camera path trim keeps only the plane near the path")

	using namespace TsdfVolumeTestsInternal;

	const TsdfVolume volume= buildFusedPlaneVolume();
	const glm::vec3 pathPosition(-0.3f, 0.f, k_planeZ);
	const float radius= 0.25f;
	const float cellTolerance= (float)CameraPathClip::k_defaultCellVoxels * volume.getVoxelSize();

	TriangleMesh untrimmed;
	if (!volume.extractMesh(untrimmed))
	{
		fprintf(stdout, "    untrimmed extraction failed\n");
		success= false;
	}

	std::vector<uint8_t> keep;
	const size_t clipped= CameraPathClip::buildKeepMask(volume, std::vector<glm::vec3>{pathPosition}, radius, keep);
	if (keep.size() != volume.getVoxelCount() || clipped == 0 || clipped == volume.getVoxelCount())
	{
		fprintf(stdout, "    mask of %zu bytes clipped %zu of %zu voxels\n", keep.size(), clipped,
				volume.getVoxelCount());
		success= false;
	}

	TriangleMesh trimmed;
	if (!volume.extractMesh(trimmed, &keep) || trimmed.getTriangleCount() == 0)
	{
		fprintf(stdout, "    trimmed extraction yielded nothing\n");
		success= false;
	}

	if (success)
	{
		float farthestTrimmed= 0.f;
		for (const glm::vec3& vertex : trimmed.vertices)
		{
			farthestTrimmed= std::max(farthestTrimmed, glm::length(vertex - pathPosition));
		}
		float farthestUntrimmed= 0.f;
		for (const glm::vec3& vertex : untrimmed.vertices)
		{
			farthestUntrimmed= std::max(farthestUntrimmed, glm::length(vertex - pathPosition));
		}

		if (farthestTrimmed > radius + cellTolerance)
		{
			fprintf(stdout, "    a trimmed vertex is %.3f m from the path, beyond %.3f + %.3f\n", farthestTrimmed,
					radius, cellTolerance);
			success= false;
		}
		if (farthestUntrimmed <= radius + cellTolerance)
		{
			fprintf(stdout, "    the untrimmed mesh never reached past the radius (%.3f m)\n", farthestUntrimmed);
			success= false;
		}
		if (trimmed.getTriangleCount() >= untrimmed.getTriangleCount())
		{
			fprintf(stdout, "    trimmed %zu triangles, untrimmed %zu\n", trimmed.getTriangleCount(),
					untrimmed.getTriangleCount());
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

// ---- Test Suite ----

bool run_tsdf_volume_unit_tests()
{
	UNIT_TEST_MODULE_BEGIN("tsdf_volume")
	UNIT_TEST_MODULE_CALL_TEST(tsdf_volume_test_marching_cubes_sphere);
	UNIT_TEST_MODULE_CALL_TEST(tsdf_volume_test_cull_flying_pixels);
	UNIT_TEST_MODULE_CALL_TEST(tsdf_volume_test_cull_keeps_near);
	UNIT_TEST_MODULE_CALL_TEST(tsdf_volume_test_fused_plane);
	UNIT_TEST_MODULE_CALL_TEST(tsdf_volume_test_fused_sphere);
	UNIT_TEST_MODULE_CALL_TEST(tsdf_volume_test_no_back_shell);
	UNIT_TEST_MODULE_CALL_TEST(tsdf_volume_test_sample_rim_is_unobserved);
	UNIT_TEST_MODULE_CALL_TEST(tsdf_volume_test_mesh_cleanup_components);
	UNIT_TEST_MODULE_CALL_TEST(tsdf_volume_test_camera_path_trim);
	UNIT_TEST_MODULE_END()
}

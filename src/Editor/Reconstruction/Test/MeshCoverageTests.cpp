#include "MeshCoverageTests.h"
#include "MeshCoverage.h"
#include "NpyWriter.h"
#include "unit_test.h"

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/geometric.hpp"
#include "glm/matrix.hpp"

#include <opencv2/core.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace MeshCoverageTestsInternal
{
// Fine enough that half a pixel of rounding on a plane seen at 60 degrees
// stays well inside the support band
const int k_depthWidth= 256;
const int k_depthHeight= 192;
const double k_depthFocal= 200.0;
const float k_pi= 3.14159265358979f;

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

// A 0.4 m square in the z = 0 plane facing +Z, as a 5 x 5 grid of vertices
TriangleMesh makePlaneMesh()
{
	TriangleMesh mesh;
	const int side= 5;
	for (int row= 0; row < side; ++row)
	{
		for (int column= 0; column < side; ++column)
		{
			mesh.vertices.push_back(glm::vec3(-0.2f + 0.1f * column, -0.2f + 0.1f * row, 0.f));
			mesh.normals.push_back(glm::vec3(0.f, 0.f, 1.f));
		}
	}
	for (int row= 0; row + 1 < side; ++row)
	{
		for (int column= 0; column + 1 < side; ++column)
		{
			const uint32_t a= (uint32_t)(row * side + column);
			const uint32_t b= a + 1;
			const uint32_t c= a + side + 1;
			const uint32_t d= a + side;
			mesh.indices.insert(mesh.indices.end(), {a, b, c, a, c, d});
		}
	}
	return mesh;
}

// A view of the infinite plane z = planeZ from the camera, every pixel
// confident and kept, at the camera-Z distance a sensor reports
CoverageView makePlaneView(const glm::mat4& cameraToWorld, float planeZ)
{
	CoverageView view;
	view.cameraToWorld= cameraToWorld;
	view.depthIntrinsics.fx= k_depthFocal;
	view.depthIntrinsics.fy= k_depthFocal;
	view.depthIntrinsics.cx= (k_depthWidth - 1) * 0.5;
	view.depthIntrinsics.cy= (k_depthHeight - 1) * 0.5;
	view.depth= cv::Mat(k_depthHeight, k_depthWidth, CV_32FC1, cv::Scalar(0.f));
	view.confidence= cv::Mat(k_depthHeight, k_depthWidth, CV_8UC1, cv::Scalar(2));
	view.keep= cv::Mat(k_depthHeight, k_depthWidth, CV_8UC1, cv::Scalar(1));
	view.depthMin= 0.1f;
	view.depthMax= 5.f;

	const glm::vec3 eye(cameraToWorld[3]);
	for (int v= 0; v < k_depthHeight; ++v)
	{
		for (int u= 0; u < k_depthWidth; ++u)
		{
			const glm::vec3 cameraDirection((float)((u - view.depthIntrinsics.cx) / view.depthIntrinsics.fx),
											(float)((v - view.depthIntrinsics.cy) / view.depthIntrinsics.fy), 1.f);
			const glm::vec3 direction= glm::mat3(cameraToWorld) * cameraDirection;
			if (std::fabs(direction.z) < 1e-6f)
				continue;
			const float t= (planeZ - eye.z) / direction.z;
			if (t > 0.f)
				view.depth.at<float>(v, u)= t; // cameraDirection has unit Z, so t is the camera-Z depth
		}
	}
	return view;
}

// A camera a metre from the origin at the given angle off the plane's
// normal and azimuth about it, looking at the origin
glm::mat4 cameraAround(float offNormalDegrees, float azimuthDegrees)
{
	const float polar= offNormalDegrees * k_pi / 180.f;
	const float azimuth= azimuthDegrees * k_pi / 180.f;
	const glm::vec3 eye(std::sin(polar) * std::cos(azimuth), std::sin(polar) * std::sin(azimuth), std::cos(polar));
	return lookAtOpenCV(eye, glm::vec3(0.f));
}

bool allEqual(const std::vector<uint16_t>& values, uint16_t expected)
{
	for (uint16_t value : values)
	{
		if (value != expected)
			return false;
	}
	return true;
}
} // namespace MeshCoverageTestsInternal

bool run_mesh_coverage_unit_tests()
{
	UNIT_TEST_MODULE_BEGIN("mesh_coverage")
	UNIT_TEST_MODULE_CALL_TEST(mesh_coverage_test_frontal_view_supports);
	UNIT_TEST_MODULE_CALL_TEST(mesh_coverage_test_farther_depth_carves);
	UNIT_TEST_MODULE_CALL_TEST(mesh_coverage_test_nearer_depth_counts_nothing);
	UNIT_TEST_MODULE_CALL_TEST(mesh_coverage_test_unusable_pixels_count_nothing);
	UNIT_TEST_MODULE_CALL_TEST(mesh_coverage_test_camera_behind_counts_nothing);
	UNIT_TEST_MODULE_CALL_TEST(mesh_coverage_test_azimuths_give_directions);
	UNIT_TEST_MODULE_CALL_TEST(mesh_coverage_test_classification);
	UNIT_TEST_MODULE_CALL_TEST(mesh_coverage_test_area_shares);
	UNIT_TEST_MODULE_CALL_TEST(mesh_coverage_test_band_follows_voxel);
	UNIT_TEST_MODULE_CALL_TEST(mesh_coverage_test_remap_follows_split);
	UNIT_TEST_MODULE_CALL_TEST(mesh_coverage_test_npy_round_trip);
	UNIT_TEST_MODULE_END()
}

bool mesh_coverage_test_frontal_view_supports()
{
	using namespace MeshCoverageTestsInternal;

	UNIT_TEST_BEGIN("a camera facing the plane supports every vertex head-on")
	const TriangleMesh mesh= makePlaneMesh();
	MeshCoverage coverage(mesh, 0.008f, MeshCoverageSettings());
	coverage.accumulateView(makePlaneView(cameraAround(0.f, 0.f), 0.f));
	MeshCoverageResult result;
	coverage.finish(result);

	success&= result.getVertexCount() == mesh.vertices.size();
	success&= allEqual(result.support, 1);
	success&= allEqual(result.supportHigh, 1);
	success&= allEqual(result.carve, 0);
	for (size_t vertex= 0; vertex < mesh.vertices.size(); ++vertex)
	{
		const float expectedDistance= glm::length(glm::vec3(0.f, 0.f, 1.f) - mesh.vertices[vertex]);
		success&= result.directions[vertex] == 1;
		success&= result.bestCos[vertex] > 0.9f;
		success&= std::fabs(result.nearest[vertex] - expectedDistance) < 1e-4f;
		success&= result.coverageClass[vertex] == (uint8_t)eSurfaceCoverage::weak;
	}
	UNIT_TEST_COMPLETE()
}

bool mesh_coverage_test_farther_depth_carves()
{
	using namespace MeshCoverageTestsInternal;

	UNIT_TEST_BEGIN("depth measured past the plane carves it and supports nothing")
	const TriangleMesh mesh= makePlaneMesh();
	MeshCoverage coverage(mesh, 0.008f, MeshCoverageSettings());
	coverage.accumulateView(makePlaneView(cameraAround(0.f, 0.f), -0.1f));
	MeshCoverageResult result;
	coverage.finish(result);

	success&= allEqual(result.support, 0);
	success&= allEqual(result.carve, 1);
	success&= result.areaUnseen > 0.999f;
	UNIT_TEST_COMPLETE()
}

bool mesh_coverage_test_nearer_depth_counts_nothing()
{
	using namespace MeshCoverageTestsInternal;

	UNIT_TEST_BEGIN("depth measured in front of the plane hides it and counts nowhere")
	const TriangleMesh mesh= makePlaneMesh();
	MeshCoverage coverage(mesh, 0.008f, MeshCoverageSettings());
	coverage.accumulateView(makePlaneView(cameraAround(0.f, 0.f), 0.1f));
	MeshCoverageResult result;
	coverage.finish(result);

	success&= allEqual(result.support, 0);
	success&= allEqual(result.carve, 0);
	UNIT_TEST_COMPLETE()
}

bool mesh_coverage_test_unusable_pixels_count_nothing()
{
	using namespace MeshCoverageTestsInternal;

	UNIT_TEST_BEGIN("a culled, unconfident, or out of range pixel counts nothing")
	const TriangleMesh mesh= makePlaneMesh();
	MeshCoverage coverage(mesh, 0.008f, MeshCoverageSettings());

	CoverageView culled= makePlaneView(cameraAround(0.f, 0.f), 0.f);
	culled.keep.setTo(cv::Scalar(0));
	coverage.accumulateView(culled);

	CoverageView unconfident= makePlaneView(cameraAround(0.f, 0.f), 0.f);
	unconfident.confidence.setTo(cv::Scalar(0));
	coverage.accumulateView(unconfident);

	CoverageView outOfRange= makePlaneView(cameraAround(0.f, 0.f), 0.f);
	outOfRange.depthMax= 0.5f;
	coverage.accumulateView(outOfRange);

	MeshCoverageResult result;
	coverage.finish(result);
	success&= allEqual(result.support, 0);
	success&= allEqual(result.carve, 0);
	UNIT_TEST_COMPLETE()
}

bool mesh_coverage_test_camera_behind_counts_nothing()
{
	using namespace MeshCoverageTestsInternal;

	UNIT_TEST_BEGIN("a camera on the far side of the surface counts nothing")
	const TriangleMesh mesh= makePlaneMesh();
	MeshCoverage coverage(mesh, 0.008f, MeshCoverageSettings());
	coverage.accumulateView(makePlaneView(lookAtOpenCV(glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f)), 0.f));
	MeshCoverageResult result;
	coverage.finish(result);

	success&= allEqual(result.support, 0);
	success&= allEqual(result.carve, 0);
	UNIT_TEST_COMPLETE()
}

bool mesh_coverage_test_azimuths_give_directions()
{
	using namespace MeshCoverageTestsInternal;

	UNIT_TEST_BEGIN("oblique views from three azimuths count three directions, a frontal one a fourth")
	const TriangleMesh mesh= makePlaneMesh();
	MeshCoverage coverage(mesh, 0.008f, MeshCoverageSettings());
	for (float azimuth : {0.f, 120.f, 240.f})
		coverage.accumulateView(makePlaneView(cameraAround(60.f, azimuth), 0.f));
	MeshCoverageResult oblique;
	coverage.finish(oblique);

	// The centre vertex sees each camera at exactly 60 degrees
	const size_t centre= 12;
	success&= oblique.support[centre] == 3;
	success&= oblique.directions[centre] == 3;
	success&= std::fabs(oblique.bestCos[centre] - 0.5f) < 1e-3f;

	coverage.accumulateView(makePlaneView(cameraAround(0.f, 0.f), 0.f));
	MeshCoverageResult withFrontal;
	coverage.finish(withFrontal);
	success&= withFrontal.directions[centre] == 4;
	success&= withFrontal.bestCos[centre] > 0.999f;
	UNIT_TEST_COMPLETE()
}

bool mesh_coverage_test_classification()
{
	UNIT_TEST_BEGIN("the class needs support, directions, a head-on view, and a small carve share")
	const MeshCoverageSettings settings;
	success&= MeshCoverage::classify(settings, 8, 2, 3, 0.8f) == eSurfaceCoverage::good;
	success&= MeshCoverage::classify(settings, 0, 4, 0, 0.f) == eSurfaceCoverage::unseen;
	success&= MeshCoverage::classify(settings, 7, 0, 3, 0.8f) == eSurfaceCoverage::weak;
	success&= MeshCoverage::classify(settings, 8, 0, 2, 0.8f) == eSurfaceCoverage::weak;
	success&= MeshCoverage::classify(settings, 8, 0, 3, 0.6f) == eSurfaceCoverage::weak;
	success&= MeshCoverage::classify(settings, 8, 5, 3, 0.8f) == eSurfaceCoverage::weak;
	UNIT_TEST_COMPLETE()
}

bool mesh_coverage_test_area_shares()
{
	using namespace MeshCoverageTestsInternal;

	UNIT_TEST_BEGIN("enough frontal and oblique views make the whole plane good")
	const TriangleMesh mesh= makePlaneMesh();
	MeshCoverage coverage(mesh, 0.008f, MeshCoverageSettings());
	for (int repeat= 0; repeat < 3; ++repeat)
	{
		coverage.accumulateView(makePlaneView(cameraAround(0.f, 0.f), 0.f));
		for (float azimuth : {0.f, 120.f, 240.f})
			coverage.accumulateView(makePlaneView(cameraAround(45.f, azimuth), 0.f));
	}
	MeshCoverageResult result;
	coverage.finish(result);

	success&= std::fabs(result.areaGood + result.areaWeak + result.areaUnseen - 1.f) < 1e-5f;
	success&= result.areaGood > 0.999f;
	UNIT_TEST_COMPLETE()
}

bool mesh_coverage_test_band_follows_voxel()
{
	using namespace MeshCoverageTestsInternal;

	UNIT_TEST_BEGIN("the support band widens to two voxels on a coarse grid")
	const TriangleMesh mesh= makePlaneMesh();
	success&= std::fabs(MeshCoverage(mesh, 0.005f, MeshCoverageSettings()).getSupportBand() - 0.015f) < 1e-6f;
	success&= std::fabs(MeshCoverage(mesh, 0.01f, MeshCoverageSettings()).getSupportBand() - 0.02f) < 1e-6f;
	UNIT_TEST_COMPLETE()
}

bool mesh_coverage_test_remap_follows_split()
{
	using namespace MeshCoverageTestsInternal;

	UNIT_TEST_BEGIN("a remap gives each split vertex its source vertex's record")
	const TriangleMesh mesh= makePlaneMesh();
	MeshCoverage coverage(mesh, 0.008f, MeshCoverageSettings());
	coverage.accumulateView(makePlaneView(cameraAround(0.f, 0.f), 0.f));
	MeshCoverageResult result;
	coverage.finish(result);
	const MeshCoverageResult before= result;

	const std::vector<uint32_t> sourceVertex= {3, 3, 0, 24};
	result.remap(sourceVertex);
	success&= result.getVertexCount() == sourceVertex.size();
	for (size_t vertex= 0; vertex < sourceVertex.size(); ++vertex)
	{
		success&= result.nearest[vertex] == before.nearest[sourceVertex[vertex]];
		success&= result.support[vertex] == before.support[sourceVertex[vertex]];
		success&= result.coverageClass[vertex] == before.coverageClass[sourceVertex[vertex]];
	}
	success&= result.areaGood == before.areaGood;
	UNIT_TEST_COMPLETE()
}

bool mesh_coverage_test_npy_round_trip()
{
	UNIT_TEST_BEGIN("an .npy array carries a 64 byte aligned header and its values")
	const std::vector<char> header= NpyWriter::makeHeader("<f4", 3);
	success&= header.size() % 64 == 0;
	success&= std::memcmp(header.data(), "\x93NUMPY\x01\x00", 8) == 0;
	const size_t dictionaryLength= (uint8_t)header[8] | ((size_t)(uint8_t)header[9] << 8);
	success&= dictionaryLength + 10 == header.size();
	success&= header.back() == '\n';
	const std::string dictionary(header.begin() + 10, header.end());
	success&= dictionary.find("'descr': '<f4'") != std::string::npos;
	success&= dictionary.find("'shape': (3,)") != std::string::npos;

	const std::vector<float> values= {1.f, -2.5f, 3.25f};
	const std::filesystem::path path= std::filesystem::temp_directory_path() / "MikanMeshCoverageTests.npy";
	success&= NpyWriter::write(path, values);
	std::ifstream file(path, std::ios::binary);
	const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();
	success&= bytes.size() == header.size() + sizeof(float) * values.size();
	if (bytes.size() == header.size() + sizeof(float) * values.size())
	{
		float readBack[3];
		std::memcpy(readBack, bytes.data() + header.size(), sizeof(readBack));
		success&= readBack[0] == values[0] && readBack[1] == values[1] && readBack[2] == values[2];
	}
	std::error_code removeError;
	std::filesystem::remove(path, removeError);
	UNIT_TEST_COMPLETE()
}

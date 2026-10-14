#include "OcclusionMeshTests.h"

#include "DepthTrackReader.h"
#include "OcclusionMeshBuilder.h"
#include "PoseTrack.h"
#include "unit_test.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <vector>

namespace OcclusionMeshTestsInternal
{
constexpr int k_depthWidth= 64;
constexpr int k_depthHeight= 48;
constexpr int k_imageWidth= 256;
constexpr int k_imageHeight= 192;
constexpr float k_floorY= -1.f;
const glm::vec3 k_boxMin(-0.4f, -1.f, 1.2f);
const glm::vec3 k_boxMax(0.4f, -0.3f, 1.9f);

// A camera-to-world in OpenCV axes looking from eye at target, X right,
// Y down, Z forward
glm::mat4 cvLookAt(const glm::vec3& eye, const glm::vec3& target)
{
	const glm::vec3 forward= glm::normalize(target - eye);
	const glm::vec3 right= glm::normalize(glm::cross(forward, glm::vec3(0.f, 1.f, 0.f)));
	const glm::vec3 down= glm::cross(forward, right);
	glm::mat4 pose(1.f);
	pose[0]= glm::vec4(right, 0.f);
	pose[1]= glm::vec4(down, 0.f);
	pose[2]= glm::vec4(forward, 0.f);
	pose[3]= glm::vec4(eye, 1.f);
	return pose;
}

PinholeIntrinsics colourIntrinsics()
{
	PinholeIntrinsics intrinsics;
	intrinsics.fx= 220.0;
	intrinsics.fy= 220.0;
	intrinsics.cx= k_imageWidth * 0.5;
	intrinsics.cy= k_imageHeight * 0.5;
	return intrinsics;
}

// Depth along +Z of the nearest of the floor plane and the box, 0 where the
// ray hits nothing within 4.5 m
void renderScene(const glm::mat4& cvCameraToWorld, std::vector<float>& outDepth)
{
	const PinholeIntrinsics intrinsics=
		scaleIntrinsics(colourIntrinsics(), k_imageWidth, k_imageHeight, k_depthWidth, k_depthHeight);
	const glm::vec3 eye(cvCameraToWorld[3]);
	outDepth.assign((size_t)k_depthWidth * k_depthHeight, 0.f);
	for (int v= 0; v < k_depthHeight; ++v)
	{
		for (int u= 0; u < k_depthWidth; ++u)
		{
			const glm::vec3 dirCamera((float)((u - intrinsics.cx) / intrinsics.fx),
									  (float)((v - intrinsics.cy) / intrinsics.fy), 1.f);
			const glm::vec3 dir= glm::mat3(cvCameraToWorld) * dirCamera;
			float best= 4.5f;

			if (std::fabs(dir.y) > 1e-6f)
			{
				const float t= (k_floorY - eye.y) / dir.y;
				if (t > 0.05f)
					best= std::min(best, t);
			}

			// Slab test against the box, in units of t along the unnormalized direction
			float tEnter= 0.f;
			float tExit= std::numeric_limits<float>::infinity();
			bool bMiss= false;
			for (int axis= 0; axis < 3 && !bMiss; ++axis)
			{
				if (std::fabs(dir[axis]) < 1e-6f)
				{
					if (eye[axis] < k_boxMin[axis] || eye[axis] > k_boxMax[axis])
						bMiss= true;
					continue;
				}
				float t0= (k_boxMin[axis] - eye[axis]) / dir[axis];
				float t1= (k_boxMax[axis] - eye[axis]) / dir[axis];
				if (t0 > t1)
					std::swap(t0, t1);
				tEnter= std::max(tEnter, t0);
				tExit= std::min(tExit, t1);
				if (tEnter > tExit)
					bMiss= true;
			}
			if (!bMiss && tEnter > 0.05f)
				best= std::min(best, tEnter);

			outDepth[(size_t)v * k_depthWidth + u]= best < 4.5f ? best : 0.f;
		}
	}
}

// Distance from a point to the scene surface (floor or box faces)
float distanceToScene(const glm::vec3& p)
{
	const float floorDistance= std::fabs(p.y - k_floorY);
	const glm::vec3 clamped= glm::clamp(p, k_boxMin, k_boxMax);
	const glm::vec3 outside= p - clamped;
	float boxDistance= glm::length(outside);
	if (boxDistance == 0.f)
	{
		// Inside: distance to the nearest face
		const glm::vec3 toMin= p - k_boxMin;
		const glm::vec3 toMax= k_boxMax - p;
		boxDistance= std::min({toMin.x, toMin.y, toMin.z, toMax.x, toMax.y, toMax.z});
	}
	return std::min(floorDistance, boxDistance);
}

void writeLittleEndianRecord(std::ofstream& depthFile, std::ofstream& confidenceFile, const std::vector<float>& depth)
{
	for (float meters : depth)
	{
		const uint16_t millimetres= meters > 0.f ? (uint16_t)std::lround(meters * 1000.f) : 0;
		const uint8_t bytes[2]= {(uint8_t)(millimetres & 0xFF), (uint8_t)(millimetres >> 8)};
		depthFile.write((const char*)bytes, 2);
		const uint8_t confidence= meters > 0.f ? 2 : 0;
		confidenceFile.write((const char*)&confidence, 1);
	}
}

float translationError(const glm::mat4& a, const glm::mat4& b) { return glm::length(glm::vec3(a[3] - b[3])); }
} // namespace OcclusionMeshTestsInternal

bool run_occlusion_mesh_unit_tests()
{
	UNIT_TEST_MODULE_BEGIN("occlusion_mesh")
	UNIT_TEST_MODULE_CALL_TEST(occlusion_mesh_test_pose_track_keeps_file_index);
	UNIT_TEST_MODULE_CALL_TEST(occlusion_mesh_test_camera_axis_conversion);
	UNIT_TEST_MODULE_CALL_TEST(occlusion_mesh_test_synthetic_take_end_to_end);
	UNIT_TEST_MODULE_END()
}

bool occlusion_mesh_test_pose_track_keeps_file_index()
{
	UNIT_TEST_BEGIN("a pose track keeps each frame's file index through the sort and round-trips a depth header")

	// Appended out of time order, so the loader sorts them
	PoseTrackWriter writer("session-a", 256, 192);
	writer.append(66666, 1000066666, glm::mat4(1.f), 220.0, 220.0, 128.0, 96.0);
	writer.append(0, 1000000000, glm::mat4(1.f), 220.0, 220.0, 128.0, 96.0);
	writer.append(33333, 1000033333, glm::mat4(1.f), 220.0, 220.0, 128.0, 96.0);

	PoseTrack plainTrack;
	success&= plainTrack.loadFromJSON(writer.toConfig());
	success&= plainTrack.getFrameCount() == 3 && !plainTrack.hasDepthTrack();
	success&= plainTrack.getFrame(0).fileIndex == 1 && plainTrack.getFrame(1).fileIndex == 2
			  && plainTrack.getFrame(2).fileIndex == 0;
	if (!success)
		fprintf(stdout, "    file indices after sort: %zu %zu %zu\n", plainTrack.getFrame(0).fileIndex,
				plainTrack.getFrame(1).fileIndex, plainTrack.getFrame(2).fileIndex);
	success&= writer.toConfig()["version"].as_integer<int>() == PoseTrack::k_minSupportedVersion;

	writer.setDepthTrack(64, 48, 1.0);
	const configuru::Config depthConfig= writer.toConfig();
	success&= depthConfig["version"].as_integer<int>() == PoseTrack::k_maxSupportedVersion;
	PoseTrack depthTrack;
	success&= depthTrack.loadFromJSON(depthConfig);
	success&= depthTrack.hasDepthTrack() && depthTrack.getDepthWidth() == 64 && depthTrack.getDepthHeight() == 48
			  && depthTrack.getDepthUnitMm() == 1.0;

	UNIT_TEST_COMPLETE()
}

bool occlusion_mesh_test_camera_axis_conversion()
{
	UNIT_TEST_BEGIN("the camera axis conversion maps a recorded pose's forward and up onto OpenCV's")

	// A recorded pose looking along world -Z with +Y up is the identity;
	// its OpenCV twin looks along world -Z too, with +Y down in camera space
	const glm::mat4 cv= OcclusionMeshBuilder::toOpenCvCameraToWorld(glm::mat4(1.f));
	const glm::vec3 forwardWorld= glm::vec3(cv * glm::vec4(0.f, 0.f, 1.f, 0.f));
	const glm::vec3 downWorld= glm::vec3(cv * glm::vec4(0.f, 1.f, 0.f, 0.f));
	const glm::vec3 rightWorld= glm::vec3(cv * glm::vec4(1.f, 0.f, 0.f, 0.f));
	success&= glm::length(forwardWorld - glm::vec3(0.f, 0.f, -1.f)) < 1e-6f;
	success&= glm::length(downWorld - glm::vec3(0.f, -1.f, 0.f)) < 1e-6f;
	success&= glm::length(rightWorld - glm::vec3(1.f, 0.f, 0.f)) < 1e-6f;
	// Applying it twice is the identity, and it keeps the camera position
	const glm::mat4 recorded= glm::translate(glm::mat4(1.f), glm::vec3(1.f, 2.f, 3.f));
	const glm::mat4 twice=
		OcclusionMeshBuilder::toOpenCvCameraToWorld(OcclusionMeshBuilder::toOpenCvCameraToWorld(recorded));
	success&= glm::length(glm::vec3(twice[3]) - glm::vec3(1.f, 2.f, 3.f)) < 1e-6f;
	success&= glm::length(glm::vec3(twice[1]) - glm::vec3(0.f, 1.f, 0.f)) < 1e-6f;

	UNIT_TEST_COMPLETE()
}

bool occlusion_mesh_test_synthetic_take_end_to_end()
{
	UNIT_TEST_BEGIN("a synthetic take fuses to its surfaces and the refined poses beat the drifted track")
	using namespace OcclusionMeshTestsInternal;

	const std::filesystem::path directory= std::filesystem::temp_directory_path() / "MikanOcclusionMeshTests";
	std::filesystem::create_directories(directory);
	const std::filesystem::path mediaPath= directory / "take.mp4";

	// Truth: an orbit around the box at about 2 m. Each frame's recorded
	// pose drifts from the truth by a random walk of a couple of
	// millimetres per frame, which is what the registration has to undo.
	constexpr int k_frameCount= 24;
	std::vector<glm::mat4> truthCv(k_frameCount);
	std::vector<glm::mat4> driftedCv(k_frameCount);
	std::mt19937 rng(7);
	std::normal_distribution<float> step(0.f, 0.002f);
	glm::vec3 drift(0.f);
	const glm::vec3 target(0.f, -0.6f, 1.55f);
	for (int frame= 0; frame < k_frameCount; ++frame)
	{
		const float angle= -0.6f + 1.2f * (float)frame / (float)(k_frameCount - 1);
		const glm::vec3 eye= target + glm::vec3(2.f * std::sin(angle), 1.1f, -2.f * std::cos(angle));
		truthCv[frame]= cvLookAt(eye, target);
		drift+= glm::vec3(step(rng), step(rng), step(rng));
		driftedCv[frame]= truthCv[frame];
		driftedCv[frame][3]+= glm::vec4(drift, 0.f);
	}

	// The track is written in a scrambled file order so the file index has
	// to carry the record mapping; the sidecars follow that file order
	std::vector<int> fileOrder(k_frameCount);
	for (int i= 0; i < k_frameCount; ++i)
		fileOrder[i]= (i * 7) % k_frameCount;

	const PinholeIntrinsics colour= colourIntrinsics();
	PoseTrackWriter writer("session-synthetic", k_imageWidth, k_imageHeight);
	writer.setDepthTrack(k_depthWidth, k_depthHeight, 1.0);
	std::ofstream depthFile(DepthTrackReader::depthSidecarPathForMedia(mediaPath), std::ios::binary);
	std::ofstream confidenceFile(DepthTrackReader::confidenceSidecarPathForMedia(mediaPath), std::ios::binary);
	std::vector<float> depth;
	for (int fileIndex= 0; fileIndex < k_frameCount; ++fileIndex)
	{
		const int frame= fileOrder[fileIndex];
		// The recorded pose is in the track's own camera convention
		const glm::mat4 recorded= OcclusionMeshBuilder::toOpenCvCameraToWorld(driftedCv[frame]);
		writer.append((int64_t)frame * 33333, 1000000000 + (int64_t)frame * 33333, recorded, colour.fx, colour.fy,
					  colour.cx, colour.cy);
		renderScene(truthCv[frame], depth);
		writeLittleEndianRecord(depthFile, confidenceFile, depth);
	}
	depthFile.close();
	confidenceFile.close();
	const std::filesystem::path trackPath= directory / "take.pose.json";
	success&= writer.writeToFile(trackPath);

	PoseTrack track;
	success&= track.loadFromFile(trackPath) && track.hasDepthTrack() && track.getFrameCount() == k_frameCount;
	// Sorted by time, the frame at track index i is truth frame i, written at file index fileOrder^-1(i)
	if (success)
	{
		for (int frame= 0; frame < k_frameCount && success; ++frame)
			success&= fileOrder[track.getFrame(frame).fileIndex] == frame;
		if (!success)
			fprintf(stdout, "    the sorted track does not map back to the file order\n");
	}

	DepthTrackReader reader;
	success&= reader.open(DepthTrackReader::depthSidecarPathForMedia(mediaPath),
						  DepthTrackReader::confidenceSidecarPathForMedia(mediaPath), k_depthWidth, k_depthHeight, 1.0,
						  k_frameCount);

	OcclusionMeshBuildSettings settings;
	settings.volume.voxelSize= 0.02f;
	settings.volume.truncation= 0.08f;
	settings.keyframeStride= 1;
	settings.minComponentTriangles= 50;
	settings.warmupFrames= 2;
	settings.registration.minCorrespondences= 300;
	OcclusionMeshBuildResult result;
	if (success)
	{
		success&= OcclusionMeshBuilder::build(track, reader, settings, nullptr, result);
		if (!success)
			fprintf(stdout, "    build failed: %s\n", result.failureReason.c_str());
	}

	if (success)
	{
		// The fused surface sits on the floor and the box
		int farVertices= 0;
		for (const glm::vec3& vertex : result.mesh.vertices)
		{
			if (distanceToScene(vertex) > 1.5f * settings.volume.voxelSize)
				++farVertices;
		}
		const float farFraction=
			result.mesh.vertices.empty() ? 1.f : (float)farVertices / (float)result.mesh.vertices.size();
		success&= result.mesh.getTriangleCount() > 500 && farFraction < 0.05f;
		if (!success)
			fprintf(stdout, "    %zu triangles, %.1f%% of vertices off the surfaces\n", result.mesh.getTriangleCount(),
					farFraction * 100.f);

		// The refined poses are indexed by file index and closer to the truth
		success&= result.bHasRefinedPoses && result.refinedCameraToWorld.size() == k_frameCount;
		if (success)
		{
			float driftedError= 0.f;
			float refinedError= 0.f;
			for (int fileIndex= 0; fileIndex < k_frameCount; ++fileIndex)
			{
				const int frame= fileOrder[fileIndex];
				const glm::mat4 truthRecorded= OcclusionMeshBuilder::toOpenCvCameraToWorld(truthCv[frame]);
				const glm::mat4 driftedRecorded= OcclusionMeshBuilder::toOpenCvCameraToWorld(driftedCv[frame]);
				driftedError+= translationError(driftedRecorded, truthRecorded);
				refinedError+= translationError(result.refinedCameraToWorld[fileIndex], truthRecorded);
			}
			driftedError/= k_frameCount;
			refinedError/= k_frameCount;
			success&= refinedError < driftedError * 0.6f;
			fprintf(stdout, "    mean position error: drifted %.1f mm, refined %.1f mm (%d accepted, %d rejected)\n",
					driftedError * 1000.f, refinedError * 1000.f, result.stats.registrationsAccepted,
					result.stats.registrationsRejected);
		}
	}

	reader.close();
	std::error_code ec;
	std::filesystem::remove_all(directory, ec);

	UNIT_TEST_COMPLETE()
}

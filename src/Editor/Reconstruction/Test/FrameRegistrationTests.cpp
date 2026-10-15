#include "FrameRegistrationTests.h"
#include "CaptureArchiveWriter.h"
#include "CaptureManifest.h"
#include "DepthTrackReader.h"
#include "PoseTrack.h"
#include "FrameToModelRegistration.h"
#include "SmallMotion.h"
#include "TsdfVolume.h"
#include "unit_test.h"

#include <glm/glm.hpp>

#include <opencv2/core.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <random>
#include <system_error>
#include <vector>

namespace FrameRegistrationTestsInternal
{
const int k_width= 256;
const int k_height= 192;

// An axis-aligned plane: points whose coordinate on axis equals value
struct AxisPlane
{
	int axis;
	double value;
};

// Walls x = 2 and z = 2 and the floor y = -1, in OpenCV axes
const std::vector<AxisPlane> k_cornerPlanes= {{0, 2.0}, {2, 2.0}, {1, -1.0}};
const std::vector<AxisPlane> k_floorPlanes= {{1, -1.0}};

PinholeIntrinsics makeIntrinsics()
{
	PinholeIntrinsics intrinsics;
	intrinsics.fx= 210.0;
	intrinsics.fy= 210.0;
	intrinsics.cx= 128.0;
	intrinsics.cy= 96.0;
	return intrinsics;
}

// Depth of axis-aligned planes seen from a camera in OpenCV axes, 0 where
// no plane is hit and beyond 4.5 m
cv::Mat renderPlanes(const PinholeIntrinsics& intrinsics, const glm::dmat4& cameraToWorld, int width, int height,
					 const std::vector<AxisPlane>& planes)
{
	const glm::dmat3 rotation(cameraToWorld);
	const glm::dvec3 origin(cameraToWorld[3]);
	const double infinity= std::numeric_limits<double>::infinity();

	cv::Mat depth(height, width, CV_32FC1);
	for (int v= 0; v < height; ++v)
	{
		float* depthRow= depth.ptr<float>(v);
		for (int u= 0; u < width; ++u)
		{
			const glm::dvec3 rayCamera(((double)u - intrinsics.cx) / intrinsics.fx,
									   ((double)v - intrinsics.cy) / intrinsics.fy, 1.0);
			const glm::dvec3 rayWorld= rotation * rayCamera;

			double best= infinity;
			for (const AxisPlane& plane : planes)
			{
				const double direction= rayWorld[plane.axis];
				double t= std::fabs(direction) > 1e-9 ? (plane.value - origin[plane.axis]) / direction : infinity;
				if (!(t > 0.05))
					t= infinity;
				best= std::min(best, t);
			}

			// The ray has unit Z in the camera, so the distance along it is the depth
			double value= std::isfinite(best) ? best : 0.0;
			if (value > 4.5)
				value= 0.0;
			depthRow[u]= (float)value;
		}
	}

	return depth;
}

// Camera-to-world pose at position, turned by yaw about Y then pitch about X
glm::dmat4 lookFrom(const glm::dvec3& position, double yawDeg, double pitchDeg)
{
	const double yaw= glm::radians(yawDeg);
	const double pitch= glm::radians(pitchDeg);
	// glm matrices are built from columns
	const glm::dmat3 yawRotation(glm::dvec3(std::cos(yaw), 0.0, -std::sin(yaw)), glm::dvec3(0.0, 1.0, 0.0),
								 glm::dvec3(std::sin(yaw), 0.0, std::cos(yaw)));
	const glm::dmat3 pitchRotation(glm::dvec3(1.0, 0.0, 0.0), glm::dvec3(0.0, std::cos(pitch), std::sin(pitch)),
								   glm::dvec3(0.0, -std::sin(pitch), std::cos(pitch)));

	glm::dmat4 pose(yawRotation * pitchRotation);
	pose[3]= glm::dvec4(position, 1.0);
	return pose;
}

// Fusion weight 1 wherever the depth is positive
cv::Mat weightFromDepth(const cv::Mat& depth)
{
	cv::Mat weight;
	cv::Mat mask= depth > 0.f;
	mask.convertTo(weight, CV_32F, 1.0 / 255.0);
	return weight;
}

void addDepthNoise(cv::Mat& depth, std::mt19937& rng, float sigma)
{
	std::normal_distribution<float> noise(0.f, sigma);
	for (int y= 0; y < depth.rows; ++y)
	{
		float* depthRow= depth.ptr<float>(y);
		for (int x= 0; x < depth.cols; ++x)
		{
			const float sample= noise(rng);
			if (depthRow[x] > 0.f)
				depthRow[x]+= sample;
		}
	}
}

std::unique_ptr<TsdfVolume> makeVolume()
{
	TsdfVolumeConfig config;
	config.voxelSize= 0.02f;
	config.truncation= 0.08f;
	return std::make_unique<TsdfVolume>(glm::vec3(-1.5f, -1.5f, -1.5f), glm::vec3(2.5f, 1.5f, 2.5f), config);
}

void fuse(TsdfVolume& volume, const glm::dmat4& pose, const std::vector<AxisPlane>& planes)
{
	const PinholeIntrinsics intrinsics= makeIntrinsics();
	const cv::Mat depth= renderPlanes(intrinsics, pose, k_width, k_height, planes);
	volume.integrate(depth, weightFromDepth(depth), intrinsics, glm::mat4(pose));
}

// Rotation angle of a rotation matrix, stable near identity
double rotationErrorDeg(const glm::dmat3& rotation)
{
	const glm::dvec3 antisymmetric(rotation[1][2] - rotation[2][1], rotation[2][0] - rotation[0][2],
								   rotation[0][1] - rotation[1][0]);
	const double cosAngle= (rotation[0][0] + rotation[1][1] + rotation[2][2] - 1.0) / 2.0;
	return glm::degrees(std::atan2(glm::length(antisymmetric) / 2.0, cosAngle));
}

bool matricesEqual(const glm::mat4& a, const glm::mat4& b)
{
	for (int col= 0; col < 4; ++col)
	{
		for (int row= 0; row < 4; ++row)
		{
			if (a[col][row] != b[col][row])
				return false;
		}
	}
	return true;
}

// The room corner fused from six views about the origin and ray cast from
// the third, shared by the tests that register against it
struct CornerFixture
{
	std::vector<glm::dmat4> poses;
	ModelMap model;
};

const CornerFixture& getCornerFixture()
{
	static std::unique_ptr<CornerFixture> fixture;
	if (!fixture)
	{
		fixture= std::make_unique<CornerFixture>();
		std::unique_ptr<TsdfVolume> volume= makeVolume();
		for (double yaw : {20.0, 35.0, 50.0})
		{
			for (double pitch : {10.0, 25.0})
			{
				fixture->poses.push_back(lookFrom(glm::dvec3(0.0), yaw, pitch));
				fuse(*volume, fixture->poses.back(), k_cornerPlanes);
			}
		}
		volume->raycast(makeIntrinsics(), glm::mat4(fixture->poses[2]), k_width, k_height, 0.5f, 2.f, fixture->model);
	}
	return *fixture;
}

// A frame of the corner from a pose a few centimetres and about a degree off
// the fixture's ray cast pose, with 3 mm depth noise
glm::dmat4 getDisplacedCornerPose() { return lookFrom(glm::dvec3(0.03, -0.02, 0.04), 36.5, 11.0); }

cv::Mat renderDisplacedCornerFrame()
{
	cv::Mat frame= renderPlanes(makeIntrinsics(), getDisplacedCornerPose(), k_width, k_height, k_cornerPlanes);
	std::mt19937 rng(1);
	addDepthNoise(frame, rng, 0.003f);
	return frame;
}

// Packs the two depth tracks into a trackcamera capture with a placeholder
// movie and a one-frame track, and returns the two members' table entries
bool writeDepthCapture(const std::filesystem::path& capturePath, const std::vector<uint8_t>& depthBytes,
					   const std::vector<uint8_t>& confidenceBytes, CaptureArchiveMember& outDepthMember,
					   CaptureArchiveMember& outConfidenceMember)
{
	CaptureManifest manifest;
	manifest.kind= eCaptureKind::trackcamera;
	manifest.sessionId= "session-depth";
	manifest.poseSpace= eCapturePoseSpace::arkit_world;
	manifest.members[eCaptureMemberRole::movie]= "movie.mp4";
	manifest.members[eCaptureMemberRole::movie_pose]= "movie.pose.json";
	manifest.members[eCaptureMemberRole::depth]= "movie.depth.bin";
	manifest.members[eCaptureMemberRole::confidence]= "movie.conf.bin";

	PoseTrackWriter trackWriter("session-depth", 8, 6);
	trackWriter.append(0, 0, glm::mat4(1.f), 4.0, 4.0, 4.0, 3.0);
	const std::string trackJson= configuru::dump_string(trackWriter.toConfig(), configuru::JSON);
	const std::string moviePlaceholder= "not a movie";

	std::string error;
	CaptureArchiveWriter archive;
	bool bOk= archive.open(capturePath, manifest, error);
	bOk&= archive.addMemberFromMemory("movie.mp4", moviePlaceholder.data(), moviePlaceholder.size(), error);
	bOk&= archive.addMemberFromMemory("movie.pose.json", trackJson.data(), trackJson.size(), error);
	bOk&= archive.addMemberFromMemory("movie.depth.bin", depthBytes.data(), depthBytes.size(), error);
	bOk&= archive.addMemberFromMemory("movie.conf.bin", confidenceBytes.data(), confidenceBytes.size(), error);
	bOk&= archive.finish(error);
	if (!bOk)
		return false;

	CaptureArchiveReader reader;
	if (!reader.open(capturePath, error))
		return false;
	const CaptureArchiveMember* depthMember= reader.findMember("movie.depth.bin");
	const CaptureArchiveMember* confidenceMember= reader.findMember("movie.conf.bin");
	if (depthMember == nullptr || confidenceMember == nullptr)
		return false;
	outDepthMember= *depthMember;
	outConfidenceMember= *confidenceMember;
	return true;
}
} // namespace FrameRegistrationTestsInternal

// ---- Test cases ----

// The exponential and log maps invert each other on a small twist, the
// rotation acts in the right handedness, and the angle reads in degrees.
bool frame_registration_test_small_motion()
{
	UNIT_TEST_BEGIN("small motion exp and log round trip")

	const double twist[6]= {0.01, -0.02, 0.015, 0.03, -0.01, 0.02};
	const glm::dmat4 transform= SmallMotion::expSe3(twist);
	const glm::dvec3 omega= SmallMotion::logSo3(glm::dmat3(transform));
	if (glm::length(omega - glm::dvec3(twist[0], twist[1], twist[2])) > 1e-9)
	{
		fprintf(stdout, "    log of exp gave (%.9f, %.9f, %.9f)\n", omega.x, omega.y, omega.z);
		success= false;
	}

	// A quarter turn about +Z takes +X to +Y
	const double quarterTurn[6]= {0.0, 0.0, glm::radians(90.0), 0.0, 0.0, 0.0};
	const glm::dvec4 turned= SmallMotion::expSe3(quarterTurn) * glm::dvec4(1.0, 0.0, 0.0, 1.0);
	if (glm::length(glm::dvec3(turned) - glm::dvec3(0.0, 1.0, 0.0)) > 1e-9)
	{
		fprintf(stdout, "    quarter turn took +X to (%.6f, %.6f, %.6f)\n", turned.x, turned.y, turned.z);
		success= false;
	}

	// A pure translation moves a point by exactly the translation
	const double shift[6]= {0.0, 0.0, 0.0, 0.1, -0.2, 0.3};
	const glm::dvec4 moved= SmallMotion::expSe3(shift) * glm::dvec4(1.0, 2.0, 3.0, 1.0);
	if (glm::length(glm::dvec3(moved) - glm::dvec3(1.1, 1.8, 3.3)) > 1e-12)
	{
		fprintf(stdout, "    translation moved the point to (%.6f, %.6f, %.6f)\n", moved.x, moved.y, moved.z);
		success= false;
	}

	const glm::dvec3 axis= glm::normalize(glm::dvec3(1.0, 2.0, -0.5));
	const glm::dvec3 tenDegrees= axis * glm::radians(10.0);
	const double rotationTwist[6]= {tenDegrees.x, tenDegrees.y, tenDegrees.z, 0.0, 0.0, 0.0};
	const double angle= SmallMotion::rotationAngleDeg(glm::dmat3(SmallMotion::expSe3(rotationTwist)));
	if (std::fabs(angle - 10.0) > 1e-6)
	{
		fprintf(stdout, "    10 degree rotation read as %.9f degrees\n", angle);
		success= false;
	}

	UNIT_TEST_COMPLETE()
}

// A frame of a room corner from a displaced pose registers back onto the
// fused corner to well under a centimetre and a tenth of a degree.
bool frame_registration_test_corner_recovers_displacement()
{
	UNIT_TEST_BEGIN("corner frame registers onto the fused model")

	const FrameRegistrationTestsInternal::CornerFixture& fixture= FrameRegistrationTestsInternal::getCornerFixture();
	const glm::dmat4 prior= fixture.poses[2];
	const glm::dmat4 truth= FrameRegistrationTestsInternal::getDisplacedCornerPose();
	const cv::Mat frame= FrameRegistrationTestsInternal::renderDisplacedCornerFrame();

	RegistrationConfig config;
	const RegistrationResult result= FrameToModelRegistration::registerFrame(
		frame, FrameRegistrationTestsInternal::weightFromDepth(frame), FrameRegistrationTestsInternal::makeIntrinsics(),
		glm::mat4(prior), fixture.model, glm::mat4(prior), config);

	const glm::dmat4 residual= glm::inverse(truth) * glm::dmat4(result.cameraToWorld);
	const double translationErrorMm= glm::length(glm::dvec3(residual[3])) * 1000.0;
	const double rotationErrorDeg= FrameRegistrationTestsInternal::rotationErrorDeg(glm::dmat3(residual));
	if (!result.bAccepted || translationErrorMm > 1.0 || rotationErrorDeg > 0.1)
	{
		const glm::dmat4 start= glm::inverse(truth) * prior;
		fprintf(stdout,
				"    accepted %d, %d iterations, %d correspondences, rms %.2f -> %.2f mm; pose error %.1f mm %.3f deg "
				"-> %.3f mm %.4f deg\n",
				result.bAccepted ? 1 : 0, result.iterations, result.correspondences, result.rmsBeforeM * 1000.f,
				result.rmsAfterM * 1000.f, glm::length(glm::dvec3(start[3])) * 1000.0,
				FrameRegistrationTestsInternal::rotationErrorDeg(glm::dmat3(start)), translationErrorMm,
				rotationErrorDeg);
		success= false;
	}

	UNIT_TEST_COMPLETE()
}

// A frame that sees only the floor corrects its height, and the prior holds
// the in-plane offset the floor cannot observe.
bool frame_registration_test_floor_only_keeps_prior_in_plane()
{
	UNIT_TEST_BEGIN("floor-only frame corrects height and holds the in-plane prior")

	const PinholeIntrinsics intrinsics= FrameRegistrationTestsInternal::makeIntrinsics();
	const glm::dmat4 floorPose= FrameRegistrationTestsInternal::lookFrom(glm::dvec3(0.0), 35.0, 45.0);

	ModelMap model;
	{
		std::unique_ptr<TsdfVolume> volume= FrameRegistrationTestsInternal::makeVolume();
		FrameRegistrationTestsInternal::fuse(*volume, floorPose, FrameRegistrationTestsInternal::k_floorPlanes);
		FrameRegistrationTestsInternal::fuse(
			*volume, FrameRegistrationTestsInternal::lookFrom(glm::dvec3(0.1, 0.0, 0.05), 30.0, 50.0),
			FrameRegistrationTestsInternal::k_floorPlanes);
		volume->raycast(intrinsics, glm::mat4(floorPose), FrameRegistrationTestsInternal::k_width,
						FrameRegistrationTestsInternal::k_height, 0.5f, 2.f, model);
	}

	// 3 cm along the floor, 2 cm off it
	const glm::dmat4 truth= FrameRegistrationTestsInternal::lookFrom(glm::dvec3(0.03, 0.02, 0.0), 35.0, 45.0);
	cv::Mat frame= FrameRegistrationTestsInternal::renderPlanes(
		intrinsics, truth, FrameRegistrationTestsInternal::k_width, FrameRegistrationTestsInternal::k_height,
		FrameRegistrationTestsInternal::k_floorPlanes);
	std::mt19937 rng(2);
	FrameRegistrationTestsInternal::addDepthNoise(frame, rng, 0.003f);

	RegistrationConfig config;
	const RegistrationResult result=
		FrameToModelRegistration::registerFrame(frame, FrameRegistrationTestsInternal::weightFromDepth(frame),
												intrinsics, glm::mat4(floorPose), model, glm::mat4(floorPose), config);

	const glm::dvec3 offset= glm::dvec3(glm::dmat4(result.cameraToWorld)[3]) - glm::dvec3(floorPose[3]);
	if (!result.bAccepted || std::fabs(offset.y - 0.02) > 0.004 || std::fabs(offset.x) > 0.004
		|| std::fabs(offset.z) > 0.004 || result.rotationDeg > 0.2f)
	{
		fprintf(stdout,
				"    accepted %d, %d correspondences, offset x %.2f mm (truth 30, unobservable), y %.2f mm (truth 20), "
				"z %.2f mm (truth 0, unobservable), rotation %.4f deg\n",
				result.bAccepted ? 1 : 0, result.correspondences, offset.x * 1000.0, offset.y * 1000.0,
				offset.z * 1000.0, result.rotationDeg);
		success= false;
	}

	UNIT_TEST_COMPLETE()
}

// Too few correspondences, or a correction past the bound, keeps the prior
// exactly.
bool frame_registration_test_rejection_keeps_prior()
{
	UNIT_TEST_BEGIN("rejected registrations return the prior")

	const FrameRegistrationTestsInternal::CornerFixture& fixture= FrameRegistrationTestsInternal::getCornerFixture();
	const glm::mat4 prior(fixture.poses[2]);
	const cv::Mat frame= FrameRegistrationTestsInternal::renderDisplacedCornerFrame();
	const cv::Mat weight= FrameRegistrationTestsInternal::weightFromDepth(frame);
	const PinholeIntrinsics intrinsics= FrameRegistrationTestsInternal::makeIntrinsics();

	{
		RegistrationConfig config;
		config.minCorrespondences= frame.rows * frame.cols + 1;
		const RegistrationResult result=
			FrameToModelRegistration::registerFrame(frame, weight, intrinsics, prior, fixture.model, prior, config);
		if (result.bAccepted || !FrameRegistrationTestsInternal::matricesEqual(result.cameraToWorld, prior))
		{
			fprintf(stdout, "    too few correspondences: accepted %d with %d correspondences, prior kept %d\n",
					result.bAccepted ? 1 : 0, result.correspondences,
					FrameRegistrationTestsInternal::matricesEqual(result.cameraToWorld, prior) ? 1 : 0);
			success= false;
		}
	}

	{
		RegistrationConfig config;
		config.maxCorrectionM= 0.001f;
		const RegistrationResult result=
			FrameToModelRegistration::registerFrame(frame, weight, intrinsics, prior, fixture.model, prior, config);
		if (result.bAccepted || !FrameRegistrationTestsInternal::matricesEqual(result.cameraToWorld, prior)
			|| result.translationM <= config.maxCorrectionM)
		{
			fprintf(stdout, "    correction past the bound: accepted %d, correction %.2f mm, prior kept %d\n",
					result.bAccepted ? 1 : 0, result.translationM * 1000.f,
					FrameRegistrationTestsInternal::matricesEqual(result.cameraToWorld, prior) ? 1 : 0);
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

// Two little-endian records round trip through the reader, read at their
// offsets inside the capture, and a member of the wrong size is refused.
bool frame_registration_test_depth_track_reader()
{
	UNIT_TEST_BEGIN("depth track reader round trip")

	const std::filesystem::path folder= std::filesystem::temp_directory_path() / "MikanFrameRegistrationTests";
	std::error_code error;
	std::filesystem::remove_all(folder, error);
	std::filesystem::create_directories(folder, error);

	const int width= 4;
	const int height= 3;
	const int pixelCount= width * height;
	const int recordCount= 2;

	// Values above 255 exercise the byte order, and a zero must stay zero
	auto depthValue= [](int record, int pixel) -> uint16_t
	{ return pixel == 5 ? (uint16_t)0 : (uint16_t)(1000 * record + 300 * pixel + 7); };
	auto confidenceValue= [](int record, int pixel) -> uint8_t { return (uint8_t)((record + pixel) % 3); };

	std::vector<uint8_t> depthBytes;
	std::vector<uint8_t> confidenceBytes;
	for (int record= 0; record < recordCount; ++record)
	{
		for (int pixel= 0; pixel < pixelCount; ++pixel)
		{
			const uint16_t value= depthValue(record, pixel);
			depthBytes.push_back((uint8_t)(value & 0xFF));
			depthBytes.push_back((uint8_t)(value >> 8));
			confidenceBytes.push_back(confidenceValue(record, pixel));
		}
	}

	const std::filesystem::path capturePath= folder / "take.trackcamera";
	CaptureArchiveMember depthMember;
	CaptureArchiveMember confidenceMember;
	if (!FrameRegistrationTestsInternal::writeDepthCapture(capturePath, depthBytes, confidenceBytes, depthMember,
														   confidenceMember))
	{
		fprintf(stdout, "    packing the depth capture failed\n");
		success= false;
	}

	{
		DepthTrackReader reader;
		if (!reader.open(capturePath, depthMember, confidenceMember, width, height, 2.0, recordCount))
		{
			fprintf(stdout, "    open failed on well-formed depth members\n");
			success= false;
		}
		else
		{
			cv::Mat depth;
			cv::Mat confidence;
			if (!reader.readRecord(1, depth, confidence))
			{
				fprintf(stdout, "    reading record 1 failed\n");
				success= false;
			}
			else if (depth.type() != CV_32FC1 || confidence.type() != CV_8UC1 || depth.rows != height
					 || depth.cols != width || confidence.rows != height || confidence.cols != width)
			{
				fprintf(stdout, "    record 1 came back with the wrong type or size\n");
				success= false;
			}
			else
			{
				cv::Mat weight;
				DepthTrackReader::confidenceToWeight(confidence, weight);

				for (int pixel= 0; pixel < pixelCount; ++pixel)
				{
					const int y= pixel / width;
					const int x= pixel % width;
					const float expectedDepth= (float)(depthValue(1, pixel) * 0.002);
					const uint8_t expectedConfidence= confidenceValue(1, pixel);
					const float expectedWeight= 0.5f * (float)expectedConfidence;
					if (std::fabs(depth.at<float>(y, x) - expectedDepth) > 1e-6f
						|| confidence.at<uint8_t>(y, x) != expectedConfidence
						|| std::fabs(weight.at<float>(y, x) - expectedWeight) > 1e-6f)
					{
						fprintf(stdout, "    pixel %d: depth %.6f (expected %.6f), confidence %d (expected %d)\n",
								pixel, depth.at<float>(y, x), expectedDepth, (int)confidence.at<uint8_t>(y, x),
								(int)expectedConfidence);
						success= false;
					}
				}
			}

			if (reader.readRecord(2, depth, confidence))
			{
				fprintf(stdout, "    reading past the last record succeeded\n");
				success= false;
			}
			reader.close();
		}

		// More records than the members hold
		if (reader.open(capturePath, depthMember, confidenceMember, width, height, 2.0, recordCount + 1))
		{
			fprintf(stdout, "    open accepted a record count larger than the members\n");
			success= false;
			reader.close();
		}

		// A confidence member one byte short
		std::vector<uint8_t> shortConfidence(confidenceBytes.begin(), confidenceBytes.end() - 1);
		const std::filesystem::path shortPath= folder / "short.trackcamera";
		CaptureArchiveMember shortDepthMember;
		CaptureArchiveMember shortConfidenceMember;
		if (!FrameRegistrationTestsInternal::writeDepthCapture(shortPath, depthBytes, shortConfidence, shortDepthMember,
															   shortConfidenceMember))
		{
			fprintf(stdout, "    packing the truncated depth capture failed\n");
			success= false;
		}
		else if (reader.open(shortPath, shortDepthMember, shortConfidenceMember, width, height, 2.0, recordCount))
		{
			fprintf(stdout, "    open accepted a truncated confidence member\n");
			success= false;
			reader.close();
		}
	}

	std::filesystem::remove_all(folder, error);

	UNIT_TEST_COMPLETE()
}

// ---- Test Suite ----

bool run_frame_registration_unit_tests()
{
	UNIT_TEST_MODULE_BEGIN("frame_registration")
	UNIT_TEST_MODULE_CALL_TEST(frame_registration_test_small_motion);
	UNIT_TEST_MODULE_CALL_TEST(frame_registration_test_corner_recovers_displacement);
	UNIT_TEST_MODULE_CALL_TEST(frame_registration_test_floor_only_keeps_prior_in_plane);
	UNIT_TEST_MODULE_CALL_TEST(frame_registration_test_rejection_keeps_prior);
	UNIT_TEST_MODULE_CALL_TEST(frame_registration_test_depth_track_reader);
	UNIT_TEST_MODULE_END()
}

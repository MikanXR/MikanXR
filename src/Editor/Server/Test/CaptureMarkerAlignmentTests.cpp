#include "CaptureMarkerAlignmentTests.h"
#include "unit_test.h"

#include "CalibrationPatternFinder.h"
#include "CaptureMarkerAlignment.h"
#include "CapturePacker.h"
#include "FileVideoSourceComponent.h"
#include "MarkerComponent.h"
#include "OcclusionMeshBuilder.h"
#include "PoseTrack.h"

#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/objdetect/aruco_detector.hpp"

#include "glm/ext/matrix_transform.hpp"
#include "glm/geometric.hpp"

#include <assert.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace CaptureMarkerAlignmentTestsInternal
{
const int k_imageWidth= 640;
const int k_imageHeight= 480;
const double k_focalLength= 500.0;
const int k_arucoId= 2;
const float k_markerLengthMM= 47.f;
const eCharucoDictionaryType k_dictionaryType= eCharucoDictionaryType::DICT_6X6;
const char* k_sessionId= "5a0c2c3e-2222-4b4b-8c8c-0123456789ab";

// A throwaway folder for the capture fixture, removed on destruction
struct FixtureDir
{
	std::filesystem::path root;

	FixtureDir()
	{
		root= std::filesystem::temp_directory_path() / "mikan_capture_marker_alignment_tests";
		std::filesystem::remove_all(root);
		std::filesystem::create_directories(root);
	}

	~FixtureDir()
	{
		std::error_code ignored;
		std::filesystem::remove_all(root, ignored);
	}
};

MarkerDefinitionConstPtr makeOriginMarker()
{
	auto marker= std::make_shared<MarkerDefinition>();
	marker->setArucoId(k_arucoId);
	marker->setLengthMM(k_markerLengthMM);
	return marker;
}

// The marker lies flat on a table in the take's world, centred at markerCentre
// with its face up, the way MikanARStreamer's operator lays it down
const glm::vec3 k_markerCentre(0.4f, -0.25f, -0.3f);

// An ARKit camera (+Y up, looking down its -Z) at eye, looking at the marker
glm::mat4 makeCameraToWorld(const glm::vec3& eye)
{
	return glm::inverse(glm::lookAt(eye, k_markerCentre, glm::vec3(0.f, 1.f, 0.f)));
}

// Renders the marker shot the phone would have taken from cameraToWorld: the
// marker with a white quiet zone on a grey table, through a pinhole camera
cv::Mat renderMarkerShot(const glm::mat4& cameraToWorld)
{
	const int markerPixels= 240;
	const int marginPixels= 60;
	cv::Mat markerImage;
	cv::aruco::generateImageMarker(*CalibrationPatternFinder::getArucoDictionary(k_dictionaryType), k_arucoId,
								   markerPixels, markerImage, 1);
	cv::Mat canvas(markerPixels + 2 * marginPixels, markerPixels + 2 * marginPixels, CV_8UC1, cv::Scalar(255));
	markerImage.copyTo(canvas(cv::Rect(marginPixels, marginPixels, markerPixels, markerPixels)));

	// The canvas corners on the table, the marker's top edge toward -Z, so the
	// camera on the +Z side sees it unmirrored
	const float halfMarker= 0.5f * k_markerLengthMM * 0.001f;
	const float halfCanvas= halfMarker * (float)canvas.cols / (float)markerPixels;
	const glm::vec3 canvasCorners[4]= {
		k_markerCentre + glm::vec3(-halfCanvas, 0.f, -halfCanvas),
		k_markerCentre + glm::vec3(halfCanvas, 0.f, -halfCanvas),
		k_markerCentre + glm::vec3(halfCanvas, 0.f, halfCanvas),
		k_markerCentre + glm::vec3(-halfCanvas, 0.f, halfCanvas),
	};

	const glm::mat4 worldToCamera= glm::inverse(cameraToWorld);
	std::vector<cv::Point2f> imageCorners;
	for (const glm::vec3& corner : canvasCorners)
	{
		// ARKit camera axes to OpenCV's: +Y down, +Z forward
		const glm::vec4 cameraPoint= worldToCamera * glm::vec4(corner, 1.f);
		const double x= cameraPoint.x, y= -cameraPoint.y, z= -cameraPoint.z;
		imageCorners.push_back(cv::Point2f((float)(k_focalLength * x / z + 0.5 * k_imageWidth),
										   (float)(k_focalLength * y / z + 0.5 * k_imageHeight)));
	}
	const std::vector<cv::Point2f> canvasPixels= {cv::Point2f(0.f, 0.f), cv::Point2f((float)canvas.cols, 0.f),
												  cv::Point2f((float)canvas.cols, (float)canvas.rows),
												  cv::Point2f(0.f, (float)canvas.rows)};

	cv::Mat shot(k_imageHeight, k_imageWidth, CV_8UC1, cv::Scalar(110));
	cv::warpPerspective(canvas, shot, cv::getPerspectiveTransform(canvasPixels, imageCorners), shot.size(),
						cv::INTER_LINEAR, cv::BORDER_TRANSPARENT);

	cv::Mat shotBgr;
	cv::cvtColor(shot, shotBgr, cv::COLOR_GRAY2BGR);
	return shotBgr;
}

PoseTrackFrame makeShotFrame(const glm::mat4& cameraToWorld)
{
	PoseTrackFrame frame;
	frame.cameraToWorld= cameraToWorld;
	frame.fx= k_focalLength;
	frame.fy= k_focalLength;
	frame.cx= 0.5 * k_imageWidth;
	frame.cy= 0.5 * k_imageHeight;
	return frame;
}

bool solveShot(const glm::vec3& eye, glm::mat4& outWorldToStage)
{
	const glm::mat4 cameraToWorld= makeCameraToWorld(eye);
	std::string error;
	return CaptureMarkerAlignment::solveWorldToStage(renderMarkerShot(cameraToWorld), makeShotFrame(cameraToWorld),
													 k_imageWidth, k_imageHeight, makeOriginMarker(), k_dictionaryType,
													 outWorldToStage, error);
}

float maxElementDifference(const glm::mat4& a, const glm::mat4& b)
{
	float maxDifference= 0.f;
	for (int column= 0; column < 4; ++column)
	{
		for (int row= 0; row < 4; ++row)
		{
			maxDifference= std::max(maxDifference, std::fabs(a[column][row] - b[column][row]));
		}
	}
	return maxDifference;
}

void writeBytes(const std::filesystem::path& path, const std::string& content)
{
	std::ofstream file(path, std::ios::binary);
	file << content;
}
} // namespace CaptureMarkerAlignmentTestsInternal

bool run_capture_marker_alignment_tests()
{
	UNIT_TEST_MODULE_BEGIN("capture_marker_alignment")
	UNIT_TEST_MODULE_CALL_TEST(capture_marker_alignment_test_marker_shot_solves_the_stage);
	UNIT_TEST_MODULE_CALL_TEST(capture_marker_alignment_test_shot_without_marker_fails);
	UNIT_TEST_MODULE_CALL_TEST(capture_marker_alignment_test_take_aligns_itself_for_its_session);
	UNIT_TEST_MODULE_CALL_TEST(capture_marker_alignment_test_height_snaps_onto_the_table);
	UNIT_TEST_MODULE_END()
}

bool capture_marker_alignment_test_marker_shot_solves_the_stage()
{
	using namespace CaptureMarkerAlignmentTestsInternal;

	UNIT_TEST_BEGIN("a marker shot puts the stage origin on the marker, the same from any viewpoint")
	glm::mat4 fromFront, fromSide;
	success= solveShot(k_markerCentre + glm::vec3(0.f, 0.25f, 0.2f), fromFront);
	success&= solveShot(k_markerCentre + glm::vec3(0.12f, 0.3f, 0.15f), fromSide);

	// The marker is the stage origin
	const glm::vec3 markerInStage= glm::vec3(fromFront * glm::vec4(k_markerCentre, 1.f));
	success&= glm::length(markerInStage) < 0.002f;

	// The marker lies face up, so the stage is upright in the take's world
	const glm::vec3 worldUpInStage= glm::normalize(glm::vec3(fromFront * glm::vec4(0.f, 1.f, 0.f, 0.f)));
	success&= glm::dot(worldUpInStage, glm::vec3(0.f, 1.f, 0.f)) > std::cos(glm::radians(1.f));

	// One world, one marker: the two shots solve the same offset
	success&= maxElementDifference(fromFront, fromSide) < 0.01f;
	UNIT_TEST_COMPLETE()
}

bool capture_marker_alignment_test_shot_without_marker_fails()
{
	using namespace CaptureMarkerAlignmentTestsInternal;

	UNIT_TEST_BEGIN("a shot without the marker, or of the wrong size, solves nothing")
	const glm::mat4 cameraToWorld= makeCameraToWorld(k_markerCentre + glm::vec3(0.f, 0.25f, 0.2f));
	const cv::Mat emptyTable(k_imageHeight, k_imageWidth, CV_8UC3, cv::Scalar(110, 110, 110));
	glm::mat4 worldToStage;
	std::string error;
	success= !CaptureMarkerAlignment::solveWorldToStage(emptyTable, makeShotFrame(cameraToWorld), k_imageWidth,
														k_imageHeight, makeOriginMarker(), k_dictionaryType,
														worldToStage, error);
	success&= !error.empty();

	error.clear();
	success&= !CaptureMarkerAlignment::solveWorldToStage(renderMarkerShot(cameraToWorld), makeShotFrame(cameraToWorld),
														 k_imageWidth / 2, k_imageHeight / 2, makeOriginMarker(),
														 k_dictionaryType, worldToStage, error);
	success&= !error.empty();
	UNIT_TEST_COMPLETE()
}

bool capture_marker_alignment_test_take_aligns_itself_for_its_session()
{
	using namespace CaptureMarkerAlignmentTestsInternal;

	UNIT_TEST_BEGIN("a take ignores another session's offset and aligns itself from its marker shot")
	FixtureDir fixture;
	const std::filesystem::path workingDir= fixture.root / ".take";
	std::filesystem::create_directories(workingDir);

	const glm::mat4 shotCameraToWorld= makeCameraToWorld(k_markerCentre + glm::vec3(0.f, 0.25f, 0.2f));
	cv::imwrite((workingDir / "marker.png").string(), renderMarkerShot(shotCameraToWorld));
	PoseTrackWriter markerTrack(k_sessionId, k_imageWidth, k_imageHeight);
	markerTrack.append(0, 1000, shotCameraToWorld, k_focalLength, k_focalLength, 0.5 * k_imageWidth,
					   0.5 * k_imageHeight);
	success= markerTrack.writeToFile(workingDir / "marker.pose.json");

	// The movie is never decoded here, only the capture's members read
	writeBytes(workingDir / "movie.mp4", "movie bytes");
	PoseTrackWriter movieTrack(k_sessionId, k_imageWidth, k_imageHeight);
	for (int frame= 0; frame < 3; ++frame)
		movieTrack.append(frame * 33333, 2000 + frame * 33333, shotCameraToWorld, k_focalLength, k_focalLength,
						  0.5 * k_imageWidth, 0.5 * k_imageHeight);
	success&= movieTrack.writeToFile(workingDir / "movie.pose.json");

	CapturePackRequest request;
	request.kind= eCaptureKind::trackcamera;
	request.workingDir= workingDir;
	request.outputPath= fixture.root / "take.trackcamera";
	request.sessionId= k_sessionId;
	request.poseSpace= eCapturePoseSpace::arkit_world;
	std::string error;
	success&= CapturePacker::pack(request, error);
	assert(success);

	// The source carries an offset solved for another take's session
	auto definition= std::make_shared<FileVideoSourceDefinition>();
	definition->setMediaPath(request.outputPath);
	glm::mat4 staleOffset(1.f);
	staleOffset[3]= glm::vec4(0.5f, 0.f, 0.f, 1.f);
	definition->setPoseOffset(staleOffset, "another-session");

	auto source= std::make_shared<FileVideoSourceComponent>(MikanObjectWeakPtr());
	source->setDefinition(definition);
	success&= source->openVideoSource();
	success&= source->getPoseSessionId() == k_sessionId;

	// Another session's offset is no alignment at all
	success&= !source->hasPoseOffset();
	success&= maxElementDifference(source->getPoseOffset(), glm::mat4(1.f)) == 0.f;

	// An offset with no session predates the tag and is treated the same way
	definition->setPoseOffset(staleOffset, std::string());
	success&= !source->hasPoseOffset();

	// The marker shot aligns it, once, for this session
	success&= source->wantsMarkerShotAlignment();
	success&= source->alignFromMarkerShot(makeOriginMarker(), k_dictionaryType);
	success&= source->hasPoseOffset();
	success&= definition->getPoseOffsetSessionId() == k_sessionId;
	success&= !source->wantsMarkerShotAlignment();

	glm::mat4 expected;
	success&= solveShot(k_markerCentre + glm::vec3(0.f, 0.25f, 0.2f), expected);
	success&= maxElementDifference(source->getPoseOffset(), expected) < 1e-4f;

	// The capture has no depth, so the shot's height stands and nothing is pending
	success&= !source->wantsHeightSnap() && !source->isAlignmentSettling();

	// The tag survives the project file
	FileVideoSourceDefinition restored;
	restored.readFromJSON(definition->writeToJSON());
	success&= restored.hasPoseOffset() && restored.getPoseOffsetSessionId() == k_sessionId;

	source->closeVideoSource();
	UNIT_TEST_COMPLETE()
}

bool capture_marker_alignment_test_height_snaps_onto_the_table()
{
	using namespace CaptureMarkerAlignmentTestsInternal;

	UNIT_TEST_BEGIN("the shot's stage origin snaps onto the table height the take's depth measures")
	// The take's depth sees a table at tableHeight. The shot solved the marker
	// 4 mm above it, the error a moment of ARKit's world leaves in the shot.
	const float tableHeight= -0.25f;
	const glm::vec3 markerOnTable(0.4f, tableHeight, -0.3f);
	const glm::vec3 shotMarker= markerOnTable + glm::vec3(0.f, 0.004f, 0.f);
	const glm::mat4 shotWorldToStage= glm::inverse(glm::translate(glm::mat4(1.f), shotMarker));

	// A small depth camera orbiting the marker at 0.4 m
	const int width= 64;
	const int height= 48;
	PinholeIntrinsics intrinsics;
	intrinsics.fx= 50.0;
	intrinsics.fy= 50.0;
	intrinsics.cx= 0.5 * width;
	intrinsics.cy= 0.5 * height;

	CaptureMarkerAlignment::TableHeightEstimator estimator(shotWorldToStage, k_markerLengthMM * 0.001f);
	for (int frame= 0; frame < 12; ++frame)
	{
		const float angle= 0.5f * (float)frame;
		const glm::vec3 eye= markerOnTable + glm::vec3(0.3f * std::sin(angle), 0.25f, 0.3f * std::cos(angle));
		const glm::mat4 recordedCameraToWorld= glm::inverse(glm::lookAt(eye, markerOnTable, glm::vec3(0.f, 1.f, 0.f)));
		const glm::mat4 cameraToWorld= OcclusionMeshBuilder::toOpenCvCameraToWorld(recordedCameraToWorld);

		// Ray cast the table: OpenCV camera rays with unit z, so the hit's ray
		// parameter is its depth
		cv::Mat depth(height, width, CV_32FC1, cv::Scalar(0.f));
		const cv::Mat confidence(height, width, CV_8UC1, cv::Scalar(2));
		const glm::vec3 origin(cameraToWorld[3]);
		for (int v= 0; v < height; ++v)
		{
			for (int u= 0; u < width; ++u)
			{
				const glm::vec3 direction= glm::mat3(cameraToWorld)
										   * glm::vec3((float)((u - intrinsics.cx) / intrinsics.fx),
													   (float)((v - intrinsics.cy) / intrinsics.fy), 1.f);
				if (direction.y < -1e-6f)
					depth.at<float>(v, u)= (tableHeight - origin.y) / direction.y;
			}
		}
		success&= estimator.addFrame(depth, confidence, intrinsics, cameraToWorld);
	}

	float heightM= 0.f, spreadM= 0.f;
	success&= estimator.estimate(heightM, spreadM);
	success&= std::fabs(heightM - (-0.004f)) < 0.0005f && spreadM < 0.0005f;

	// The snapped origin is on the table under the shot's marker, and the stage
	// keeps the shot's orientation
	const glm::mat4 snapped= CaptureMarkerAlignment::snapWorldToStageHeight(shotWorldToStage, heightM);
	success&= glm::length(glm::vec3(snapped * glm::vec4(markerOnTable, 1.f))) < 0.0005f;
	success&= maxElementDifference(glm::mat4(glm::mat3(snapped)), glm::mat4(glm::mat3(shotWorldToStage))) < 1e-5f;

	// A frame looking away from the marker measures nothing
	const glm::mat4 awayCameraToWorld= OcclusionMeshBuilder::toOpenCvCameraToWorld(
		glm::inverse(glm::lookAt(markerOnTable + glm::vec3(0.f, 0.25f, 0.3f),
								 markerOnTable + glm::vec3(0.f, 0.25f, 1.3f), glm::vec3(0.f, 1.f, 0.f))));
	const cv::Mat emptyDepth(height, width, CV_32FC1, cv::Scalar(0.f));
	const cv::Mat emptyConfidence(height, width, CV_8UC1, cv::Scalar(2));
	success&= !estimator.addFrame(emptyDepth, emptyConfidence, intrinsics, awayCameraToWorld);
	UNIT_TEST_COMPLETE()
}

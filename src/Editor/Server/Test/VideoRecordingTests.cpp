#include "VideoRecordingTests.h"
#include "unit_test.h"

#include "Capture.h"
#include "CaptureFlow.h"
#include "CapturePacker.h"
#include "MovieDecoder.h"
#include "MovieWriter.h"
#include "PoseTrack.h"
#include "TakeNaming.h"

#include "opencv2/opencv.hpp"
#include "opencv2/videoio/registry.hpp"

#include "glm/ext/matrix_transform.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdio.h>
#include <string>
#include <vector>

namespace
{
// A throwaway folder for the written media, removed on destruction
struct VideoRecordingFixtureDir
{
	std::filesystem::path root;

	VideoRecordingFixtureDir()
	{
		root= std::filesystem::temp_directory_path() / "mikan_video_recording_tests";
		std::filesystem::remove_all(root);
		std::filesystem::create_directories(root);
	}

	~VideoRecordingFixtureDir()
	{
		std::error_code ec;
		std::filesystem::remove_all(root, ec);
	}
};

const int k_movieWidth= 128;
const int k_movieHeight= 96;
const int k_movieFrameCount= 24;
const double k_movieFps= 24.0;

// Distinct enough per frame that a lossy codec still tells them apart
cv::Scalar movieFrameColor(int frameIndex) { return cv::Scalar(10 * frameIndex, 128, 250 - 10 * frameIndex); }

bool centrePixelMatches(const cv::Mat& frame, const cv::Scalar& expected, int tolerance)
{
	const cv::Vec3b pixel= frame.at<cv::Vec3b>(frame.rows / 2, frame.cols / 2);
	for (int channel= 0; channel < 3; ++channel)
	{
		if (std::abs(static_cast<int>(pixel[channel]) - static_cast<int>(expected[channel])) > tolerance)
			return false;
	}
	return true;
}
} // namespace

bool video_recording_test_movie_writer_round_trips()
{
	UNIT_TEST_BEGIN("movie writer output reads back through the movie decoder")

	VideoRecordingFixtureDir fixture;

	std::string backends;
	for (const cv::VideoCaptureAPIs api : cv::videoio_registry::getWriterBackends())
	{
		backends+= (backends.empty() ? "" : ", ") + cv::videoio_registry::getBackendName(api);
	}
	printf("      writer backends: %s\n", backends.c_str());

	MovieWriter writer;
	success&= writer.open(fixture.root / "take", k_movieWidth, k_movieHeight, k_movieFps);
	assert(success);
	if (success)
	{
		printf("      chose %s -> %s\n", writer.getBackendName().c_str(), writer.getPath().filename().string().c_str());

		for (int frameIndex= 0; frameIndex < k_movieFrameCount; ++frameIndex)
		{
			const cv::Mat frame(k_movieHeight, k_movieWidth, CV_8UC3, movieFrameColor(frameIndex));
			success&= writer.write(frame);
		}
		assert(success);

		// A frame of the wrong size is refused rather than corrupting the stream
		success&= !writer.write(cv::Mat(8, 8, CV_8UC3, cv::Scalar(0, 0, 0)));
		assert(success);
		success&= (writer.getFrameCount() == k_movieFrameCount);
		assert(success);

		const std::filesystem::path moviePath= writer.getPath();
		success&= writer.close();
		assert(success);
		success&= !writer.isOpen();
		assert(success);

		MovieDecoder decoder;
		success&= decoder.open(moviePath);
		assert(success);
		success&= (decoder.getWidth() == k_movieWidth && decoder.getHeight() == k_movieHeight);
		assert(success);
		success&= (std::abs(decoder.getFps() - k_movieFps) < 0.5);
		assert(success);
		success&= (decoder.getFrameCount() == k_movieFrameCount);
		assert(success);

		// Every frame comes back in order, at its constant-rate presentation time
		cv::Mat frame;
		int64_t ptsUs= 0;
		int readCount= 0;
		int matchedCount= 0;
		while (decoder.readNext(frame, ptsUs))
		{
			const int64_t expectedPtsUs= static_cast<int64_t>(std::llround(readCount * 1000000.0 / k_movieFps));
			success&= (std::abs(ptsUs - expectedPtsUs) <= 1000);
			if (centrePixelMatches(frame, movieFrameColor(readCount), 40))
				++matchedCount;
			++readCount;
		}
		success&= (readCount == k_movieFrameCount);
		assert(success);
		// Lossy codecs blur the first frames of a group, so most rather than all must match
		success&= (matchedCount >= k_movieFrameCount - 4);
		if (matchedCount < k_movieFrameCount - 4)
			printf("      only %d of %d frames matched their colour\n", matchedCount, k_movieFrameCount);
		assert(success);
	}

	UNIT_TEST_COMPLETE()
}

bool video_recording_test_pose_track_writer_round_trips()
{
	UNIT_TEST_BEGIN("pose track writer round-trips through the reader")

	VideoRecordingFixtureDir fixture;
	const std::filesystem::path trackPath= fixture.root / "take.pose.json";

	PoseTrackWriter writer("session-abc", 1920, 1440);
	success&= (writer.getFrameCount() == 0);

	// Camera-to-world with a translation and a 90 degree yaw, so the transpose is visible
	const glm::mat4 pose= glm::rotate(glm::translate(glm::mat4(1.f), glm::vec3(1.f, 2.f, 3.f)), glm::radians(90.f),
									  glm::vec3(0.f, 1.f, 0.f));
	writer.append(0, 5000000, pose, 1450.0, 1451.0, 960.0, 720.0);
	writer.append(41666, 5041666, pose, 1450.0, 1451.0, 960.0, 720.0);
	writer.append(83333, 5083333, glm::mat4(1.f), 1450.0, 1451.0, 960.0, 720.0);
	success&= (writer.getFrameCount() == 3);
	assert(success);
	success&= writer.writeToFile(trackPath);
	assert(success);
	success&= std::filesystem::exists(trackPath) && !std::filesystem::exists(trackPath.string() + ".tmp");
	assert(success);

	PoseTrack track;
	success&= track.loadFromFile(trackPath);
	assert(success);
	success&= track.isLoaded() && track.getFrameCount() == 3;
	assert(success);
	success&= (track.getSessionId() == "session-abc");
	success&= (track.getImageWidth() == 1920 && track.getImageHeight() == 1440);
	success&= (track.getMedianFramePeriodUs() == 41666 || track.getMedianFramePeriodUs() == 41667);
	assert(success);

	if (success)
	{
		const PoseTrackFrame& frame= track.getFrame(1);
		success&= (frame.timeUs == 41666 && frame.captureTimestampUs == 5041666);
		assert(success);
		// The matrix survives the row-major write and the column-major read
		for (int column= 0; column < 4; ++column)
		{
			for (int row= 0; row < 4; ++row)
			{
				success&= (std::abs(frame.cameraToWorld[column][row] - pose[column][row]) < 1e-4f);
			}
		}
		assert(success);
		success&= (frame.cameraToWorld[3] == glm::vec4(1.f, 2.f, 3.f, 1.f));
		assert(success);
		success&= (frame.fx == 1450.0 && frame.fy == 1451.0 && frame.cx == 960.0 && frame.cy == 720.0);
		assert(success);
		success&= (track.getFrame(2).cameraToWorld == glm::mat4(1.f));
		assert(success);
	}

	UNIT_TEST_COMPLETE()
}

bool video_recording_test_take_names()
{
	UNIT_TEST_BEGIN("take names carry the timestamp and the marker suffix")

	std::tm localTime{};
	localTime.tm_year= 2026 - 1900;
	localTime.tm_mon= 8; // September
	localTime.tm_mday= 15;
	localTime.tm_hour= 19;
	localTime.tm_min= 30;
	localTime.tm_sec= 7;

	const std::string takeName= TakeNaming::makeTakeName(localTime);
	success&= (takeName == "take_20260915_193007");
	assert(success);
	success&= (TakeNaming::makeMarkerName(takeName) == "take_20260915_193007_marker");
	assert(success);
	success&= TakeNaming::isMarkerName("take_20260915_193007_marker");
	success&= !TakeNaming::isMarkerName(takeName);
	success&= !TakeNaming::isMarkerName("_marker");
	assert(success);

	// The live name has the same shape: prefix plus 15 characters of timestamp
	const std::string nowName= TakeNaming::makeTakeNameNow();
	success&= (nowName.size() == takeName.size() && nowName.rfind("take_", 0) == 0 && nowName[13] == '_');
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool video_recording_test_capture_flow_steps()
{
	UNIT_TEST_BEGIN("each capture kind walks its steps, packs after the last, and cancels from any")

	// The step sequences
	const std::vector<eCaptureFlowStep>& stillPhoto= CaptureFlow::stepsForKind(eCaptureKind::stillphoto);
	success&=
		(stillPhoto == std::vector<eCaptureFlowStep>{eCaptureFlowStep::markerImage, eCaptureFlowStep::plateImage});
	const std::vector<eCaptureFlowStep>& stillCamera= CaptureFlow::stepsForKind(eCaptureKind::stillcamera);
	success&= (stillCamera
			   == std::vector<eCaptureFlowStep>{eCaptureFlowStep::markerImage, eCaptureFlowStep::plateImage,
												eCaptureFlowStep::readyToRecord, eCaptureFlowStep::recording});
	const std::vector<eCaptureFlowStep>& trackCamera= CaptureFlow::stepsForKind(eCaptureKind::trackcamera);
	success&=
		(trackCamera == std::vector<eCaptureFlowStep>{eCaptureFlowStep::readyToRecord, eCaptureFlowStep::recording});
	success&= CaptureFlow::stepsForKind(eCaptureKind::INVALID).empty();

	// Idle refuses to advance, and an invalid kind refuses to start
	CaptureFlow flow;
	success&= !flow.isActive() && !flow.advance() && !flow.start(eCaptureKind::INVALID);

	// Each kind walks its steps into packing, then completes back to idle
	for (int kindIndex= 0; kindIndex < (int)eCaptureKind::COUNT; ++kindIndex)
	{
		const eCaptureKind kind= (eCaptureKind)kindIndex;
		const std::vector<eCaptureFlowStep>& steps= CaptureFlow::stepsForKind(kind);

		success&= flow.start(kind) && flow.isActive() && flow.getKind() == kind && flow.getStep() == steps[0];
		// A second start is refused while active
		success&= !flow.start(kind);
		for (size_t index= 1; index < steps.size(); ++index)
		{
			success&= flow.advance() && flow.getStep() == steps[index];
		}
		success&= flow.isMovieStep() == (steps.back() == eCaptureFlowStep::recording);
		success&= flow.advance() && flow.getStep() == eCaptureFlowStep::packing;
		success&= !flow.advance() && flow.getStep() == eCaptureFlowStep::packing;
		flow.complete();
		success&= !flow.isActive() && flow.getKind() == eCaptureKind::INVALID;
	}

	// Cancel from every step of the longest flow lands on idle
	for (size_t cancelAt= 0; cancelAt <= stillCamera.size(); ++cancelAt)
	{
		success&= flow.start(eCaptureKind::stillcamera);
		for (size_t index= 0; index < cancelAt; ++index)
			flow.advance();
		flow.cancel();
		success&= !flow.isActive() && flow.getStep() == eCaptureFlowStep::idle;
	}

	// Step names for the automation channel
	success&= std::string(CaptureFlowUtils::stepToString(eCaptureFlowStep::readyToRecord)) == "ready_to_record";
	success&= std::string(CaptureFlowUtils::stepToString(eCaptureFlowStep::packing)) == "packing";

	UNIT_TEST_COMPLETE()
}

namespace
{
void writeBytes(const std::filesystem::path& path, const std::string& content)
{
	std::ofstream file(path, std::ios::binary);
	file << content;
}

void writeOneFrameTrack(const std::filesystem::path& path, const std::string& sessionId)
{
	PoseTrackWriter writer(sessionId, k_movieWidth, k_movieHeight);
	writer.append(0, 1000, glm::mat4(1.f), 100.0, 100.0, 64.0, 48.0);
	writer.writeToFile(path);
}
} // namespace

bool video_recording_test_capture_packer_packs_each_kind()
{
	UNIT_TEST_BEGIN("a working directory packs into a capture of each kind that the reader validates")

	VideoRecordingFixtureDir fixture;
	const std::string sessionId= "0123456789abcdef0123456789abcdef";
	std::string error;

	// A stillphoto: the plate and the marker shot with its track
	{
		const std::filesystem::path workingDir= fixture.root / ".capture_photo";
		std::filesystem::create_directories(workingDir);
		writeBytes(workingDir / "plate.jpg", "plate bytes");
		writeBytes(workingDir / "marker.jpg", "marker bytes");
		writeOneFrameTrack(workingDir / "marker.pose.json", sessionId);

		success&=
			CapturePacker::findMemberFile(workingDir, eCaptureMemberRole::plate_image) == workingDir / "plate.jpg";
		success&= CapturePacker::findMemberFile(workingDir, eCaptureMemberRole::movie).empty();

		CapturePackRequest request;
		request.kind= eCaptureKind::stillphoto;
		request.workingDir= workingDir;
		request.outputPath= fixture.root / "photo.stillphoto";
		request.sessionId= sessionId;
		request.producer= CaptureManifest::makeEditorProducer();
		success&= CapturePacker::pack(request, error);
		if (!success)
			printf("      stillphoto pack failed: %s\n", error.c_str());

		Capture capture;
		success&= capture.open(request.outputPath, error);
		success&= capture.getKind() == eCaptureKind::stillphoto && capture.isStageSpace();
		success&= capture.getManifest().getMemberName(eCaptureMemberRole::plate_image) == "plate.jpg";
		success&= capture.getManifest().getMemberName(eCaptureMemberRole::marker_image) == "marker.jpg";
		success&= capture.getPoseTrack(eCaptureMemberRole::marker_pose).getFrameCount() == 1;
		success&= capture.getManifest().producer.rfind("MikanXR ", 0) == 0;
		// The working directory is the recorder's to remove
		success&= std::filesystem::exists(workingDir / "plate.jpg");
	}

	// A stillcamera: a real movie beside the stills, no movie track
	{
		const std::filesystem::path workingDir= fixture.root / ".capture_still";
		std::filesystem::create_directories(workingDir);
		MovieWriter writer;
		success&= writer.open(workingDir / "movie", k_movieWidth, k_movieHeight, k_movieFps);
		for (int frameIndex= 0; frameIndex < 6; ++frameIndex)
			writer.write(cv::Mat(k_movieHeight, k_movieWidth, CV_8UC3, movieFrameColor(frameIndex)));
		success&= writer.close();
		writeBytes(workingDir / "plate.jpg", "plate bytes");
		writeBytes(workingDir / "marker.jpg", "marker bytes");
		writeOneFrameTrack(workingDir / "marker.pose.json", sessionId);

		CapturePackRequest request;
		request.kind= eCaptureKind::stillcamera;
		request.workingDir= workingDir;
		request.outputPath= fixture.root / "still.stillcamera";
		request.sessionId= sessionId;
		success&= CapturePacker::pack(request, error);
		if (!success)
			printf("      stillcamera pack failed: %s\n", error.c_str());

		Capture capture;
		success&= capture.open(request.outputPath, error);
		success&= capture.getKind() == eCaptureKind::stillcamera;
		success&=
			capture.getManifest().getMemberName(eCaptureMemberRole::movie) == writer.getPath().filename().string();
		success&= !capture.hasMember(eCaptureMemberRole::movie_pose);

		// The packed movie plays in place
		MovieDecoder decoder;
		success&= decoder.openMovieUrl(capture.makeSubfileUrl(eCaptureMemberRole::movie));
		success&= decoder.getFrameCount() == 6;
	}

	// A trackcamera: the movie with its track, no marker steps
	{
		const std::filesystem::path workingDir= fixture.root / ".capture_track";
		std::filesystem::create_directories(workingDir);
		writeBytes(workingDir / "movie.mp4", "movie bytes");
		PoseTrackWriter track(sessionId, k_movieWidth, k_movieHeight);
		for (int frame= 0; frame < 3; ++frame)
			track.append(frame * 41667, 1000 + frame * 41667, glm::mat4(1.f), 100.0, 100.0, 64.0, 48.0);
		success&= track.writeToFile(workingDir / "movie.pose.json");

		CapturePackRequest request;
		request.kind= eCaptureKind::trackcamera;
		request.workingDir= workingDir;
		request.outputPath= fixture.root / "track.trackcamera";
		request.sessionId= sessionId;
		success&= CapturePacker::pack(request, error);
		if (!success)
			printf("      trackcamera pack failed: %s\n", error.c_str());

		Capture capture;
		success&= capture.open(request.outputPath, error);
		success&= capture.getKind() == eCaptureKind::trackcamera && capture.isStageSpace();
		success&= capture.getPoseTrack(eCaptureMemberRole::movie_pose).getFrameCount() == 3;
		success&= !capture.hasMember(eCaptureMemberRole::marker_image) && !capture.hasMember(eCaptureMemberRole::depth);

		// A trackcamera without its movie track is refused, and leaves no archive
		std::filesystem::remove(workingDir / "movie.pose.json");
		request.outputPath= fixture.root / "trackless.trackcamera";
		success&= !CapturePacker::pack(request, error);
		success&= !std::filesystem::exists(request.outputPath);
	}

	// A track without its media is left out rather than packed alone
	{
		const std::filesystem::path workingDir= fixture.root / ".capture_orphan";
		std::filesystem::create_directories(workingDir);
		writeBytes(workingDir / "plate.png", "plate bytes");
		writeOneFrameTrack(workingDir / "marker.pose.json", sessionId);

		CapturePackRequest request;
		request.kind= eCaptureKind::stillphoto;
		request.workingDir= workingDir;
		request.outputPath= fixture.root / "orphan.stillphoto";
		request.sessionId= sessionId;
		success&= CapturePacker::pack(request, error);
		Capture capture;
		success&= capture.open(request.outputPath, error);
		success&= capture.getManifest().getMemberName(eCaptureMemberRole::plate_image) == "plate.png";
		success&= !capture.hasMember(eCaptureMemberRole::marker_pose);
	}

	UNIT_TEST_COMPLETE()
}

bool run_video_recording_tests()
{
	UNIT_TEST_MODULE_BEGIN("video_recording")
	UNIT_TEST_MODULE_CALL_TEST(video_recording_test_movie_writer_round_trips);
	UNIT_TEST_MODULE_CALL_TEST(video_recording_test_pose_track_writer_round_trips);
	UNIT_TEST_MODULE_CALL_TEST(video_recording_test_take_names);
	UNIT_TEST_MODULE_CALL_TEST(video_recording_test_capture_flow_steps);
	UNIT_TEST_MODULE_CALL_TEST(video_recording_test_capture_packer_packs_each_kind);
	UNIT_TEST_MODULE_END()
}

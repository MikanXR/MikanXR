#include "FileVideoSourceTests.h"
#include "unit_test.h"

#include "CameraMath.h"
#include "MikanVideoSourceTypes.h"
#include "PlaybackTime.h"
#include "MovieDecoder.h"
#include "PoseTrack.h"
#include "FileVideoSourceComponent.h"

#include "opencv2/opencv.hpp"

#include "glm/trigonometric.hpp"

#include <configuru.hpp>

#include <assert.h>
#include <cstdlib>
#include <filesystem>
#include <math.h>
#include <stdio.h>
#include <string>

namespace
{
// A throwaway folder for the media fixtures, removed on destruction
struct FileVideoSourceFixtureDir
{
	std::filesystem::path root;

	FileVideoSourceFixtureDir()
	{
		root= std::filesystem::temp_directory_path() / "mikan_file_video_source_tests";
		std::filesystem::remove_all(root);
		std::filesystem::create_directories(root);
	}

	~FileVideoSourceFixtureDir()
	{
		std::error_code ignored;
		std::filesystem::remove_all(root, ignored);
	}
};

const int k_movieWidth= 64;
const int k_movieHeight= 48;
const int k_movieFrameCount= 12;
const double k_movieFps= 24.0;

cv::Scalar movieFrameColor(int frameIndex) { return cv::Scalar(20 * frameIndex, 100, 255 - 20 * frameIndex); }

bool writeMovieFixture(const std::filesystem::path& path)
{
	cv::VideoWriter writer(path.string(), cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), k_movieFps,
						   cv::Size(k_movieWidth, k_movieHeight), true);
	if (!writer.isOpened())
		return false;

	for (int frameIndex= 0; frameIndex < k_movieFrameCount; ++frameIndex)
	{
		const cv::Mat frame(k_movieHeight, k_movieWidth, CV_8UC3, movieFrameColor(frameIndex));
		writer.write(frame);
	}

	writer.release();
	return true;
}

// MJPG is lossy, so the centre pixel is compared against the written colour
// with a tolerance
bool centrePixelMatches(const cv::Mat& frame, const cv::Scalar& expected, int tolerance)
{
	if (frame.empty() || frame.type() != CV_8UC3)
		return false;

	const cv::Vec3b pixel= frame.at<cv::Vec3b>(frame.rows / 2, frame.cols / 2);
	for (int channel= 0; channel < 3; ++channel)
	{
		if (std::abs((int)pixel[channel] - (int)expected[channel]) > tolerance)
			return false;
	}

	return true;
}

std::string makePoseTrackJson(const std::string& format, int version, const std::string& frames)
{
	std::string json= "{";
	json+= "\"format\": \"" + format + "\", ";
	json+= "\"version\": " + std::to_string(version) + ", ";
	json+= "\"session_id\": \"7c1d1a2e-1111-4a4a-9b9b-0123456789ab\", ";
	json+= "\"image_width\": 1920, \"image_height\": 1440, ";
	json+= "\"first_capture_timestamp_us\": 123456789";
	if (!frames.empty())
	{
		json+= ", \"frames\": [" + frames + "]";
	}
	json+= "}";
	return json;
}

// A row-major camera-to-world transform whose translation (1, 2, 3) sits in
// the last column of each row
std::string makePoseTrackFrame(int64_t timeUs)
{
	return "{\"time_us\": " + std::to_string(timeUs) + ", \"capture_timestamp_us\": "
		   + std::to_string(123456789 + timeUs) + ", \"transform\": [1, 0, 0, 1,  0, 1, 0, 2,  0, 0, 1, 3,  0, 0, 0, 1]"
		   + ", \"fx\": 1450.0, \"fy\": 1450.0, \"cx\": 960.0, \"cy\": 720.0}";
}

configuru::Config parsePoseTrackJson(const std::string& json)
{
	return configuru::parse_string(json.c_str(), configuru::JSON, "test");
}
} // namespace

bool run_file_video_source_tests()
{
	UNIT_TEST_MODULE_BEGIN("file_video_source")
	UNIT_TEST_MODULE_CALL_TEST(file_video_source_test_playback_time);
	UNIT_TEST_MODULE_CALL_TEST(file_video_source_test_pinhole_intrinsics);
	UNIT_TEST_MODULE_CALL_TEST(file_video_source_test_pose_track);
	UNIT_TEST_MODULE_CALL_TEST(file_video_source_test_movie_decoder);
	UNIT_TEST_MODULE_CALL_TEST(file_video_source_test_still_image);
	UNIT_TEST_MODULE_CALL_TEST(file_video_source_test_definition_round_trip);
	UNIT_TEST_MODULE_CALL_TEST(file_video_source_test_playback_controls);
	UNIT_TEST_MODULE_END()
}

bool file_video_source_test_definition_round_trip()
{
	UNIT_TEST_BEGIN("definition round-trips through JSON, with and without a pose offset")

	FileVideoSourceDefinition source;
	source.setMediaPath("movies/take.mp4");
	source.setMarkerMediaPath("movies/take_marker.mp4");
	source.setPoseTrackPath("movies/take.pose.json");
	source.setMarkerPoseTrackPath("movies/take_marker.pose.json");
	source.setLoop(false);

	FileVideoSourceDefinition restored;
	restored.readFromJSON(source.writeToJSON());
	success= (restored.getMediaPath() == std::filesystem::path("movies/take.mp4"));
	assert(success);
	success&= (restored.getMarkerMediaPath() == std::filesystem::path("movies/take_marker.mp4"));
	assert(success);
	success&= (restored.getPoseTrackPath() == std::filesystem::path("movies/take.pose.json"));
	assert(success);
	success&= (restored.getMarkerPoseTrackPath() == std::filesystem::path("movies/take_marker.pose.json"));
	assert(success);
	success&= !restored.getLoop();
	assert(success);
	// No alignment was ever solved, so none reads back
	success&= !restored.hasPoseOffset();
	assert(success);

	// A solved offset survives, column-major
	glm::mat4 offset(1.f);
	offset[3]= glm::vec4(1.f, 2.f, 3.f, 1.f);
	source.setPoseOffset(offset);
	FileVideoSourceDefinition aligned;
	aligned.readFromJSON(source.writeToJSON());
	success&= aligned.hasPoseOffset();
	assert(success);
	success&= (aligned.getPoseOffset()[3] == glm::vec4(1.f, 2.f, 3.f, 1.f));
	assert(success);

	// Clearing the offset removes the key rather than writing identity
	source.clearPoseOffset();
	FileVideoSourceDefinition cleared;
	cleared.readFromJSON(source.writeToJSON());
	success&= !cleared.hasPoseOffset();
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool file_video_source_test_playback_controls()
{
	UNIT_TEST_BEGIN("play, pause, stop, and seek move the playback state without media")

	auto component= std::make_shared<FileVideoSourceComponent>(MikanObjectWeakPtr());
	component->setDefinition(std::make_shared<FileVideoSourceDefinition>());

	success= (component->getPlaybackState() == eFilePlaybackState::stopped);
	assert(success);
	success&= (component->getPlaybackTime() == 0.f);
	assert(success);

	component->play();
	success&= (component->getPlaybackState() == eFilePlaybackState::playing);
	assert(success);

	component->pause();
	success&= (component->getPlaybackState() == eFilePlaybackState::paused);
	assert(success);

	// Resuming keeps the time rather than restarting
	component->play();
	success&= (component->getPlaybackState() == eFilePlaybackState::playing);
	assert(success);

	component->stop();
	success&= (component->getPlaybackState() == eFilePlaybackState::stopped);
	assert(success);
	success&= (component->getPlaybackTime() == 0.f);
	assert(success);

	// Pausing from stopped is a no-op
	component->pause();
	success&= (component->getPlaybackState() == eFilePlaybackState::stopped);
	assert(success);

	// A seek clamps into the media, which is empty here, so it lands at zero
	component->seekTo(5.0);
	success&= (component->getPlaybackTime() == 0.f);
	assert(success);

	// The stream reports stopped until a consumer asks for it
	success&= (component->getVideoStreamingStatus() == eVideoStreamingStatus::stopped);
	assert(success);

	// No track, so no frame-coupled pose is on offer
	glm::mat4 transform;
	uint32_t frameSeq= 0;
	success&= !component->getLatestSourceWorldPose(transform, frameSeq);
	assert(success);
	success&= !component->hasAlignmentReference();
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool file_video_source_test_playback_time()
{
	UNIT_TEST_BEGIN("advancePlaybackTime wraps, finishes, or runs unbounded")

	bool bWrapped= false;
	bool bFinished= false;

	// Looping wraps past the duration: five 0.5 steps over a duration of 2
	// land back at 0.5 with exactly one wrap
	float time= 0.f;
	int wrapCount= 0;
	for (int step= 0; step < 5; ++step)
	{
		advancePlaybackTime(time, 0.5f, 2.f, true, bWrapped, bFinished);
		if (bWrapped)
			++wrapCount;
		success&= !bFinished;
		assert(success);
	}
	success&= (wrapCount == 1) && fabsf(time - 0.5f) < 1e-5f;
	assert(success);

	// Not looping clamps to the duration and finishes
	time= 1.8f;
	advancePlaybackTime(time, 0.5f, 2.f, false, bWrapped, bFinished);
	success&= !bWrapped && bFinished && time == 2.f;
	assert(success);

	// Inside the duration nothing happens
	time= 0.5f;
	advancePlaybackTime(time, 0.25f, 2.f, false, bWrapped, bFinished);
	success&= !bWrapped && !bFinished && fabsf(time - 0.75f) < 1e-5f;
	assert(success);

	// Zero duration is unbounded, looping or not
	time= 100.f;
	advancePlaybackTime(time, 1.f, 0.f, false, bWrapped, bFinished);
	success&= !bWrapped && !bFinished && time == 101.f;
	assert(success);
	advancePlaybackTime(time, 1.f, 0.f, true, bWrapped, bFinished);
	success&= !bWrapped && !bFinished && time == 102.f;
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool file_video_source_test_pinhole_intrinsics()
{
	UNIT_TEST_BEGIN("pinhole intrinsics fill the camera matrices and the field of view")

	MikanMonoIntrinsics intrinsics;
	createMonoIntrinsicsFromPinhole(1920, 1440, 1450.0, 1450.0, 960.0, 720.0, intrinsics);

	// The focal lengths sit on the diagonal and the principal point in the
	// third column of the column-major matrix
	success= (intrinsics.undistorted_camera_matrix.x0 == 1450.0);
	assert(success);
	success&= (intrinsics.undistorted_camera_matrix.y1 == 1450.0);
	assert(success);
	success&= (intrinsics.undistorted_camera_matrix.z0 == 960.0);
	assert(success);
	success&= (intrinsics.undistorted_camera_matrix.z1 == 720.0);
	assert(success);

	// A pinhole has no distortion, so both matrices agree
	const MikanMatrix3d& distorted= intrinsics.distorted_camera_matrix;
	const MikanMatrix3d& undistorted= intrinsics.undistorted_camera_matrix;
	success&= (distorted.x0 == undistorted.x0 && distorted.x1 == undistorted.x1 && distorted.x2 == undistorted.x2
			   && distorted.y0 == undistorted.y0 && distorted.y1 == undistorted.y1 && distorted.y2 == undistorted.y2
			   && distorted.z0 == undistorted.z0 && distorted.z1 == undistorted.z1 && distorted.z2 == undistorted.z2);
	assert(success);

	success&= (intrinsics.pixel_width == 1920.0 && intrinsics.pixel_height == 1440.0);
	assert(success);

	// hfov = 2 * atan(w / 2fx), vfov = 2 * atan(h / 2fy), in degrees
	const double expectedHfov= glm::degrees(2.0 * atan(1920.0 / (2.0 * 1450.0)));
	const double expectedVfov= glm::degrees(2.0 * atan(1440.0 / (2.0 * 1450.0)));
	success&= fabs(intrinsics.hfov - expectedHfov) < 1e-6 && fabs(intrinsics.hfov - 67.01) < 0.01;
	assert(success);
	success&= fabs(intrinsics.vfov - expectedVfov) < 1e-6 && fabs(intrinsics.vfov - 52.81) < 0.01;
	assert(success);

	success&= (intrinsics.znear == DEFAULT_MONO_ZNEAR && intrinsics.zfar == DEFAULT_MONO_ZFAR);
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool file_video_source_test_pose_track()
{
	UNIT_TEST_BEGIN("a pose track loads, sorts, validates, and answers nearest lookups")

	const std::string sortedFrames=
		makePoseTrackFrame(0) + ", " + makePoseTrackFrame(33333) + ", " + makePoseTrackFrame(66666);

	PoseTrack track;
	success= track.loadFromJSON(parsePoseTrackJson(makePoseTrackJson(PoseTrack::k_formatName, 1, sortedFrames)));
	assert(success);
	success&= track.isLoaded() && track.getFrameCount() == 3;
	assert(success);
	success&= (track.getSessionId() == "7c1d1a2e-1111-4a4a-9b9b-0123456789ab");
	assert(success);
	success&= (track.getImageWidth() == 1920 && track.getImageHeight() == 1440);
	assert(success);
	success&= (track.getMedianFramePeriodUs() == 33333);
	assert(success);

	// The file's row-major translation lands in glm's translation column
	if (success)
	{
		const PoseTrackFrame& frame= track.getFrame(1);
		success&= (frame.timeUs == 33333 && frame.captureTimestampUs == 123456789 + 33333);
		assert(success);
		success&= (frame.cameraToWorld[3] == glm::vec4(1.f, 2.f, 3.f, 1.f));
		assert(success);
		success&= (frame.cameraToWorld[0] == glm::vec4(1.f, 0.f, 0.f, 0.f));
		assert(success);
		success&= (frame.fx == 1450.0 && frame.fy == 1450.0 && frame.cx == 960.0 && frame.cy == 720.0);
		assert(success);
	}

	// Nearest lookups pick the closer neighbour on either side
	size_t index= 99;
	success&= track.findNearestFrame(15000, 33333, index) && index == 0;
	assert(success);
	success&= track.findNearestFrame(20000, 33333, index) && index == 1;
	assert(success);
	success&= track.findNearestFrame(66000, 33333, index) && index == 2;
	assert(success);
	// Past the end by more than the tolerance is a miss
	success&= !track.findNearestFrame(100000, 16666, index);
	assert(success);

	// An empty track has nothing to find
	PoseTrack emptyTrack;
	success&= !emptyTrack.findNearestFrame(0, 1000000, index);
	assert(success);
	success&= (emptyTrack.getMedianFramePeriodUs() == 0);
	assert(success);

	// Out-of-order frames are sorted on load
	const std::string unsortedFrames=
		makePoseTrackFrame(66666) + ", " + makePoseTrackFrame(0) + ", " + makePoseTrackFrame(33333);
	PoseTrack unsortedTrack;
	success&=
		unsortedTrack.loadFromJSON(parsePoseTrackJson(makePoseTrackJson(PoseTrack::k_formatName, 1, unsortedFrames)));
	assert(success);
	success&= unsortedTrack.getFrameCount() == 3 && unsortedTrack.getFrame(0).timeUs == 0
			  && unsortedTrack.getFrame(1).timeUs == 33333 && unsortedTrack.getFrame(2).timeUs == 66666;
	assert(success);

	// A bad header leaves the track empty
	PoseTrack badTrack;
	success&= !badTrack.loadFromJSON(parsePoseTrackJson(makePoseTrackJson("not-a-pose-track", 1, sortedFrames)));
	assert(success);
	success&= !badTrack.isLoaded() && badTrack.getFrameCount() == 0;
	assert(success);
	success&= !badTrack.loadFromJSON(parsePoseTrackJson(makePoseTrackJson(PoseTrack::k_formatName, 2, sortedFrames)));
	assert(success);
	success&= !badTrack.isLoaded();
	assert(success);
	success&= !badTrack.loadFromJSON(parsePoseTrackJson(makePoseTrackJson(PoseTrack::k_formatName, 1, "")));
	assert(success);
	success&= !badTrack.isLoaded() && badTrack.getSessionId().empty();
	assert(success);

	// A previously loaded track that fails a reload is emptied, not left stale
	success&= !track.loadFromJSON(parsePoseTrackJson(makePoseTrackJson(PoseTrack::k_formatName, 1, "")));
	assert(success);
	success&= !track.isLoaded() && track.getFrameCount() == 0;
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool file_video_source_test_movie_decoder()
{
	UNIT_TEST_BEGIN("the movie decoder reads frames with presentation times and seeks")

	FileVideoSourceFixtureDir fixtures;
	const std::filesystem::path moviePath= fixtures.root / "clip.avi";

	// A failed write means the videoio backend DLL is not next to the exe
	success= writeMovieFixture(moviePath);
	assert(success);

	MovieDecoder decoder;
	success&= decoder.open(moviePath);
	assert(success);
	success&= decoder.isOpen() && !decoder.isStill();
	assert(success);
	success&= (decoder.getWidth() == k_movieWidth && decoder.getHeight() == k_movieHeight);
	assert(success);
	success&= fabs(decoder.getFps() - k_movieFps) < 0.01;
	assert(success);
	success&= (decoder.getFrameCount() == k_movieFrameCount);
	assert(success);
	success&= fabs(decoder.getDurationSeconds() - (double)k_movieFrameCount / k_movieFps) < 0.01;
	assert(success);

	// Every frame comes out in order with its own presentation time
	const int64_t framePeriodUs= (int64_t)llround(1000000.0 / k_movieFps);
	cv::Mat frame;
	int64_t ptsUs= -1;
	for (int frameIndex= 0; frameIndex < k_movieFrameCount && success; ++frameIndex)
	{
		success&= decoder.readNext(frame, ptsUs);
		assert(success);
		success&= (frame.type() == CV_8UC3 && frame.isContinuous() && frame.cols == k_movieWidth
				   && frame.rows == k_movieHeight);
		assert(success);
		success&= (std::llabs(ptsUs - frameIndex * framePeriodUs) <= 2000);
		assert(success);
		if (frameIndex == 0 || frameIndex == 5 || frameIndex == 11)
		{
			success&= centrePixelMatches(frame, movieFrameColor(frameIndex), 8);
			assert(success);
		}
	}

	// End of stream after the last frame
	success&= !decoder.readNext(frame, ptsUs);
	assert(success);

	// Seeking just past frame 7's time lands on frame 7
	success&= decoder.seek(7.0 / k_movieFps + 0.001);
	assert(success);
	success&= decoder.readNext(frame, ptsUs) && centrePixelMatches(frame, movieFrameColor(7), 8);
	assert(success);

	// Seeking to the start rewinds to frame 0
	success&= decoder.seek(0.0);
	assert(success);
	success&= decoder.readNext(frame, ptsUs) && centrePixelMatches(frame, movieFrameColor(0), 8);
	assert(success);
	success&= (std::llabs(ptsUs) <= 2000);
	assert(success);

	decoder.close();
	success&= !decoder.isOpen();
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool file_video_source_test_still_image()
{
	UNIT_TEST_BEGIN("a still image decodes as a one-frame movie")

	FileVideoSourceFixtureDir fixtures;
	const std::filesystem::path imagePath= fixtures.root / "still.png";
	const cv::Scalar color(10, 200, 30);

	success= cv::imwrite(imagePath.string(), cv::Mat(16, 32, CV_8UC3, color));
	assert(success);

	MovieDecoder decoder;
	success&= decoder.open(imagePath);
	assert(success);
	success&= decoder.isOpen() && decoder.isStill();
	assert(success);
	success&= (decoder.getWidth() == 32 && decoder.getHeight() == 16);
	assert(success);
	success&= (decoder.getFps() == MovieDecoder::k_stillNominalFps);
	assert(success);
	success&= (decoder.getDurationSeconds() == 0.0 && decoder.getFrameCount() == 1);
	assert(success);

	// The frame repeats on every read at time zero
	cv::Mat frame;
	int64_t ptsUs= -1;
	for (int readIndex= 0; readIndex < 3 && success; ++readIndex)
	{
		success&= decoder.readNext(frame, ptsUs);
		assert(success);
		success&= (ptsUs == 0);
		assert(success);
		success&= (frame.type() == CV_8UC3 && frame.isContinuous() && centrePixelMatches(frame, color, 0));
		assert(success);
	}

	// Seeking a still is accepted and changes nothing
	success&= decoder.seek(5.0);
	assert(success);
	success&= decoder.readNext(frame, ptsUs) && ptsUs == 0 && centrePixelMatches(frame, color, 0);
	assert(success);

	UNIT_TEST_COMPLETE()
}

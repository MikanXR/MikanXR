#pragma once

#include <string>

// -- Constants ----
enum class eCalibrationPatternType : int
{
	INVALID= -1,

	mode_chessboard,
	mode_charuco,
	mode_aruco,

	COUNT
};
extern const std::string* k_patternTypeStrings;

enum class eCharucoDictionaryType : int
{
	INVALID= -1,

	DICT_4X4,
	DICT_5X5,
	DICT_6X6,
	DICT_7X7,

	COUNT
};
extern const std::string* k_charucoDictionaryStrings;

enum class eShapeType : int
{
	INVALID= -1,

	quad,
	box,
	model,

	COUNT
};
extern const char** k_szShapeTypeStrings;
extern const std::string* k_shapeTypeStrings;

enum class eStencilType : int
{
	INVALID= -1,

	quad,
	box,
	model,

	COUNT
};
extern const char** k_szStencilTypeStrings;
extern const std::string* k_stencilTypeStrings;

enum class eTrackingVolumeType : int
{
	INVALID= -1,

	marker,
	vr,

	COUNT
};
extern const char** k_szTrackingVolumeTypeStrings;
extern const std::string* k_trackingVolumeTypeStrings;

enum class eTrackingRuntime : int
{
	INVALID= -1,

	SteamVR,

	COUNT
};
extern const std::string* k_trackingRuntimeStrings;

enum class eTextureSourceType : int
{
	INVALID= -1,

	client,
	spout,
	cef,

	COUNT
};
extern const char** k_szTextureSourceTypeStrings;
extern const std::string* k_textureSourceTypeStrings;

enum class eVideoSourceType : int
{
	INVALID= -1,

	usb,
	networked,
	arkit,
	file,

	COUNT
};
extern const char** k_szVideoSourceTypeStrings;
extern const std::string* k_videoSourceTypeStrings;

#define CHESSBOARD_PATTERN_W 7 // Internal corners
#define CHESSBOARD_PATTERN_H 5
#define DEFAULT_SQUARE_LEN_MM 30

#define CHARUCO_PATTERN_W 11
#define CHARUCO_PATTERN_H 8
#define DEFAULT_CHARUCO_SQUARE_LEN_MM 16
#define DEFAULT_CHARUCO_MARKER_LEN_MM 12
#define DEFAULT_CHARUCO_DICTIONARY_TYPE eCharucoDictionaryType::DICT_6X6

#define DEFAULT_PUCK_HORIZONTAL_OFFSET_MM 211 // 51mm puck to pattern + 16mm squares *10squares
#define DEFAULT_PUCK_VERTICAL_OFFSET_MM 48    // 16mm square * 3 squares
#define DEFAULT_PUCK_DEPTH_OFFSET_MM 0

#define DEFAULT_ORIGIN_MARKER_ID 0
#define DEFAULT_UTILITY_MARKER_ID 16
#define DEFAULT_MARKER_SIZE_MM 100.f
#define DEFAULT_ARUCO_DICTIONARY_TYPE eCharucoDictionaryType::DICT_6X6

#define MIN_CHARUCO_CORNER_COUNT 4
#define MAX_CHARUCO_CORNER_COUNT 15

#define DEFAULT_VIDEO_FRAME_QUEUE_SIZE 2
#define DEFAULT_SPOUT_OUTPUT_NAME "MikanXR"

#define DEFAULT_RTMP_PORT 1935 // Default RTMP port for networked cameras
#define DEFAULT_RTSP_PORT 8554 // Default RTSP port for networked cameras
#define DEFAULT_NETWORKED_CAMERA_ADDRESS "rtspstream:X14yQN79F7O4sgqDz45CR@zephyr.rtsp.stream"
#define DEFAULT_NETWORKED_CAMERA_PATH "pattern"

// video RTP on basePort+0, depth on basePort+1, pose on basePort+2 (see
// ARKitWireProtocol.h)
#define DEFAULT_ARKIT_BASE_PORT 27015

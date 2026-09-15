#include "ProjectConfigConstants.h"

const std::string g_patternTypeStrings[(int)eCalibrationPatternType::COUNT]= {"chessboard", "charuco", "aruco"};
const std::string* k_patternTypeStrings= g_patternTypeStrings;

const std::string g_charucoDictionaryStrings[(int)eCharucoDictionaryType::COUNT]= {"DICT_4X4", "DICT_5X5", "DICT_6X6",
																				   "DICT_7X7"};
const std::string* k_charucoDictionaryStrings= g_charucoDictionaryStrings;

const char* g_szShapeTypeStrings[(int)eShapeType::COUNT]= {
	"quad",
	"box",
	"model",
};
const std::string g_shapeTypeStrings[(int)eShapeType::COUNT]= {
	g_szShapeTypeStrings[0],
	g_szShapeTypeStrings[1],
	g_szShapeTypeStrings[2],
};
const char** k_szShapeTypeStrings= g_szShapeTypeStrings;
const std::string* k_shapeTypeStrings= g_shapeTypeStrings;

const char* g_szStencilTypeStrings[(int)eStencilType::COUNT]= {
	"quad",
	"box",
	"model",
};
const std::string g_stencilTypeStrings[(int)eStencilType::COUNT]= {
	g_szStencilTypeStrings[0],
	g_szStencilTypeStrings[1],
	g_szStencilTypeStrings[2],
};
const char** k_szStencilTypeStrings= g_szStencilTypeStrings;
const std::string* k_stencilTypeStrings= g_stencilTypeStrings;

const char* g_szTrackingVolumeTypeStrings[(int)eTrackingVolumeType::COUNT]= {
	"marker",
	"vr",
};
const std::string g_TrackingVolumeTypeStrings[(int)eTrackingVolumeType::COUNT]= {
	g_szTrackingVolumeTypeStrings[0],
	g_szTrackingVolumeTypeStrings[1],
};
const char** k_szTrackingVolumeTypeStrings= g_szTrackingVolumeTypeStrings;
const std::string* k_trackingVolumeTypeStrings= g_TrackingVolumeTypeStrings;

const std::string g_trackingRuntimeStrings[(int)eTrackingRuntime::COUNT]= {"SteamVR"};
const std::string* k_trackingRuntimeStrings= g_trackingRuntimeStrings;

const char* g_szTextureSourceTypeStrings[(int)eTextureSourceType::COUNT]= {"client", "spout", "cef"};
const std::string g_textureSourceTypeStrings[(int)eTextureSourceType::COUNT]= {
	g_szTextureSourceTypeStrings[0], g_szTextureSourceTypeStrings[1], g_szTextureSourceTypeStrings[2]};
extern const char** k_szTextureSourceTypeStrings= g_szTextureSourceTypeStrings;
extern const std::string* k_textureSourceTypeStrings= g_textureSourceTypeStrings;

const char* g_szVideoSourceTypeStrings[(int)eVideoSourceType::COUNT]= {"usb", "networked", "arkit", "file"};
const std::string g_videoSourceTypeStrings[(int)eVideoSourceType::COUNT]= {
	g_szVideoSourceTypeStrings[0], g_szVideoSourceTypeStrings[1], g_szVideoSourceTypeStrings[2],
	g_szVideoSourceTypeStrings[3]};
extern const char** k_szVideoSourceTypeStrings= g_szVideoSourceTypeStrings;
extern const std::string* k_videoSourceTypeStrings= g_videoSourceTypeStrings;
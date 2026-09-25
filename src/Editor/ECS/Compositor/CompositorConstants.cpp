#include "CompositorConstants.h"
#include "Logger.h"
#include "StringUtils.h"
#include "opencv2/opencv.hpp"

const std::string g_compositorLayerAlphaStrings[(int)eCompositorLayerAlphaMode::COUNT]= {"NoAlpha", "ColorKey",
																						 "AlphaChannel", "MagicPortal"};
const std::string* k_compositorLayerAlphaStrings= g_compositorLayerAlphaStrings;

const std::string g_supportedCodecName[(int)eSupportedCodec::COUNT]= {
	"MP4V",
	"MJPG",
	"RGBA",
};
const std::string* k_supportedCodecName= g_supportedCodecName;

const std::string g_supportedCodecFileSuffix[(int)eSupportedCodec::COUNT]= {
	".m4v",
	".avi",
	".avi",
};
const std::string* k_supportedCodecFileSuffix= g_supportedCodecFileSuffix;

const int g_supportedCodecFourCC[(int)eSupportedCodec::COUNT]= {
	cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
	cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
	cv::VideoWriter::fourcc('R', 'G', 'B', 'A'),
};
const int* k_supportedCodecFourCC= g_supportedCodecFourCC;

const std::string g_compositorStencilModeStrings[(int)eCompositorStencilMode::COUNT]= {"noStencil", "insideStencil",
																					   "outsideStencil"};
const std::string* k_compositorStencilModeStrings= g_compositorStencilModeStrings;

const std::string g_compositorBlendModeStrings[(int)eCompositorBlendMode::COUNT]= {"blendOff", "blendNormal",
																				   "blendMultiply"};
const std::string* k_compositorBlendModeStrings= g_compositorBlendModeStrings;

eCompositorBlendMode resolveCompositorBlendMode(const std::string& blendModeString,
												eCompositorBlendMode defaultBlendMode)
{
	const eCompositorBlendMode blendMode=
		StringUtils::FindEnumValue<eCompositorBlendMode>(blendModeString, k_compositorBlendModeStrings);

	if (blendMode == eCompositorBlendMode::INVALID)
	{
		MIKAN_LOG_WARNING("resolveCompositorBlendMode")
			<< "Unknown blend mode '" << blendModeString << "', falling back to '"
			<< k_compositorBlendModeStrings[(int)defaultBlendMode] << "'";

		return defaultBlendMode;
	}

	return blendMode;
}

const std::string g_stencilCullModeStrings[(int)eStencilCullMode::COUNT]= {
	"none",
	"zAxis",
	"yAxis",
	"xAxis",
};
const std::string* k_stencilCullModeStrings= g_stencilCullModeStrings;

const std::string g_textureSourceColorTypeStrings[(int)eTextureSourceColorType::COUNT]= {"colorRGB", "colorRGBA",
																						 "shadowRGB", "shadowRGBA"};
const std::string* k_textureSourceColorTypeStrings= g_textureSourceColorTypeStrings;

const std::string g_colorTextureFallbackModeStrings[(int)eColorTextureFallbackMode::COUNT]= {
	"auto", "transparentBlack", "opaqueBlack", "opaqueWhite"};
const std::string* k_colorTextureFallbackModeStrings= g_colorTextureFallbackModeStrings;

const std::string g_colorTextureResolveAlphaModeStrings[(int)eColorTextureResolveAlphaMode::COUNT]= {"none", "straight",
																									 "inverted"};
const std::string* k_colorTextureResolveAlphaModeStrings= g_colorTextureResolveAlphaModeStrings;

const std::string g_textureSourceDepthTypeStrings[(int)eTextureSourceDepthType::COUNT]= {"depthPackRGBA"};
const std::string* k_textureSourceDepthTypeStrings= g_textureSourceDepthTypeStrings;

const std::string g_compositorEvaluatorWindow[(int)eCompositorEvaluatorWindow::COUNT]= {"mainWindow", "editorWindow"};
const std::string* k_compositorEvaluatorWindow= g_compositorEvaluatorWindow;
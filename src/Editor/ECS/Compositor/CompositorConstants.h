#pragma once

#include <string>

#define DEFAULT_COMPOSITOR_CONFIG_NAME "Alpha Channel"
#define MAX_CLIENT_SOURCES 8
#define EMPTY_SOURCE_NAME "empty"

enum class eSupportedCodec : int
{
	INVALID= -1,

	MP4V,
	MJPG,
	RGBA,

	COUNT
};
extern const std::string* k_supportedCodecName;
extern const std::string* k_supportedCodecFileSuffix;
extern const int* k_supportedCodecFourCC;

enum class eCompositorLayerAlphaMode : int
{
	INVALID= -1,

	NoAlpha,
	ColorKey,
	AlphaChannel,
	MagicPortal,

	COUNT
};
extern const std::string* k_compositorLayerAlphaStrings;

enum class eCompositorStencilMode : int
{
	INVALID= -1,

	noStencil,
	insideStencil,
	outsideStencil,

	COUNT
};
extern const std::string* k_compositorStencilModeStrings;

enum class eCompositorBlendMode : int
{
	INVALID= -1,

	blendOff= 0,
	blendNormal= 1,
	blendMultiply= 2,

	COUNT
};
extern const std::string* k_compositorBlendModeStrings;

// Resolves a persisted blend mode string, falling back to defaultBlendMode when it names none of
// k_compositorBlendModeStrings. Worth going through rather than calling FindEnumValue directly:
// that returns INVALID for an unrecognized string, and an INVALID blend mode matches no case in the
// draw nodes' switch, so the draw silently inherits whatever blend state was already set. Saved
// graphs carrying a stencil mode string in this field have been seen in the wild.
eCompositorBlendMode resolveCompositorBlendMode(const std::string& blendModeString,
												eCompositorBlendMode defaultBlendMode);

enum class eStencilCullMode : int
{
	INVALID= -1,

	none,
	zAxis,
	yAxis,
	xAxis,

	COUNT
};
extern const std::string* k_stencilCullModeStrings;

enum class eTextureSourceColorType : int
{
	INVALID= -1,
	// Base source color texture, intended to be composited using "Over"/"Normal" blending
	// i.e. result = source*a + video*(1-a)
	colorRGB,
	colorRGBA,
	// Shadow (light survival) factor buffer, intended to be composited multiplicatively; identity is white.
	// i.e. result = video*shadow
	shadowRGB,
	shadowRGBA,

	COUNT
};
extern const std::string* k_textureSourceColorTypeStrings;

// Fallback texture a ColorTextureSourceNode emits when no client texture is available yet.
// The "identity" texture that composites as a no-op depends on the downstream material/blend
// (e.g. an inverted-alpha "Normal" layer needs opaque black, a "Multiply" layer needs white),
// which can't be inferred in C++ — so the graph author selects it per node. autoByType keeps the
// legacy behavior of picking the default from the color type.
enum class eColorTextureFallbackMode : int
{
	INVALID= -1,

	autoByType,       // default derived from eTextureSourceColorType (backward compatible)
	transparentBlack, // (0,0,0,0)
	opaqueBlack,      // (0,0,0,1) — identity for an inverted-alpha "Normal"/"Over" layer
	opaqueWhite,      // (1,1,1,1) — identity for a "Multiply" layer

	COUNT
};
extern const std::string* k_colorTextureFallbackModeStrings;

// What the alpha channel of a client color texture means, which decides how a
// ColorTextureSourceNode filters the texture down when the client renders above the video
// resolution. Averaging color and alpha separately drags the empty region's color into a
// silhouette edge; weighting color by coverage first does not. Which encoding the client
// used is a property of the downstream layer material (the bundled rgbInvAlphaFrame reads
// 1-a as coverage), and C++ cannot introspect GLSL, so the graph author selects it per node.
// none keeps the plain average every graph got before the option existed.
enum class eColorTextureResolveAlphaMode : int
{
	INVALID= -1,

	none,     // alpha carries no coverage (shadow buffers, or a layer that ignores alpha)
	straight, // alpha is coverage
	inverted, // alpha is 1 - coverage, what an inverted-alpha "Normal"/"Over" layer reads

	COUNT
};
extern const std::string* k_colorTextureResolveAlphaModeStrings;

enum class eTextureSourceDepthType : int
{
	INVALID= -1,

	depthPackRGBA,

	COUNT
};
extern const std::string* k_textureSourceDepthTypeStrings;

enum class eCompositorEvaluatorWindow : int
{
	INVALID= -1,

	mainWindow,
	editorWindow,

	COUNT
};
extern const std::string* k_compositorEvaluatorWindow;
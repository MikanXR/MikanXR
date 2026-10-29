#pragma once

#include "glm/ext/vector_float4.hpp"

#include <vector>

// CPU reference of the depth of field the DepthOfFieldNode runs on the GPU,
// with the same passes and the same formulas, so the shaders can be read
// against it and the tests can pin the behaviour without a GL context. Images
// are row-major, width * height * channels. Depth is in the compositor's
// normalized linear convention (1 where nothing was drawn), color is four
// channels in the alpha convention given by alphaMode (the integer value of
// eColorTextureResolveAlphaMode: 0 none, read as coverage, 1 coverage, 2 one
// minus coverage).
namespace DepthOfFieldReference
{
struct Image
{
	int width= 0;
	int height= 0;
	std::vector<float> values; // width * height * channels

	Image() {}
	Image(int w, int h, int channels, float fill= 0.f)
		: width(w)
		, height(h)
		, values((size_t)w * h * channels, fill)
	{
	}
};

// Linear depth at or above this is a pixel the client drew nothing in
constexpr float k_emptyDepth= 0.999f;
// Taps in the gather's golden angle spiral
constexpr int k_tapCount= 64;

// The lens uniform the shaders take: zNear and zFar in meters, the circle of
// confusion scale (half the focal length in pixels times the aperture diameter
// in meters), and 1 / focus distance. The thin lens blur diameter on the image
// is focalLength * aperture * |1/focus - 1/depth|, with the exact factor
// focus / (focus - focalLength) dropped as within a few percent at any focus
// distance a compositing camera uses.
glm::vec4 computeLensParams(float zNear, float zFar, float focalLengthPixels, float apertureMeters,
							float focusDistance);

// Eye depth in meters at a normalized coordinate of a depth image of any size,
// or -1 where the client drew nothing
float eyeDepthAt(const Image& depth, float u, float v, const glm::vec4& lensParams);

// Blur radius in pixels for an eye depth, 0 where the client drew nothing
float cocRadius(float eyeDepth, const glm::vec4& lensParams, float maxRadius);

// Pass 1: the blur radius per pixel of an image this size, one channel
Image circleOfConfusion(const Image& depth, int width, int height, const glm::vec4& lensParams, float maxRadius);

// The separable min/max pair the depth min/max shaders run, with clamped
// borders and the radius clamped to [1, 32] as the shaders clamp it. The
// horizontal pass takes one channel and gives min then max over
// [x - radius, x + radius]; the vertical pass takes that and gives the min of
// the mins and the max of the maxes over [y - radius, y + radius].
Image minMaxHorizontal(const Image& image, int radius);
Image minMaxVertical(const Image& horizontalMinMax, int radius);

// Pass 2: the min/max pair over the radius bound; channel 1 (the max) is the
// largest blur that reaches each pixel
Image dilate(const Image& coc, int maxRadius);

// Pass 3: the scatter-as-gather disc blur of the color out to the dilated reach
Image gather(const Image& color, const Image& depth, const Image& reach, const glm::vec4& lensParams, float maxRadius,
			 int alphaMode);

// All passes
Image applyDepthOfField(const Image& color, const Image& depth, const glm::vec4& lensParams, int maxRadius,
						int alphaMode);
} // namespace DepthOfFieldReference

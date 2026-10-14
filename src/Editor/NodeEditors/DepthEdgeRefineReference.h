#pragma once

#include <vector>

// CPU reference of the depth edge refinement the DepthEdgeRefineNode runs on
// the GPU, with the same three passes and the same formulas, so the shaders
// can be read against it and the tests can pin the behaviour without a GL
// context. Images are row-major, width * height, depth in the compositor's
// normalized linear convention (1 where nothing was drawn).
namespace DepthEdgeRefineReference
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

// Pass 1: per pixel, the min and max depth over [x - radius, x + radius] with
// clamped borders. Output has two channels: min then max.
Image minMaxHorizontal(const Image& depth, int radius);

// Pass 2: per pixel, the min of the mins and the max of the maxes over
// [y - radius, y + radius] of a pass 1 result. Output has two channels.
Image minMaxVertical(const Image& horizontalMinMax, int radius);

// Pass 3: where the window's (max - min) exceeds depthJump * max the pixel
// takes the joint bilateral two-sided vote over the (2 radius + 1)^2 window,
// guided by the linear colour image (three channels, same size as depth);
// elsewhere the depth passes through. Spatial sigma is radius / 2.
Image refine(const Image& depth, const Image& colorLinear, const Image& windowMinMax, int radius, float colorSigma,
			 float depthJump);

// All three passes
Image refineDepth(const Image& depth, const Image& colorLinear, int radius, float colorSigma, float depthJump);
} // namespace DepthEdgeRefineReference

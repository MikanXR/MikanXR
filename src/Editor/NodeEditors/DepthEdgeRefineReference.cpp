#include "DepthEdgeRefineReference.h"

#include <algorithm>

namespace DepthEdgeRefineReferenceInternal
{
// The shaders clamp the band radius to the same range
const int k_minRadius= 1;
const int k_maxRadius= 32;

struct Color
{
	float r= 0.f;
	float g= 0.f;
	float b= 0.f;
};

float distanceSquared(const Color& a, const Color& b)
{
	const float dr= a.r - b.r;
	const float dg= a.g - b.g;
	const float db= a.b - b.b;

	return dr * dr + dg * dg + db * db;
}

bool hasLayout(const DepthEdgeRefineReference::Image& image, int channels)
{
	return image.width > 0 && image.height > 0 && image.values.size() == (size_t)image.width * image.height * channels;
}

bool hasSameSize(const DepthEdgeRefineReference::Image& a, const DepthEdgeRefineReference::Image& b)
{
	return a.width == b.width && a.height == b.height;
}

int clampRadius(int radius) { return std::clamp(radius, k_minRadius, k_maxRadius); }

size_t pixelIndex(const DepthEdgeRefineReference::Image& image, int x, int y) { return (size_t)y * image.width + x; }
} // namespace DepthEdgeRefineReferenceInternal

namespace DepthEdgeRefineReference
{
Image minMaxHorizontal(const Image& depth, int radius)
{
	namespace Internal= DepthEdgeRefineReferenceInternal;

	if (!Internal::hasLayout(depth, 1))
	{
		return Image();
	}

	const int r= Internal::clampRadius(radius);
	const int lastX= depth.width - 1;

	Image result(depth.width, depth.height, 2);
	for (int y= 0; y < depth.height; ++y)
	{
		for (int x= 0; x < depth.width; ++x)
		{
			const float center= depth.values[Internal::pixelIndex(depth, x, y)];
			float mn= center;
			float mx= center;

			for (int i= -r; i <= r; ++i)
			{
				const int sampleX= std::clamp(x + i, 0, lastX);
				const float d= depth.values[Internal::pixelIndex(depth, sampleX, y)];

				mn= std::min(mn, d);
				mx= std::max(mx, d);
			}

			const size_t outIndex= Internal::pixelIndex(result, x, y) * 2;
			result.values[outIndex]= mn;
			result.values[outIndex + 1]= mx;
		}
	}

	return result;
}

Image minMaxVertical(const Image& horizontalMinMax, int radius)
{
	namespace Internal= DepthEdgeRefineReferenceInternal;

	if (!Internal::hasLayout(horizontalMinMax, 2))
	{
		return Image();
	}

	const int r= Internal::clampRadius(radius);
	const int lastY= horizontalMinMax.height - 1;

	Image result(horizontalMinMax.width, horizontalMinMax.height, 2);
	for (int y= 0; y < horizontalMinMax.height; ++y)
	{
		for (int x= 0; x < horizontalMinMax.width; ++x)
		{
			const size_t centerIndex= Internal::pixelIndex(horizontalMinMax, x, y) * 2;
			float mn= horizontalMinMax.values[centerIndex];
			float mx= horizontalMinMax.values[centerIndex + 1];

			for (int j= -r; j <= r; ++j)
			{
				const int sampleY= std::clamp(y + j, 0, lastY);
				const size_t sampleIndex= Internal::pixelIndex(horizontalMinMax, x, sampleY) * 2;

				mn= std::min(mn, horizontalMinMax.values[sampleIndex]);
				mx= std::max(mx, horizontalMinMax.values[sampleIndex + 1]);
			}

			const size_t outIndex= Internal::pixelIndex(result, x, y) * 2;
			result.values[outIndex]= mn;
			result.values[outIndex + 1]= mx;
		}
	}

	return result;
}

Image refine(const Image& depth, const Image& colorLinear, const Image& windowMinMax, int radius, float colorSigma,
			 float depthJump)
{
	namespace Internal= DepthEdgeRefineReferenceInternal;

	if (!Internal::hasLayout(depth, 1) || !Internal::hasLayout(colorLinear, 3) || !Internal::hasLayout(windowMinMax, 2)
		|| !Internal::hasSameSize(depth, colorLinear) || !Internal::hasSameSize(depth, windowMinMax))
	{
		return Image();
	}

	const int r= Internal::clampRadius(radius);
	const int lastX= depth.width - 1;
	const int lastY= depth.height - 1;
	const float separationSq= colorSigma * colorSigma;

	Image result(depth.width, depth.height, 1);
	for (int y= 0; y < depth.height; ++y)
	{
		for (int x= 0; x < depth.width; ++x)
		{
			const size_t pIndex= Internal::pixelIndex(depth, x, y);
			const float dp= depth.values[pIndex];
			const float mn= windowMinMax.values[pIndex * 2];
			const float mx= windowMinMax.values[pIndex * 2 + 1];

			// Every pixel without evidence for a move keeps its own depth
			result.values[pIndex]= dp;

			// Away from a depth discontinuity the mask passes through untouched
			if (mx - mn <= depthJump * mx)
			{
				continue;
			}

			// Split the window at the mid depth and build each side's colour prototype
			// and mean depth, counts kept as floats as the shader does
			const float mid= 0.5f * (mn + mx);

			float nNear= 0.f;
			float nFar= 0.f;
			float sumDNear= 0.f;
			float sumDFar= 0.f;
			Internal::Color sumINear;
			Internal::Color sumIFar;
			for (int dy= -r; dy <= r; ++dy)
			{
				for (int dx= -r; dx <= r; ++dx)
				{
					const int qx= std::clamp(x + dx, 0, lastX);
					const int qy= std::clamp(y + dy, 0, lastY);
					const size_t qIndex= Internal::pixelIndex(depth, qx, qy);
					const float dq= depth.values[qIndex];
					const float* colorQ= &colorLinear.values[qIndex * 3];

					const bool bNear= dq < mid;
					float& n= bNear ? nNear : nFar;
					float& sumD= bNear ? sumDNear : sumDFar;
					Internal::Color& sumI= bNear ? sumINear : sumIFar;

					n+= 1.f;
					sumD+= dq;
					sumI.r+= colorQ[0];
					sumI.g+= colorQ[1];
					sumI.b+= colorQ[2];
				}
			}

			// One side missing leaves nothing to choose between
			if (nNear == 0.f || nFar == 0.f)
			{
				continue;
			}

			const Internal::Color muNear= {sumINear.r / nNear, sumINear.g / nNear, sumINear.b / nNear};
			const Internal::Color muFar= {sumIFar.r / nFar, sumIFar.g / nFar, sumIFar.b / nFar};

			// Prototypes closer than the colour threshold cannot tell the sides apart
			if (Internal::distanceSquared(muNear, muFar) < separationSq)
			{
				continue;
			}

			// The pixel joins the side whose colour prototype it is nearer to
			const float* colorPValues= &colorLinear.values[pIndex * 3];
			const Internal::Color colorP= {colorPValues[0], colorPValues[1], colorPValues[2]};
			const float distNear= Internal::distanceSquared(colorP, muNear);
			const float distFar= Internal::distanceSquared(colorP, muFar);

			result.values[pIndex]= distNear <= distFar ? sumDNear / nNear : sumDFar / nFar;
		}
	}

	return result;
}

Image refineDepth(const Image& depth, const Image& colorLinear, int radius, float colorSigma, float depthJump)
{
	const Image windowMinMax= minMaxVertical(minMaxHorizontal(depth, radius), radius);

	return refine(depth, colorLinear, windowMinMax, radius, colorSigma, depthJump);
}
} // namespace DepthEdgeRefineReference

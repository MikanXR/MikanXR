#include "DepthOfFieldReference.h"

#include <algorithm>
#include <cmath>

namespace DepthOfFieldReferenceInternal
{
const float k_goldenAngle= 2.39996323f;
const float k_pi= 3.14159265f;
// The depth min/max shaders clamp the radius to the same range
const int k_minMaxMinRadius= 1;
const int k_minMaxMaxRadius= 32;

bool hasLayout(const DepthOfFieldReference::Image& image, int channels)
{
	return image.width > 0 && image.height > 0 && image.values.size() == (size_t)image.width * image.height * channels;
}

int clampMinMaxRadius(int radius) { return std::clamp(radius, k_minMaxMinRadius, k_minMaxMaxRadius); }

size_t pixelIndex(const DepthOfFieldReference::Image& image, int x, int y) { return (size_t)y * image.width + x; }

struct Premultiplied
{
	float r= 0.f;
	float g= 0.f;
	float b= 0.f;
	float a= 0.f;

	Premultiplied& operator+=(const Premultiplied& other)
	{
		r+= other.r;
		g+= other.g;
		b+= other.b;
		a+= other.a;
		return *this;
	}
};

Premultiplied scaled(const Premultiplied& value, float scale)
{
	return Premultiplied{value.r * scale, value.g * scale, value.b * scale, value.a * scale};
}

// texelFetch at a normalized coordinate, clamped to the edges
const float* texelAt(const DepthOfFieldReference::Image& image, float u, float v)
{
	const int x= std::clamp((int)(u * (float)image.width), 0, image.width - 1);
	const int y= std::clamp((int)(v * (float)image.height), 0, image.height - 1);

	return &image.values[((size_t)y * image.width + x) * 4];
}

Premultiplied premultipliedAt(const DepthOfFieldReference::Image& color, float u, float v, int alphaMode)
{
	const float* texel= texelAt(color, u, v);

	const float coverage= alphaMode == 2 ? (1.f - texel[3]) : texel[3];
	return Premultiplied{texel[0] * coverage, texel[1] * coverage, texel[2] * coverage, coverage};
}

// The share of a source's spread energy that one tap of the given area receives. A source
// covers its own half pixel plus its blur radius, and spreads over at least that half
// pixel's area.
float tapWeight(float radius, float tapDistance, float tapArea)
{
	const float r= std::max(radius, 0.5f);
	return tapArea * std::clamp(radius - tapDistance + 0.5f, 0.f, 1.f) / (k_pi * r * r);
}
} // namespace DepthOfFieldReferenceInternal

glm::vec4 DepthOfFieldReference::computeLensParams(float zNear, float zFar, float focalLengthPixels,
												   float apertureMeters, float focusDistance)
{
	return glm::vec4(zNear, zFar, 0.5f * focalLengthPixels * apertureMeters, 1.f / focusDistance);
}

float DepthOfFieldReference::eyeDepthAt(const Image& depth, float u, float v, const glm::vec4& lensParams)
{
	const int x= std::clamp((int)(u * (float)depth.width), 0, depth.width - 1);
	const int y= std::clamp((int)(v * (float)depth.height), 0, depth.height - 1);
	const float linearDepth= depth.values[(size_t)y * depth.width + x];

	return linearDepth >= k_emptyDepth ? -1.f : lensParams.x + (lensParams.y - lensParams.x) * linearDepth;
}

float DepthOfFieldReference::cocRadius(float eyeDepth, const glm::vec4& lensParams, float maxRadius)
{
	if (eyeDepth < 0.f)
		return 0.f;

	return std::min(lensParams.z * std::abs(lensParams.w - 1.f / std::max(eyeDepth, 1e-4f)), maxRadius);
}

DepthOfFieldReference::Image DepthOfFieldReference::circleOfConfusion(const Image& depth, int width, int height,
																	  const glm::vec4& lensParams, float maxRadius)
{
	Image coc(width, height, 1);
	for (int y= 0; y < height; ++y)
	{
		for (int x= 0; x < width; ++x)
		{
			const float u= ((float)x + 0.5f) / (float)width;
			const float v= ((float)y + 0.5f) / (float)height;
			coc.values[(size_t)y * width + x]= cocRadius(eyeDepthAt(depth, u, v, lensParams), lensParams, maxRadius);
		}
	}

	return coc;
}

DepthOfFieldReference::Image DepthOfFieldReference::minMaxHorizontal(const Image& image, int radius)
{
	namespace Internal= DepthOfFieldReferenceInternal;

	if (!Internal::hasLayout(image, 1))
	{
		return Image();
	}

	const int r= Internal::clampMinMaxRadius(radius);
	const int lastX= image.width - 1;

	Image result(image.width, image.height, 2);
	for (int y= 0; y < image.height; ++y)
	{
		for (int x= 0; x < image.width; ++x)
		{
			const float center= image.values[Internal::pixelIndex(image, x, y)];
			float mn= center;
			float mx= center;

			for (int i= -r; i <= r; ++i)
			{
				const int sampleX= std::clamp(x + i, 0, lastX);
				const float d= image.values[Internal::pixelIndex(image, sampleX, y)];

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

DepthOfFieldReference::Image DepthOfFieldReference::minMaxVertical(const Image& horizontalMinMax, int radius)
{
	namespace Internal= DepthOfFieldReferenceInternal;

	if (!Internal::hasLayout(horizontalMinMax, 2))
	{
		return Image();
	}

	const int r= Internal::clampMinMaxRadius(radius);
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

DepthOfFieldReference::Image DepthOfFieldReference::dilate(const Image& coc, int maxRadius)
{
	return minMaxVertical(minMaxHorizontal(coc, maxRadius), maxRadius);
}

DepthOfFieldReference::Image DepthOfFieldReference::gather(const Image& color, const Image& depth, const Image& reach,
														   const glm::vec4& lensParams, float maxRadius, int alphaMode)
{
	using namespace DepthOfFieldReferenceInternal;

	const int width= color.width;
	const int height= color.height;
	Image result(width, height, 4);

	for (int y= 0; y < height; ++y)
	{
		for (int x= 0; x < width; ++x)
		{
			const size_t pixel= (size_t)y * width + x;
			const float u= ((float)x + 0.5f) / (float)width;
			const float v= ((float)y + 0.5f) / (float)height;
			const float pixelReach= reach.values[pixel * 2 + 1];

			if (pixelReach < 0.5f)
			{
				const float* texel= texelAt(color, u, v);
				std::copy(texel, texel + 4, result.values.begin() + pixel * 4);
				continue;
			}

			const float centerDepth= eyeDepthAt(depth, u, v, lensParams);
			const float centerRadius= cocRadius(centerDepth, lensParams, maxRadius);
			const float tapArea= k_pi * pixelReach * pixelReach / (float)k_tapCount;

			// The center pixel is background at its own area
			float backgroundWeight= tapWeight(centerRadius, 0.f, 1.f);
			Premultiplied backgroundSum= scaled(premultipliedAt(color, u, v, alphaMode), backgroundWeight);
			Premultiplied foregroundSum;
			float foregroundEnergy= 0.f;

			for (int i= 0; i < k_tapCount; ++i)
			{
				const float tapDistance= pixelReach * std::sqrt(((float)i + 0.5f) / (float)k_tapCount);
				const float angle= (float)i * k_goldenAngle;
				const float tapU= u + tapDistance * std::cos(angle) / (float)width;
				const float tapV= v + tapDistance * std::sin(angle) / (float)height;

				const float tapDepth= eyeDepthAt(depth, tapU, tapV, lensParams);
				const Premultiplied tapColor= premultipliedAt(color, tapU, tapV, alphaMode);
				const bool bNearer= tapDepth >= 0.f && (centerDepth < 0.f || tapDepth < centerDepth * 0.98f);
				if (bNearer)
				{
					const float weight= tapWeight(cocRadius(tapDepth, lensParams, maxRadius), tapDistance, tapArea);
					foregroundSum+= scaled(tapColor, weight);
					foregroundEnergy+= weight;
				}
				else
				{
					const float radius= tapDepth < 0.f
											? centerRadius
											: std::min(cocRadius(tapDepth, lensParams, maxRadius), centerRadius);
					const float weight= tapWeight(radius, tapDistance, tapArea);
					backgroundSum+= scaled(tapColor, weight);
					backgroundWeight+= weight;
				}
			}

			// A nearer surface's energy covers at most the whole pixel
			const Premultiplied foreground=
				foregroundEnergy > 0.f ? scaled(foregroundSum, std::min(foregroundEnergy, 1.f) / foregroundEnergy)
									   : Premultiplied();
			const Premultiplied background= scaled(backgroundSum, 1.f / backgroundWeight);
			Premultiplied combined= foreground;
			combined+= scaled(background, 1.f - foreground.a);

			float* out= &result.values[pixel * 4];
			const float unpremultiply= combined.a > 1e-5f ? 1.f / combined.a : 0.f;
			out[0]= combined.r * unpremultiply;
			out[1]= combined.g * unpremultiply;
			out[2]= combined.b * unpremultiply;
			out[3]= alphaMode == 2 ? (1.f - combined.a) : combined.a;
		}
	}

	return result;
}

DepthOfFieldReference::Image DepthOfFieldReference::applyDepthOfField(const Image& color, const Image& depth,
																	  const glm::vec4& lensParams, int maxRadius,
																	  int alphaMode)
{
	const Image coc= circleOfConfusion(depth, color.width, color.height, lensParams, (float)maxRadius);
	const Image reach= dilate(coc, maxRadius);

	return gather(color, depth, reach, lensParams, (float)maxRadius, alphaMode);
}

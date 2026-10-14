#include "DepthEdgeRefineTests.h"
#include "DepthEdgeRefineReference.h"
#include "unit_test.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <random>
#include <stdio.h>

namespace DepthEdgeRefineTestsInternal
{
using Image= DepthEdgeRefineReference::Image;

const int k_stepX= 20;
const float k_nearDepth= 0.2f;
const float k_farDepth= 1.f;

// Near depth left of the step column, far depth from it onwards, every row alike
Image makeDepthStep(int width, int height)
{
	Image depth(width, height, 1);
	for (int y= 0; y < height; ++y)
	{
		for (int x= 0; x < width; ++x)
		{
			depth.values[(size_t)y * width + x]= x < k_stepX ? k_nearDepth : k_farDepth;
		}
	}

	return depth;
}

// Grey level `left` before the edge column and `right` from it onwards
Image makeColorStep(int width, int height, int edgeX, float left, float right)
{
	Image color(width, height, 3);
	for (int y= 0; y < height; ++y)
	{
		for (int x= 0; x < width; ++x)
		{
			const float grey= x < edgeX ? left : right;
			const size_t index= ((size_t)y * width + x) * 3;
			color.values[index]= grey;
			color.values[index + 1]= grey;
			color.values[index + 2]= grey;
		}
	}

	return color;
}

float depthAt(const Image& image, int x, int y) { return image.values[(size_t)y * image.width + x]; }
} // namespace DepthEdgeRefineTestsInternal

bool run_depth_edge_refine_tests()
{
	UNIT_TEST_MODULE_BEGIN("depth_edge_refine")
	UNIT_TEST_MODULE_CALL_TEST(depth_edge_refine_test_config_round_trip);
	UNIT_TEST_MODULE_CALL_TEST(depth_edge_refine_test_minmax_separable);
	UNIT_TEST_MODULE_CALL_TEST(depth_edge_refine_test_vote_moves_edge);
	UNIT_TEST_MODULE_CALL_TEST(depth_edge_refine_test_uniform_color_keeps_majority);
	UNIT_TEST_MODULE_END()
}

bool depth_edge_refine_test_minmax_separable()
{
	UNIT_TEST_BEGIN("the separable min/max passes match a brute force window")

	using Image= DepthEdgeRefineReference::Image;

	const int width= 24;
	const int height= 16;

	std::mt19937 generator(1234);
	std::uniform_real_distribution<float> distribution(0.f, 1.f);

	Image depth(width, height, 1);
	for (float& value : depth.values)
	{
		value= distribution(generator);
	}

	const int radii[]= {1, 3, 8};
	for (int radius : radii)
	{
		const Image separable=
			DepthEdgeRefineReference::minMaxVertical(DepthEdgeRefineReference::minMaxHorizontal(depth, radius), radius);
		if (separable.width != width || separable.height != height
			|| separable.values.size() != (size_t)width * height * 2)
		{
			fprintf(stdout, "    radius %d: separable result has the wrong size\n", radius);
			success= false;
			continue;
		}

		bool bRadiusMatches= true;
		for (int y= 0; y < height && bRadiusMatches; ++y)
		{
			for (int x= 0; x < width && bRadiusMatches; ++x)
			{
				float mn= depth.values[(size_t)y * width + x];
				float mx= mn;
				for (int j= -radius; j <= radius; ++j)
				{
					for (int i= -radius; i <= radius; ++i)
					{
						const int sampleX= std::clamp(x + i, 0, width - 1);
						const int sampleY= std::clamp(y + j, 0, height - 1);
						const float d= depth.values[(size_t)sampleY * width + sampleX];
						mn= std::min(mn, d);
						mx= std::max(mx, d);
					}
				}

				const size_t index= ((size_t)y * width + x) * 2;
				if (separable.values[index] != mn || separable.values[index + 1] != mx)
				{
					fprintf(stdout, "    radius %d at (%d, %d): separable (%f, %f), brute force (%f, %f)\n", radius, x,
							y, separable.values[index], separable.values[index + 1], mn, mx);
					bRadiusMatches= false;
				}
			}
		}

		success&= bRadiusMatches;
	}

	UNIT_TEST_COMPLETE()
}

bool depth_edge_refine_test_vote_moves_edge()
{
	UNIT_TEST_BEGIN("the colour prototypes move the depth edge onto the colour edge")

	namespace Internal= DepthEdgeRefineTestsInternal;
	using Image= DepthEdgeRefineReference::Image;

	const int width= 48;
	const int height= 16;
	const int radius= 8;
	// Four columns past the depth step are dark like the near side, so they belong
	// to it once the sides are told apart by colour
	const int colorEdgeX= Internal::k_stepX + 4;

	const Image depth= Internal::makeDepthStep(width, height);
	const Image color= Internal::makeColorStep(width, height, colorEdgeX, 0.1f, 0.9f);
	const Image refined= DepthEdgeRefineReference::refineDepth(depth, color, radius, 0.1f, 0.05f);
	if (refined.width != width || refined.height != height || refined.values.size() != (size_t)width * height)
	{
		fprintf(stdout, "    refined result has the wrong size\n");
		success= false;
	}
	else
	{
		for (int y= 0; y < height; ++y)
		{
			for (int x= 0; x < width; ++x)
			{
				const float value= Internal::depthAt(refined, x, y);
				const float input= Internal::depthAt(depth, x, y);
				const bool bOutsideBand= x >= Internal::k_stepX + radius + 1 || x < Internal::k_stepX - radius - 1;

				bool bPixelOk= true;
				if (bOutsideBand && value != input)
				{
					fprintf(stdout, "    (%d, %d) outside the band changed from %f to %f\n", x, y, input, value);
					bPixelOk= false;
				}
				else if (x < colorEdgeX && std::fabs(value - Internal::k_nearDepth) > 1e-4f)
				{
					fprintf(stdout, "    (%d, %d) left of the colour edge reads %f, expected %f\n", x, y, value,
							Internal::k_nearDepth);
					bPixelOk= false;
				}
				else if (x >= colorEdgeX && value != Internal::k_farDepth)
				{
					fprintf(stdout, "    (%d, %d) right of the colour edge reads %f, expected %f\n", x, y, value,
							Internal::k_farDepth);
					bPixelOk= false;
				}

				if (!bPixelOk)
				{
					// One report per row keeps the log readable
					success= false;
					break;
				}
			}
		}
	}

	// A flat mask has no discontinuity, so nothing moves
	const Image flatDepth(width, height, 1, 0.5f);
	const Image flatRefined= DepthEdgeRefineReference::refineDepth(flatDepth, color, radius, 0.1f, 0.05f);
	if (flatRefined.values != flatDepth.values)
	{
		fprintf(stdout, "    a flat mask did not pass through unchanged\n");
		success= false;
	}

	UNIT_TEST_COMPLETE()
}

bool depth_edge_refine_test_uniform_color_keeps_majority()
{
	UNIT_TEST_BEGIN("under a uniform colour the sides cannot be told apart and the mask is unchanged")

	namespace Internal= DepthEdgeRefineTestsInternal;
	using Image= DepthEdgeRefineReference::Image;

	const int width= 48;
	const int height= 16;
	const int radius= 8;

	const Image depth= Internal::makeDepthStep(width, height);
	const Image color= Internal::makeColorStep(width, height, 0, 0.5f, 0.5f);
	const Image refined= DepthEdgeRefineReference::refineDepth(depth, color, radius, 0.1f, 0.05f);
	if (refined.width != width || refined.height != height || refined.values.size() != (size_t)width * height)
	{
		fprintf(stdout, "    refined result has the wrong size\n");
		success= false;
	}
	else if (refined.values != depth.values)
	{
		// The two prototypes coincide, so no pixel has evidence to change side
		for (int y= 0; y < height && success; ++y)
		{
			for (int x= 0; x < width; ++x)
			{
				if (Internal::depthAt(refined, x, y) != Internal::depthAt(depth, x, y))
				{
					fprintf(stdout, "    (%d, %d) changed from %f to %f under a uniform colour\n", x, y,
							Internal::depthAt(depth, x, y), Internal::depthAt(refined, x, y));
					success= false;
					break;
				}
			}
		}
	}

	UNIT_TEST_COMPLETE()
}

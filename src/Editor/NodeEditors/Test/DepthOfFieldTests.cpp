#include "DepthOfFieldTests.h"
#include "DepthOfFieldReference.h"
#include "unit_test.h"

#include "Nodes/DepthOfFieldNode.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace DepthOfFieldTestsInternal
{
using Image= DepthOfFieldReference::Image;

// A 1000 pixel focal length, a 2mm aperture and a 25cm focus: one pixel of blur
// radius per diopter away from the focus
const float k_zNear= 0.1f;
const float k_zFar= 10.f;
const float k_focusDistance= 0.25f;
const glm::vec4 k_lensParams=
	DepthOfFieldReference::computeLensParams(k_zNear, k_zFar, 1000.f, 0.002f, k_focusDistance);
const int k_maxRadius= 16;

// 2m is 3.5 pixels of blur radius at the lens above
const float k_blurryEyeDepth= 2.f;

float toLinearDepth(float eyeDepth) { return (eyeDepth - k_zNear) / (k_zFar - k_zNear); }

const int k_width= 64;
const int k_height= 32;
const int k_edgeX= 32;

// Opaque from the edge column onwards at the given eye depth, nothing drawn before it
void makeHalfPlane(float eyeDepth, float r, float g, float b, int alphaMode, Image& outColor, Image& outDepth)
{
	outColor= Image(k_width, k_height, 4);
	outDepth= Image(k_width, k_height, 1, 1.f);
	for (int y= 0; y < k_height; ++y)
	{
		for (int x= k_edgeX; x < k_width; ++x)
		{
			const size_t pixel= (size_t)y * k_width + x;
			outColor.values[pixel * 4]= r;
			outColor.values[pixel * 4 + 1]= g;
			outColor.values[pixel * 4 + 2]= b;
			outColor.values[pixel * 4 + 3]= 1.f;
			outDepth.values[pixel]= toLinearDepth(eyeDepth);
		}
	}

	// The empty region has zero coverage in either convention
	if (alphaMode == 2)
	{
		for (size_t pixel= 0; pixel < (size_t)k_width * k_height; ++pixel)
		{
			outColor.values[pixel * 4 + 3]= 1.f - outColor.values[pixel * 4 + 3];
		}
	}
}

const float* texelAt(const Image& image, int x, int y) { return &image.values[((size_t)y * image.width + x) * 4]; }
} // namespace DepthOfFieldTestsInternal

bool run_depth_of_field_tests()
{
	UNIT_TEST_MODULE_BEGIN("depth_of_field")
	UNIT_TEST_MODULE_CALL_TEST(depth_of_field_test_config_round_trip);
	UNIT_TEST_MODULE_CALL_TEST(depth_of_field_test_circle_of_confusion);
	UNIT_TEST_MODULE_CALL_TEST(depth_of_field_test_in_focus_passes_through);
	UNIT_TEST_MODULE_CALL_TEST(depth_of_field_test_blurry_edge_fades_out);
	UNIT_TEST_MODULE_CALL_TEST(depth_of_field_test_background_does_not_bleed_forward);
	UNIT_TEST_MODULE_CALL_TEST(depth_of_field_test_sharp_detail_survives_beside_blur);
	UNIT_TEST_MODULE_CALL_TEST(depth_of_field_test_inverted_alpha_matches_straight);
	UNIT_TEST_MODULE_END()
}

bool depth_of_field_test_config_round_trip()
{
	UNIT_TEST_BEGIN("the node config round-trips its settings and clamps the radius")

	DepthOfFieldNodeConfig config("dof");
	config.maxRadius= 24;
	config.alphaMode= eColorTextureResolveAlphaMode::inverted;

	configuru::Config pt= config.writeToJSON();
	success&= pt.has_key("max_radius") && pt.has_key("alpha_mode");

	DepthOfFieldNodeConfig restored;
	restored.readFromJSON(pt);
	success&= restored.maxRadius == 24 && restored.alphaMode == eColorTextureResolveAlphaMode::inverted;

	// A file written before the keys existed reads the defaults. The base node
	// keys stay, since the base reader requires its pin arrays.
	configuru::Config bare= config.writeToJSON();
	bare.erase("max_radius");
	bare.erase("alpha_mode");
	DepthOfFieldNodeConfig defaults;
	defaults.readFromJSON(bare);
	success&= defaults.maxRadius == 16 && defaults.alphaMode == eColorTextureResolveAlphaMode::straight;

	// The radius clamps to the shaders' bound on load
	configuru::Config tooLarge= config.writeToJSON();
	tooLarge["max_radius"]= (int64_t)100;
	DepthOfFieldNodeConfig largeConfig;
	largeConfig.readFromJSON(tooLarge);
	success&= largeConfig.maxRadius == DepthOfFieldNode::k_maxRadius;

	configuru::Config tooSmall= config.writeToJSON();
	tooSmall["max_radius"]= (int64_t)0;
	DepthOfFieldNodeConfig smallConfig;
	smallConfig.readFromJSON(tooSmall);
	success&= smallConfig.maxRadius == DepthOfFieldNode::k_minRadius;

	UNIT_TEST_COMPLETE()
}

bool depth_of_field_test_circle_of_confusion()
{
	UNIT_TEST_BEGIN("the blur radius follows the thin lens and is zero in focus and where nothing was drawn")

	namespace Internal= DepthOfFieldTestsInternal;
	const glm::vec4& lens= Internal::k_lensParams;
	const float maxRadius= (float)Internal::k_maxRadius;

	// Half of focal length * aperture * |1/focus - 1/depth|
	const float inFocus= DepthOfFieldReference::cocRadius(Internal::k_focusDistance, lens, maxRadius);
	const float behind= DepthOfFieldReference::cocRadius(0.5f, lens, maxRadius);
	const float farRadius= DepthOfFieldReference::cocRadius(Internal::k_blurryEyeDepth, lens, maxRadius);
	const float nearRadius= DepthOfFieldReference::cocRadius(0.2f, lens, maxRadius);
	const float empty= DepthOfFieldReference::cocRadius(-1.f, lens, maxRadius);
	const float clamped= DepthOfFieldReference::cocRadius(0.02f, lens, maxRadius);

	success&= std::abs(inFocus) < 1e-5f;
	success&= std::abs(behind - 2.f) < 1e-4f;
	success&= std::abs(farRadius - 3.5f) < 1e-4f;
	success&= std::abs(nearRadius - 1.f) < 1e-4f;
	success&= empty == 0.f;
	success&= clamped == maxRadius;

	// The linear depth round-trip lands on the same eye depth, and an empty pixel reads as empty
	DepthOfFieldReference::Image depth(2, 1, 1);
	depth.values[0]= Internal::toLinearDepth(0.5f);
	depth.values[1]= 1.f;
	success&= std::abs(DepthOfFieldReference::eyeDepthAt(depth, 0.25f, 0.5f, lens) - 0.5f) < 1e-4f;
	success&= DepthOfFieldReference::eyeDepthAt(depth, 0.75f, 0.5f, lens) < 0.f;

	if (!success)
	{
		fprintf(stdout, "    radii %f %f %f %f %f %f\n", inFocus, behind, farRadius, nearRadius, empty, clamped);
	}

	UNIT_TEST_COMPLETE()
}

bool depth_of_field_test_in_focus_passes_through()
{
	UNIT_TEST_BEGIN("a layer at the focus distance comes out unchanged")

	namespace Internal= DepthOfFieldTestsInternal;
	Internal::Image color, depth;
	Internal::makeHalfPlane(Internal::k_focusDistance, 0.8f, 0.4f, 0.2f, 1, color, depth);

	const Internal::Image result=
		DepthOfFieldReference::applyDepthOfField(color, depth, Internal::k_lensParams, Internal::k_maxRadius, 1);
	for (size_t index= 0; index < color.values.size() && success; ++index)
	{
		if (std::abs(result.values[index] - color.values[index]) > 1e-6f)
		{
			fprintf(stdout, "    value %zu: %f, expected %f\n", index, result.values[index], color.values[index]);
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

bool depth_of_field_test_blurry_edge_fades_out()
{
	UNIT_TEST_BEGIN("a blurry silhouette fades out over the empty region with no dark fringe")

	namespace Internal= DepthOfFieldTestsInternal;
	Internal::Image color, depth;
	Internal::makeHalfPlane(Internal::k_blurryEyeDepth, 1.f, 0.5f, 0.f, 1, color, depth);

	const Internal::Image result=
		DepthOfFieldReference::applyDepthOfField(color, depth, Internal::k_lensParams, Internal::k_maxRadius, 1);

	// A blur radius of 3.5 pixels: the coverage ramps across the edge, from empty well
	// outside to opaque well inside, and never rises away from the surface
	const int row= Internal::k_height / 2;
	const float outside= Internal::texelAt(result, Internal::k_edgeX - 8, row)[3];
	const float inside= Internal::texelAt(result, Internal::k_edgeX + 8, row)[3];
	const float justOutside= Internal::texelAt(result, Internal::k_edgeX - 2, row)[3];
	const float justInside= Internal::texelAt(result, Internal::k_edgeX + 1, row)[3];

	success&= outside < 1e-4f;
	success&= std::abs(inside - 1.f) < 1e-3f;
	success&= justOutside > 0.02f && justOutside < 0.5f;
	success&= justInside > 0.5f && justInside < 0.99f;

	bool bMonotonic= true;
	for (int x= Internal::k_edgeX - 8; x < Internal::k_edgeX + 8; ++x)
	{
		const float coverage= Internal::texelAt(result, x, row)[3];
		const float nextCoverage= Internal::texelAt(result, x + 1, row)[3];
		bMonotonic&= nextCoverage >= coverage - 0.02f;

		// The color stays the surface's wherever it shows, since it is averaged premultiplied
		const float* texel= Internal::texelAt(result, x, row);
		if (coverage > 0.01f && (std::abs(texel[0] - 1.f) > 0.02f || std::abs(texel[1] - 0.5f) > 0.02f))
		{
			fprintf(stdout, "    fringe at x %d: (%f, %f, %f) coverage %f\n", x, texel[0], texel[1], texel[2],
					coverage);
			success= false;
		}
	}
	success&= bMonotonic;

	if (!success)
	{
		fprintf(stdout, "    coverage outside %f just outside %f just inside %f inside %f\n", outside, justOutside,
				justInside, inside);
	}

	UNIT_TEST_COMPLETE()
}

bool depth_of_field_test_background_does_not_bleed_forward()
{
	UNIT_TEST_BEGIN("a blurry surface behind does not bleed over a sharp one in front")

	namespace Internal= DepthOfFieldTestsInternal;
	using Image= DepthOfFieldReference::Image;

	// A blurry green surface on the left, a sharp red one at the focus on the right
	Image color(Internal::k_width, Internal::k_height, 4);
	Image depth(Internal::k_width, Internal::k_height, 1);
	for (int y= 0; y < Internal::k_height; ++y)
	{
		for (int x= 0; x < Internal::k_width; ++x)
		{
			const size_t pixel= (size_t)y * Internal::k_width + x;
			const bool bSharp= x >= Internal::k_edgeX;
			color.values[pixel * 4]= bSharp ? 1.f : 0.f;
			color.values[pixel * 4 + 1]= bSharp ? 0.f : 1.f;
			color.values[pixel * 4 + 2]= 0.f;
			color.values[pixel * 4 + 3]= 1.f;
			depth.values[pixel]=
				Internal::toLinearDepth(bSharp ? Internal::k_focusDistance : Internal::k_blurryEyeDepth);
		}
	}

	const Image result=
		DepthOfFieldReference::applyDepthOfField(color, depth, Internal::k_lensParams, Internal::k_maxRadius, 1);

	const int row= Internal::k_height / 2;
	for (int x= Internal::k_edgeX + 1; x < Internal::k_edgeX + 6; ++x)
	{
		const float* texel= Internal::texelAt(result, x, row);
		if (texel[1] > 0.02f || std::abs(texel[0] - 1.f) > 0.02f || std::abs(texel[3] - 1.f) > 1e-3f)
		{
			fprintf(stdout, "    sharp side x %d: (%f, %f, %f, %f)\n", x, texel[0], texel[1], texel[2], texel[3]);
			success= false;
		}
	}

	// Nor does the sharp surface spread over the blurry one beside it: in focus, it
	// covers only its own pixels
	const float* blurrySide= Internal::texelAt(result, Internal::k_edgeX - 1, row);
	if (blurrySide[0] > 0.02f || std::abs(blurrySide[1] - 1.f) > 0.02f)
	{
		fprintf(stdout, "    blurry side: (%f, %f, %f, %f)\n", blurrySide[0], blurrySide[1], blurrySide[2],
				blurrySide[3]);
		success= false;
	}

	UNIT_TEST_COMPLETE()
}

bool depth_of_field_test_sharp_detail_survives_beside_blur()
{
	UNIT_TEST_BEGIN("pixel detail at the focus distance stays exact beside a blurry surface")

	namespace Internal= DepthOfFieldTestsInternal;
	using Image= DepthOfFieldReference::Image;

	// A blurry surface on the left, and on the right a one pixel checkerboard at the focus,
	// whose every pixel the blur's reach covers, so the gather runs there rather than the
	// pass-through. A source in focus covers only its own pixel, so the checks must not mix.
	Image color(Internal::k_width, Internal::k_height, 4);
	Image depth(Internal::k_width, Internal::k_height, 1);
	for (int y= 0; y < Internal::k_height; ++y)
	{
		for (int x= 0; x < Internal::k_width; ++x)
		{
			const size_t pixel= (size_t)y * Internal::k_width + x;
			const bool bSharp= x >= Internal::k_edgeX;
			const float grey= bSharp ? (((x + y) & 1) ? 1.f : 0.f) : 0.5f;
			color.values[pixel * 4]= grey;
			color.values[pixel * 4 + 1]= grey;
			color.values[pixel * 4 + 2]= grey;
			color.values[pixel * 4 + 3]= 1.f;
			depth.values[pixel]=
				Internal::toLinearDepth(bSharp ? Internal::k_focusDistance : Internal::k_blurryEyeDepth);
		}
	}

	const Image result=
		DepthOfFieldReference::applyDepthOfField(color, depth, Internal::k_lensParams, Internal::k_maxRadius, 1);

	const int row= Internal::k_height / 2;
	for (int x= Internal::k_edgeX; x < Internal::k_edgeX + 8; ++x)
	{
		const float* in= Internal::texelAt(color, x, row);
		const float* out= Internal::texelAt(result, x, row);
		if (std::abs(out[0] - in[0]) > 1e-5f || std::abs(out[3] - 1.f) > 1e-5f)
		{
			fprintf(stdout, "    x %d: %f, expected %f (coverage %f)\n", x, out[0], in[0], out[3]);
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

bool depth_of_field_test_inverted_alpha_matches_straight()
{
	UNIT_TEST_BEGIN("inverted alpha blurs the same as straight and is written back inverted")

	namespace Internal= DepthOfFieldTestsInternal;
	Internal::Image straightColor, invertedColor, depth;
	Internal::makeHalfPlane(Internal::k_blurryEyeDepth, 1.f, 0.5f, 0.f, 1, straightColor, depth);
	Internal::makeHalfPlane(Internal::k_blurryEyeDepth, 1.f, 0.5f, 0.f, 2, invertedColor, depth);

	const Internal::Image straight= DepthOfFieldReference::applyDepthOfField(
		straightColor, depth, Internal::k_lensParams, Internal::k_maxRadius, 1);
	const Internal::Image inverted= DepthOfFieldReference::applyDepthOfField(
		invertedColor, depth, Internal::k_lensParams, Internal::k_maxRadius, 2);

	for (size_t pixel= 0; pixel < (size_t)Internal::k_width * Internal::k_height && success; ++pixel)
	{
		const float* s= &straight.values[pixel * 4];
		const float* i= &inverted.values[pixel * 4];
		if (std::abs(s[0] - i[0]) > 1e-5f || std::abs(s[1] - i[1]) > 1e-5f || std::abs(s[3] - (1.f - i[3])) > 1e-5f)
		{
			fprintf(stdout, "    pixel %zu: straight (%f, %f, %f) inverted (%f, %f, %f)\n", pixel, s[0], s[1], s[3],
					i[0], i[1], i[3]);
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

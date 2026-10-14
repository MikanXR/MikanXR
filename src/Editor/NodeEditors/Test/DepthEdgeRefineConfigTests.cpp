#include "DepthEdgeRefineTests.h"
#include "unit_test.h"

#include "Nodes/DepthEdgeRefineNode.h"

#include <cmath>
#include <cstdio>
#include <memory>

// The module's call list lives beside the filter tests; this file carries the
// config test so the node's persistence is pinned where the node is
bool depth_edge_refine_test_config_round_trip()
{
	UNIT_TEST_BEGIN("the node config round-trips its tunables and clamps the band radius")

	DepthEdgeRefineNodeConfig config("refine");
	config.bandRadius= 24;
	config.colorSigma= 0.25f;
	config.depthJump= 0.02f;

	configuru::Config pt= config.writeToJSON();
	success&= pt.has_key("band_radius") && pt.has_key("color_sigma") && pt.has_key("depth_jump");

	DepthEdgeRefineNodeConfig restored;
	restored.readFromJSON(pt);
	success&= restored.bandRadius == 24;
	success&= std::abs(restored.colorSigma - 0.25f) < 1e-6f;
	success&= std::abs(restored.depthJump - 0.02f) < 1e-6f;
	if (!success)
	{
		fprintf(stdout, "    restored %d %.4f %.4f (expected 24 0.25 0.02)", restored.bandRadius, restored.colorSigma,
				restored.depthJump);
		fprintf(stdout, "%s", "\n");
	}

	// A file written before the keys existed reads the defaults. The base node
	// keys stay, since the base reader requires its pin arrays.
	configuru::Config bare= config.writeToJSON();
	bare.erase("band_radius");
	bare.erase("color_sigma");
	bare.erase("depth_jump");
	DepthEdgeRefineNodeConfig defaults;
	defaults.readFromJSON(bare);
	success&= defaults.bandRadius == 16 && std::abs(defaults.colorSigma - 0.2f) < 1e-6f
			  && std::abs(defaults.depthJump - 0.05f) < 1e-6f;

	// The radius clamps to the shader's bound on load
	configuru::Config tooSmall= config.writeToJSON();
	tooSmall["band_radius"]= (int64_t)0;
	DepthEdgeRefineNodeConfig smallConfig;
	smallConfig.readFromJSON(tooSmall);
	success&= smallConfig.bandRadius == DepthEdgeRefineNode::k_minBandRadius;

	configuru::Config tooLarge= config.writeToJSON();
	tooLarge["band_radius"]= (int64_t)99;
	DepthEdgeRefineNodeConfig largeConfig;
	largeConfig.readFromJSON(tooLarge);
	success&= largeConfig.bandRadius == DepthEdgeRefineNode::k_maxBandRadius;

	UNIT_TEST_COMPLETE()
}

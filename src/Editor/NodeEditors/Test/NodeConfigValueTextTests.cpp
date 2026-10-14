#include "NodeConfigValueTextTests.h"
#include "unit_test.h"

#include "Graphs/NodeConfigValueText.h"

#include <configuru.hpp>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <stdio.h>
#include <string>

namespace NodeConfigValueTextTestsLocal
{
// One key of each scalar type a node config holds, plus editor-owned keys
configuru::Config makeNodeConfig()
{
	configuru::Config nodeConfig= configuru::Config::object();
	nodeConfig["class_name"]= std::string("TestNode");
	nodeConfig["pins_in"]= configuru::Config::array({1, 2});
	nodeConfig["band_radius"]= 8;
	nodeConfig["color_sigma"]= 0.1;
	nodeConfig["enabled"]= true;
	nodeConfig["name"]= std::string("x");

	return nodeConfig;
}

bool applyFails(configuru::Config& nodeConfig, const std::string& key, const std::string& valueText,
				const std::string& expectedError)
{
	std::string error;
	const bool bApplied= NodeConfigValueText::applyToConfig(nodeConfig, key, valueText, error);

	return !bApplied && error == expectedError;
}

bool readsAs(const configuru::Config& nodeConfig, const std::string& key, const std::string& expectedText)
{
	std::string text;
	std::string error;
	const bool bRead= NodeConfigValueText::readFromConfig(nodeConfig, key, text, error);

	return bRead && text == expectedText;
}
} // namespace NodeConfigValueTextTestsLocal

bool run_node_config_value_text_tests()
{
	UNIT_TEST_MODULE_BEGIN("node_config_value_text")
	UNIT_TEST_MODULE_CALL_TEST(node_config_value_text_test_int);
	UNIT_TEST_MODULE_CALL_TEST(node_config_value_text_test_float);
	UNIT_TEST_MODULE_CALL_TEST(node_config_value_text_test_bool);
	UNIT_TEST_MODULE_CALL_TEST(node_config_value_text_test_string);
	UNIT_TEST_MODULE_CALL_TEST(node_config_value_text_test_refused_keys);
	UNIT_TEST_MODULE_CALL_TEST(node_config_value_text_test_read);
	UNIT_TEST_MODULE_END()
}

bool node_config_value_text_test_int()
{
	UNIT_TEST_BEGIN("an int key takes a whole integer token only")

	configuru::Config nodeConfig= NodeConfigValueTextTestsLocal::makeNodeConfig();
	std::string error;

	success&= NodeConfigValueText::applyToConfig(nodeConfig, "band_radius", "16", error);
	success&= nodeConfig["band_radius"].is_int();
	success&= nodeConfig["band_radius"].as_integer<int64_t>() == 16;

	success&= NodeConfigValueTextTestsLocal::applyFails(nodeConfig, "band_radius", "16.5",
														"expected int for 'band_radius', got '16.5'");
	success&= NodeConfigValueTextTestsLocal::applyFails(nodeConfig, "band_radius", "abc",
														"expected int for 'band_radius', got 'abc'");

	// A refused value leaves the stored one alone
	success&= nodeConfig["band_radius"].as_integer<int64_t>() == 16;

	UNIT_TEST_COMPLETE()
}

bool node_config_value_text_test_float()
{
	UNIT_TEST_BEGIN("a float key takes any number and stays a float")

	configuru::Config nodeConfig= NodeConfigValueTextTestsLocal::makeNodeConfig();
	std::string error;

	success&= NodeConfigValueText::applyToConfig(nodeConfig, "color_sigma", "0.25", error);
	success&= nodeConfig["color_sigma"].is_float();
	success&= nodeConfig["color_sigma"].as_double() == 0.25;

	success&= NodeConfigValueText::applyToConfig(nodeConfig, "color_sigma", "1", error);
	success&= nodeConfig["color_sigma"].is_float();
	success&= nodeConfig["color_sigma"].as_double() == 1.0;

	success&= NodeConfigValueTextTestsLocal::applyFails(nodeConfig, "color_sigma", "wide",
														"expected float for 'color_sigma', got 'wide'");

	UNIT_TEST_COMPLETE()
}

bool node_config_value_text_test_bool()
{
	UNIT_TEST_BEGIN("a bool key takes true/false/1/0")

	configuru::Config nodeConfig= NodeConfigValueTextTestsLocal::makeNodeConfig();
	std::string error;

	success&= NodeConfigValueText::applyToConfig(nodeConfig, "enabled", "false", error);
	success&= nodeConfig["enabled"].is_bool();
	success&= !nodeConfig["enabled"].as_bool();

	success&= NodeConfigValueText::applyToConfig(nodeConfig, "enabled", "1", error);
	success&= nodeConfig["enabled"].as_bool();

	success&= NodeConfigValueTextTestsLocal::applyFails(nodeConfig, "enabled", "yes",
														"expected bool for 'enabled', got 'yes'");

	UNIT_TEST_COMPLETE()
}

bool node_config_value_text_test_string()
{
	UNIT_TEST_BEGIN("a string key takes the text verbatim")

	configuru::Config nodeConfig= NodeConfigValueTextTestsLocal::makeNodeConfig();
	std::string error;

	success&= NodeConfigValueText::applyToConfig(nodeConfig, "name", "hello world", error);
	success&= nodeConfig["name"].is_string();
	success&= nodeConfig["name"].as_string() == "hello world";

	UNIT_TEST_COMPLETE()
}

bool node_config_value_text_test_refused_keys()
{
	UNIT_TEST_BEGIN("missing and editor-owned keys are refused")

	configuru::Config nodeConfig= NodeConfigValueTextTestsLocal::makeNodeConfig();

	success&= NodeConfigValueTextTestsLocal::applyFails(nodeConfig, "missing_key", "1", "no config key 'missing_key'");
	success&= NodeConfigValueTextTestsLocal::applyFails(nodeConfig, "class_name", "OtherNode",
														"key 'class_name' belongs to the editor");
	success&=
		NodeConfigValueTextTestsLocal::applyFails(nodeConfig, "pins_in", "3", "key 'pins_in' belongs to the editor");
	success&= nodeConfig["class_name"].as_string() == "TestNode";

	UNIT_TEST_COMPLETE()
}

bool node_config_value_text_test_read()
{
	UNIT_TEST_BEGIN("each scalar reads back as text, containers and missing keys fail")

	configuru::Config nodeConfig= NodeConfigValueTextTestsLocal::makeNodeConfig();

	success&= NodeConfigValueTextTestsLocal::readsAs(nodeConfig, "band_radius", "8");
	success&= NodeConfigValueTextTestsLocal::readsAs(nodeConfig, "enabled", "true");
	success&= NodeConfigValueTextTestsLocal::readsAs(nodeConfig, "name", "x");
	success&= NodeConfigValueTextTestsLocal::readsAs(nodeConfig, "class_name", "TestNode");

	// The float text parses back to the stored value
	{
		std::string text;
		std::string error;
		success&= NodeConfigValueText::readFromConfig(nodeConfig, "color_sigma", text, error);
		success&= std::fabs(std::strtod(text.c_str(), nullptr) - 0.1) < 1e-9;
	}

	// Text read out applies back unchanged
	{
		std::string text;
		std::string error;
		success&= NodeConfigValueText::applyToConfig(nodeConfig, "color_sigma", "0.25", error);
		success&= NodeConfigValueText::readFromConfig(nodeConfig, "color_sigma", text, error);
		success&= text == "0.25";
		success&= NodeConfigValueText::applyToConfig(nodeConfig, "color_sigma", text, error);
		success&= nodeConfig["color_sigma"].as_double() == 0.25;
	}

	{
		std::string text;
		std::string error;
		success&= !NodeConfigValueText::readFromConfig(nodeConfig, "pins_in", text, error);
		success&= error == "unsupported value type for 'pins_in'";
		success&= !NodeConfigValueText::readFromConfig(nodeConfig, "missing_key", text, error);
		success&= error == "no config key 'missing_key'";
	}

	UNIT_TEST_COMPLETE()
}

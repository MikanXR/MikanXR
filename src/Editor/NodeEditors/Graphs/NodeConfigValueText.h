#pragma once

#include <configuru.hpp>

#include <string>

// Writes a text value into one key of a node's config JSON for the automation
// channel, coercing by the type the key already holds (bool, int, float,
// string). Refuses a missing key, a container value, and the keys that belong
// to the editor rather than the node (class_name, id, pins_in, pins_out, pos,
// page). Returns false with outError naming what was expected.
namespace NodeConfigValueText
{
bool applyToConfig(configuru::Config& nodeConfig, const std::string& key, const std::string& valueText,
				   std::string& outError);

// The key's current value as the text a reply carries (bool as true/false,
// numbers as printed, strings raw). Returns false for a missing or
// container-valued key.
bool readFromConfig(const configuru::Config& nodeConfig, const std::string& key, std::string& outText,
					std::string& outError);
} // namespace NodeConfigValueText

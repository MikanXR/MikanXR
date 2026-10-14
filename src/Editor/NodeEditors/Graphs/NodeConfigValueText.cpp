#include "NodeConfigValueText.h"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace NodeConfigValueTextLocal
{
// Keys written by NodeConfig itself: the editor owns them, so an edit through
// the node's config would desync pins, links, or canvas placement
const char* const k_editorOwnedKeys[]= {"class_name", "id", "pins_in", "pins_out", "pos", "page"};

bool isEditorOwnedKey(const std::string& key)
{
	for (const char* editorKey : k_editorOwnedKeys)
	{
		if (key == editorKey)
		{
			return true;
		}
	}

	return false;
}

bool parseBool(const std::string& text, bool& outValue)
{
	if (text == "true" || text == "1")
	{
		outValue= true;
		return true;
	}
	if (text == "false" || text == "0")
	{
		outValue= false;
		return true;
	}

	return false;
}

// The whole token has to be the number, so "16.5" or "16abc" is not an int
bool parseInt(const std::string& text, int64_t& outValue)
{
	if (text.empty() || std::isspace((unsigned char)text[0]))
	{
		return false;
	}

	char* end= nullptr;
	errno= 0;
	const long long value= std::strtoll(text.c_str(), &end, 10);
	if (errno == ERANGE || end == text.c_str() || *end != '\0')
	{
		return false;
	}

	outValue= (int64_t)value;
	return true;
}

// Non-finite values are refused since the JSON writer cannot store them
bool parseFloat(const std::string& text, double& outValue)
{
	if (text.empty() || std::isspace((unsigned char)text[0]))
	{
		return false;
	}

	char* end= nullptr;
	errno= 0;
	const double value= std::strtod(text.c_str(), &end);
	if (errno == ERANGE || end == text.c_str() || *end != '\0' || !std::isfinite(value))
	{
		return false;
	}

	outValue= value;
	return true;
}

std::string makeExpectedError(const char* typeName, const std::string& key, const std::string& valueText)
{
	return std::string("expected ") + typeName + " for '" + key + "', got '" + valueText + "'";
}
} // namespace NodeConfigValueTextLocal

namespace NodeConfigValueText
{
bool applyToConfig(configuru::Config& nodeConfig, const std::string& key, const std::string& valueText,
				   std::string& outError)
{
	if (NodeConfigValueTextLocal::isEditorOwnedKey(key))
	{
		outError= "key '" + key + "' belongs to the editor";
		return false;
	}

	if (!nodeConfig.is_object() || !nodeConfig.has_key(key))
	{
		outError= "no config key '" + key + "'";
		return false;
	}

	const configuru::Config& currentValue= nodeConfig[key];
	if (currentValue.is_bool())
	{
		bool value= false;
		if (!NodeConfigValueTextLocal::parseBool(valueText, value))
		{
			outError= NodeConfigValueTextLocal::makeExpectedError("bool", key, valueText);
			return false;
		}

		nodeConfig[key]= value;
	}
	else if (currentValue.is_int())
	{
		int64_t value= 0;
		if (!NodeConfigValueTextLocal::parseInt(valueText, value))
		{
			outError= NodeConfigValueTextLocal::makeExpectedError("int", key, valueText);
			return false;
		}

		nodeConfig[key]= value;
	}
	else if (currentValue.is_float())
	{
		double value= 0.0;
		if (!NodeConfigValueTextLocal::parseFloat(valueText, value))
		{
			outError= NodeConfigValueTextLocal::makeExpectedError("float", key, valueText);
			return false;
		}

		nodeConfig[key]= value;
	}
	else if (currentValue.is_string())
	{
		nodeConfig[key]= valueText;
	}
	else
	{
		outError= "unsupported value type for '" + key + "'";
		return false;
	}

	return true;
}

bool readFromConfig(const configuru::Config& nodeConfig, const std::string& key, std::string& outText,
					std::string& outError)
{
	if (!nodeConfig.is_object() || !nodeConfig.has_key(key))
	{
		outError= "no config key '" + key + "'";
		return false;
	}

	const configuru::Config& value= nodeConfig[key];
	if (value.is_bool())
	{
		outText= value.as_bool() ? "true" : "false";
	}
	else if (value.is_int())
	{
		outText= std::to_string(value.as_integer<int64_t>());
	}
	else if (value.is_float())
	{
		// Nine significant digits so a float field reads back to the same value
		std::ostringstream stream;
		stream << std::setprecision(9) << value.as_double();
		outText= stream.str();
	}
	else if (value.is_string())
	{
		outText= value.as_string();
	}
	else
	{
		outError= "unsupported value type for '" + key + "'";
		return false;
	}

	return true;
}
} // namespace NodeConfigValueText

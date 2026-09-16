#include "ScriptHttpRouteTable.h"
#include "Logger.h"

#include <cctype>
#include <stdexcept>

namespace
{
const char* k_routeKey= "route";
const char* k_scriptComponentIdKey= "script_component_id";
const char* k_functionKey= "function";

configuru::FormatOptions makeSingleLineRouteJsonFormat()
{
	configuru::FormatOptions options= configuru::make_json_options();
	options.indentation= "";
	return options;
}

// Parse one {"route", "script_component_id", "function"} object; false on any malformed piece
bool readRouteFromJSON(const configuru::Config& entryConfig, ScriptHttpRoute& outEntry)
{
	if (!entryConfig.is_object() || !entryConfig.has_key(k_routeKey) || !entryConfig[k_routeKey].is_string()
		|| !entryConfig.has_key(k_scriptComponentIdKey) || !entryConfig[k_scriptComponentIdKey].is_number()
		|| !entryConfig.has_key(k_functionKey) || !entryConfig[k_functionKey].is_string())
	{
		return false;
	}

	const std::string route= entryConfig[k_routeKey].as_string();
	if (!ScriptHttpRouteTable::isValidRoute(route))
		return false;

	outEntry.route= route;
	outEntry.scriptId= entryConfig[k_scriptComponentIdKey].as_integer<int>();
	outEntry.functionName= entryConfig[k_functionKey].as_string();
	return true;
}

configuru::Config writeRouteToJSON(const ScriptHttpRoute& entry)
{
	configuru::Config entryConfig= configuru::Config::object();
	entryConfig[k_routeKey]= entry.route;
	entryConfig[k_scriptComponentIdKey]= static_cast<int>(entry.scriptId);
	entryConfig[k_functionKey]= entry.functionName;
	return entryConfig;
}
} // namespace

// -- ScriptHttpRoute -----
std::string ScriptHttpRoute::getRoutePath() const { return "/trigger/" + route; }

bool ScriptHttpRoute::operator==(const ScriptHttpRoute& other) const
{
	return route == other.route && scriptId == other.scriptId && functionName == other.functionName;
}

// -- ScriptHttpRouteTable -----
bool ScriptHttpRouteTable::isValidRoute(const std::string& route)
{
	if (route.empty() || route.front() == '/' || route.back() == '/')
		return false;

	char previous= '\0';
	for (char c : route)
	{
		if (std::isspace(static_cast<unsigned char>(c)))
			return false;
		if (c == '/' && previous == '/')
			return false;
		previous= c;
	}

	return true;
}

const std::vector<ScriptHttpRoute>& ScriptHttpRouteTable::getRoutes() const { return m_routes; }

size_t ScriptHttpRouteTable::getRouteCount() const { return m_routes.size(); }

int ScriptHttpRouteTable::findRouteIndex(const std::string& route) const
{
	for (size_t i= 0; i < m_routes.size(); ++i)
	{
		if (m_routes[i].route == route)
			return static_cast<int>(i);
	}

	return -1;
}

bool ScriptHttpRouteTable::addRoute(const ScriptHttpRoute& entry)
{
	if (!isValidRoute(entry.route) || findRouteIndex(entry.route) >= 0)
		return false;

	m_routes.push_back(entry);
	return true;
}

bool ScriptHttpRouteTable::setRoute(size_t index, const ScriptHttpRoute& entry)
{
	if (index >= m_routes.size() || !isValidRoute(entry.route))
		return false;

	const int collisionIndex= findRouteIndex(entry.route);
	if (collisionIndex >= 0 && static_cast<size_t>(collisionIndex) != index)
		return false;

	m_routes[index]= entry;
	return true;
}

bool ScriptHttpRouteTable::removeRoute(size_t index)
{
	if (index >= m_routes.size())
		return false;

	m_routes.erase(m_routes.begin() + index);
	return true;
}

void ScriptHttpRouteTable::clear() { m_routes.clear(); }

bool ScriptHttpRouteTable::empty() const { return m_routes.empty(); }

configuru::Config ScriptHttpRouteTable::writeToJSON() const
{
	configuru::Config pt= configuru::Config::array();

	for (const ScriptHttpRoute& entry : m_routes)
		pt.push_back(writeRouteToJSON(entry));

	return pt;
}

void ScriptHttpRouteTable::readFromJSON(const configuru::Config& pt)
{
	m_routes.clear();

	if (!pt.is_array())
		return;

	for (const configuru::Config& entryConfig : pt.as_array())
	{
		ScriptHttpRoute entry;
		if (!readRouteFromJSON(entryConfig, entry))
		{
			MIKAN_LOG_WARNING("ScriptHttpRouteTable::readFromJSON") << "Skipping malformed HTTP route entry";
			continue;
		}

		if (findRouteIndex(entry.route) >= 0)
		{
			MIKAN_LOG_WARNING("ScriptHttpRouteTable::readFromJSON") << "Skipping duplicate HTTP route " << entry.route;
			continue;
		}

		m_routes.push_back(entry);
	}
}

std::string ScriptHttpRouteTable::toJsonString() const
{
	return configuru::dump_string(writeToJSON(), makeSingleLineRouteJsonFormat());
}

bool ScriptHttpRouteTable::fromJsonString(const std::string& jsonText)
{
	try
	{
		configuru::Config pt= configuru::parse_string(jsonText.c_str(), configuru::JSON, "ScriptHttpRouteTable");
		if (!pt.is_array())
			return false;

		readFromJSON(pt);
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

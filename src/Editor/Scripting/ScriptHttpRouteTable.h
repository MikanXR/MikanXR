#pragma once

#include "CommonConfig.h"
#include "MikanCoreTypes.h"
#include "MikanTypeFwd.h"

#include <string>
#include <vector>

// One HTTP trigger route: /trigger/<route> calls HttpTrigger_<functionName>
// on the ScriptComponent with scriptId
struct ScriptHttpRoute
{
	std::string route; // "gift_sub" or "compositor/desk_camera", no leading slash
	MikanScriptID scriptId= INVALID_MIKAN_ID;
	std::string functionName; // "X" for HttpTrigger_X

	std::string getRoutePath() const;
	bool operator==(const ScriptHttpRoute& other) const;
};

// A project-level table of HTTP trigger routes, persisted on the script
// object system's definition and edited in the HTTP Triggers panel.
class ScriptHttpRouteTable
{
public:
	// Non-empty, no whitespace, no leading or trailing slash, no "//"
	static bool isValidRoute(const std::string& route);

	const std::vector<ScriptHttpRoute>& getRoutes() const;
	size_t getRouteCount() const;
	int findRouteIndex(const std::string& route) const; // -1 when absent
	// False on an invalid or duplicate route
	bool addRoute(const ScriptHttpRoute& entry);
	// False when the index is out of range, the route is invalid, or the new
	// route name collides with another entry
	bool setRoute(size_t index, const ScriptHttpRoute& entry);
	bool removeRoute(size_t index);
	void clear();
	bool empty() const;

	// Array of {"route", "script_component_id", "function"} entries.
	// A malformed or duplicate entry is skipped with a warning.
	configuru::Config writeToJSON() const;
	void readFromJSON(const configuru::Config& pt);
	// Single-line JSON of writeToJSON(), the text form the http_routes
	// property carries
	std::string toJsonString() const;
	// False and untouched when the text is not a JSON array
	bool fromJsonString(const std::string& jsonText);

private:
	std::vector<ScriptHttpRoute> m_routes;
};

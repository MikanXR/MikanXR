#pragma once

#include "MikanCoreTypes.h"
#include "MikanTypeFwd.h"

#include <filesystem>
#include <string>

// Where a Lua error surfaced. A load error means the script state is gone,
// the others leave it running.
enum class eScriptErrorKind : int
{
	load,
	trigger,
	httpTrigger,
	message,
	sequence,
	coroutine,
};

// One Lua error, parsed from the message Lua raised. chunkName and line are
// what the message carried ("scripts/foo.lua:12: ..."), resolvedPath is the
// file that chunk name maps to when one could be found.
struct ScriptError
{
	eScriptErrorKind kind= eScriptErrorKind::load;
	MikanScriptID scriptId= INVALID_MIKAN_ID;
	std::string chunkName;
	std::filesystem::path resolvedPath;
	int line= 0;
	std::string message;
	std::string traceback;
	// The trigger, route function, or method that was running
	std::string context;

	inline bool hasLocation() const { return !resolvedPath.empty() && line > 0; }
};

const char* scriptErrorKindToString(eScriptErrorKind kind);

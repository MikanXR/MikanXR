#include "CommonScriptContext.h"
#include "CompositorConstants.h"
#include "LuaDebugServer.h"
#include "MathGLM.h"
#include "LuaMath.h"
#include "Logger.h"
#include "MikanComponent.h"
#include "PathUtils.h"
#include "ScriptVariableTable.h"

#include <algorithm>
#include <assert.h>
#include <fstream>
#include <filesystem>

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

#include "easy/profiler.h"

const std::string CommonScriptContext::k_triggerMethodPrefix= "Trigger_";
const std::string CommonScriptContext::k_httpTriggerMethodPrefix= "HttpTrigger_";
const std::string CommonScriptContext::k_messageHandlerMethodName= "OnMessage";

const char* scriptErrorKindToString(eScriptErrorKind kind)
{
	switch (kind)
	{
	case eScriptErrorKind::load:
		return "load";
	case eScriptErrorKind::trigger:
		return "trigger";
	case eScriptErrorKind::httpTrigger:
		return "http trigger";
	case eScriptErrorKind::message:
		return "message";
	case eScriptErrorKind::sequence:
		return "sequence";
	case eScriptErrorKind::coroutine:
		return "coroutine";
	default:
		return "unknown";
	}
}

namespace
{
// Registry slot holding the context that owns a Lua state. Its address is the
// key, so it only has to be unique.
const char k_scriptContextRegistryKey= 0;

// The field the ComponentRef sentinel table carries
const char* k_componentRefClassField= "__componentRefClass";
// The list of public fields init assigned, in order
const char* k_paramOrderField= "_paramOrder";
const char* k_behaviorClassMarkerField= "__isBehaviorClass";
const char* k_behaviorClassNameField= "__className";
const char* k_baseBehaviorClassName= "ScriptBehavior";
const char* k_tracebackMarker= "\nstack traceback:";
const char* k_shortenedChunkPrefix= "...";

// Map a Lua value to the variant type a script parameter can hold. Lua keeps
// integer and float subtypes apart, so 30 reflects INT and 30.0 FLOAT.
bool luaRefToVariant(const luabridge::LuaRef& ref, MikanVariant& outValue)
{
	if (ref.isBool())
	{
		outValue.setValue(ref.unsafe_cast<bool>());
		return true;
	}
	else if (ref.isNumber())
	{
		lua_State* L= ref.state();
		ref.push(L);
		const bool bIsInteger= lua_isinteger(L, -1) != 0;
		lua_pop(L, 1);

		if (bIsInteger)
			outValue.setValue(ref.unsafe_cast<int>());
		else
			outValue.setValue(ref.unsafe_cast<float>());
		return true;
	}
	else if (ref.isString())
	{
		outValue.setValue(ref.unsafe_cast<std::string>());
		return true;
	}
	else if (ref.isUserdata() && ref.isInstance<LuaVec3f>())
	{
		outValue.setValue(ref.unsafe_cast<LuaVec3f>().toMikanVector3f());
		return true;
	}

	return false;
}

// lua_pcall message handler: append the traceback while the erroring frames
// are still on the stack
int tracebackMessageHandler(lua_State* L)
{
	const char* message= lua_tostring(L, 1);
	luaL_traceback(L, L, message, 1);
	return 1;
}

// The chunk name a script file loads under: the path relative to the project
// folder. The vscode-lrdb extension sends breakpoint paths relative to its
// "sourceRoot", which the project's generated launch config sets to the
// workspace (the project folder), so both sides agree on the same relative
// form. A script outside the project keeps its absolute path.
std::string makeChunkName(const std::filesystem::path& scriptPath)
{
	const std::filesystem::path projectDir= PathUtils::getProjectDirectory();
	std::filesystem::path relPath=
		projectDir.empty() ? std::filesystem::path() : scriptPath.lexically_relative(projectDir);
	const bool isUnderProject=
		!relPath.empty() && relPath.native().substr(0, 2) != L".." && relPath.native().front() != L'/';

	return "@" + (isUnderProject ? relPath.generic_string() : scriptPath.generic_string());
}

// The package.searchers entry that replaces Lua's stock file loader. It finds
// the module through package.path exactly as the stock loader does, but loads
// the chunk under makeChunkName, so a breakpoint set in a required module lands
// the same way it does in a script the editor ran itself.
int projectModuleSearcher(lua_State* L)
{
	const char* moduleName= luaL_checkstring(L, 1);

	// package.searchpath does the module name to file path substitution and,
	// on failure, reports every path it tried
	lua_getglobal(L, "package");
	lua_getfield(L, -1, "searchpath");
	lua_pushvalue(L, 1);
	lua_getfield(L, -3, "path");
	lua_call(L, 2, 2);

	if (lua_isnil(L, -2))
	{
		// Hand back the list of paths tried; Lua folds it into the error
		return 1;
	}

	const std::string filePath= lua_tostring(L, -2);
	lua_pop(L, 2);

	std::ifstream moduleFile(filePath, std::ios::binary);
	if (!moduleFile.is_open())
	{
		lua_pushfstring(L, "\n\tno file '%s'", filePath.c_str());
		return 1;
	}
	std::string moduleContent((std::istreambuf_iterator<char>(moduleFile)), {});

	const std::string chunkName= makeChunkName(filePath);
	if (luaL_loadbuffer(L, moduleContent.c_str(), moduleContent.size(), chunkName.c_str()) != LUA_OK)
	{
		return luaL_error(L, "error loading module '%s' from file '%s':\n\t%s", moduleName, filePath.c_str(),
						  lua_tostring(L, -1));
	}

	// The chunk, then the file path Lua passes it as its second argument
	lua_pushstring(L, filePath.c_str());
	return 2;
}

bool readFileToString(const std::filesystem::path& filePath, std::string& outContent)
{
	std::ifstream file(filePath, std::ios::binary);
	if (!file.is_open())
		return false;

	outContent.assign((std::istreambuf_iterator<char>(file)), {});
	return true;
}

// True for a table created by ScriptBehavior:extend(), the base class excluded
bool isBehaviorClassAt(lua_State* L, int index)
{
	if (!lua_istable(L, index))
		return false;

	const int absIndex= lua_absindex(L, index);
	lua_pushstring(L, k_behaviorClassMarkerField);
	lua_rawget(L, absIndex);
	const bool bIsClass= lua_toboolean(L, -1) != 0;
	lua_pop(L, 1);
	if (!bIsClass)
		return false;

	lua_pushstring(L, k_behaviorClassNameField);
	lua_rawget(L, absIndex);
	const char* className= lua_tostring(L, -1);
	const bool bIsBase= className != nullptr && std::string(className) == k_baseBehaviorClassName;
	lua_pop(L, 1);

	return !bIsBase;
}

bool pathEndsWith(const std::string& path, const std::string& suffix)
{
	return path.size() >= suffix.size() && path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// The first .lua file under directory whose generic path ends with suffix
std::filesystem::path findScriptBySuffix(const std::filesystem::path& directory, const std::string& suffix)
{
	std::error_code ignored;
	if (directory.empty() || !std::filesystem::is_directory(directory, ignored))
		return std::filesystem::path();

	for (const auto& entry : std::filesystem::recursive_directory_iterator(
			 directory, std::filesystem::directory_options::skip_permission_denied, ignored))
	{
		if (!entry.is_regular_file(ignored))
			continue;

		if (pathEndsWith(entry.path().generic_string(), suffix))
			return entry.path();
	}

	return std::filesystem::path();
}
} // namespace

// -- CommonScriptContext -----
CommonScriptContext::CommonScriptContext() {}

CommonScriptContext::~CommonScriptContext() { disposeScriptState(); }

int CommonScriptContext::panicHandler(lua_State* state)
{
	const char* err= lua_tostring(state, 1);
	MIKAN_LOG_ERROR("CommonScriptContext::panicHandler") << err;

	return -1;
}

bool CommonScriptContext::checkLuaResult(int ret, const char* filename, int line)
{
	if (m_luaState == nullptr)
		return false;

	if (ret != 0)
	{
		MIKAN_LOG_ERROR("CommonScriptContext::checkLuaState") << filename << ", Line " << line;

		switch (ret)
		{
		case LUA_ERRFILE:
			MIKAN_LOG_ERROR("CommonScriptContext::checkLuaState") << "Couldn't open the given file";
			break;
		case LUA_ERRSYNTAX:
		{
			luaL_traceback(m_luaState, m_luaState, nullptr, 1);
			const std::string traceback= lua_tostring(m_luaState, -1);

			MIKAN_LOG_ERROR("CommonScriptContext::checkLuaState") << "Syntax error during pre-compilation";
			MIKAN_LOG_ERROR("CommonScriptContext::checkLuaState") << traceback;
		}
		break;
		case LUA_ERRMEM:
			MIKAN_LOG_ERROR("CommonScriptContext::checkLuaState") << "Memory allocation error";
			break;
		case LUA_ERRRUN:
		{
			const std::string errMsg= lua_tostring(m_luaState, -1);
			luaL_traceback(m_luaState, m_luaState, nullptr, 1);
			const std::string traceback= lua_tostring(m_luaState, -1);

			MIKAN_LOG_ERROR("CommonScriptContext::checkLuaState") << errMsg;
			MIKAN_LOG_ERROR("CommonScriptContext::checkLuaState") << traceback;
		}
		break;
		case LUA_ERRERR:
			MIKAN_LOG_ERROR("CommonScriptContext::checkLuaState") << "Error while running the error handler function";
			break;
		default:
			const std::string errMsg= lua_tostring(m_luaState, -1);
			MIKAN_LOG_ERROR("CommonScriptContext::checkLuaState") << errMsg;
			break;
		}

		// Terminate the script state
		disposeScriptState();

		return false;
	}

	return true;
}

bool CommonScriptContext::createScriptState()
{
	disposeScriptState();

	m_luaState= luaL_newstate();
	if (m_luaState == nullptr)
	{
		MIKAN_LOG_ERROR("CommonScriptContext::createScriptState") << "Failed to create new Lua state";
		return false;
	}

	lua_atpanic(m_luaState, panicHandler);
	luaL_openlibs(m_luaState);

	// Bindings reach the context back through the state they are called with
	lua_pushlightuserdata(m_luaState, this);
	lua_rawsetp(m_luaState, LUA_REGISTRYINDEX, &k_scriptContextRegistryKey);

	setupModuleSearchPath();

	if (!bindContextFunctions())
	{
		MIKAN_LOG_ERROR("CommonScriptContext::createScriptState") << "Failed to bind script context functions";
		disposeScriptState();
		return false;
	}

	// One state per project: the debugger attaches here and follows every
	// script that runs in it
	auto* debugServer= LuaDebugServer::getInstance();
	if (debugServer->isListening())
	{
		debugServer->attach(this);
	}

	return true;
}

void CommonScriptContext::setupModuleSearchPath()
{
	lua_State* L= m_luaState;

	// require() searches the project's own scripts folder first, then the bundled
	// scripts behind it, so a project module shadows a bundled one of the same
	// name. Lua's stock entries stay behind ours rather than being replaced, so
	// nothing that resolved before stops resolving.
	std::string searchPath;
	const std::filesystem::path projectDir= PathUtils::getProjectDirectory();
	if (!projectDir.empty())
	{
		// Forward slashes: Windows accepts them, and package.path treats the
		// string literally
		const std::string scriptsDir= (projectDir / "scripts").generic_string();

		searchPath+= scriptsDir + "/?.lua;";
		searchPath+= scriptsDir + "/?/init.lua;";
	}

	const std::string bundledScriptsDir= (PathUtils::getResourceDirectory() / "scripts").generic_string();
	searchPath+= bundledScriptsDir + "/?.lua;";
	searchPath+= bundledScriptsDir + "/?/init.lua;";

	lua_getglobal(L, "package");
	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		MIKAN_LOG_ERROR("CommonScriptContext::setupModuleSearchPath") << "No package table in the Lua state";
		return;
	}

	lua_getfield(L, -1, "path");
	const char* defaultPath= lua_tostring(L, -1);
	if (defaultPath != nullptr)
		searchPath+= defaultPath;
	lua_pop(L, 1);

	lua_pushstring(L, searchPath.c_str());
	lua_setfield(L, -2, "path");

	// Take over the file searcher, leaving package.preload ahead of it and the
	// C loaders behind it
	lua_getfield(L, -1, "searchers");
	if (lua_istable(L, -1))
	{
		lua_pushcfunction(L, projectModuleSearcher);
		lua_seti(L, -2, 2);
	}
	lua_pop(L, 2);
}

bool CommonScriptContext::runScriptFile(const std::filesystem::path& scriptPath, MikanScriptID scriptId)
{
	if (m_luaState == nullptr)
	{
		MIKAN_LOG_ERROR("CommonScriptContext::runScriptFile") << "No script state to run " << scriptPath << " in";
		return false;
	}

	const std::string chunkName= makeChunkName(scriptPath);

	// Read the file ourselves so we can supply the custom chunk name to lua_load.
	std::string scriptContent;
	if (!readFileToString(scriptPath, scriptContent))
	{
		MIKAN_LOG_ERROR("CommonScriptContext::runScriptFile") << "Failed to open lua script " << scriptPath;
		return false;
	}

	m_loadingScriptId= scriptId;
	int ret= luaL_loadbuffer(m_luaState, scriptContent.c_str(), scriptContent.size(), chunkName.c_str());
	if (ret == LUA_OK)
		ret= lua_pcall(m_luaState, 0, LUA_MULTRET, 0);
	m_loadingScriptId= INVALID_MIKAN_ID;

	if (!checkLuaResult(ret, __FILE__, __LINE__))
	{
		MIKAN_LOG_ERROR("CommonScriptContext::runScriptFile") << "Failed to run lua script " << scriptPath;
		return false;
	}

	m_loadedScripts.push_back({scriptId, scriptPath});

	return true;
}

bool CommonScriptContext::isScriptLoaded(MikanScriptID scriptId) const
{
	return std::find_if(m_loadedScripts.begin(), m_loadedScripts.end(),
						[scriptId](const LoadedScript& script) { return script.scriptId == scriptId; })
		   != m_loadedScripts.end();
}

void CommonScriptContext::updateScript(float deltaSeconds)
{
	EASY_FUNCTION();

	if (m_luaState == nullptr)
		return;

	// A coroutine that fails is reported like the trigger that started it,
	// and the scheduler drops it, so the state stays usable
	const int baseTop= lua_gettop(m_luaState);
	lua_getglobal(m_luaState, "update_scheduler");
	if (pcallWithTraceback(0, 0) != LUA_OK)
	{
		reportLuaError(eScriptErrorKind::coroutine, INVALID_MIKAN_ID, "update_scheduler");
	}
	lua_settop(m_luaState, baseTop);
}

bool CommonScriptContext::bindContextFunctions()
{
	if (!addLuaCoroutineScheduler())
	{
		MIKAN_LOG_ERROR("CommonScriptContext::bindContextFunctions")
			<< "Failed to add coroutine scheduler to Lua state";
		return false;
	}

	bindCommonScriptFunctions();
	LuaVec3f::bindFunctions(m_luaState);
	LuaQuatf::bindFunctions(m_luaState);

	// The base class calls back into the class notifier bound above
	if (!addScriptBehaviorBase())
	{
		MIKAN_LOG_ERROR("CommonScriptContext::bindContextFunctions") << "Failed to add ScriptBehavior to Lua state";
		return false;
	}

	return true;
}

// -- Error reporting -----
int CommonScriptContext::pcallWithTraceback(int argCount, int resultCount)
{
	lua_State* L= m_luaState;
	const int functionIndex= lua_gettop(L) - argCount;

	lua_pushcfunction(L, tracebackMessageHandler);
	lua_insert(L, functionIndex);

	const int ret= lua_pcall(L, argCount, resultCount, functionIndex);

	// The handler sits under the results or under the error message
	lua_remove(L, functionIndex);
	return ret;
}

bool CommonScriptContext::parseLuaErrorLocation(const std::string& luaMessage, std::string& outChunkName, int& outLine,
												std::string& outMessage)
{
	// The location prefix is "<chunk>:<line>: ". Scan for the first colon that
	// is followed by digits and another colon, so "D:/x.lua:12: msg" splits
	// after the 12 rather than after the drive letter.
	size_t colonPos= luaMessage.find(':');
	while (colonPos != std::string::npos)
	{
		size_t digitEnd= colonPos + 1;
		while (digitEnd < luaMessage.size() && std::isdigit(static_cast<unsigned char>(luaMessage[digitEnd])))
			++digitEnd;

		if (digitEnd > colonPos + 1 && digitEnd < luaMessage.size() && luaMessage[digitEnd] == ':')
		{
			outChunkName= luaMessage.substr(0, colonPos);
			outLine= std::atoi(luaMessage.c_str() + colonPos + 1);

			size_t messageStart= digitEnd + 1;
			if (messageStart < luaMessage.size() && luaMessage[messageStart] == ' ')
				++messageStart;
			outMessage= luaMessage.substr(messageStart);
			return true;
		}

		colonPos= luaMessage.find(':', colonPos + 1);
	}

	outChunkName.clear();
	outLine= 0;
	outMessage= luaMessage;
	return false;
}

std::filesystem::path CommonScriptContext::resolveScriptErrorPath(const std::string& chunkName) const
{
	std::string name= chunkName;
	if (!name.empty() && name.front() == '@')
		name.erase(0, 1);
	if (name.empty())
		return std::filesystem::path();

	std::error_code ignored;
	const std::filesystem::path projectDir= PathUtils::getProjectDirectory();
	const std::filesystem::path bundledDir= PathUtils::getResourceDirectory() / "scripts";

	// Lua shortens a long source name to "...<tail>", so match the tail
	if (name.rfind(k_shortenedChunkPrefix, 0) == 0)
	{
		const std::string suffix= name.substr(std::string(k_shortenedChunkPrefix).size());
		if (suffix.empty())
			return std::filesystem::path();

		for (const auto& [pathString, classRef] : m_classRefByPath)
		{
			if (pathEndsWith(pathString, suffix))
				return std::filesystem::path(pathString);
		}
		for (const LoadedScript& script : m_loadedScripts)
		{
			if (pathEndsWith(script.path.generic_string(), suffix))
				return script.path;
		}

		std::filesystem::path found;
		if (!projectDir.empty())
			found= findScriptBySuffix(projectDir / "scripts", suffix);
		if (found.empty())
			found= findScriptBySuffix(bundledDir, suffix);
		return found;
	}

	const std::filesystem::path namedPath(name);
	if (namedPath.is_absolute())
	{
		return std::filesystem::exists(namedPath, ignored) ? namedPath : std::filesystem::path();
	}

	if (!projectDir.empty() && std::filesystem::exists(projectDir / namedPath, ignored))
		return projectDir / namedPath;

	const std::filesystem::path resourcePath= PathUtils::getResourceDirectory() / namedPath;
	if (std::filesystem::exists(resourcePath, ignored))
		return resourcePath;

	return std::filesystem::path();
}

void CommonScriptContext::reportLuaError(eScriptErrorKind kind, MikanScriptID scriptId, const std::string& context)
{
	const char* rawMessage= m_luaState != nullptr ? lua_tostring(m_luaState, -1) : nullptr;
	std::string fullText= rawMessage != nullptr ? rawMessage : "unknown Lua error";

	ScriptError error;
	error.kind= kind;
	error.scriptId= scriptId;
	error.context= context;

	// The traceback handler appends the stack under a fixed header
	const size_t tracebackPos= fullText.find(k_tracebackMarker);
	if (tracebackPos != std::string::npos)
	{
		error.traceback= fullText.substr(tracebackPos + 1);
		fullText.erase(tracebackPos);
	}

	parseLuaErrorLocation(fullText, error.chunkName, error.line, error.message);
	error.resolvedPath= resolveScriptErrorPath(error.chunkName);

	reportScriptError(error);
}

void CommonScriptContext::reportScriptError(const ScriptError& error)
{
	MIKAN_LOG_ERROR("CommonScriptContext::reportScriptError")
		<< "Lua " << scriptErrorKindToString(error.kind) << " error"
		<< (error.context.empty() ? "" : " in " + error.context) << ": "
		<< (error.chunkName.empty() ? "" : error.chunkName + ":" + std::to_string(error.line) + ": ") << error.message;
	if (!error.traceback.empty())
	{
		MIKAN_LOG_ERROR("CommonScriptContext::reportScriptError") << error.traceback;
	}

	if (OnScriptError)
		OnScriptError(error);
}

// -- Behaviors -----
bool CommonScriptContext::loadBehavior(const std::filesystem::path& scriptPath, MikanScriptID scriptId,
									   IScriptVariableStore* variableStore)
{
	if (m_luaState == nullptr)
	{
		MIKAN_LOG_ERROR("CommonScriptContext::loadBehavior") << "No script state to load " << scriptPath << " in";
		return false;
	}

	if (hasBehavior(scriptId))
	{
		MIKAN_LOG_ERROR("CommonScriptContext::loadBehavior") << "Script " << scriptId << " already has a behavior";
		return false;
	}

	// The chunk runs once per state however many components share the file
	const std::string pathKey= scriptPath.generic_string();
	int classRef= k_invalidLuaRef;
	auto classIt= m_classRefByPath.find(pathKey);
	if (classIt != m_classRefByPath.end())
	{
		classRef= classIt->second;
	}
	else if (runBehaviorChunk(scriptPath, scriptId, classRef))
	{
		m_classRefByPath[pathKey]= classRef;
	}
	else
	{
		disposeScriptState();
		return false;
	}

	BehaviorInstance instance;
	instance.scriptId= scriptId;
	instance.scriptPath= scriptPath;
	if (!instantiateBehavior(classRef, instance) || !collectBehaviorMethods(instance)
		|| !reflectParameters(instance, variableStore))
	{
		if (instance.instanceRef != k_invalidLuaRef)
			luaL_unref(m_luaState, LUA_REGISTRYINDEX, instance.instanceRef);
		disposeScriptState();
		return false;
	}

	m_loadedScripts.push_back({scriptId, scriptPath});
	m_behaviors.push_back(instance);

	return true;
}

bool CommonScriptContext::runBehaviorChunk(const std::filesystem::path& scriptPath, MikanScriptID scriptId,
										   int& outClassRef)
{
	lua_State* L= m_luaState;
	const std::string chunkName= makeChunkName(scriptPath);
	outClassRef= k_invalidLuaRef;

	std::string scriptContent;
	if (!readFileToString(scriptPath, scriptContent))
	{
		ScriptError error;
		error.kind= eScriptErrorKind::load;
		error.scriptId= scriptId;
		error.chunkName= chunkName.substr(1);
		error.resolvedPath= scriptPath;
		error.message= "failed to open the script file";
		reportScriptError(error);
		return false;
	}

	const int baseTop= lua_gettop(L);
	if (luaL_loadbuffer(L, scriptContent.c_str(), scriptContent.size(), chunkName.c_str()) != LUA_OK)
	{
		reportLuaError(eScriptErrorKind::load, scriptId, "");
		lua_settop(L, baseTop);
		return false;
	}

	// Classes created while the chunk runs are attributed to this script
	m_loadingScriptId= scriptId;
	m_chunkClassRefs.clear();
	const int ret= pcallWithTraceback(0, 1);
	m_loadingScriptId= INVALID_MIKAN_ID;

	if (ret != LUA_OK)
	{
		reportLuaError(eScriptErrorKind::load, scriptId, "");
		lua_settop(L, baseTop);
		for (int ref : m_chunkClassRefs)
			luaL_unref(L, LUA_REGISTRYINDEX, ref);
		m_chunkClassRefs.clear();
		return false;
	}

	// The returned class wins; otherwise the last one the chunk created
	if (isBehaviorClassAt(L, -1))
	{
		outClassRef= luaL_ref(L, LUA_REGISTRYINDEX);
	}
	else
	{
		lua_pop(L, 1);
		if (!m_chunkClassRefs.empty())
		{
			outClassRef= m_chunkClassRefs.back();
			m_chunkClassRefs.pop_back();
		}
	}
	for (int ref : m_chunkClassRefs)
		luaL_unref(L, LUA_REGISTRYINDEX, ref);
	m_chunkClassRefs.clear();

	if (outClassRef == k_invalidLuaRef)
	{
		ScriptError error;
		error.kind= eScriptErrorKind::load;
		error.scriptId= scriptId;
		error.chunkName= chunkName.substr(1);
		error.resolvedPath= scriptPath;
		error.line= 1;
		error.message= "the script defines no ScriptBehavior class";
		reportScriptError(error);
		lua_settop(L, baseTop);
		return false;
	}

	// A class left with the base name takes the file's name
	lua_rawgeti(L, LUA_REGISTRYINDEX, outClassRef);
	lua_pushstring(L, k_behaviorClassNameField);
	lua_rawget(L, -2);
	const char* className= lua_tostring(L, -1);
	const bool bUnnamed= className == nullptr || std::string(className) == k_baseBehaviorClassName;
	lua_pop(L, 1);
	if (bUnnamed)
	{
		lua_pushstring(L, k_behaviorClassNameField);
		lua_pushstring(L, scriptPath.stem().string().c_str());
		lua_rawset(L, -3);
	}
	lua_settop(L, baseTop);

	return true;
}

bool CommonScriptContext::instantiateBehavior(int classRef, BehaviorInstance& inoutInstance)
{
	lua_State* L= m_luaState;
	const int baseTop= lua_gettop(L);

	lua_rawgeti(L, LUA_REGISTRYINDEX, classRef);
	lua_pushstring(L, k_behaviorClassNameField);
	lua_rawget(L, -2);
	const char* className= lua_tostring(L, -1);
	inoutInstance.className= className != nullptr ? className : "";
	lua_pop(L, 1);

	// Calling the class constructs an instance and runs its init
	if (pcallWithTraceback(0, 1) != LUA_OK)
	{
		reportLuaError(eScriptErrorKind::load, inoutInstance.scriptId, inoutInstance.className + ":init");
		lua_settop(L, baseTop);
		return false;
	}

	if (!lua_istable(L, -1))
	{
		ScriptError error;
		error.kind= eScriptErrorKind::load;
		error.scriptId= inoutInstance.scriptId;
		error.resolvedPath= inoutInstance.scriptPath;
		error.message= "constructing " + inoutInstance.className + " did not produce an instance table";
		reportScriptError(error);
		lua_settop(L, baseTop);
		return false;
	}

	inoutInstance.instanceRef= luaL_ref(L, LUA_REGISTRYINDEX);
	lua_settop(L, baseTop);
	return true;
}

bool CommonScriptContext::collectBehaviorMethods(BehaviorInstance& inoutInstance)
{
	lua_State* L= m_luaState;
	const int baseTop= lua_gettop(L);

	// ScriptBehavior.__collectMethodNames(class) walks the class chain in
	// declaration order
	lua_getglobal(L, k_baseBehaviorClassName);
	lua_getfield(L, -1, "__collectMethodNames");
	lua_rawgeti(L, LUA_REGISTRYINDEX, inoutInstance.instanceRef);
	if (!lua_getmetatable(L, -1))
	{
		lua_settop(L, baseTop);
		return false;
	}
	lua_remove(L, -2);
	if (lua_pcall(L, 1, 1, 0) != LUA_OK)
	{
		MIKAN_LOG_ERROR("CommonScriptContext::collectBehaviorMethods")
			<< "Failed to list methods of " << inoutInstance.className << ": " << lua_tostring(L, -1);
		lua_settop(L, baseTop);
		return false;
	}

	inoutInstance.methodNames.clear();
	inoutInstance.triggerNames.clear();
	inoutInstance.httpTriggerNames.clear();
	inoutInstance.bHasMessageHandler= false;

	const lua_Integer count= luaL_len(L, -1);
	for (lua_Integer i= 1; i <= count; ++i)
	{
		lua_rawgeti(L, -1, i);
		const char* name= lua_tostring(L, -1);
		if (name != nullptr)
		{
			const std::string methodName= name;
			inoutInstance.methodNames.push_back(methodName);

			if (methodName.rfind(k_httpTriggerMethodPrefix, 0) == 0)
				inoutInstance.httpTriggerNames.push_back(methodName.substr(k_httpTriggerMethodPrefix.size()));
			else if (methodName.rfind(k_triggerMethodPrefix, 0) == 0)
				inoutInstance.triggerNames.push_back(methodName.substr(k_triggerMethodPrefix.size()));
			else if (methodName == k_messageHandlerMethodName)
				inoutInstance.bHasMessageHandler= true;
		}
		lua_pop(L, 1);
	}

	lua_settop(L, baseTop);
	return true;
}

bool CommonScriptContext::reflectParameters(BehaviorInstance& inoutInstance, IScriptVariableStore* variableStore)
{
	lua_State* L= m_luaState;
	const int baseTop= lua_gettop(L);

	lua_rawgeti(L, LUA_REGISTRYINDEX, inoutInstance.instanceRef);
	const int instanceIndex= lua_gettop(L);
	lua_getfield(L, instanceIndex, k_paramOrderField);
	if (!lua_istable(L, -1))
	{
		// An instance that skipped the base constructor has no parameters
		lua_settop(L, baseTop);
		return true;
	}
	const int orderIndex= lua_gettop(L);

	inoutInstance.parameterNames.clear();

	const lua_Integer count= luaL_len(L, orderIndex);
	for (lua_Integer i= 1; i <= count; ++i)
	{
		lua_rawgeti(L, orderIndex, i);
		const char* rawName= lua_tostring(L, -1);
		const std::string name= rawName != nullptr ? rawName : "";
		lua_pop(L, 1);
		if (name.empty())
			continue;

		lua_getfield(L, instanceIndex, name.c_str());
		luabridge::LuaRef fieldValue= luabridge::LuaRef::fromStack(L, -1);
		lua_pop(L, 1);

		if (fieldValue.isNil())
			continue;

		// A ComponentRef sentinel names the class the field may hold
		std::string componentClass;
		if (fieldValue.isTable())
		{
			luabridge::LuaRef classField= fieldValue[k_componentRefClassField];
			if (classField.isString())
				componentClass= classField.unsafe_cast<std::string>();
		}

		if (!componentClass.empty())
		{
			if (m_componentPushFunctions.find(componentClass) == m_componentPushFunctions.end())
			{
				ScriptError error;
				error.kind= eScriptErrorKind::load;
				error.scriptId= inoutInstance.scriptId;
				error.resolvedPath= inoutInstance.scriptPath;
				error.context= inoutInstance.className + ":init";
				error.message= "parameter " + name + " names unknown component class " + componentClass;
				reportScriptError(error);
				lua_settop(L, baseTop);
				return false;
			}

			// A stored reference of the same class wins; otherwise none is adopted
			MikanComponentID componentId= INVALID_MIKAN_ID;
			if (variableStore != nullptr)
			{
				MikanComponentID storedId= INVALID_MIKAN_ID;
				if (variableStore->getScriptComponentVariable(name, componentClass, storedId))
					componentId= storedId;
				else
					variableStore->setScriptComponentVariable(name, componentClass, componentId);
			}

			// The instance is not registered until the load completes, so the
			// field is written through the instance itself
			VariableBinding binding{name, inoutInstance.scriptId, MikanVariantType::INT, componentClass, componentId};
			writeComponentField(inoutInstance, binding);
			m_variables.push_back(binding);
			inoutInstance.parameterNames.push_back(name);
			continue;
		}

		MikanVariant defaultValue;
		if (!luaRefToVariant(fieldValue, defaultValue))
		{
			MIKAN_LOG_WARNING("CommonScriptContext::reflectParameters")
				<< inoutInstance.className << " field " << name << " has type " << lua_typename(L, fieldValue.type())
				<< ", which is not a parameter type; prefix it with _ to keep it private";
			continue;
		}

		// A stored value wins over the script's default; a default with no
		// stored value is adopted into the store
		MikanVariant effectiveValue= defaultValue;
		if (variableStore != nullptr)
		{
			MikanVariant storedValue;
			if (variableStore->getScriptVariableOfType(name, defaultValue.value_type, storedValue))
				effectiveValue= storedValue;
			else
				variableStore->setScriptVariable(name, defaultValue);
		}

		if (!writeInstanceField(inoutInstance, name, effectiveValue))
		{
			lua_settop(L, baseTop);
			return false;
		}

		m_variables.push_back({name, inoutInstance.scriptId, defaultValue.value_type, "", INVALID_MIKAN_ID});
		inoutInstance.parameterNames.push_back(name);
	}

	lua_settop(L, baseTop);
	return true;
}

void CommonScriptContext::releaseBehaviorReferences()
{
	if (m_luaState != nullptr)
	{
		for (const BehaviorInstance& instance : m_behaviors)
		{
			if (instance.instanceRef != k_invalidLuaRef)
				luaL_unref(m_luaState, LUA_REGISTRYINDEX, instance.instanceRef);
		}
		for (const auto& [pathString, classRef] : m_classRefByPath)
		{
			luaL_unref(m_luaState, LUA_REGISTRYINDEX, classRef);
		}
		for (int ref : m_chunkClassRefs)
		{
			luaL_unref(m_luaState, LUA_REGISTRYINDEX, ref);
		}
	}

	m_behaviors.clear();
	m_classRefByPath.clear();
	m_chunkClassRefs.clear();
}

const CommonScriptContext::BehaviorInstance* CommonScriptContext::findBehavior(MikanScriptID scriptId) const
{
	auto it= std::find_if(m_behaviors.begin(), m_behaviors.end(),
						  [scriptId](const BehaviorInstance& instance) { return instance.scriptId == scriptId; });
	return it != m_behaviors.end() ? &(*it) : nullptr;
}

bool CommonScriptContext::pushBehaviorInstance(lua_State* L, MikanScriptID scriptId) const
{
	const BehaviorInstance* instance= findBehavior(scriptId);
	if (instance == nullptr)
	{
		lua_pushnil(L);
		return false;
	}

	lua_rawgeti(L, LUA_REGISTRYINDEX, instance->instanceRef);
	return true;
}

bool CommonScriptContext::behaviorHasMethod(MikanScriptID scriptId, const std::string& methodName) const
{
	const BehaviorInstance* instance= findBehavior(scriptId);
	if (instance == nullptr || m_luaState == nullptr)
		return false;

	lua_State* L= m_luaState;
	const int baseTop= lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, instance->instanceRef);
	lua_getfield(L, -1, methodName.c_str());
	const bool bHasMethod= lua_isfunction(L, -1);
	lua_settop(L, baseTop);

	return bHasMethod;
}

void CommonScriptContext::getBehaviorTriggerNames(MikanScriptID scriptId, std::vector<std::string>& outNames) const
{
	if (const BehaviorInstance* instance= findBehavior(scriptId))
	{
		outNames.insert(outNames.end(), instance->triggerNames.begin(), instance->triggerNames.end());
	}
}

void CommonScriptContext::getBehaviorHttpTriggerNames(MikanScriptID scriptId, std::vector<std::string>& outNames) const
{
	if (const BehaviorInstance* instance= findBehavior(scriptId))
	{
		outNames.insert(outNames.end(), instance->httpTriggerNames.begin(), instance->httpTriggerNames.end());
	}
}

void CommonScriptContext::findBehaviorsWithTrigger(const std::string& triggerName,
												   std::vector<MikanScriptID>& outScriptIds) const
{
	for (const BehaviorInstance& instance : m_behaviors)
	{
		if (std::find(instance.triggerNames.begin(), instance.triggerNames.end(), triggerName)
			!= instance.triggerNames.end())
		{
			outScriptIds.push_back(instance.scriptId);
		}
	}
}

bool CommonScriptContext::callBehaviorMethodInternal(const BehaviorInstance& instance, const std::string& methodName,
													 const LuaArgPusher& pushArgs, int resultCount,
													 std::string& outError, eScriptErrorKind errorKind)
{
	lua_State* L= m_luaState;
	const int baseTop= lua_gettop(L);

	lua_rawgeti(L, LUA_REGISTRYINDEX, instance.instanceRef);
	lua_getfield(L, -1, methodName.c_str());
	if (!lua_isfunction(L, -1))
	{
		lua_settop(L, baseTop);
		for (int i= 0; i < resultCount; ++i)
			lua_pushnil(L);
		return true;
	}

	// self goes under the arguments: method, instance, args...
	lua_insert(L, -2);
	const int argCount= (pushArgs ? pushArgs(L) : 0) + 1;

	const int ret= pcallWithTraceback(argCount, resultCount);
	if (ret != LUA_OK)
	{
		const char* message= lua_tostring(L, -1);
		outError= message != nullptr ? message : "unknown Lua error";
		reportLuaError(errorKind, instance.scriptId, instance.className + ":" + methodName);
		lua_settop(L, baseTop);
		return false;
	}

	return true;
}

bool CommonScriptContext::callBehaviorMethod(MikanScriptID scriptId, const std::string& methodName,
											 const LuaArgPusher& pushArgs, std::string& outError,
											 eScriptErrorKind errorKind)
{
	outError.clear();
	if (m_luaState == nullptr)
	{
		outError= "no script state";
		return false;
	}

	const BehaviorInstance* instance= findBehavior(scriptId);
	if (instance == nullptr)
	{
		outError= "script " + std::to_string(scriptId) + " has no behavior instance";
		return false;
	}

	const int baseTop= lua_gettop(m_luaState);
	const bool bSuccess= callBehaviorMethodInternal(*instance, methodName, pushArgs, 0, outError, errorKind);
	lua_settop(m_luaState, baseTop);
	return bSuccess;
}

bool CommonScriptContext::invokeScriptTrigger(const std::string& triggerName,
											  const std::map<std::string, std::string>& args,
											  MikanScriptID targetScriptId)
{
	if (m_luaState == nullptr)
	{
		MIKAN_LOG_ERROR("CommonScriptContext::invokeScriptTrigger")
			<< "No script state to invoke trigger " << triggerName << " in";
		return false;
	}

	std::vector<MikanScriptID> targets;
	if (targetScriptId != INVALID_MIKAN_ID)
	{
		const BehaviorInstance* instance= findBehavior(targetScriptId);
		if (instance != nullptr
			&& std::find(instance->triggerNames.begin(), instance->triggerNames.end(), triggerName)
				   != instance->triggerNames.end())
		{
			targets.push_back(targetScriptId);
		}
	}
	else
	{
		findBehaviorsWithTrigger(triggerName, targets);
	}

	if (targets.empty())
	{
		MIKAN_LOG_ERROR("CommonScriptContext::invokeScriptTrigger") << "No script has trigger " << triggerName;
		return false;
	}

	const LuaArgPusher pushArgs= [&args](lua_State* L)
	{
		lua_createtable(L, 0, static_cast<int>(args.size()));
		for (const auto& [key, value] : args)
		{
			lua_pushlstring(L, value.c_str(), value.size());
			lua_setfield(L, -2, key.c_str());
		}
		return 1;
	};

	bool bSuccess= true;
	for (MikanScriptID scriptId : targets)
	{
		std::string error;
		bSuccess&= callBehaviorMethod(scriptId, k_triggerMethodPrefix + triggerName, pushArgs, error,
									  eScriptErrorKind::trigger);
	}

	return bSuccess;
}

bool CommonScriptContext::invokeScriptHttpTrigger(MikanScriptID scriptId, const std::string& functionName,
												  const std::map<std::string, std::string>& args)
{
	const std::string methodName= k_httpTriggerMethodPrefix + functionName;
	if (!behaviorHasMethod(scriptId, methodName))
	{
		MIKAN_LOG_ERROR("CommonScriptContext::invokeScriptHttpTrigger")
			<< "Script " << scriptId << " has no method " << methodName;
		return false;
	}

	const LuaArgPusher pushArgs= [&args](lua_State* L)
	{
		lua_createtable(L, 0, static_cast<int>(args.size()));
		for (const auto& [key, value] : args)
		{
			lua_pushlstring(L, value.c_str(), value.size());
			lua_setfield(L, -2, key.c_str());
		}
		return 1;
	};

	std::string error;
	return callBehaviorMethod(scriptId, methodName, pushArgs, error, eScriptErrorKind::httpTrigger);
}

bool CommonScriptContext::invokeScriptMessageHandler(const std::string& message)
{
	if (m_luaState == nullptr)
		return false;

	const LuaArgPusher pushArgs= [&message](lua_State* L)
	{
		lua_pushlstring(L, message.c_str(), message.size());
		return 1;
	};

	for (const BehaviorInstance& instance : m_behaviors)
	{
		if (!instance.bHasMessageHandler)
			continue;

		const int baseTop= lua_gettop(m_luaState);
		std::string error;
		const bool bCalled= callBehaviorMethodInternal(instance, k_messageHandlerMethodName, pushArgs, 1, error,
													   eScriptErrorKind::message);
		const bool bHandled= bCalled && lua_toboolean(m_luaState, -1) != 0;
		lua_settop(m_luaState, baseTop);

		if (bHandled)
			return true;
	}

	return false;
}

bool CommonScriptContext::evalString(const std::string& code, std::string& outResult)
{
	outResult.clear();

	if (m_luaState == nullptr)
	{
		outResult= "no script loaded";
		return false;
	}

	const int stackTop= lua_gettop(m_luaState);

	if (luaL_dostring(m_luaState, code.c_str()) != LUA_OK)
	{
		const char* errorMessage= lua_tostring(m_luaState, -1);
		outResult= errorMessage != nullptr ? errorMessage : "unknown lua error";
		lua_settop(m_luaState, stackTop);
		return false;
	}

	// Stringify any values the statement returned
	const int resultCount= lua_gettop(m_luaState) - stackTop;
	for (int i= 0; i < resultCount; ++i)
	{
		const char* valueString= luaL_tolstring(m_luaState, stackTop + 1 + i, nullptr);
		if (!outResult.empty())
			outResult+= " ";
		outResult+= valueString != nullptr ? valueString : "nil";
		lua_pop(m_luaState, 1); // pop luaL_tolstring's string copy
	}

	lua_settop(m_luaState, stackTop);
	return true;
}

// -- Parameters -----
void CommonScriptContext::getVariableNamesForScript(MikanScriptID scriptId, std::vector<std::string>& outNames) const
{
	if (const BehaviorInstance* instance= findBehavior(scriptId))
	{
		outNames.insert(outNames.end(), instance->parameterNames.begin(), instance->parameterNames.end());
	}
}

bool CommonScriptContext::hasVariable(MikanScriptID scriptId, const std::string& name) const
{
	return std::find_if(m_variables.begin(), m_variables.end(), [scriptId, &name](const VariableBinding& variable)
						{ return variable.scriptId == scriptId && variable.name == name; })
		   != m_variables.end();
}

bool CommonScriptContext::setVariableValue(MikanScriptID scriptId, const std::string& name, const MikanVariant& value)
{
	if (m_luaState == nullptr)
		return false;

	auto it= std::find_if(m_variables.begin(), m_variables.end(), [scriptId, &name](const VariableBinding& variable)
						  { return variable.scriptId == scriptId && variable.name == name; });
	if (it == m_variables.end() || it->type != value.value_type)
		return false;

	if (it->isComponentReference())
	{
		it->componentId= value.getIntValue();
		return writeComponentField(*it);
	}

	const BehaviorInstance* instance= findBehavior(scriptId);
	return instance != nullptr && writeInstanceField(*instance, name, value);
}

void CommonScriptContext::refreshComponentVariables(MikanComponentID excludedId)
{
	if (m_luaState == nullptr)
		return;

	for (const VariableBinding& binding : m_variables)
	{
		if (binding.isComponentReference())
		{
			writeComponentField(binding, excludedId);
		}
	}
}

void CommonScriptContext::registerComponentClass(const std::string& className, ComponentPushFunction pushFunction)
{
	m_componentPushFunctions[className]= pushFunction;
}

bool CommonScriptContext::pushComponent(lua_State* L, MikanComponentPtr component) const
{
	if (!component)
	{
		lua_pushnil(L);
		return true;
	}

	auto pushIt= m_componentPushFunctions.find(component->getComponentClassName());
	if (pushIt == m_componentPushFunctions.end())
	{
		MIKAN_LOG_ERROR("CommonScriptContext::pushComponent")
			<< "Component class " << component->getComponentClassName() << " has no Lua push thunk";
		lua_pushnil(L);
		return false;
	}

	// Exactly one value lands on the stack either way, so callers can pop or
	// assign it without checking. A failed push leaves nothing behind.
	const int baseTop= lua_gettop(L);
	const bool bPushed= pushIt->second(L, component);
	if (lua_gettop(L) != baseTop + 1)
	{
		lua_settop(L, baseTop);
		lua_pushnil(L);
		return false;
	}

	return bPushed;
}

CommonScriptContext* CommonScriptContext::getFromLuaState(lua_State* L)
{
	lua_rawgetp(L, LUA_REGISTRYINDEX, &k_scriptContextRegistryKey);
	auto* context= static_cast<CommonScriptContext*>(lua_touserdata(L, -1));
	lua_pop(L, 1);

	return context;
}

MikanComponentPtr CommonScriptContext::resolveComponent(const std::string& componentClass,
														MikanComponentID componentId) const
{
	return nullptr;
}

bool CommonScriptContext::writeComponentField(const VariableBinding& binding, MikanComponentID excludedId)
{
	const BehaviorInstance* instance= findBehavior(binding.scriptId);
	if (instance == nullptr)
		return false;

	return writeComponentField(*instance, binding, excludedId);
}

bool CommonScriptContext::writeComponentField(const BehaviorInstance& instance, const VariableBinding& binding,
											  MikanComponentID excludedId)
{
	if (m_componentPushFunctions.find(binding.componentClass) == m_componentPushFunctions.end())
		return false;

	MikanComponentPtr component;
	if (binding.componentId != INVALID_MIKAN_ID && binding.componentId != excludedId)
	{
		component= resolveComponent(binding.componentClass, binding.componentId);
	}

	// A null component writes nil
	lua_State* L= m_luaState;
	lua_rawgeti(L, LUA_REGISTRYINDEX, instance.instanceRef);
	const bool bPushed= pushComponent(L, component);
	lua_setfield(L, -2, binding.name.c_str());
	lua_pop(L, 1);

	return bPushed;
}

bool CommonScriptContext::pushVariant(lua_State* L, const MikanVariant& value) const
{
	switch (value.value_type)
	{
	case MikanVariantType::BOOL:
		lua_pushboolean(L, value.getBoolValue() ? 1 : 0);
		return true;
	case MikanVariantType::INT:
		lua_pushinteger(L, value.getIntValue());
		return true;
	case MikanVariantType::FLOAT:
		lua_pushnumber(L, value.getFloatValue());
		return true;
	case MikanVariantType::STRING:
	{
		const std::string text= value.getUtf8Value();
		lua_pushlstring(L, text.c_str(), text.size());
		return true;
	}
	case MikanVariantType::VECTOR3F:
		return static_cast<bool>(luabridge::push(L, LuaVec3f(value.getVector3fValue())));
	default:
		return false;
	}
}

bool CommonScriptContext::writeInstanceField(const BehaviorInstance& instance, const std::string& name,
											 const MikanVariant& value)
{
	lua_State* L= m_luaState;
	const int baseTop= lua_gettop(L);

	lua_rawgeti(L, LUA_REGISTRYINDEX, instance.instanceRef);
	if (!pushVariant(L, value))
	{
		MIKAN_LOG_ERROR("CommonScriptContext::writeInstanceField")
			<< "Parameter " << name << " has unsupported type " << mikanVariantTypeToString(value.value_type);
		lua_settop(L, baseTop);
		return false;
	}
	lua_setfield(L, -2, name.c_str());

	lua_settop(L, baseTop);
	return true;
}

// -- Embedded Lua -----
bool CommonScriptContext::addLuaCoroutineScheduler()
{
	// Adapted from: https://stackoverflow.com/a/24969185
	// A coroutine that fails is dropped and its error re-raised once the
	// frame's other coroutines have run, so one broken coroutine neither
	// starves the rest nor fails again on every later frame.
	static const char* x_coroutineScript=
		R""""(
		local function make_coroutine_scheduler()
			local coroutine_container = {}
			return {
				schedule_coroutine = function(frame, coroutine_thread)
					if coroutine_container[frame] == nil then
						coroutine_container[frame] = {}
					end
					table.insert(coroutine_container[frame], coroutine_thread)
				end,
				run = function(frame_number)
					local coroutine_threads = coroutine_container[frame_number]
					if coroutine_threads == nil then
						return
					end
					coroutine_container[frame_number] = nil

					local first_error = nil
					local i = 1
					--recheck length every time, to allow coroutine to resume on the same frame
					while i <= #coroutine_threads do
						local thread = coroutine_threads[i]
						local success, msg = coroutine.resume(thread)
						if not success and first_error == nil then
							first_error = debug.traceback(thread, msg)
						end
						i = i + 1
					end
					if first_error ~= nil then
						error(first_error, 0)
					end
				end
			}
		end

		wait_frames = function(frame_duration)
			scheduler.schedule_coroutine(
				frame_number+math.floor(frame_duration),
				coroutine.running())
			coroutine.yield()
		end

		wait_next_frame = function()
			return wait_frames(1)
		end

		wait_seconds = function(seconds_duration)
			return wait_frames(math.floor(seconds_duration*fps))
		end

		get_frame_delta_seconds = function()
			return 1.0/fps;
		end

		start_coroutine = function(task)
			local coroutine_thread = coroutine.create(task)
			local success, msg = coroutine.resume(coroutine_thread)
			if not success then error(debug.traceback(coroutine_thread, msg), 0) end
		end

		fps = 60
		frame_number = 1
		scheduler = make_coroutine_scheduler()

		function update_scheduler()
			local frame = frame_number
			frame_number = frame_number+1
			scheduler.run(frame)
		end
	)"""";

	int ret= luaL_dostring(m_luaState, x_coroutineScript);
	return checkLuaResult(ret, __FILE__, __LINE__);
}

bool CommonScriptContext::addScriptBehaviorBase()
{
	// The base class every project script derives from. A class is a table
	// with __index = itself; an instance is a table whose metatable is the
	// class. Metatables chain, nothing is copied per instance. A class records
	// the order its methods were declared, and an instance records the order
	// init assigned its public fields, which is the order the editor shows
	// triggers and parameters in.
	static const char* x_behaviorScript=
		R""""(
		local ScriptBehavior = {}
		ScriptBehavior.__index = ScriptBehavior
		ScriptBehavior.__isBehaviorClass = true
		ScriptBehavior.__className = "ScriptBehavior"
		ScriptBehavior.__methodOrder = {}

		-- Override to declare parameters as self.<name> = <default>
		function ScriptBehavior:init() end

		function ScriptBehavior:is(cls)
			local mt = getmetatable(self)
			while mt do
				if mt == cls then return true end
				mt = rawget(mt, "super")
			end
			return false
		end

		function ScriptBehavior:__tostring()
			return getmetatable(self).__className .. " instance"
		end

		-- Assigning a method to a class records its position
		local function classNewIndex(cls, key, value)
			rawset(cls, key, value)
			if type(key) == "string" and type(value) == "function" then
				table.insert(rawget(cls, "__methodOrder"), key)
			end
		end

		-- Installed on an instance only while init runs: every new public
		-- field lands in _paramOrder
		local function initNewIndex(obj, key, value)
			rawset(obj, key, value)
			if type(key) == "string" and key:sub(1, 1) ~= "_" and type(value) ~= "function" then
				table.insert(rawget(obj, "_paramOrder"), key)
			end
		end

		local function construct(cls, ...)
			local obj = { _paramOrder = {} }
			setmetatable(obj, { __index = cls, __newindex = initNewIndex })
			obj:init(...)
			setmetatable(obj, cls)
			return obj
		end

		local function classCall(cls, ...)
			-- ScriptBehavior() is shorthand for ScriptBehavior:extend()
			if cls == ScriptBehavior then return cls:extend(...) end
			return construct(cls, ...)
		end

		local function classToString(cls)
			return rawget(cls, "__className")
		end

		function ScriptBehavior:extend(name)
			local cls = {}
			-- Metamethods are looked up raw on the metatable, so they are the
			-- one thing a subclass copies; everything else resolves through __index
			for key, value in pairs(self) do
				if type(key) == "string" and key:sub(1, 2) == "__" and key ~= "__index"
					and key ~= "__methodOrder" and key ~= "__className" then
					cls[key] = value
				end
			end
			cls.__index = cls
			cls.__isBehaviorClass = true
			cls.__className = name or "ScriptBehavior"
			cls.__methodOrder = {}
			cls.super = self
			setmetatable(cls, { __index = self, __call = classCall,
			                    __newindex = classNewIndex, __tostring = classToString })
			__mikan_behavior_class_created(cls)
			return cls
		end

		-- Every method the class or an ancestor declared (the base excluded),
		-- subclass first, in declaration order
		function ScriptBehavior.__collectMethodNames(cls)
			local names, seen = {}, {}
			while cls ~= nil and cls ~= ScriptBehavior do
				for _, name in ipairs(rawget(cls, "__methodOrder")) do
					if not seen[name] and type(rawget(cls, name)) == "function" then
						seen[name] = true
						table.insert(names, name)
					end
				end
				cls = rawget(cls, "super")
			end
			return names
		end

		-- A parameter default naming a component of the given class. The
		-- editor replaces it with the component handle (or nil) after init.
		function ComponentRef(componentClassName)
			return { __componentRefClass = componentClassName }
		end

		setmetatable(ScriptBehavior, { __call = classCall, __tostring = classToString })
		_G.ScriptBehavior = ScriptBehavior
		package.loaded["ScriptBehavior"] = ScriptBehavior
	)"""";

	int ret= luaL_dostring(m_luaState, x_behaviorScript);
	return checkLuaResult(ret, __FILE__, __LINE__);
}

template <typename t_enum_class>
static void addEnumToLua(luabridge::Namespace& globalNamespace, const std::string& enumName,
						 const std::string* enumStrings)
{
	for (int enumIntValue= 0; enumIntValue < (int)t_enum_class::COUNT; ++enumIntValue)
	{
		const std::string enumString= enumStrings[enumIntValue];

		globalNamespace.addProperty(enumString.c_str(), [enumIntValue]() { return enumIntValue; });
	}
}

void CommonScriptContext::bindCommonScriptFunctions()
{
	auto globalNamespace= luabridge::getGlobalNamespace(m_luaState);
	auto contextNamespace= globalNamespace.beginNamespace("ScriptContext");

	contextNamespace.addFunction("broadcastMessage",
								 [this](const char* message)
								 {
									 if (OnScriptMessage)
										 OnScriptMessage(message);
								 });

	// Register enums
	addEnumToLua<eStencilCullMode>(contextNamespace, "CullMode", k_stencilCullModeStrings);

	contextNamespace.endNamespace();

	// ScriptBehavior:extend() reports each class it creates, so the file's
	// class can be picked once its chunk returns. A class created outside a
	// chunk (inside a trigger, say) is nobody's file class and is ignored.
	// A namespace object hands its stack ownership to the one beginNamespace
	// returns, so the global namespace is fetched afresh here.
	luabridge::getGlobalNamespace(m_luaState)
		.addFunction("__mikan_behavior_class_created",
					 [this](luabridge::LuaRef cls)
					 {
						 if (m_loadingScriptId == INVALID_MIKAN_ID || !cls.isTable())
							 return;

						 cls.push(m_luaState);
						 m_chunkClassRefs.push_back(luaL_ref(m_luaState, LUA_REGISTRYINDEX));
					 });

	// Programmatic breakpoint helper: call lrdb_break() anywhere in a script to
	// force a pause on the next line event, without needing gutter breakpoints.
	luabridge::getGlobalNamespace(m_luaState)
		.addFunction("lrdb_break", []() { LuaDebugServer::getInstance()->pauseOnNextLine(); });
}

void CommonScriptContext::disposeScriptState()
{
	m_loadedScripts.clear();
	m_variables.clear();
	m_componentPushFunctions.clear();

	// Instances and classes are registry references, released while the state
	// is alive
	releaseBehaviorReferences();

	if (m_luaState != nullptr)
	{
		// Detach the debug server before closing the Lua state so it doesn't
		// call lua_sethook on a freed state during its own teardown.
		auto* debugServer= LuaDebugServer::getInstance();
		if (debugServer->getAttachedContext() == this)
			debugServer->detach();

		lua_close(m_luaState);
		m_luaState= nullptr;
	}
}

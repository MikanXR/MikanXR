#pragma once

#include "ComponentFwd.h"
#include "MikanCoreTypes.h"
#include "MikanTypeFwd.h"
#include "MikanVariantTypes.h"
#include "MulticastDelegate.h"
#include "ScriptError.h"
#include "ScriptingFwd.h"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

//-- definitions -----
// One Lua state hosting any number of script files. Each file defines a
// ScriptBehavior class, the chunk runs once per state, and every script
// component instantiates that class, so parameters, triggers, and sequence
// callbacks belong to an instance rather than to the shared globals.
class CommonScriptContext : public std::enable_shared_from_this<CommonScriptContext>
{
public:
	struct LoadedScript
	{
		MikanScriptID scriptId;
		std::filesystem::path path;
	};

	// One parameter of one behavior instance: a public field init assigned.
	// The value is owned by the instance's definition. A component reference
	// has a class, an INT type, and the id the field currently resolves.
	struct VariableBinding
	{
		std::string name;
		MikanScriptID scriptId;
		MikanVariantType type;
		std::string componentClass;
		MikanComponentID componentId= INVALID_MIKAN_ID;

		inline bool isComponentReference() const { return !componentClass.empty(); }
	};

	// Pushes a component onto the stack as its concrete class (nil for null)
	using ComponentPushFunction= std::function<bool(lua_State*, MikanComponentPtr)>;

	// The value of LUA_NOREF, so the header needs no Lua include
	static constexpr int k_invalidLuaRef= -2;

	// One script component's instance of its file's behavior class, held as a
	// registry reference that is released before the state closes
	struct BehaviorInstance
	{
		MikanScriptID scriptId= INVALID_MIKAN_ID;
		std::string className;
		std::filesystem::path scriptPath;
		int instanceRef= k_invalidLuaRef;
		// Every method the class chain declares, declaration order
		std::vector<std::string> methodNames;
		// "X" for each Trigger_X and HttpTrigger_X method
		std::vector<std::string> triggerNames;
		std::vector<std::string> httpTriggerNames;
		// Public fields init assigned, declaration order
		std::vector<std::string> parameterNames;
		bool bHasMessageHandler= false;
	};

	// Pushes a method call's arguments and returns how many it pushed
	using LuaArgPusher= std::function<int(lua_State*)>;

	static const std::string k_triggerMethodPrefix;
	static const std::string k_httpTriggerMethodPrefix;
	static const std::string k_messageHandlerMethodName;

	CommonScriptContext();
	virtual ~CommonScriptContext();

	// Create the Lua state, bind the context functions, and attach the debugger
	bool createScriptState();
	// Run one file's chunk into the live state with no class discovery. The
	// module tests use it; the editor loads scripts through loadBehavior.
	bool runScriptFile(const std::filesystem::path& scriptPath, MikanScriptID scriptId);
	// Run the file's chunk (once per path per state), pick its behavior class,
	// construct this component's instance, and reflect the instance's public
	// fields as parameters resolved against variableStore. Any failure reports
	// a load error and disposes the state.
	bool loadBehavior(const std::filesystem::path& scriptPath, MikanScriptID scriptId,
					  IScriptVariableStore* variableStore= nullptr);
	void disposeScriptState();
	void updateScript(float deltaSeconds);
	inline bool hasLoadedScript() const { return m_luaState != nullptr; }
	inline lua_State* getLuaState() const { return m_luaState; }

	const std::vector<LoadedScript>& getLoadedScripts() const { return m_loadedScripts; }
	bool isScriptLoaded(MikanScriptID scriptId) const;

	const std::vector<BehaviorInstance>& getBehaviors() const { return m_behaviors; }
	const BehaviorInstance* findBehavior(MikanScriptID scriptId) const;
	inline bool hasBehavior(MikanScriptID scriptId) const { return findBehavior(scriptId) != nullptr; }
	// Push the instance table, or nil when the script has none
	bool pushBehaviorInstance(lua_State* L, MikanScriptID scriptId) const;
	// A live lookup on the instance, so a method assigned after init counts too
	bool behaviorHasMethod(MikanScriptID scriptId, const std::string& methodName) const;
	void getBehaviorTriggerNames(MikanScriptID scriptId, std::vector<std::string>& outNames) const;
	void getBehaviorHttpTriggerNames(MikanScriptID scriptId, std::vector<std::string>& outNames) const;
	void findBehaviorsWithTrigger(const std::string& triggerName, std::vector<MikanScriptID>& outScriptIds) const;

	// Call self:Trigger_<triggerName>(args) on the target instance, or on every
	// instance that has the trigger when targetScriptId is INVALID_MIKAN_ID.
	// The args table is always passed, empty when args is. False when no
	// instance has the trigger or any call failed. A Lua error is reported
	// through OnScriptError and leaves the state alive.
	bool invokeScriptTrigger(const std::string& triggerName, const std::map<std::string, std::string>& args= {},
							 MikanScriptID targetScriptId= INVALID_MIKAN_ID);
	// Call self:HttpTrigger_<functionName>(args) on one instance
	bool invokeScriptHttpTrigger(MikanScriptID scriptId, const std::string& functionName,
								 const std::map<std::string, std::string>& args);
	// Offer the message to each instance's OnMessage in load order until one
	// returns true
	bool invokeScriptMessageHandler(const std::string& message);
	// Call self:<methodName>(args...) on one instance. A missing method is not
	// an error. A Lua error fills outError, fires OnScriptError with errorKind,
	// and returns false without disposing the state, so a broken callback
	// stops only its own caller.
	bool callBehaviorMethod(MikanScriptID scriptId, const std::string& methodName, const LuaArgPusher& pushArgs,
							std::string& outError, eScriptErrorKind errorKind= eScriptErrorKind::sequence);

	/// Run a Lua statement in this context's state and stringify what it
	/// returns (or the error message on failure). Used by the automation
	/// server's script eval command.
	bool evalString(const std::string& code, std::string& outResult);

	const std::vector<VariableBinding>& getScriptVariables() const { return m_variables; }
	void getVariableNamesForScript(MikanScriptID scriptId, std::vector<std::string>& outNames) const;
	bool hasVariable(MikanScriptID scriptId, const std::string& name) const;
	// Write one parameter field of one instance. False with no state, an
	// unknown name, or a value of another type than the field's. A component
	// reference takes the INT component id and writes the resolved component.
	bool setVariableValue(MikanScriptID scriptId, const std::string& name, const MikanVariant& value);
	// Re-resolve and re-write every component reference field. An id equal to
	// excludedId writes nil, for an object that is about to be destroyed.
	void refreshComponentVariables(MikanComponentID excludedId= INVALID_MIKAN_ID);
	// Classes ComponentRef accepts, keyed by k_componentClassName
	void registerComponentClass(const std::string& className, ComponentPushFunction pushFunction);
	// Push a component onto the stack as its concrete class, which is what a
	// binding returning a base pointer needs: LuaBridge pushes by static type,
	// so a subclass returned as its base loses the subclass's own bindings. A
	// null component, or one of a class this context does not bind, pushes nil.
	bool pushComponent(lua_State* L, MikanComponentPtr component) const;
	// The context owning a Lua state, for bindings that reach back into it.
	// Null for a state this class did not create.
	static CommonScriptContext* getFromLuaState(lua_State* L);

	// Split "<chunk>:<line>: <msg>" into its parts. False leaves outMessage as
	// the whole text. A Windows drive colon is skipped, since a line number
	// is digits between two colons.
	static bool parseLuaErrorLocation(const std::string& luaMessage, std::string& outChunkName, int& outLine,
									  std::string& outMessage);
	// The file a chunk name in an error message refers to: a project relative
	// name against the project folder then the bundled resources, an absolute
	// name as is, and a name Lua shortened to "...<suffix>" by matching the
	// suffix against the loaded scripts and the two script folders. Empty
	// when nothing matches.
	std::filesystem::path resolveScriptErrorPath(const std::string& chunkName) const;

	MulticastDelegate<void(const std::string& message)> OnScriptMessage;
	MulticastDelegate<void(const ScriptError& error)> OnScriptError;

protected:
	static int panicHandler(lua_State* state);
	bool checkLuaResult(int ret, const char* filename, int line);

	virtual bool bindContextFunctions();
	void bindCommonScriptFunctions();
	bool addLuaCoroutineScheduler();
	bool addScriptBehaviorBase();
	// Point require() at the project's scripts folder and take over the file
	// searcher so required modules load under the same chunk names
	void setupModuleSearchPath();

	// lua_pcall with the traceback handler under the function. The function
	// and its arguments are on the stack; on failure the message with its
	// traceback is at the top.
	int pcallWithTraceback(int argCount, int resultCount);
	// Parse the message at the top of the stack into a ScriptError, log it,
	// and fire OnScriptError. Leaves the stack as it was.
	void reportLuaError(eScriptErrorKind kind, MikanScriptID scriptId, const std::string& context);
	void reportScriptError(const ScriptError& error);

	// Run the chunk and leave a registry reference to its behavior class
	bool runBehaviorChunk(const std::filesystem::path& scriptPath, MikanScriptID scriptId, int& outClassRef);
	bool instantiateBehavior(int classRef, BehaviorInstance& inoutInstance);
	bool collectBehaviorMethods(BehaviorInstance& inoutInstance);
	// Turn each public field init assigned into a VariableBinding whose
	// effective value comes from the store, and write that value back
	bool reflectParameters(BehaviorInstance& inoutInstance, IScriptVariableStore* variableStore);
	bool writeInstanceField(const BehaviorInstance& instance, const std::string& name, const MikanVariant& value);
	// Resolve the binding's id and write the component (or nil) to its field
	bool writeComponentField(const VariableBinding& binding, MikanComponentID excludedId= INVALID_MIKAN_ID);
	bool writeComponentField(const BehaviorInstance& instance, const VariableBinding& binding,
							 MikanComponentID excludedId= INVALID_MIKAN_ID);
	bool pushVariant(lua_State* L, const MikanVariant& value) const;
	// The component a reference field should hold: subclasses with scene
	// access resolve the id and check the class, the base resolves nothing
	virtual MikanComponentPtr resolveComponent(const std::string& componentClass, MikanComponentID componentId) const;
	// Call self:<method>(args...) leaving resultCount results above the caller's
	// stack top on success
	bool callBehaviorMethodInternal(const BehaviorInstance& instance, const std::string& methodName,
									const LuaArgPusher& pushArgs, int resultCount, std::string& outError,
									eScriptErrorKind errorKind);
	void releaseBehaviorReferences();

	std::vector<LoadedScript> m_loadedScripts;
	std::vector<VariableBinding> m_variables;
	std::vector<BehaviorInstance> m_behaviors;
	// Behavior class registry references by generic path string: a chunk runs
	// once per state however many components share its file
	std::map<std::string, int> m_classRefByPath;
	// Classes created while the current chunk runs, so the file's class can be
	// picked once the chunk returns
	std::vector<int> m_chunkClassRefs;
	std::map<std::string, ComponentPushFunction> m_componentPushFunctions;
	// The script whose chunk is executing, so class creation can be attributed
	MikanScriptID m_loadingScriptId= INVALID_MIKAN_ID;
	lua_State* m_luaState= nullptr;
};

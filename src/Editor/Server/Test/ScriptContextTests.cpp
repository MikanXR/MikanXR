#include "ScriptContextTests.h"
#include "unit_test.h"

#include "CommonScriptContext.h"
#include "Light/DMXFixtureComponent.h"
#include "Light/DMXFixtureGroupComponent.h"
#include "Light/RGBPixelGridComponent.h"
#include "MikanComponent.h"
#include "PathUtils.h"
#include "Scene/TransformComponent.h"
#include "Script/ScriptComponent.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

#include <assert.h>
#include <fstream>
#include <filesystem>
#include <memory>
#include <stdio.h>
#include <string>

namespace
{
// A throwaway project folder holding a scripts/ tree, since the module search
// path is built from whatever project is loaded. The previous project directory
// is restored on the way out so the rest of the suite is unaffected.
class ScopedTestProject
{
public:
	ScopedTestProject()
	{
		m_previousProjectDir= PathUtils::getProjectDirectory();

		m_projectDir= std::filesystem::temp_directory_path() / "mikan_script_module_tests";
		std::filesystem::remove_all(m_projectDir);
		std::filesystem::create_directories(getScriptsDir());

		PathUtils::setProjectDirectory(m_projectDir);
	}

	~ScopedTestProject()
	{
		PathUtils::setProjectDirectory(m_previousProjectDir);

		std::error_code ignored;
		std::filesystem::remove_all(m_projectDir, ignored);
	}

	std::filesystem::path getProjectDir() const { return m_projectDir; }
	std::filesystem::path getScriptsDir() const { return m_projectDir / "scripts"; }

	// Write one file under scripts/, creating any folders its relative path names
	std::filesystem::path writeScript(const std::string& relativePath, const std::string& content) const
	{
		const std::filesystem::path filePath= getScriptsDir() / relativePath;
		std::filesystem::create_directories(filePath.parent_path());

		std::ofstream file(filePath, std::ios::binary);
		file << content;

		return filePath;
	}

private:
	std::filesystem::path m_projectDir;
	std::filesystem::path m_previousProjectDir;
};

// Collects every error a context reports
class ScriptErrorListener
{
public:
	void onScriptError(const ScriptError& error) { errors.push_back(error); }

	std::vector<ScriptError> errors;
};

// A context that resolves one component id to one component, standing in for
// the project context's scene lookup
class SingleComponentScriptContext : public CommonScriptContext
{
public:
	MikanComponentID resolvableId= INVALID_MIKAN_ID;
	MikanComponentPtr resolvableComponent;

protected:
	virtual MikanComponentPtr resolveComponent(const std::string& componentClass,
											   MikanComponentID componentId) const override
	{
		if (componentId == resolvableId && resolvableComponent
			&& resolvableComponent->getComponentClassName() == componentClass)
		{
			return resolvableComponent;
		}
		return nullptr;
	}
};

// Expose an instance to evalString as the global test_inst
bool exposeInstance(const std::shared_ptr<CommonScriptContext>& context, MikanScriptID scriptId,
					const char* globalName= "test_inst")
{
	lua_State* L= context->getLuaState();
	const bool bPushed= context->pushBehaviorInstance(L, scriptId);
	lua_setglobal(L, globalName);
	return bPushed;
}

bool evalEquals(const std::shared_ptr<CommonScriptContext>& context, const std::string& code,
				const std::string& expected)
{
	std::string result;
	const bool bOk= context->evalString(code, result);
	if (!bOk || result != expected)
	{
		printf("  eval '%s' gave '%s', expected '%s'\n", code.c_str(), result.c_str(), expected.c_str());
		return false;
	}
	return true;
}

void bindPixelGridClass(const std::shared_ptr<CommonScriptContext>& context)
{
	lua_State* L= context->getLuaState();
	MikanComponent::bindLuaFunctions(L);
	TransformComponent::bindLuaFunctions(L);
	DMXFixtureComponent::bindLuaFunctions(L);
	RGBPixelGridComponent::bindLuaFunctions(L);
	context->registerComponentClass(RGBPixelGridComponent::k_componentClassName,
									[](lua_State* state, MikanComponentPtr component)
									{
										return static_cast<bool>(luabridge::push(
											state, std::dynamic_pointer_cast<RGBPixelGridComponent>(component).get()));
									});
}
} // namespace

bool run_script_context_tests()
{
	UNIT_TEST_MODULE_BEGIN("script_context")
	UNIT_TEST_MODULE_CALL_TEST(script_module_test_resolves_project_scripts);
	UNIT_TEST_MODULE_CALL_TEST(script_module_test_resolves_bundled_scripts);
	UNIT_TEST_MODULE_CALL_TEST(script_module_test_chunk_name_is_project_relative);
	UNIT_TEST_MODULE_CALL_TEST(script_module_test_missing_module_names_scripts_folder);
	UNIT_TEST_MODULE_CALL_TEST(script_context_test_pushes_concrete_component_class);
	UNIT_TEST_MODULE_CALL_TEST(script_behavior_test_extend_and_construct);
	UNIT_TEST_MODULE_CALL_TEST(script_behavior_test_class_from_return_value);
	UNIT_TEST_MODULE_CALL_TEST(script_behavior_test_class_from_last_created);
	UNIT_TEST_MODULE_CALL_TEST(script_behavior_test_no_class_is_load_error);
	UNIT_TEST_MODULE_CALL_TEST(script_behavior_test_init_error_is_load_error);
	UNIT_TEST_MODULE_CALL_TEST(script_behavior_test_parameter_reflection);
	UNIT_TEST_MODULE_CALL_TEST(script_behavior_test_store_resolution);
	UNIT_TEST_MODULE_CALL_TEST(script_behavior_test_shared_file_per_instance);
	UNIT_TEST_MODULE_CALL_TEST(script_behavior_test_trigger_targeting);
	UNIT_TEST_MODULE_CALL_TEST(script_behavior_test_trigger_error_keeps_state);
	UNIT_TEST_MODULE_CALL_TEST(script_behavior_test_coroutine_error_keeps_state);
	UNIT_TEST_MODULE_CALL_TEST(script_behavior_test_message_handler_order);
	UNIT_TEST_MODULE_CALL_TEST(script_behavior_test_http_trigger_names);
	UNIT_TEST_MODULE_CALL_TEST(script_behavior_test_call_behavior_method);
	UNIT_TEST_MODULE_CALL_TEST(script_behavior_test_component_ref_refresh);
	UNIT_TEST_MODULE_CALL_TEST(script_error_test_parse_location);
	UNIT_TEST_MODULE_CALL_TEST(script_error_test_resolves_project_then_bundled);
	UNIT_TEST_MODULE_END()
}

bool script_module_test_resolves_project_scripts()
{
	UNIT_TEST_BEGIN("require finds modules in the project scripts folder")

	ScopedTestProject project;
	project.writeScript("test_color.lua", "return { shade = function() return 42 end }\n");
	project.writeScript("test_pkg/sub.lua", "return { name = 'sub' }\n");
	project.writeScript("test_folder/init.lua", "return { name = 'init' }\n");
	const std::filesystem::path callerPath=
		project.writeScript("test_caller.lua", "local color = require('test_color')\n"
											   "local sub = require('test_pkg.sub')\n"
											   "local folder = require('test_folder')\n"
											   "test_shade = color.shade()\n"
											   "test_sub_name = sub.name\n"
											   "test_folder_name = folder.name\n");

	auto context= std::make_shared<CommonScriptContext>();
	success&= context->createScriptState();
	assert(success);
	success&= context->runScriptFile(callerPath, 1);
	assert(success);

	std::string result;
	success&= context->evalString("return test_shade", result) && result == "42";
	assert(success);

	// A dotted name maps to a subfolder, and a folder name to its init.lua
	success&= context->evalString("return test_sub_name", result) && result == "sub";
	assert(success);
	success&= context->evalString("return test_folder_name", result) && result == "init";
	assert(success);

	// The module ran once and its result is cached under the name it was required by
	success&= context->evalString("return package.loaded['test_color'] ~= nil", result) && result == "true";
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_module_test_resolves_bundled_scripts()
{
	UNIT_TEST_BEGIN("require finds bundled modules behind the project scripts folder")

	// The project has no easing.lua of its own; the bundled resources/scripts copy answers
	ScopedTestProject project;
	const std::filesystem::path callerPath=
		project.writeScript("test_caller.lua", "local easing = require('easing')\n"
											   "test_has_easing = type(easing) == 'table'\n");

	auto context= std::make_shared<CommonScriptContext>();
	success&= context->createScriptState();
	assert(success);
	success&= context->runScriptFile(callerPath, 1);
	assert(success);

	std::string result;
	success&= context->evalString("return test_has_easing", result) && result == "true";
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_module_test_chunk_name_is_project_relative()
{
	UNIT_TEST_BEGIN("a required module loads under its project relative path")

	ScopedTestProject project;
	project.writeScript("test_source.lua", "return { source = debug.getinfo(1, 'S').source }\n");
	const std::filesystem::path callerPath=
		project.writeScript("test_caller.lua", "test_source = require('test_source').source\n");

	auto context= std::make_shared<CommonScriptContext>();
	success&= context->createScriptState();
	assert(success);
	success&= context->runScriptFile(callerPath, 1);
	assert(success);

	// The same form runScriptFile assigns, which is what the debugger's
	// source root makes breakpoint paths relative to
	std::string result;
	success&= context->evalString("return test_source", result) && result == "@scripts/test_source.lua";
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_module_test_missing_module_names_scripts_folder()
{
	UNIT_TEST_BEGIN("a missing module reports the project scripts folder")

	ScopedTestProject project;
	// pcall so the failure stays inside the chunk: an uncaught error would
	// dispose the state before the message could be read back
	const std::filesystem::path callerPath=
		project.writeScript("test_caller.lua", "local ok, err = pcall(require, 'test_absent')\n"
											   "test_ok = ok\n"
											   "test_err = err\n");

	auto context= std::make_shared<CommonScriptContext>();
	success&= context->createScriptState();
	assert(success);
	success&= context->runScriptFile(callerPath, 1);
	assert(success);

	std::string result;
	success&= context->evalString("return test_ok", result) && result == "false";
	assert(success);

	const std::string scriptsDir= project.getScriptsDir().generic_string();
	success&= context->evalString("return test_err", result) && result.find(scriptsDir) != std::string::npos;
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_context_test_pushes_concrete_component_class()
{
	UNIT_TEST_BEGIN("a component pushes as its concrete class, not the static one")

	auto context= std::make_shared<CommonScriptContext>();
	success&= context->createScriptState();
	assert(success);

	lua_State* L= context->getLuaState();

	// Parent classes before derived ones, as ProjectScriptContext binds them
	MikanComponent::bindLuaFunctions(L);
	TransformComponent::bindLuaFunctions(L);
	DMXFixtureComponent::bindLuaFunctions(L);
	RGBPixelGridComponent::bindLuaFunctions(L);

	context->registerComponentClass(DMXFixtureComponent::k_componentClassName,
									[](lua_State* state, MikanComponentPtr component)
									{
										return static_cast<bool>(luabridge::push(
											state, std::dynamic_pointer_cast<DMXFixtureComponent>(component).get()));
									});
	context->registerComponentClass(RGBPixelGridComponent::k_componentClassName,
									[](lua_State* state, MikanComponentPtr component)
									{
										return static_cast<bool>(luabridge::push(
											state, std::dynamic_pointer_cast<RGBPixelGridComponent>(component).get()));
									});

	auto grid= std::make_shared<RGBPixelGridComponent>(MikanObjectWeakPtr());
	auto gridDefinition= std::make_shared<RGBPixelGridDefinition>();
	gridDefinition->resizeGrid(6, 4);
	grid->setDefinition(gridDefinition);

	// Held as the base type, which is how a group hands its fixtures out
	MikanComponentPtr fixture= grid;
	success&= context->pushComponent(L, fixture);
	assert(success);
	lua_setglobal(L, "test_fixture");

	// columns and rows live on RGBPixelGridComponent. They read back only if
	// the userdata carries that class's metatable rather than the base's.
	std::string result;
	success&= context->evalString("return test_fixture.columns", result) && result == "6";
	assert(success);
	success&= context->evalString("return test_fixture.rows", result) && result == "4";
	assert(success);

	// The inherited surface still resolves through the class hierarchy
	success&= context->evalString("return test_fixture.className", result)
			  && result == RGBPixelGridComponent::k_componentClassName;
	assert(success);

	// A null component is nil rather than a missing global
	success&= context->pushComponent(L, MikanComponentPtr());
	assert(success);
	lua_setglobal(L, "test_absent_fixture");
	success&= context->evalString("return test_absent_fixture == nil", result) && result == "true";
	assert(success);

	// getFixtureAtIndex reaches the context back through the state it is called
	// with. This group has no owner, so nothing resolves and every index is nil,
	// which is enough to exercise that path.
	DMXFixtureGroupComponent::bindLuaFunctions(L);
	auto group= std::make_shared<DMXFixtureGroupComponent>(MikanObjectWeakPtr());
	group->setDefinition(std::make_shared<DMXFixtureGroupDefinition>());
	success&= static_cast<bool>(luabridge::setGlobal(L, group.get(), "test_group"));
	assert(success);
	success&= context->evalString("return test_group:getFixtureAtIndex(0) == nil", result) && result == "true";
	assert(success);

	UNIT_TEST_COMPLETE()
}

// -- ScriptBehavior -----
bool script_behavior_test_extend_and_construct()
{
	UNIT_TEST_BEGIN("ScriptBehavior classes extend, construct, and chain")

	auto context= std::make_shared<CommonScriptContext>();
	success&= context->createScriptState();
	assert(success);

	// Both spellings make a subclass; calling a subclass makes an instance
	std::string result;
	success&= context->evalString("Base = ScriptBehavior:extend('Base')\n"
								  "function Base:init() self.value = 1 end\n"
								  "function Base:describe() return 'base ' .. self.value end\n"
								  "Derived = Base:extend('Derived')\n"
								  "function Derived:init() Derived.super.init(self) self.value = self.value + 1 end\n"
								  "Short = ScriptBehavior()\n"
								  "inst = Derived()\n"
								  "short_inst = Short()\n",
								  result);
	assert(success);

	success&= evalEquals(context, "return inst.value", "2");
	success&= evalEquals(context, "return inst:describe()", "base 2");
	success&= evalEquals(context, "return inst:is(Derived)", "true");
	success&= evalEquals(context, "return inst:is(Base)", "true");
	success&= evalEquals(context, "return inst:is(Short)", "false");
	success&= evalEquals(context, "return tostring(Derived)", "Derived");
	success&= evalEquals(context, "return tostring(inst)", "Derived instance");
	success&= evalEquals(context, "return tostring(Short)", "ScriptBehavior");
	success&= evalEquals(context, "return type(short_inst._paramOrder)", "table");
	success&= evalEquals(context, "return require('ScriptBehavior') == ScriptBehavior", "true");
	assert(success);

	// Methods list in declaration order, subclass first, base excluded
	success&=
		evalEquals(context, "return table.concat(ScriptBehavior.__collectMethodNames(Derived), ',')", "init,describe");
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_behavior_test_class_from_return_value()
{
	UNIT_TEST_BEGIN("a chunk that returns a class makes it the file's class")

	ScopedTestProject project;
	const std::filesystem::path path=
		project.writeScript("returned.lua", "local Mine = ScriptBehavior:extend('Mine')\n"
											"function Mine:init() self.tag = 'mine' end\n"
											"local Helper = ScriptBehavior:extend('Helper')\n"
											"return Mine\n");

	auto context= std::make_shared<CommonScriptContext>();
	success&= context->createScriptState();
	assert(success);
	success&= context->loadBehavior(path, 7);
	assert(success);

	// The returned class wins over the one created last
	const CommonScriptContext::BehaviorInstance* instance= context->findBehavior(7);
	success&= instance != nullptr && instance->className == "Mine";
	assert(success);
	success&= context->isScriptLoaded(7);
	assert(success);
	success&= exposeInstance(context, 7) && evalEquals(context, "return test_inst.tag", "mine");
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_behavior_test_class_from_last_created()
{
	UNIT_TEST_BEGIN("a chunk with no return value uses the last class it created")

	ScopedTestProject project;
	const std::filesystem::path path=
		project.writeScript("stage_lights.lua", "Helper = ScriptBehavior:extend('Helper')\n"
												"Lights = ScriptBehavior()\n"
												"function Lights:init() self.count = 3 end\n");

	auto context= std::make_shared<CommonScriptContext>();
	success&= context->createScriptState();
	assert(success);
	success&= context->loadBehavior(path, 3);
	assert(success);

	// An unnamed class takes the file stem
	const CommonScriptContext::BehaviorInstance* instance= context->findBehavior(3);
	success&= instance != nullptr && instance->className == "stage_lights";
	assert(success);
	success&= evalEquals(context, "return tostring(Lights)", "stage_lights");
	success&= exposeInstance(context, 3) && evalEquals(context, "return test_inst.count", "3");
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_behavior_test_no_class_is_load_error()
{
	UNIT_TEST_BEGIN("a file defining no class is a load error that disposes the state")

	ScopedTestProject project;
	const std::filesystem::path path= project.writeScript("plain.lua", "x = 1\n");

	auto context= std::make_shared<CommonScriptContext>();
	ScriptErrorListener listener;
	context->OnScriptError+= MakeDelegate(&listener, &ScriptErrorListener::onScriptError);
	success&= context->createScriptState();
	assert(success);

	success&= !context->loadBehavior(path, 1);
	assert(success);
	success&= !context->hasLoadedScript();
	assert(success);
	success&= listener.errors.size() == 1 && listener.errors[0].kind == eScriptErrorKind::load
			  && listener.errors[0].scriptId == 1 && listener.errors[0].resolvedPath == path
			  && listener.errors[0].message.find("no ScriptBehavior class") != std::string::npos;
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_behavior_test_init_error_is_load_error()
{
	UNIT_TEST_BEGIN("an error inside init is a load error with a line number")

	ScopedTestProject project;
	const std::filesystem::path path= project.writeScript("broken.lua", "Broken = ScriptBehavior()\n"
																		"function Broken:init()\n"
																		"  error('boom')\n"
																		"end\n");

	auto context= std::make_shared<CommonScriptContext>();
	ScriptErrorListener listener;
	context->OnScriptError+= MakeDelegate(&listener, &ScriptErrorListener::onScriptError);
	success&= context->createScriptState();
	assert(success);

	success&= !context->loadBehavior(path, 1);
	assert(success);
	success&= !context->hasLoadedScript();
	assert(success);
	success&= listener.errors.size() == 1;
	assert(success);
	const ScriptError& error= listener.errors[0];
	success&= error.kind == eScriptErrorKind::load && error.chunkName == "scripts/broken.lua" && error.line == 3
			  && error.message == "boom" && error.resolvedPath == path
			  && error.traceback.find("stack traceback") != std::string::npos && error.context == "broken:init";
	assert(success);

	// A syntax error is reported the same way
	const std::filesystem::path badPath= project.writeScript("syntax.lua", "Bad = ScriptBehavior()\n"
																		   "function Bad:init(\n");
	success&= context->createScriptState();
	assert(success);
	success&= !context->loadBehavior(badPath, 2);
	assert(success);
	success&= listener.errors.size() == 2 && listener.errors[1].kind == eScriptErrorKind::load
			  && listener.errors[1].chunkName == "scripts/syntax.lua" && listener.errors[1].line > 0;
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_behavior_test_parameter_reflection()
{
	UNIT_TEST_BEGIN("public fields init assigns become typed parameters in declaration order")

	ScopedTestProject project;
	const std::filesystem::path path=
		project.writeScript("params.lua", "Params = ScriptBehavior:extend('Params')\n"
										  "function Params:init()\n"
										  "  self.enabled = true\n"
										  "  self.count = 2\n"
										  "  self.speed = 2.0\n"
										  "  self.label = 'hi'\n"
										  "  self.offset = Vec3f(1.0, 2.0, 3.0)\n"
										  "  self.grid = ComponentRef('RGBPixelGridComponent')\n"
										  "  self._private = 5\n"
										  "  self.items = {}\n"
										  "  self.helper = function() end\n"
										  "end\n"
										  "function Params:Trigger_Go(args) end\n");

	auto context= std::make_shared<CommonScriptContext>();
	success&= context->createScriptState();
	assert(success);
	bindPixelGridClass(context);

	success&= context->loadBehavior(path, 1);
	assert(success);

	// Declaration order, without the private field, the table, or the function
	std::vector<std::string> names;
	context->getVariableNamesForScript(1, names);
	success&= names == std::vector<std::string>{"enabled", "count", "speed", "label", "offset", "grid"};
	assert(success);

	const std::vector<CommonScriptContext::VariableBinding>& bindings= context->getScriptVariables();
	success&= bindings.size() == 6 && bindings[0].type == MikanVariantType::BOOL
			  && bindings[1].type == MikanVariantType::INT && bindings[2].type == MikanVariantType::FLOAT
			  && bindings[3].type == MikanVariantType::STRING && bindings[4].type == MikanVariantType::VECTOR3F
			  && bindings[5].isComponentReference() && bindings[5].componentClass == "RGBPixelGridComponent";
	assert(success);

	// The sentinel became nil, the other defaults stayed in place
	success&= exposeInstance(context, 1);
	success&= evalEquals(context, "return test_inst.grid == nil", "true");
	success&= evalEquals(context, "return test_inst.count", "2");
	success&= evalEquals(context, "return test_inst.offset.y", "2.0");
	success&= evalEquals(context, "return test_inst._private", "5");
	assert(success);

	// A write of the matching type lands on the instance; a mismatch is refused
	success&=
		context->setVariableValue(1, "count", MikanVariant(9)) && evalEquals(context, "return test_inst.count", "9");
	success&= !context->setVariableValue(1, "count", MikanVariant(9.5f));
	success&= !context->setVariableValue(2, "count", MikanVariant(9));
	success&= context->setVariableValue(1, "offset", MikanVariant(MikanVector3f{4.f, 5.f, 6.f}))
			  && evalEquals(context, "return test_inst.offset.z", "6.0");
	success&= context->hasVariable(1, "label") && !context->hasVariable(1, "_private");
	assert(success);

	// An unknown component class in a ComponentRef is a load error
	ScriptErrorListener listener;
	context->OnScriptError+= MakeDelegate(&listener, &ScriptErrorListener::onScriptError);
	const std::filesystem::path badPath=
		project.writeScript("bad_ref.lua", "BadRef = ScriptBehavior()\n"
										   "function BadRef:init() self.thing = ComponentRef('NoSuchComponent') end\n");
	success&= !context->loadBehavior(badPath, 2);
	success&= listener.errors.size() == 1 && listener.errors[0].message.find("NoSuchComponent") != std::string::npos;
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_behavior_test_store_resolution()
{
	UNIT_TEST_BEGIN("a stored value of the same type wins over the default, otherwise the default is adopted")

	ScopedTestProject project;
	const std::filesystem::path path= project.writeScript("stored.lua", "Stored = ScriptBehavior:extend('Stored')\n"
																		"function Stored:init()\n"
																		"  self.count = 2\n"
																		"  self.speed = 2.0\n"
																		"  self.name = 'default'\n"
																		"end\n");

	// count has a stored INT, speed a stored value of the wrong type, name nothing
	ScriptDefinition definition;
	definition.setScriptVariable("count", MikanVariant(11));
	definition.setScriptVariable("speed", MikanVariant(std::string("fast")));

	auto context= std::make_shared<CommonScriptContext>();
	success&= context->createScriptState();
	assert(success);
	success&= context->loadBehavior(path, 1, &definition);
	assert(success);

	success&= exposeInstance(context, 1);
	success&= evalEquals(context, "return test_inst.count", "11");
	success&= evalEquals(context, "return test_inst.speed", "2.0");
	success&= evalEquals(context, "return test_inst.name", "default");
	assert(success);

	// The adopted defaults are now in the store, and the wrong-typed entry was replaced
	MikanVariant stored;
	success&=
		definition.getScriptVariableOfType("speed", MikanVariantType::FLOAT, stored) && stored.getFloatValue() == 2.f;
	success&= definition.getScriptVariableOfType("name", MikanVariantType::STRING, stored)
			  && std::string(stored.getUtf8Value()) == "default";
	success&= definition.getScriptVariableOfType("count", MikanVariantType::INT, stored) && stored.getIntValue() == 11;
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_behavior_test_shared_file_per_instance()
{
	UNIT_TEST_BEGIN("two scripts sharing a file run its chunk once and get separate instances")

	ScopedTestProject project;
	const std::filesystem::path path=
		project.writeScript("shared.lua", "chunk_runs = (chunk_runs or 0) + 1\n"
										  "Shared = ScriptBehavior:extend('Shared')\n"
										  "function Shared:init() self.speed = 1.0 end\n"
										  "function Shared:Trigger_Bump(args) self.speed = self.speed + 1 end\n");

	auto context= std::make_shared<CommonScriptContext>();
	success&= context->createScriptState();
	assert(success);
	success&= context->loadBehavior(path, 1) && context->loadBehavior(path, 2);
	assert(success);
	success&= evalEquals(context, "return chunk_runs", "1");
	assert(success);

	// A parameter write reaches one instance only
	success&= context->setVariableValue(1, "speed", MikanVariant(5.f));
	success&= exposeInstance(context, 1, "inst_a") && exposeInstance(context, 2, "inst_b");
	success&= evalEquals(context, "return inst_a.speed", "5.0");
	success&= evalEquals(context, "return inst_b.speed", "1.0");
	assert(success);

	// So does a targeted trigger, while a broadcast reaches both
	success&= context->invokeScriptTrigger("Bump", {}, 2);
	success&= evalEquals(context, "return inst_a.speed", "5.0");
	success&= evalEquals(context, "return inst_b.speed", "2.0");
	success&= context->invokeScriptTrigger("Bump");
	success&= evalEquals(context, "return inst_a.speed", "6.0");
	success&= evalEquals(context, "return inst_b.speed", "3.0");
	assert(success);

	// A second load of the same id is refused
	success&= !context->loadBehavior(path, 1);
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_behavior_test_trigger_targeting()
{
	UNIT_TEST_BEGIN("triggers list, target one script, broadcast, and pass their args")

	ScopedTestProject project;
	const std::filesystem::path pathA=
		project.writeScript("a.lua", "A = ScriptBehavior:extend('A')\n"
									 "function A:Trigger_Ping(args) a_hits = (a_hits or 0) + 1 a_user = args.user end\n"
									 "function A:Trigger_Only(args) end\n");
	const std::filesystem::path pathB=
		project.writeScript("b.lua", "B = ScriptBehavior:extend('B')\n"
									 "function B:Trigger_Ping(args) b_hits = (b_hits or 0) + 1 end\n");

	auto context= std::make_shared<CommonScriptContext>();
	success&= context->createScriptState();
	assert(success);
	success&= context->loadBehavior(pathA, 1) && context->loadBehavior(pathB, 2);
	assert(success);

	std::vector<std::string> names;
	context->getBehaviorTriggerNames(1, names);
	success&= names == std::vector<std::string>{"Ping", "Only"};
	assert(success);

	std::vector<MikanScriptID> targets;
	context->findBehaviorsWithTrigger("Ping", targets);
	success&= targets == std::vector<MikanScriptID>{1, 2};
	targets.clear();
	context->findBehaviorsWithTrigger("Only", targets);
	success&= targets == std::vector<MikanScriptID>{1};
	assert(success);

	success&= context->invokeScriptTrigger("Ping", {{"user", "bob"}}, 1);
	success&= evalEquals(context, "return a_hits", "1") && evalEquals(context, "return a_user", "bob")
			  && evalEquals(context, "return b_hits", "nil");
	success&= context->invokeScriptTrigger("Ping");
	success&= evalEquals(context, "return a_hits", "2") && evalEquals(context, "return b_hits", "1");
	assert(success);

	// An unknown trigger, or one the target lacks, fires nothing
	success&= !context->invokeScriptTrigger("Nope");
	success&= !context->invokeScriptTrigger("Only", {}, 2);
	success&= evalEquals(context, "return a_hits", "2");
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_behavior_test_trigger_error_keeps_state()
{
	UNIT_TEST_BEGIN("a trigger error is reported with its location and leaves the state alive")

	ScopedTestProject project;
	const std::filesystem::path path=
		project.writeScript("boom.lua", "Boom = ScriptBehavior:extend('Boom')\n"
										"function Boom:Trigger_Boom(args)\n"
										"  local t = nil\n"
										"  return t.field\n"
										"end\n"
										"function Boom:Trigger_Fine(args) fine = true end\n");

	auto context= std::make_shared<CommonScriptContext>();
	ScriptErrorListener listener;
	context->OnScriptError+= MakeDelegate(&listener, &ScriptErrorListener::onScriptError);
	success&= context->createScriptState();
	assert(success);
	success&= context->loadBehavior(path, 4);
	assert(success);

	success&= !context->invokeScriptTrigger("Boom");
	assert(success);
	success&= context->hasLoadedScript() && context->hasBehavior(4);
	assert(success);
	success&= listener.errors.size() == 1;
	assert(success);
	const ScriptError& error= listener.errors[0];
	success&= error.kind == eScriptErrorKind::trigger && error.scriptId == 4 && error.context == "Boom:Trigger_Boom"
			  && error.chunkName == "scripts/boom.lua" && error.line == 4 && error.resolvedPath == path
			  && error.message.find("attempt to index") != std::string::npos
			  && error.traceback.find("stack traceback") != std::string::npos;
	assert(success);

	// The next trigger still runs
	success&= context->invokeScriptTrigger("Fine") && evalEquals(context, "return fine", "true");
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_behavior_test_coroutine_error_keeps_state()
{
	UNIT_TEST_BEGIN("a coroutine error is reported once and the scheduler carries on")

	ScopedTestProject project;
	const std::filesystem::path path=
		project.writeScript("co.lua", "Co = ScriptBehavior:extend('Co')\n"
									  "function Co:Trigger_Start(args)\n"
									  "  start_coroutine(function() wait_next_frame() error('late') end)\n"
									  "  start_coroutine(function() wait_next_frame() survivor = true end)\n"
									  "end\n");

	auto context= std::make_shared<CommonScriptContext>();
	ScriptErrorListener listener;
	context->OnScriptError+= MakeDelegate(&listener, &ScriptErrorListener::onScriptError);
	success&= context->createScriptState();
	assert(success);
	success&= context->loadBehavior(path, 1);
	assert(success);

	success&= context->invokeScriptTrigger("Start");
	assert(success);
	context->updateScript(1.f / 60.f);
	context->updateScript(1.f / 60.f);
	context->updateScript(1.f / 60.f);

	// The failing coroutine reported once, the other one still ran, and the
	// state is still there
	success&= listener.errors.size() == 1 && listener.errors[0].kind == eScriptErrorKind::coroutine
			  && listener.errors[0].message == "late" && listener.errors[0].line == 3;
	assert(success);
	success&= context->hasLoadedScript() && evalEquals(context, "return survivor", "true");
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_behavior_test_message_handler_order()
{
	UNIT_TEST_BEGIN("OnMessage is offered in load order until one handles it")

	ScopedTestProject project;
	const std::filesystem::path pathA=
		project.writeScript("ma.lua", "MA = ScriptBehavior:extend('MA')\n"
									  "function MA:OnMessage(m) seen = 'a' return false end\n");
	const std::filesystem::path pathB=
		project.writeScript("mb.lua", "MB = ScriptBehavior:extend('MB')\n"
									  "function MB:OnMessage(m) seen = seen .. 'b' .. m return true end\n");
	const std::filesystem::path pathC=
		project.writeScript("mc.lua", "MC = ScriptBehavior:extend('MC')\n"
									  "function MC:OnMessage(m) seen = seen .. 'c' return true end\n");

	auto context= std::make_shared<CommonScriptContext>();
	success&= context->createScriptState();
	assert(success);
	success&= context->loadBehavior(pathA, 1) && context->loadBehavior(pathB, 2) && context->loadBehavior(pathC, 3);
	assert(success);
	success&= context->findBehavior(1)->bHasMessageHandler && !context->findBehavior(1)->triggerNames.size();
	assert(success);

	success&= context->invokeScriptMessageHandler("hi") && evalEquals(context, "return seen", "abhi");
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_behavior_test_http_trigger_names()
{
	UNIT_TEST_BEGIN("HttpTrigger_ methods are listed apart from triggers and receive the args")

	ScopedTestProject project;
	const std::filesystem::path path=
		project.writeScript("http.lua", "Http = ScriptBehavior:extend('Http')\n"
										"function Http:HttpTrigger_GiftSub(args) gift_user = args.user end\n"
										"function Http:Trigger_Local(args) end\n");

	auto context= std::make_shared<CommonScriptContext>();
	success&= context->createScriptState();
	assert(success);
	success&= context->loadBehavior(path, 1);
	assert(success);

	std::vector<std::string> triggers;
	context->getBehaviorTriggerNames(1, triggers);
	std::vector<std::string> httpTriggers;
	context->getBehaviorHttpTriggerNames(1, httpTriggers);
	success&= triggers == std::vector<std::string>{"Local"} && httpTriggers == std::vector<std::string>{"GiftSub"};
	assert(success);

	// An HTTP trigger is not reachable as a plain trigger
	success&= !context->invokeScriptTrigger("GiftSub");
	success&= context->invokeScriptHttpTrigger(1, "GiftSub", {{"user", "amy"}});
	success&= evalEquals(context, "return gift_user", "amy");
	success&= !context->invokeScriptHttpTrigger(1, "Missing", {});
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_behavior_test_call_behavior_method()
{
	UNIT_TEST_BEGIN("callBehaviorMethod passes pushed args, tolerates a missing method, and reports errors")

	ScopedTestProject project;
	const std::filesystem::path path=
		project.writeScript("seq.lua", "Seq = ScriptBehavior:extend('Seq')\n"
									   "function Seq:SequenceUpdate(sequence, t, dt) last_t = t last_dt = dt end\n"
									   "function Seq:SequenceStop(sequence) error('stop failed') end\n");

	auto context= std::make_shared<CommonScriptContext>();
	ScriptErrorListener listener;
	context->OnScriptError+= MakeDelegate(&listener, &ScriptErrorListener::onScriptError);
	success&= context->createScriptState();
	assert(success);
	success&= context->loadBehavior(path, 1);
	assert(success);

	success&= context->behaviorHasMethod(1, "SequenceUpdate") && !context->behaviorHasMethod(1, "SequenceStart")
			  && !context->behaviorHasMethod(2, "SequenceUpdate");
	assert(success);

	const CommonScriptContext::LuaArgPusher pushArgs= [](lua_State* L)
	{
		lua_pushnil(L);
		lua_pushnumber(L, 1.5);
		lua_pushnumber(L, 0.25);
		return 3;
	};

	std::string error;
	success&= context->callBehaviorMethod(1, "SequenceUpdate", pushArgs, error) && error.empty();
	success&= evalEquals(context, "return last_t", "1.5") && evalEquals(context, "return last_dt", "0.25");
	assert(success);

	// A missing method is not an error; a missing script is
	success&= context->callBehaviorMethod(1, "SequenceStart", pushArgs, error) && error.empty();
	success&= !context->callBehaviorMethod(2, "SequenceStart", pushArgs, error) && !error.empty();
	assert(success);

	// An error fills outError, reports as a sequence error, and keeps the state
	success&= !context->callBehaviorMethod(1, "SequenceStop", pushArgs, error, eScriptErrorKind::sequence);
	success&= error.find("stop failed") != std::string::npos;
	success&= listener.errors.size() == 1 && listener.errors[0].kind == eScriptErrorKind::sequence
			  && listener.errors[0].context == "Seq:SequenceStop" && listener.errors[0].line == 3;
	success&= context->hasLoadedScript();
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_behavior_test_component_ref_refresh()
{
	UNIT_TEST_BEGIN("a component reference field resolves, excludes a dying object, and comes back")

	ScopedTestProject project;
	const std::filesystem::path path=
		project.writeScript("ref.lua", "Ref = ScriptBehavior:extend('Ref')\n"
									   "function Ref:init() self.grid = ComponentRef('RGBPixelGridComponent') end\n");

	auto context= std::make_shared<SingleComponentScriptContext>();
	success&= context->createScriptState();
	assert(success);
	bindPixelGridClass(context);

	auto grid= std::make_shared<RGBPixelGridComponent>(MikanObjectWeakPtr());
	auto gridDefinition= std::make_shared<RGBPixelGridDefinition>();
	gridDefinition->resizeGrid(6, 4);
	grid->setDefinition(gridDefinition);
	context->resolvableId= 1008;
	context->resolvableComponent= grid;

	// The stored reference resolves at load
	ScriptDefinition definition;
	definition.setScriptComponentVariable("grid", "RGBPixelGridComponent", 1008);
	success&= context->loadBehavior(path, 1, &definition);
	assert(success);
	success&= exposeInstance(context, 1) && evalEquals(context, "return test_inst.grid.columns", "6");
	assert(success);

	// The object about to be destroyed reads as nil, then comes back
	context->refreshComponentVariables(1008);
	success&= evalEquals(context, "return test_inst.grid == nil", "true");
	context->refreshComponentVariables();
	success&= evalEquals(context, "return test_inst.grid.rows", "4");
	assert(success);

	// A write of another id resolves nothing, a write back resolves again
	success&= context->setVariableValue(1, "grid", MikanVariant(1049))
			  && evalEquals(context, "return test_inst.grid == nil", "true");
	success&= context->setVariableValue(1, "grid", MikanVariant(1008))
			  && evalEquals(context, "return test_inst.grid.columns", "6");
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_error_test_parse_location()
{
	UNIT_TEST_BEGIN("a Lua error message splits into chunk, line, and text")

	std::string chunk;
	int line= 0;
	std::string message;

	success= CommonScriptContext::parseLuaErrorLocation("scripts/a.lua:12: attempt to index a nil value", chunk, line,
														message)
			 && chunk == "scripts/a.lua" && line == 12 && message == "attempt to index a nil value";
	assert(success);

	// The drive colon is not a line separator
	success&= CommonScriptContext::parseLuaErrorLocation("D:/x/a.lua:3: msg", chunk, line, message)
			  && chunk == "D:/x/a.lua" && line == 3 && message == "msg";
	assert(success);

	// Lua shortens long names; the prefix is kept for the resolver
	success&= CommonScriptContext::parseLuaErrorLocation("...ources/scripts/long.lua:7: msg", chunk, line, message)
			  && chunk == "...ources/scripts/long.lua" && line == 7;
	assert(success);

	success&= !CommonScriptContext::parseLuaErrorLocation("no location here", chunk, line, message) && chunk.empty()
			  && line == 0 && message == "no location here";
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool script_error_test_resolves_project_then_bundled()
{
	UNIT_TEST_BEGIN("a chunk name resolves against the project, then the bundled scripts")

	ScopedTestProject project;
	const std::filesystem::path projectScript= project.writeScript("mine.lua", "");

	auto context= std::make_shared<CommonScriptContext>();

	success= context->resolveScriptErrorPath("scripts/mine.lua") == projectScript;
	assert(success);
	success&= context->resolveScriptErrorPath("@scripts/mine.lua") == projectScript;
	assert(success);

	// Not in the project, so the bundled copy answers
	const std::filesystem::path bundledEasing= PathUtils::getResourceDirectory() / "scripts" / "easing.lua";
	success&= context->resolveScriptErrorPath("scripts/easing.lua") == bundledEasing;
	assert(success);

	// A shortened name matches by suffix
	success&= context->resolveScriptErrorPath("...cripts/easing.lua") == bundledEasing;
	assert(success);
	success&= context->resolveScriptErrorPath("...ripts/mine.lua") == projectScript;
	assert(success);

	// An absolute path stands, a missing one is empty
	success&= context->resolveScriptErrorPath(projectScript.generic_string()) == projectScript;
	success&= context->resolveScriptErrorPath("scripts/absent.lua").empty();
	success&= context->resolveScriptErrorPath("").empty();
	assert(success);

	UNIT_TEST_COMPLETE()
}

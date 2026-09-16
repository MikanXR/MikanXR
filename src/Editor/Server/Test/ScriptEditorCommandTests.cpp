#include "ScriptEditorCommandTests.h"
#include "unit_test.h"

#include "AppSettingsConfig.h"
#include "ScriptAssetReference.h"

#include <configuru.hpp>
#include <filesystem>
#include <memory>
#include <stdio.h>
#include <string>

bool run_script_editor_command_tests()
{
	UNIT_TEST_MODULE_BEGIN("script_editor_command")
	UNIT_TEST_MODULE_CALL_TEST(script_editor_command_test_expands_placeholders);
	UNIT_TEST_MODULE_CALL_TEST(script_editor_command_test_detects_placeholders);
	UNIT_TEST_MODULE_CALL_TEST(script_editor_command_test_legacy_default_upgrades);
	UNIT_TEST_MODULE_END()
}

bool script_editor_command_test_expands_placeholders()
{
	UNIT_TEST_BEGIN("expandEditorCommand replaces every placeholder with a quoted forward-slash path")

	const std::filesystem::path projectDir= "C:/Projects/MyProject";
	const std::filesystem::path scriptPath= "C:/Projects/MyProject/scripts/test.lua";

	// Line 0 becomes 1, and a placeholder used twice is replaced both times
	const std::string expanded= ScriptAssetReference::expandEditorCommand(
		"code --reuse-window {project} --goto {file}:{line} {file}", projectDir, scriptPath, 0);
	success&= expanded
			  == "code --reuse-window \"C:/Projects/MyProject\" --goto "
				 "\"C:/Projects/MyProject/scripts/test.lua\":1 "
				 "\"C:/Projects/MyProject/scripts/test.lua\"";

	const std::string expandedWithLine=
		ScriptAssetReference::expandEditorCommand("code --goto {file}:{line}", projectDir, scriptPath, 42);
	success&= expandedWithLine == "code --goto \"C:/Projects/MyProject/scripts/test.lua\":42";

	UNIT_TEST_COMPLETE()
}

bool script_editor_command_test_detects_placeholders()
{
	UNIT_TEST_BEGIN("scriptEditorCommandHasPlaceholders looks for {file}")

	success&= AppSettingsConfig::scriptEditorCommandHasPlaceholders(AppSettingsConfig::k_defaultScriptEditorCommand);
	success&= !AppSettingsConfig::scriptEditorCommandHasPlaceholders(AppSettingsConfig::k_legacyScriptEditorCommand);

	UNIT_TEST_COMPLETE()
}

bool script_editor_command_test_legacy_default_upgrades()
{
	UNIT_TEST_BEGIN("a stored legacy default upgrades to the new default, anything else is kept")

	auto legacySettings= std::make_shared<AppSettingsConfig>("ScriptEditorCommandTest");
	configuru::Config legacyJson= configuru::Config::object();
	legacyJson[AppSettingsConfig::k_scriptEditorCommandPropertyId]= AppSettingsConfig::k_legacyScriptEditorCommand;
	legacySettings->readFromJSON(legacyJson);
	success&= legacySettings->getScriptEditorCommand() == AppSettingsConfig::k_defaultScriptEditorCommand;

	auto customSettings= std::make_shared<AppSettingsConfig>("ScriptEditorCommandTest");
	configuru::Config customJson= configuru::Config::object();
	const std::string customCommand= "notepad++ {file}";
	customJson[AppSettingsConfig::k_scriptEditorCommandPropertyId]= customCommand;
	customSettings->readFromJSON(customJson);
	success&= customSettings->getScriptEditorCommand() == customCommand;

	UNIT_TEST_COMPLETE()
}

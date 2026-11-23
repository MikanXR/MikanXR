#include "LocalizationTests.h"
#include "EditorEntitySchemaTable.h"
#include "EnumPropertyMetaData.h"
#include "LocText.h"
#include "LocalizationManager.h"
#include "MkGuiTheme.h"
#include "PathUtils.h"

#include "unit_test.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
// Loads the bundled tables only (no CDN cache overlay, no background fetch) so
// the result depends on the repo, not on what this machine has downloaded
static bool startupTestManager(LocalizationManager& manager)
{
	const std::filesystem::path localizationDir= PathUtils::getResourceDirectory() / "localization";

	return manager.startup(localizationDir, AppSettingsConfigPtr(), /*bEnableCommunityTranslations=*/false);
}
} // namespace

bool localization_test_tables_load_clean()
{
	UNIT_TEST_BEGIN("tables load without validation warnings")

	LocalizationManager manager;
	if (!startupTestManager(manager))
	{
		fprintf(stdout, "    FAILED: manager startup (missing or corrupt en.json)\n");
		manager.shutdown();
		success= false;
		UNIT_TEST_COMPLETE()
	}

	for (const std::string& warning : manager.getLoadWarnings())
	{
		fprintf(stdout, "    load warning: %s\n", warning.c_str());
		success= false;
	}

	manager.shutdown();
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool localization_test_window_titles_are_unique_ids()
{
	UNIT_TEST_BEGIN("window titles are non-empty and unique")

	LocalizationManager manager;
	if (!startupTestManager(manager))
	{
		fprintf(stdout, "    FAILED: manager startup\n");
		manager.shutdown();
		success= false;
		UNIT_TEST_COMPLETE()
	}

	const std::map<std::string, std::string>* englishStrings= manager.getRawStrings("en");
	if (englishStrings == nullptr)
	{
		fprintf(stdout, "    FAILED: no English table loaded\n");
		manager.shutdown();
		success= false;
		UNIT_TEST_COMPLETE()
	}

	// The English text of every windows.* key IS the window's ImGui ID, so an
	// empty or duplicated title would merge two windows
	std::map<std::string, std::string> titleToKey;
	for (const auto& [key, text] : *englishStrings)
	{
		if (key.rfind("windows.", 0) != 0)
			continue;

		if (text.empty())
		{
			fprintf(stdout, "    FAILED: empty window title: %s\n", key.c_str());
			success= false;
			continue;
		}

		const auto [existingIt, bInserted]= titleToKey.emplace(text, key);
		if (!bInserted)
		{
			fprintf(stdout, "    FAILED: duplicate window title '%s' (%s vs %s)\n", text.c_str(), key.c_str(),
					existingIt->second.c_str());
			success= false;
		}
	}

	manager.shutdown();
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool localization_test_unknown_key_passes_through()
{
	UNIT_TEST_BEGIN("unknown key returns the key itself")

	LocalizationManager manager;
	if (!startupTestManager(manager))
	{
		fprintf(stdout, "    FAILED: manager startup\n");
		manager.shutdown();
		success= false;
		UNIT_TEST_COMPLETE()
	}

	const char* bogusKey= "bogus.key";
	success&= (manager.fetchText(bogusKey) == bogusKey);
	success&= (manager.fetchLabel(bogusKey) == bogusKey);
	success&= (manager.fetchWindowTitle(bogusKey) == bogusKey);

	manager.shutdown();
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool localization_test_glyph_coverage()
{
	UNIT_TEST_BEGIN("every string renders with the baked glyph ranges")

	LocalizationManager manager;
	if (!startupTestManager(manager))
	{
		fprintf(stdout, "    FAILED: manager startup\n");
		manager.shutdown();
		success= false;
		UNIT_TEST_COMPLETE()
	}

	size_t checkedStringCount= 0;

	for (const LocalizationManager::LanguageInfo& info : manager.getSupportedLanguageInfos())
	{
		const std::map<std::string, std::string>* rawStrings= manager.getRawStrings(info.code);
		if (rawStrings == nullptr)
			continue;

		// The native language name is displayed in the language selector, so
		// it has to render too
		std::vector<std::pair<std::string, std::string>> checkStrings(rawStrings->begin(), rawStrings->end());
		checkStrings.emplace_back("_meta.nativeName", info.nativeName);

		for (const auto& [key, text] : checkStrings)
		{
			++checkedStringCount;

			unsigned int badCodepoint= 0;
			if (MkGuiTheme::isTextRenderableWithUiGlyphs(text, &badCodepoint))
				continue;

			if (badCodepoint == 0)
			{
				fprintf(stdout, "    FAILED: %s: '%s' has malformed UTF-8\n", info.code.c_str(), key.c_str());
			}
			else
			{
				fprintf(stdout, "    FAILED: %s: '%s' uses codepoint U+%04X outside the baked glyph ranges\n",
						info.code.c_str(), key.c_str(), badCodepoint);
			}
			success= false;
		}
	}

	fprintf(stdout, "      %zu string(s) glyph-checked\n", checkedStringCount);

	manager.shutdown();
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool localization_test_glyph_ranges_file_is_current()
{
	UNIT_TEST_BEGIN("the committed glyph range file matches the baked ranges")

	// The Python localization check reads this file to run the coverage test
	// above without the font stack, so a file that has fallen behind the baked
	// ranges would pass or reject strings the app treats differently
	const std::filesystem::path rangesPath= PathUtils::getResourceDirectory() / "font" / "ui_glyph_ranges.txt";

	std::ifstream rangesFile(rangesPath, std::ios::binary);
	if (!rangesFile.is_open())
	{
		fprintf(stdout, "    FAILED: cannot read %s\n", rangesPath.string().c_str());
		success= false;
	}
	else
	{
		std::stringstream contents;
		contents << rangesFile.rdbuf();

		// A Windows checkout may hold the file with CRLF line endings
		std::string committedText= contents.str();
		committedText.erase(std::remove(committedText.begin(), committedText.end(), '\r'), committedText.end());

		if (committedText != MkGuiTheme::formatUiGlyphRanges())
		{
			fprintf(stdout,
					"    FAILED: resources/font/ui_glyph_ranges.txt is stale, regenerate it in the source tree with\n"
					"            MikanCmd -writeUiGlyphRanges=<repo>/resources/font/ui_glyph_ranges.txt\n");
			success= false;
		}
	}

	assert(success);

	UNIT_TEST_COMPLETE()
}

bool localization_test_descriptor_labels_have_keys()
{
	UNIT_TEST_BEGIN("every descriptor label resolves to a key")

	LocalizationManager manager;
	if (!startupTestManager(manager))
	{
		fprintf(stdout, "    FAILED: manager startup\n");
		manager.shutdown();
		success= false;
		UNIT_TEST_COMPLETE()
	}

	// The entity panels build their widgets from the descriptor databases, so a
	// descriptor without a label key would render its raw id (e.g.
	// "tracking_mount_id") in every language
	std::set<std::string> reported;
	auto checkResolved=
		[&](const std::string& resolvedKey, const std::string& descriptorId, const char* what, const char* owner)
	{
		// An empty resolution means neither the per-class override nor the
		// shared key is defined, so the panel would fall back to the raw id
		if (!resolvedKey.empty())
			return;
		if (reported.insert(std::string(owner) + "." + descriptorId).second)
			fprintf(stdout, "    FAILED: no %s label for '%s' (%s)\n", what, descriptorId.c_str(), owner);
		success= false;
	};

	for (const SchemaTestEntry& entry : k_schemaTestEntries)
	{
		std::vector<PropertyDescriptorConstPtr> propertyDescriptors;
		entry.getDescriptors(propertyDescriptors);
		for (const PropertyDescriptorConstPtr& descriptor : propertyDescriptors)
		{
			// A UI-hidden property only reaches a panel when that panel
			// registers a custom renderer that draws its own label, so it needs
			// no entry of its own
			if (descriptor->isUIHidden())
				continue;

			const std::string& propertyName= descriptor->getName();
			checkResolved(locResolveDescriptorKey(entry.label, "properties", propertyName), propertyName, "property",
						  entry.label);

			// Enum choices carry localization keys rather than display text
			const auto* enumMeta= descriptor->getMetaDataOfType<EnumPropertyMetaData>();
			if (enumMeta != nullptr)
			{
				for (const std::string& choiceKey : enumMeta->getStrings())
				{
					if (!manager.hasKey(choiceKey.c_str()))
					{
						if (reported.insert(choiceKey).second)
							fprintf(stdout, "    FAILED: no enum choice text for key '%s' (%s)\n", choiceKey.c_str(),
									entry.label);
						success= false;
					}
				}
			}
		}

		std::vector<FunctionDescriptorConstPtr> functionDescriptors;
		entry.getFunctionDescriptors(functionDescriptors);
		for (const FunctionDescriptorConstPtr& descriptor : functionDescriptors)
		{
			if (descriptor->isUIHidden())
				continue;

			const std::string& functionName= descriptor->getFunctionName();
			checkResolved(locResolveDescriptorKey(entry.label, "functions", functionName), functionName, "function",
						  entry.label);
		}
	}

	manager.shutdown();
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool run_localization_unit_tests()
{
	UNIT_TEST_MODULE_BEGIN("localization")
	UNIT_TEST_MODULE_CALL_TEST(localization_test_tables_load_clean);
	UNIT_TEST_MODULE_CALL_TEST(localization_test_window_titles_are_unique_ids);
	UNIT_TEST_MODULE_CALL_TEST(localization_test_unknown_key_passes_through);
	UNIT_TEST_MODULE_CALL_TEST(localization_test_glyph_coverage);
	UNIT_TEST_MODULE_CALL_TEST(localization_test_glyph_ranges_file_is_current);
	UNIT_TEST_MODULE_CALL_TEST(localization_test_descriptor_labels_have_keys);
	UNIT_TEST_MODULE_END()
}

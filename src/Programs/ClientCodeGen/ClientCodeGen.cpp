//-- includes -----
#include "CodeGenDatabase.h"
#include "MikanClientLanguageGen.h"

#include "Logger.h"

#include "Refureku/Refureku.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "nlohmann/json.hpp"

using json= nlohmann::json;

#ifdef _MSC_VER
#pragma warning(disable : 4996) // ignore strncpy warning
#endif

enum class TargetLanguage
{
	CSharp,
	TypeScript
};

// Walks the reflection database once, then hands the result to the generator for the language
// the config named. Everything language specific lives behind MikanClientLanguageGen, so adding
// a language is a new generator file plus a case in createLanguageGen below.
class MikanClientCodeGen
{
public:
	MikanClientCodeGen() {}

	virtual ~MikanClientCodeGen() { shutdown(); }

	int exec(int argc, char** argv)
	{
		int result= 0;

		if (startup(argc, argv))
		{
			CodeGenDatabase codeGenDatabase;

			// Fetch all reflection data, sorted by module name
			fetchModules(codeGenDatabase);

			// Give every emitter below the same entity order, whatever order the reflection
			// database happened to enumerate them in
			codeGenDatabase.sortEntities();

			if (codeGenDatabase.modules.size() > 0)
			{
				ClientCodeGenContext context;
				context.outputPath= std::filesystem::absolute(m_outputPath);
				context.database= &codeGenDatabase;

				// Nuke any previously generated code
				std::filesystem::remove_all(context.outputPath);

				// (Re)create the the output folder
				std::filesystem::create_directories(context.outputPath);

				// Generate code for all the modules we found
				result= generateCode(context) ? 0 : -1;
			}
		}
		else
		{
			MIKAN_LOG_ERROR("MikanClientCodeGen") << "Failed to initialize!";
			result= -1;
		}

		shutdown();

		return result;
	}

protected:
	bool startup(int argc, char** argv)
	{
		bool success= true;

		LoggerSettings settings= {};
		settings.min_log_level= LogSeverityLevel::info;
		settings.enable_console= true;

		log_init(settings);

		if (argc < 2)
		{
			MIKAN_LOG_ERROR("MikanClientCodeGen") << "Usage: MikanClientCodeGen <path to config.json>";
			success= false;
		}

		std::filesystem::path configPath= argv[1];
		if (!std::filesystem::exists(configPath))
		{
			MIKAN_LOG_ERROR("MikanClientCodeGen") << "Config file not found: " << configPath;
			success= false;
		}

		if (success)
		{
			success= parseConfig(configPath);
		}

		if (success)
		{
			MIKAN_LOG_INFO("MikanClientCodeGen") << "Working Directory: " << std::filesystem::current_path();
			MIKAN_LOG_INFO("MikanClientCodeGen") << "Loaded config: " << configPath;
		}

		return success;
	}

	void shutdown() { log_dispose(); }

	bool parseConfig(const std::filesystem::path& configPath)
	{
		std::string configPathString= configPath.string();
		MIKAN_LOG_INFO("MikanClientCodeGen") << "Loading config file: " << configPath;

		try
		{
			std::ifstream configFile(configPathString);
			std::stringstream configStream;
			configStream << configFile.rdbuf();
			std::string configString= configStream.str();

			json configJson= json::parse(configString);
			m_outputPath= (std::string)configJson["output_path"];

			// Parse target language (defaults to CSharp for backwards compatibility)
			if (configJson.contains("target_language"))
			{
				std::string targetLang= (std::string)configJson["target_language"];

				if (targetLang == "typescript")
				{
					m_targetLanguage= TargetLanguage::TypeScript;
				}
				else if (targetLang == "csharp")
				{
					m_targetLanguage= TargetLanguage::CSharp;
				}
				else
				{
					throw std::runtime_error("Invalid 'target_language' field.");
				}
			}
			else
			{
				throw std::runtime_error("Config file missing 'target_language' field.");
			}
		}
		catch (const std::exception& e)
		{
			MIKAN_LOG_ERROR("MikanClientCodeGen") << "Failed to parse config file: " << e.what();
			return false;
		}

		return true;
	}

	void fetchModules(CodeGenDatabase& codeGenDatabase)
	{
		rfk::Database const& database= rfk::getDatabase();

		database.foreachFileLevelStruct(
			[](rfk::Struct const& entity, void* userData) -> bool
			{
				auto* codeGenDB= reinterpret_cast<CodeGenDatabase*>(userData);
				codeGenDB->visitStruct(entity);
				return true;
			},
			&codeGenDatabase);

		database.foreachFileLevelEnum(
			[](rfk::Enum const& entity, void* userData) -> bool
			{
				auto* codeGenDB= reinterpret_cast<CodeGenDatabase*>(userData);
				codeGenDB->visitEnum(entity);
				return true;
			},
			&codeGenDatabase);
	}

	// The one place a target language maps to the generator that emits it
	MikanClientLanguageGenPtr createLanguageGen(ClientCodeGenContext const& context) const
	{
		switch (m_targetLanguage)
		{
		case TargetLanguage::TypeScript:
			return createTypeScriptClientGen(context);
		case TargetLanguage::CSharp:
		default:
			return createCSharpClientGen(context);
		}
	}

	bool generateCode(ClientCodeGenContext const& context)
	{
		MikanClientLanguageGenPtr languageGen= createLanguageGen(context);

		for (auto const& module : context.database->modules)
		{
			const std::string& moduleName= module.first;
			ClientModulePtr modulePtr= module.second;

			MIKAN_LOG_INFO("MikanClientCodeGen") << "Generate Code for Module: " << moduleName;
			if (!languageGen->generateModuleFile(modulePtr))
			{
				return false;
			}
		}

		return languageGen->generateWholeOutputFiles();
	}

	std::filesystem::path m_outputPath;
	TargetLanguage m_targetLanguage= TargetLanguage::CSharp;
};

int main(int argc, char* argv[])
{
	MikanClientCodeGen app;

	return app.exec(argc, argv);
}

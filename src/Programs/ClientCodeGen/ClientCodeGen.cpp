//-- includes -----
#include "SerializationProperty.h"
#include "Logger.h"

#include "MikanAPI.h"
#include "MikanClientEvents.h"
#include "MikanScriptEvents.h"
#include "MikanVideoSourceEvents.h"
#include "MikanMathTypes.h"
#include "MikanScriptTypes.h"
#include "MikanStencilTypes.h"
#include "MikanCameraTypes.h"
#include "MikanVideoSourceTypes.h"
#include "MikanVRDeviceTypes.h"

#include "Refureku/Refureku.h"

#include "stdio.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <ostream>
#include <string>
#include <map>
#include <set>
#include <memory>
#include <vector>

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

struct ClientModule
{
	std::string name;
	std::vector<rfk::Struct const*> serializableStructs;
	std::vector<rfk::Enum const*> enums;
};
using ClientModulePtr= std::shared_ptr<ClientModule>;

struct CodeGenDatabase
{
	std::map<std::string, ClientModulePtr> modules;

	void visitStruct(rfk::Struct const& entity)
	{
		ClientModulePtr module= findOrAddModule(entity);

		if (module != nullptr)
		{
			module->serializableStructs.push_back(&entity);
		}
	}

	void visitEnum(rfk::Enum const& entity)
	{
		ClientModulePtr module= findOrAddModule(entity);

		if (module != nullptr)
		{
			module->enums.push_back(&entity);
		}
	}

	bool getEntityModuleName(rfk::Entity const& entity, std::string& outModuleName)
	{
		Serialization::CodeGenModule const* property= entity.getProperty<Serialization::CodeGenModule>();
		if (property != nullptr)
		{
			outModuleName= property->getModuleName();
			return true;
		}

		return false;
	}

	ClientModulePtr findOrAddModule(rfk::Entity const& entity)
	{
		std::string moduleName;

		if (getEntityModuleName(entity, moduleName))
		{
			auto it= modules.find(moduleName);
			if (it != modules.end())
			{
				return it->second;
			}
			else
			{
				ClientModulePtr module= std::make_shared<ClientModule>();
				module->name= moduleName;

				modules.insert({moduleName, module});
				return module;
			}
		}

		return ClientModulePtr();
	}

	// Put every module's entities in a canonical order, once, before anything emits.
	//
	// The reflection database enumerates entities in an order that varies between runs, so
	// anything that walks these vectors as filled writes a different file each time even when
	// no type changed. Sorting here rather than at each emission site means every consumer
	// gets the same order: the per-module emitters, the registration files that walk the
	// modules again, and any generator added later. The modules map is a std::map, so module
	// order is already canonical.
	void sortEntities()
	{
		for (auto const& [moduleName, module] : modules)
		{
			std::sort(module->enums.begin(), module->enums.end(),
					  [](rfk::Enum const* a, rfk::Enum const* b) { return stricmp(a->getName(), b->getName()) < 0; });

			std::sort(module->serializableStructs.begin(), module->serializableStructs.end(),
					  [](rfk::Struct const* a, rfk::Struct const* b)
					  { return stricmp(a->getName(), b->getName()) < 0; });
		}
	}
};

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
				// Store reference to database for TypeScript import lookups
				m_codeGenDatabase= &codeGenDatabase;

				auto absOutputPath= std::filesystem::absolute(m_outputPath);

				// Nuke any previously generated code
				std::filesystem::remove_all(absOutputPath);

				// (Re)create the the output folder
				std::filesystem::create_directories(absOutputPath);

				// Generate code for all the modules we found
				result= generateCodeForModules(absOutputPath, codeGenDatabase) ? 0 : -1;

				m_codeGenDatabase= nullptr;
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

	bool generateCodeForModules(const std::filesystem::path& absOutputPath, CodeGenDatabase const& codeGenDatabase)
	{
		for (auto const& module : codeGenDatabase.modules)
		{
			const std::string& moduleName= module.first;
			ClientModulePtr modulePtr= module.second;

			MIKAN_LOG_INFO("MikanClientCodeGen") << "Generate Code for Module: " << moduleName;
			if (!generateCodeForModule(absOutputPath, modulePtr))
			{
				return false;
			}
		}

		if (m_targetLanguage == TargetLanguage::TypeScript)
		{
			if (!generateTypeScriptSerializationTypesFile(absOutputPath))
			{
				return false;
			}

			if (!generateTypeScriptEnumRegistrationFile(absOutputPath, codeGenDatabase))
			{
				return false;
			}

			if (!generateTypeScriptTypeRegistrationFile(absOutputPath, codeGenDatabase))
			{
				return false;
			}

			if (!generateTypeScriptIndexFile(absOutputPath, codeGenDatabase))
			{
				return false;
			}
		}

		return true;
	}

	bool generateTypeScriptSerializationTypesFile(const std::filesystem::path& absOutputPath)
	{
		std::filesystem::path filePath= absOutputPath / "SerializationTypes.ts";

		try
		{
			std::ofstream file(filePath);
			file << "// This file is auto generated. DO NOT EDIT." << std::endl;
			file << std::endl;
			file << "export type SerializationField = "
				 << "{name: string, type: string, isArray?: boolean, isMap?: boolean, keyType?: string, valueType?: "
					"string};"
				 << std::endl;
			file.close();
		}
		catch (std::exception* e)
		{
			MIKAN_LOG_ERROR("MikanClientCodeGen") << "Failed to write SerializationTypes file: " << filePath;
			return false;
		}

		return true;
	}

	bool generateTypeScriptIndexFile(const std::filesystem::path& absOutputPath, CodeGenDatabase const& codeGenDatabase)
	{
		std::filesystem::path indexFilePath= absOutputPath / "index.ts";

		try
		{
			std::ofstream moduleFile(indexFilePath);
			moduleFile << "// This file is auto generated. DO NO EDIT." << std::endl;
			moduleFile << "// Re-export all generated type modules" << std::endl;
			moduleFile << std::endl;

			for (auto const& module : codeGenDatabase.modules)
			{
				const std::string& moduleName= module.first;
				moduleFile << "export * from './" << moduleName << ".js';" << std::endl;
			}

			// Re-export the generated shared types and registration helpers
			moduleFile << "export * from './SerializationTypes.js';" << std::endl;
			moduleFile << "export * from './EnumRegistration.js';" << std::endl;
			moduleFile << "export * from './TypeRegistration.js';" << std::endl;

			moduleFile.close();
		}
		catch (std::exception* e)
		{
			MIKAN_LOG_ERROR("MikanClientCodeGen") << "Failed to write index file: " << indexFilePath;
			return false;
		}

		return true;
	}

	bool generateTypeScriptEnumRegistrationFile(const std::filesystem::path& absOutputPath,
												CodeGenDatabase const& codeGenDatabase)
	{
		std::filesystem::path filePath= absOutputPath / "EnumRegistration.ts";

		// Build (moduleName -> [enumNames]) map
		std::map<std::string, std::vector<std::string>> moduleToEnums;
		std::vector<std::string> allEnumNames; // ordered for registration calls

		for (auto const& [moduleName, modulePtr] : codeGenDatabase.modules)
		{
			for (rfk::Enum const* enumRef : modulePtr->enums)
			{
				const std::string enumName= enumRef->getName();
				moduleToEnums[moduleName].push_back(enumName);
				allEnumNames.push_back(enumName);
			}
		}

		try
		{
			std::ofstream file(filePath);
			file << "// This file is auto generated. DO NOT EDIT." << std::endl;
			file << std::endl;
			file << "import { EnumRegistry } from '../Serialization/EnumRegistry.js';" << std::endl;

			// One import line per module that has enums
			for (auto const& [moduleName, enumNames] : moduleToEnums)
			{
				file << "import { ";
				for (size_t i= 0; i < enumNames.size(); ++i)
				{
					if (i > 0)
						file << ", ";
					file << enumNames[i];
				}
				file << " } from './" << moduleName << ".js';" << std::endl;
			}

			file << std::endl;
			file << "export function registerAllEnums(): void {" << std::endl;
			for (const std::string& enumName : allEnumNames)
			{
				file << "  EnumRegistry.register('" << enumName << "', " << enumName << ");" << std::endl;
			}
			file << "}" << std::endl;
			file << std::endl;
			file << "// Auto-register when this module is first imported." << std::endl;
			file << "// Because this file is re-exported from types/index.ts -> bindings/index.ts," << std::endl;
			file << "// this call runs automatically the moment any symbol from @mikanxr/client is imported."
				 << std::endl;
			file << "registerAllEnums();" << std::endl;

			file.close();
		}
		catch (std::exception* e)
		{
			MIKAN_LOG_ERROR("MikanClientCodeGen") << "Failed to write EnumRegistration file: " << filePath;
			return false;
		}

		return true;
	}

	bool generateTypeScriptTypeRegistrationFile(const std::filesystem::path& absOutputPath,
												CodeGenDatabase const& codeGenDatabase)
	{
		std::filesystem::path filePath= absOutputPath / "TypeRegistration.ts";

		// Build (moduleName -> [classNames]) map, preserving module iteration order
		std::map<std::string, std::vector<std::string>> moduleToClasses;
		std::vector<std::string> allClassNames; // ordered for registration calls

		for (auto const& [moduleName, modulePtr] : codeGenDatabase.modules)
		{
			for (rfk::Struct const* structRef : modulePtr->serializableStructs)
			{
				const std::string className= structRef->getName();
				moduleToClasses[moduleName].push_back(className);
				allClassNames.push_back(className);
			}
		}

		try
		{
			std::ofstream file(filePath);
			file << "// This file is auto generated. DO NOT EDIT." << std::endl;
			file << std::endl;
			file << "import { TypeRegistry } from '../Serialization/JsonDeserializer.js';" << std::endl;

			// One import line per module that has classes
			for (auto const& [moduleName, classNames] : moduleToClasses)
			{
				file << "import { ";
				for (size_t i= 0; i < classNames.size(); ++i)
				{
					if (i > 0)
						file << ", ";
					file << classNames[i];
				}
				file << " } from './" << moduleName << ".js';" << std::endl;
			}

			file << std::endl;
			file << "export function registerAllTypes(): void {" << std::endl;
			for (const std::string& className : allClassNames)
			{
				file << "  TypeRegistry.register('" << className << "', " << className << ");" << std::endl;
			}
			file << "}" << std::endl;
			file << std::endl;
			file << "// Auto-register when this module is first imported." << std::endl;
			file << "// Because this file is re-exported from types/index.ts -> bindings/index.ts," << std::endl;
			file << "// this call runs automatically the moment any symbol from @mikanxr/client is imported."
				 << std::endl;
			file << "registerAllTypes();" << std::endl;

			file.close();
		}
		catch (std::exception* e)
		{
			MIKAN_LOG_ERROR("MikanClientCodeGen") << "Failed to write TypeRegistration file: " << filePath;
			return false;
		}

		return true;
	}

	bool generateCodeForModule(const std::filesystem::path& absOutputPath, ClientModulePtr const& module)
	{
		if (m_targetLanguage == TargetLanguage::TypeScript)
		{
			return generateTypeScriptCodeForModule(absOutputPath, module);
		}
		else
		{
			return generateCSharpCodeForModule(absOutputPath, module);
		}
	}

	bool generateCSharpCodeForModule(const std::filesystem::path& absOutputPath, ClientModulePtr const& module)
	{
		std::string moduleFileName= module->name + ".cs";
		std::filesystem::path modulePath= absOutputPath / moduleFileName;

		try
		{
			std::ofstream moduleFile(modulePath);
			moduleFile << "// This file is auto generated. DO NO EDIT." << std::endl;
			moduleFile << "using System;" << std::endl;
			moduleFile << "using System.Collections.Generic;" << std::endl;
			moduleFile << std::endl;
			moduleFile << "namespace MikanXR" << std::endl;
			moduleFile << "{" << std::endl;

			emitCSharpModuleEntities(moduleFile, module);

			moduleFile << "}" << std::endl;

			moduleFile.close();
		}
		catch (std::exception* e)
		{
			MIKAN_LOG_ERROR("MikanClientCodeGen") << "Failed to write module file: " << modulePath;
			return false;
		}

		return true;
	}

	bool generateTypeScriptCodeForModule(const std::filesystem::path& absOutputPath, ClientModulePtr const& module)
	{
		std::string moduleFileName= module->name + ".ts";
		std::filesystem::path modulePath= absOutputPath / moduleFileName;

		try
		{
			std::ofstream moduleFile(modulePath);
			moduleFile << "// This file is auto generated. DO NOT EDIT." << std::endl;
			moduleFile << std::endl;

			emitTypeScriptModuleEntities(moduleFile, module);

			moduleFile.close();
		}
		catch (std::exception* e)
		{
			MIKAN_LOG_ERROR("MikanClientCodeGen") << "Failed to write module file: " << modulePath;
			return false;
		}

		return true;
	}

	void emitCSharpModuleEntities(std::ofstream& moduleFile, ClientModulePtr const& module)
	{
		// Both lists arrive in name order from CodeGenDatabase::sortEntities. A C# class can
		// reference one declared later in the same namespace, so name order is all this needs.
		for (rfk::Enum const* enumRef : module->enums)
		{
			emitCSharpEnum(moduleFile, *enumRef);
			moduleFile << std::endl;
		}

		for (rfk::Struct const* structRef : module->serializableStructs)
		{
			emitCSharpSerializableClass(moduleFile, *structRef);
			moduleFile << std::endl;
		}
	}

	void emitTypeScriptModuleEntities(std::ofstream& moduleFile, ClientModulePtr const& module)
	{
		// Collect import dependencies and group them by source module
		std::set<std::string> importedTypes;
		collectTypeScriptImports(module, importedTypes);

		// Build a map of module name -> list of types to import from that module
		std::map<std::string, std::vector<std::string>> moduleImports;

		for (const auto& typeName : importedTypes)
		{
			// Find which module this type belongs to by searching all modules in the database
			std::string sourceModuleName= findModuleForType(typeName);

			if (!sourceModuleName.empty())
			{
				moduleImports[sourceModuleName].push_back(typeName);
			}
		}

		// Emit imports grouped by module
		for (const auto& moduleImport : moduleImports)
		{
			const std::string& sourceModule= moduleImport.first;
			const std::vector<std::string>& types= moduleImport.second;

			moduleFile << "import { ";
			for (size_t i= 0; i < types.size(); ++i)
			{
				if (i > 0)
					moduleFile << ", ";
				moduleFile << types[i];
			}
			moduleFile << " } from './" << sourceModule << ".js';" << std::endl;
		}
		// If this module has structs, import SerializationField from the shared generated file
		if (module->serializableStructs.size() > 0)
		{
			moduleFile << "import type { SerializationField } from './SerializationTypes.js';" << std::endl;
		}

		if (!moduleImports.empty() || module->serializableStructs.size() > 0)
		{
			moduleFile << std::endl;
		}

		// Emit enums, already in name order from CodeGenDatabase::sortEntities
		for (rfk::Enum const* enumRef : module->enums)
		{
			emitTypeScriptEnum(moduleFile, *enumRef);
			moduleFile << std::endl;
		}

		// Emit interfaces. A TypeScript interface cannot extend one declared later in the
		// file, so these are reordered to put every base ahead of its derived types.
		if (module->serializableStructs.size() > 0)
		{
			std::vector<rfk::Struct const*> sortedStructs=
				topologicalSortStructsByInheritance(module->serializableStructs);

			for (rfk::Struct const* structRef : sortedStructs)
			{
				emitTypeScriptInterface(moduleFile, *structRef);
				moduleFile << std::endl;
			}
		}
	}

	// Returns the typeName field name if structRef is (or inherits from) MikanRequest/Response/Event,
	// or an empty string for all other structs.
	static std::string getTypeNameFieldForStruct(rfk::Struct const& structRef)
	{
		auto& mikanRequestClass= MikanRequest::staticGetArchetype();
		auto& mikanResponseClass= MikanResponse::staticGetArchetype();
		auto& mikanEventClass= MikanEvent::staticGetArchetype();

		if (structRef.getId() == mikanRequestClass.getId() || structRef.isSubclassOf(mikanRequestClass))
			return "requestTypeName";
		else if (structRef.getId() == mikanResponseClass.getId() || structRef.isSubclassOf(mikanResponseClass))
			return "responseTypeName";
		else if (structRef.getId() == mikanEventClass.getId() || structRef.isSubclassOf(mikanEventClass))
			return "eventTypeName";

		return "";
	}

	void emitCSharpSerializableClass(std::ofstream& moduleFile, rfk::Struct const& structRef)
	{
		// Get a list of parent struct this struct inherits from
		using ParentList= std::vector<std::string>;
		ParentList parentStructNames;
		structRef.foreachDirectParent(
			[](rfk::ParentStruct const& parentStruct, void* userData) -> bool
			{
				ParentList* parentStructNamesPtr= reinterpret_cast<ParentList*>(userData);
				std::string parentStructName= parentStruct.getArchetype().getName();

				parentStructNamesPtr->push_back(parentStructName);
				return true;
			},
			&parentStructNames);

		// Generate struct inheritance string
		std::string structInheritance;
		if (parentStructNames.size() > 0)
		{
			structInheritance= " : ";
			for (size_t i= 0; i < parentStructNames.size(); ++i)
			{
				if (i > 0)
				{
					structInheritance+= ", ";
				}
				structInheritance+= parentStructNames[i];
			}
		}

		// Start of the struct definition
		const std::string& className= structRef.getName();
		moduleFile << "\tpublic class " << className << structInheritance << std::endl;
		moduleFile << "\t{" << std::endl;

		// For some reason Refureku doesn't return fields in the order they were declared
		// So we extract fields into a vector and sort them by memory offset
		using FieldList= std::vector<rfk::Field const*>;
		FieldList sortedFields;
		structRef.foreachField(
			[](rfk::Field const& field, void* userData) -> bool
			{
				FieldList* sortedFieldsPtr= reinterpret_cast<FieldList*>(userData);
				sortedFieldsPtr->push_back(&field);
				return true;
			},
			&sortedFields);

		std::sort(sortedFields.begin(), sortedFields.end(),
				  [](rfk::Field const* a, rfk::Field const* b) { return a->getMemoryOffset() < b->getMemoryOffset(); });

		// Emit the fields
		for (rfk::Field const* field : sortedFields)
		{
			std::string csharpType= getCSharpType(*field);
			moduleFile << "\t\tpublic " << csharpType << " " << field->getName() << ";" << std::endl;
		}

		// Emit a constructor to set the typeName field if this is a MikanRequest/Response/Event
		std::string typeNameField= getTypeNameFieldForStruct(structRef);
		if (!typeNameField.empty())
		{
			moduleFile << std::endl;
			moduleFile << "\t\tpublic " << className << "()" << std::endl;
			moduleFile << "\t\t{" << std::endl;
			moduleFile << "\t\t\t" << typeNameField << " = \"" << className << "\";" << std::endl;
			moduleFile << "\t\t}" << std::endl;
		}

		// End of the struct definition
		moduleFile << "\t};" << std::endl;
	}

	void emitCSharpEnum(std::ofstream& moduleFile, rfk::Enum const& enumRef)
	{
		moduleFile << "\tpublic enum " << enumRef.getName() << std::endl;
		moduleFile << "\t{" << std::endl;

		enumRef.foreachEnumValue(
			[](rfk::EnumValue const& enumValue, void* userData) -> bool
			{
				std::ofstream* moduleFilePtr= reinterpret_cast<std::ofstream*>(userData);

				// Only emit enum values with a string property
				auto const* property= enumValue.getProperty<Serialization::EnumStringValue>();
				if (property != nullptr)
				{
					const std::string enumValueString= property->getValue();
					const int64_t enumInt64Value= enumValue.getValue();

					(*moduleFilePtr) << "\t\t" << enumValueString << "= " << enumInt64Value << "," << std::endl;
				}

				return true;
			},
			&moduleFile);

		moduleFile << "\t};" << std::endl;
	}

	static std::string getCSharpType(rfk::Field const& field)
	{
		rfk::Type const& fieldType= field.getType();

		return getCSharpType(fieldType);
	}

	static std::string getCSharpType(rfk::Type const& type)
	{
		rfk::Archetype const* archetype= type.getArchetype();
		rfk::EEntityKind fieldArchetypeKind= archetype ? archetype->getKind() : rfk::EEntityKind::Undefined;

		if (type.isPointer())
		{
			// All pointer types are IntPtr in C#
			return "IntPtr";
		}
		else if (type == rfk::getType<std::string>())
		{
			return type.isCArray() ? "string[]" : "string";
		}
		else if (fieldArchetypeKind == rfk::EEntityKind::Class)
		{
			rfk::Class const* classType= rfk::classCast(archetype);
			rfk::EClassKind classKind= classType->getClassKind();

			std::string cppType= classType->getName();

			if (cppType == "String")
			{
				return type.isCArray() ? "string[]" : "string";
			}
			else if (cppType == "PolymorphicObjectPtr")
			{
				return "PolymorphicObject";
			}
			else if (classKind == rfk::EClassKind::TemplateInstantiation)
			{
				const auto* templateClassInstanceType= rfk::classTemplateInstantiationCast(classType);
				std::string templateTypeName= templateClassInstanceType->getClassTemplate().getName();

				if (templateTypeName == "List" && templateClassInstanceType->getTemplateArgumentsCount() == 1)
				{
					auto const& templateArg= static_cast<rfk::TypeTemplateArgument const&>(
						templateClassInstanceType->getTemplateArgumentAt(0));
					rfk::Type const& elementType= templateArg.getType();
					std::string elementTypeString= getCSharpType(elementType);

					return "List<" + elementTypeString + ">";
				}
				else if (templateTypeName == "Map" && templateClassInstanceType->getTemplateArgumentsCount() == 2)
				{
					auto const& templateKeyArg= static_cast<rfk::TypeTemplateArgument const&>(
						templateClassInstanceType->getTemplateArgumentAt(0));
					rfk::Type const& keyType= templateKeyArg.getType();
					std::string keyTypeString= getCSharpType(keyType);

					auto const& templateValueArg= static_cast<rfk::TypeTemplateArgument const&>(
						templateClassInstanceType->getTemplateArgumentAt(1));
					rfk::Type const& valueType= templateValueArg.getType();
					std::string valueTypeString= getCSharpType(valueType);

					return "Dictionary<" + keyTypeString + ", " + valueTypeString + ">";
				}
			}
			else
			{
				return type.isCArray() ? cppType + "[]" : cppType;
			}
		}
		else if (fieldArchetypeKind == rfk::EEntityKind::Struct)
		{
			rfk::Struct const* structType= rfk::structCast(archetype);
			std::string structTypeName= structType->getName();

			return type.isCArray() ? structTypeName + "[]" : structTypeName;
		}
		else if (fieldArchetypeKind == rfk::EEntityKind::Enum)
		{
			rfk::Enum const* enumType= rfk::enumCast(archetype);
			std::string enumTypeName= enumType->getName();

			return type.isCArray() ? enumTypeName + "[]" : enumTypeName;
		}
		else if (fieldArchetypeKind == rfk::EEntityKind::FundamentalArchetype)
		{
			std::string csType;

			if (type == rfk::getType<bool>())
			{
				csType= "bool";
			}
			else if (type == rfk::getType<uint8_t>())
			{
				csType= "byte";
			}
			else if (type == rfk::getType<int8_t>())
			{
				csType= "sbyte";
			}
			else if (type == rfk::getType<uint16_t>())
			{
				csType= "ushort";
			}
			else if (type == rfk::getType<int16_t>())
			{
				csType= "short";
			}
			else if (type == rfk::getType<uint32_t>())
			{
				csType= "uint";
			}
			else if (type == rfk::getType<int32_t>() || type == rfk::getType<int>())
			{
				csType= "int";
			}
			else if (type == rfk::getType<uint64_t>())
			{
				csType= "ulong";
			}
			else if (type == rfk::getType<int64_t>() || type == rfk::getType<long>())
			{
				csType= "long";
			}
			else if (type == rfk::getType<float>())
			{
				csType= "float";
			}
			else if (type == rfk::getType<double>())
			{
				csType= "double";
			}

			if (!csType.empty())
			{
				return type.isCArray() ? csType + "[]" : csType;
			}
			else
			{
				csType= "UNKNOWN_TYPE";
			}
		}

		return "UNKNOWN_TYPE";
	}

	// TypeScript Code Generation Methods
	// Topological sort to order structs so base classes come before derived classes
	// Reorders structs so every base comes before the types deriving from it, which is what a
	// TypeScript interface's extends clause requires of the file it lives in.
	//
	// Two structs with no inheritance relationship keep their relative input order, so this is
	// only as stable as what it is handed. The input comes from CodeGenDatabase::sortEntities
	// in name order, which is what makes the result the same on every run.
	std::vector<rfk::Struct const*> topologicalSortStructsByInheritance(const std::vector<rfk::Struct const*>& structs)
	{
		std::vector<rfk::Struct const*> result;
		std::set<rfk::Struct const*> visited;
		std::set<rfk::Struct const*> visiting;

		// Build a map of struct name to struct pointer for quick lookup
		std::map<std::string, rfk::Struct const*> nameToStruct;
		for (rfk::Struct const* s : structs)
		{
			nameToStruct[s->getName()]= s;
		}

		// Context struct to pass to the foreachDirectParent callback
		struct VisitContext
		{
			std::map<std::string, rfk::Struct const*>* nameToStruct;
			std::function<void(rfk::Struct const*)>* visit;
		};

		// Recursive DFS function
		std::function<void(rfk::Struct const*)> visit= [&](rfk::Struct const* s)
		{
			if (visited.count(s))
				return;
			if (visiting.count(s))
				return; // Cycle detected, skip

			visiting.insert(s);

			// Visit all parent structs first (only those in the same module)
			VisitContext ctx= {&nameToStruct, &visit};
			s->foreachDirectParent(
				[](rfk::ParentStruct const& parentStruct, void* userData) -> bool
				{
					VisitContext* ctx= static_cast<VisitContext*>(userData);
					rfk::Struct const& parentArchetype= parentStruct.getArchetype();
					std::string parentName= parentArchetype.getName();

					// Check if parent is in our list of structs to sort
					auto it= ctx->nameToStruct->find(parentName);
					if (it != ctx->nameToStruct->end())
					{
						(*ctx->visit)(it->second);
					}

					return true;
				},
				&ctx);

			visiting.erase(s);
			visited.insert(s);
			result.push_back(s);
		};

		// Visit all structs
		for (rfk::Struct const* s : structs)
		{
			visit(s);
		}

		return result;
	}

	void collectTypeScriptImports(ClientModulePtr const& module, std::set<std::string>& imports)
	{
		std::string currentModuleName= module->name;

		// Check parent types for structs
		for (rfk::Struct const* structRef : module->serializableStructs)
		{
			// Check parent structs
			auto parentParams= std::make_pair(&imports, &currentModuleName);
			structRef->foreachDirectParent(
				[](rfk::ParentStruct const& parentStruct, void* userData) -> bool
				{
					auto* params= reinterpret_cast<std::pair<std::set<std::string>*, std::string*>*>(userData);
					std::set<std::string>* importsPtr= params->first;
					std::string* currentModulePtr= params->second;

					rfk::Struct const& parentArchetype= parentStruct.getArchetype();
					std::string parentName= parentArchetype.getName();

					// Get the module name of the parent
					std::string parentModuleName;
					Serialization::CodeGenModule const* property=
						parentArchetype.getProperty<Serialization::CodeGenModule>();
					if (property != nullptr)
					{
						parentModuleName= property->getModuleName();

						// Only add import if parent is from a different module
						if (parentModuleName != *currentModulePtr)
						{
							importsPtr->insert(parentName);
						}
					}
					else
					{
						// Parent doesn't have CodeGenModule property - it's probably a manual type like
						// PolymorphicStruct Always add it to imports so findModuleForType can handle it
						importsPtr->insert(parentName);
					}

					return true;
				},
				&parentParams);

			// Check field types
			auto fieldParams= std::make_pair(&imports, &currentModuleName);
			structRef->foreachField(
				[](rfk::Field const& field, void* userData) -> bool
				{
					auto* params= reinterpret_cast<std::pair<std::set<std::string>*, std::string*>*>(userData);
					std::set<std::string>* importsPtr= params->first;
					std::string* currentModulePtr= params->second;

					collectTypeScriptFieldImports(field, *importsPtr, *currentModulePtr);
					return true;
				},
				&fieldParams);
		}
	}

	static void collectTypeScriptFieldImports(rfk::Field const& field, std::set<std::string>& imports,
											  const std::string& currentModule)
	{
		rfk::Type const& fieldType= field.getType();
		collectTypeScriptTypeImports(fieldType, imports, currentModule);
	}

	static void collectTypeScriptTypeImports(rfk::Type const& type, std::set<std::string>& imports,
											 const std::string& currentModule)
	{
		// Skip primitive types and built-in types
		if (type == rfk::getType<std::string>())
		{
			return;
		}

		rfk::Archetype const* archetype= type.getArchetype();
		if (!archetype)
			return;

		rfk::EEntityKind kind= archetype->getKind();
		std::string typeName= archetype->getName();

		// Skip primitive types and built-in types
		if (kind == rfk::EEntityKind::FundamentalArchetype || typeName == "String")
		{
			return;
		}

		// Handle PolymorphicObjectPtr - needs both classes from parent module
		if (kind == rfk::EEntityKind::Class && typeName == "PolymorphicObjectPtr")
		{
			imports.insert("PolymorphicObject");
			imports.insert("PolymorphicStruct");
			return;
		}

		// Check if this type needs an import
		if (kind == rfk::EEntityKind::Struct || kind == rfk::EEntityKind::Enum)
		{
			// Get the module name of this type
			std::string typeModuleName;
			Serialization::CodeGenModule const* property= archetype->getProperty<Serialization::CodeGenModule>();
			if (property != nullptr)
			{
				typeModuleName= property->getModuleName();

				// Only add import if type is from a different module
				if (typeModuleName != currentModule)
				{
					imports.insert(typeName);
				}
			}
		}
		else if (kind == rfk::EEntityKind::Class)
		{
			// Handle template types (List<T>, Map<K,V>)
			rfk::Class const* classType= rfk::classCast(archetype);
			rfk::EClassKind classKind= classType->getClassKind();

			if (classKind == rfk::EClassKind::TemplateInstantiation)
			{
				const auto* templateClassInstanceType= rfk::classTemplateInstantiationCast(classType);
				std::string templateTypeName= templateClassInstanceType->getClassTemplate().getName();

				// Recursively check template arguments (e.g., List<T>, Map<K,V>)
				// For List and Map templates, we know all arguments are type arguments
				if (templateTypeName == "List" || templateTypeName == "Map")
				{
					for (uint8_t i= 0; i < templateClassInstanceType->getTemplateArgumentsCount(); ++i)
					{
						auto const& templateArg= static_cast<rfk::TypeTemplateArgument const&>(
							templateClassInstanceType->getTemplateArgumentAt(i));
						collectTypeScriptTypeImports(templateArg.getType(), imports, currentModule);
					}
				}
			}
		}
	}

	void emitTypeScriptEnum(std::ofstream& moduleFile, rfk::Enum const& enumRef)
	{
		moduleFile << "export enum " << enumRef.getName() << " {" << std::endl;

		bool first= true;
		auto enumParams= std::make_pair(&moduleFile, &first);
		enumRef.foreachEnumValue(
			[](rfk::EnumValue const& enumValue, void* userData) -> bool
			{
				auto* params= reinterpret_cast<std::pair<std::ofstream*, bool*>*>(userData);
				std::ofstream* moduleFilePtr= params->first;
				bool* firstPtr= params->second;

				auto const* property= enumValue.getProperty<Serialization::EnumStringValue>();
				if (property != nullptr)
				{
					if (!*firstPtr)
					{
						(*moduleFilePtr) << "," << std::endl;
					}
					*firstPtr= false;

					const std::string enumValueString= property->getValue();
					const int64_t enumInt64Value= enumValue.getValue();
					(*moduleFilePtr) << "  " << enumValueString << " = " << enumInt64Value;
				}

				return true;
			},
			&enumParams);

		moduleFile << std::endl << "}" << std::endl;
	}

	static std::string getTypeScriptSerializationType(rfk::Type const& type)
	{
		rfk::Archetype const* archetype= type.getArchetype();
		rfk::EEntityKind fieldArchetypeKind= archetype ? archetype->getKind() : rfk::EEntityKind::Undefined;

		if (type.isPointer())
		{
			return "any";
		}
		else if (type == rfk::getType<std::string>())
		{
			return "string";
		}
		else if (fieldArchetypeKind == rfk::EEntityKind::Class)
		{
			rfk::Class const* classType= rfk::classCast(archetype);
			rfk::EClassKind classKind= classType->getClassKind();
			std::string cppType= classType->getName();

			if (cppType == "String")
			{
				return "string";
			}
			else if (cppType == "PolymorphicObjectPtr")
			{
				return "PolymorphicObject";
			}
			else if (classKind == rfk::EClassKind::TemplateInstantiation)
			{
				const auto* templateClassInstanceType= rfk::classTemplateInstantiationCast(classType);
				std::string templateTypeName= templateClassInstanceType->getClassTemplate().getName();

				if (templateTypeName == "List" && templateClassInstanceType->getTemplateArgumentsCount() == 1)
				{
					auto const& templateArg= static_cast<rfk::TypeTemplateArgument const&>(
						templateClassInstanceType->getTemplateArgumentAt(0));
					rfk::Type const& elementType= templateArg.getType();
					return getTypeScriptSerializationType(elementType);
				}
				else if (templateTypeName == "Map" && templateClassInstanceType->getTemplateArgumentsCount() == 2)
				{
					return "Map";
				}
			}
			else
			{
				return cppType;
			}
		}
		else if (fieldArchetypeKind == rfk::EEntityKind::Struct)
		{
			rfk::Struct const* structType= rfk::structCast(archetype);
			return structType->getName();
		}
		else if (fieldArchetypeKind == rfk::EEntityKind::Enum)
		{
			rfk::Enum const* enumType= rfk::enumCast(archetype);
			return "enum:" + std::string(enumType->getName());
		}
		else if (fieldArchetypeKind == rfk::EEntityKind::FundamentalArchetype)
		{
			if (type == rfk::getType<bool>())
			{
				return "boolean";
			}
			else if (type == rfk::getType<uint8_t>())
			{
				return "uint8";
			}
			else if (type == rfk::getType<int8_t>())
			{
				return "int8";
			}
			else if (type == rfk::getType<uint16_t>())
			{
				return "uint16";
			}
			else if (type == rfk::getType<int16_t>())
			{
				return "int16";
			}
			else if (type == rfk::getType<uint32_t>())
			{
				return "uint32";
			}
			else if (type == rfk::getType<int32_t>() || type == rfk::getType<int>())
			{
				return "int32";
			}
			else if (type == rfk::getType<uint64_t>())
			{
				return "uint64";
			}
			else if (type == rfk::getType<int64_t>() || type == rfk::getType<long>())
			{
				return "int64";
			}
			else if (type == rfk::getType<float>())
			{
				return "float";
			}
			else if (type == rfk::getType<double>())
			{
				return "double";
			}
		}

		return "any";
	}

	// Holds a live default-constructed instance of a struct so we can read actual field default values.
	// One of the holders keeps the shared_ptr alive; rawPtr points into the same allocation.
	struct AllocatedInstance
	{
		void* rawPtr= nullptr;
		std::shared_ptr<MikanEvent> eventHolder;
		std::shared_ptr<MikanRequest> requestHolder;
		std::shared_ptr<MikanResponse> responseHolder;
		std::shared_ptr<Serialization::PolymorphicStruct> polymorphicHolder;

		bool isValid() const { return rawPtr != nullptr; }
	};

	// Try to allocate a default-constructed instance of structRef via rfk instantiators.
	// Returns an invalid AllocatedInstance if no known base class is found or allocation fails.
	static AllocatedInstance tryAllocateStructInstance(rfk::Struct const& structRef)
	{
		AllocatedInstance result;

		auto& mikanEventClass= MikanEvent::staticGetArchetype();
		auto& mikanRequestClass= MikanRequest::staticGetArchetype();
		auto& mikanResponseClass= MikanResponse::staticGetArchetype();
		auto& polymorphicStructClass= Serialization::PolymorphicStruct::staticGetArchetype();

		if (structRef.getId() == mikanEventClass.getId() || structRef.isSubclassOf(mikanEventClass))
		{
			result.eventHolder= structRef.makeSharedInstance<MikanEvent>();
			result.rawPtr= result.eventHolder.get();
		}
		else if (structRef.getId() == mikanRequestClass.getId() || structRef.isSubclassOf(mikanRequestClass))
		{
			result.requestHolder= structRef.makeSharedInstance<MikanRequest>();
			result.rawPtr= result.requestHolder.get();
		}
		else if (structRef.getId() == mikanResponseClass.getId() || structRef.isSubclassOf(mikanResponseClass))
		{
			result.responseHolder= structRef.makeSharedInstance<MikanResponse>();
			result.rawPtr= result.responseHolder.get();
		}
		else if (structRef.getId() == polymorphicStructClass.getId() || structRef.isSubclassOf(polymorphicStructClass))
		{
			result.polymorphicHolder= structRef.makeSharedInstance<Serialization::PolymorphicStruct>();
			result.rawPtr= result.polymorphicHolder.get();
		}

		return result;
	}

	// Read the actual default value of a field from a live instance and emit it as a TypeScript literal.
	// Falls back to getTypeScriptDefaultValue() for complex/unhandled types.
	static std::string getTypeScriptDefaultValueFromInstance(rfk::Type const& type, void const* fieldPtr)
	{
		rfk::Archetype const* archetype= type.getArchetype();
		rfk::EEntityKind kind= archetype ? archetype->getKind() : rfk::EEntityKind::Undefined;

		// Arrays and pointers are not representable as simple literals — fall back.
		if (type.isPointer() || type.isCArray())
		{
			return getTypeScriptDefaultValue(type);
		}
		else if (kind == rfk::EEntityKind::FundamentalArchetype)
		{
			if (type == rfk::getType<bool>())
			{
				return *reinterpret_cast<bool const*>(fieldPtr) ? "true" : "false";
			}
			else if (type == rfk::getType<int64_t>())
			{
				return std::to_string(*reinterpret_cast<int64_t const*>(fieldPtr)) + "n";
			}
			else if (type == rfk::getType<uint64_t>())
			{
				return std::to_string(*reinterpret_cast<uint64_t const*>(fieldPtr)) + "n";
			}
			else if (type == rfk::getType<float>())
			{
				float v= *reinterpret_cast<float const*>(fieldPtr);
				if (!std::isfinite(v))
					return getTypeScriptDefaultValue(type);
				char buf[64];
				snprintf(buf, sizeof(buf), "%.9g", (double)v);
				return buf;
			}
			else if (type == rfk::getType<double>())
			{
				double v= *reinterpret_cast<double const*>(fieldPtr);
				if (!std::isfinite(v))
					return getTypeScriptDefaultValue(type);
				char buf[64];
				snprintf(buf, sizeof(buf), "%.17g", v);
				return buf;
			}
			else if (type == rfk::getType<uint8_t>())
			{
				return std::to_string((unsigned)*reinterpret_cast<uint8_t const*>(fieldPtr));
			}
			else if (type == rfk::getType<int8_t>())
			{
				return std::to_string((int)*reinterpret_cast<int8_t const*>(fieldPtr));
			}
			else if (type == rfk::getType<uint16_t>())
			{
				return std::to_string((unsigned)*reinterpret_cast<uint16_t const*>(fieldPtr));
			}
			else if (type == rfk::getType<int16_t>())
			{
				return std::to_string((int)*reinterpret_cast<int16_t const*>(fieldPtr));
			}
			else if (type == rfk::getType<uint32_t>())
			{
				return std::to_string(*reinterpret_cast<uint32_t const*>(fieldPtr));
			}
			else if (type == rfk::getType<int32_t>())
			{
				return std::to_string(*reinterpret_cast<int32_t const*>(fieldPtr));
			}
		}
		else if (kind == rfk::EEntityKind::Enum)
		{
			rfk::Enum const* enumType= rfk::enumCast(archetype);
			rfk::Archetype const& underlyingArchetype= enumType->getUnderlyingArchetype();

			int64_t enumIntValue= 0;
			if (underlyingArchetype.getMemorySize() == sizeof(int64_t))
				enumIntValue= *reinterpret_cast<int64_t const*>(fieldPtr);
			else if (underlyingArchetype.getMemorySize() == sizeof(int32_t))
				enumIntValue= (int64_t)*reinterpret_cast<int32_t const*>(fieldPtr);
			else if (underlyingArchetype.getMemorySize() == sizeof(int16_t))
				enumIntValue= (int64_t)*reinterpret_cast<int16_t const*>(fieldPtr);
			else if (underlyingArchetype.getMemorySize() == sizeof(int8_t))
				enumIntValue= (int64_t)*reinterpret_cast<int8_t const*>(fieldPtr);

			rfk::EnumValue const* enumValue= enumType->getEnumValue(enumIntValue);
			if (enumValue != nullptr)
			{
				auto const* property= enumValue->getProperty<Serialization::EnumStringValue>();
				std::string valueName= property ? std::string(property->getValue()) : std::string(enumValue->getName());
				return std::string(enumType->getName()) + "." + valueName;
			}
		}
		else if (type == rfk::getType<std::string>())
		{
			std::string const* strPtr= reinterpret_cast<std::string const*>(fieldPtr);
			return "'" + *strPtr + "'";
		}

		// Fall back to type-based defaults for structs, classes, arrays, maps, etc.
		return getTypeScriptDefaultValue(type);
	}

	void emitTypeScriptInterface(std::ofstream& moduleFile, rfk::Struct const& structRef)
	{
		// Get parent classes
		using ParentList= std::vector<std::string>;
		ParentList parentStructNames;
		structRef.foreachDirectParent(
			[](rfk::ParentStruct const& parentStruct, void* userData) -> bool
			{
				ParentList* parentStructNamesPtr= reinterpret_cast<ParentList*>(userData);
				std::string parentStructName= parentStruct.getArchetype().getName();
				parentStructNamesPtr->push_back(parentStructName);
				return true;
			},
			&parentStructNames);

		// Generate class inheritance string
		std::string classInheritance;
		if (parentStructNames.size() > 0)
		{
			classInheritance= " extends ";
			for (size_t i= 0; i < parentStructNames.size(); ++i)
			{
				if (i > 0)
				{
					classInheritance+= ", ";
				}
				classInheritance+= parentStructNames[i];
			}
		}

		const std::string& className= structRef.getName();
		moduleFile << "export class " << className << classInheritance << " {" << std::endl;

		// Sort fields by memory offset
		using FieldList= std::vector<rfk::Field const*>;
		FieldList sortedFields;
		structRef.foreachField(
			[](rfk::Field const& field, void* userData) -> bool
			{
				FieldList* sortedFieldsPtr= reinterpret_cast<FieldList*>(userData);
				sortedFieldsPtr->push_back(&field);
				return true;
			},
			&sortedFields);

		std::sort(sortedFields.begin(), sortedFields.end(),
				  [](rfk::Field const* a, rfk::Field const* b) { return a->getMemoryOffset() < b->getMemoryOffset(); });

		// Allocate a default-constructed instance to read actual field defaults from.
		// Falls back to type-based defaults if no known base class is available.
		AllocatedInstance instance= tryAllocateStructInstance(structRef);

		// Emit fields with default initialization
		for (rfk::Field const* field : sortedFields)
		{
			std::string tsType= getTypeScriptType(*field);
			std::string tsDefaultValue;
			if (instance.isValid())
			{
				void const* fieldPtr= field->getConstPtrUnsafe(instance.rawPtr);
				tsDefaultValue= getTypeScriptDefaultValueFromInstance(field->getType(), fieldPtr);
			}
			else
			{
				tsDefaultValue= getTypeScriptDefaultValue(field->getType());
			}
			moduleFile << "  " << field->getName() << ": " << tsType << " = " << tsDefaultValue << ";" << std::endl;
		}

		// Emit a constructor to set the typeName field if this is a MikanRequest/Response/Event
		std::string typeNameField= getTypeNameFieldForStruct(structRef);
		if (!typeNameField.empty())
		{
			moduleFile << std::endl;
			moduleFile << "  constructor() {" << std::endl;
			if (parentStructNames.size() > 0)
			{
				moduleFile << "    super();" << std::endl;
			}
			moduleFile << "    this." << typeNameField << " = '" << className << "';" << std::endl;
			moduleFile << "  }" << std::endl;
		}

		// Emit serialization metadata
		moduleFile << std::endl;
		moduleFile << "  static __serializationMetadata: SerializationField[] = [" << std::endl;

		for (size_t i= 0; i < sortedFields.size(); ++i)
		{
			rfk::Field const* field= sortedFields[i];
			rfk::Type const& fieldType= field->getType();
			std::string serializationType= getTypeScriptSerializationType(fieldType);

			moduleFile << "    { name: '" << field->getName() << "', type: '" << serializationType << "'";

			// Check if it's an array
			if (fieldType.getArchetype() && fieldType.getArchetype()->getKind() == rfk::EEntityKind::Class)
			{
				rfk::Class const* classType= rfk::classCast(fieldType.getArchetype());
				if (classType->getClassKind() == rfk::EClassKind::TemplateInstantiation)
				{
					const auto* templateClassInstanceType= rfk::classTemplateInstantiationCast(classType);
					std::string templateTypeName= templateClassInstanceType->getClassTemplate().getName();

					if (templateTypeName == "List")
					{
						moduleFile << ", isArray: true";
					}
					else if (templateTypeName == "Map" && templateClassInstanceType->getTemplateArgumentsCount() == 2)
					{
						// Get key and value types
						auto const& keyArg= static_cast<rfk::TypeTemplateArgument const&>(
							templateClassInstanceType->getTemplateArgumentAt(0));
						auto const& valueArg= static_cast<rfk::TypeTemplateArgument const&>(
							templateClassInstanceType->getTemplateArgumentAt(1));

						std::string keyType= getTypeScriptSerializationType(keyArg.getType());
						std::string valueType= getTypeScriptSerializationType(valueArg.getType());

						moduleFile << ", isMap: true, keyType: '" << keyType << "', valueType: '" << valueType << "'";
					}
				}
			}
			else if (fieldType.isCArray())
			{
				moduleFile << ", isArray: true";
			}

			moduleFile << " }";
			if (i < sortedFields.size() - 1)
			{
				moduleFile << ",";
			}
			moduleFile << std::endl;
		}

		moduleFile << "  ];" << std::endl;
		moduleFile << "}" << std::endl;
	}

	static std::string getTypeScriptDefaultValue(rfk::Type const& type)
	{
		rfk::Archetype const* archetype= type.getArchetype();
		rfk::EEntityKind kind= archetype ? archetype->getKind() : rfk::EEntityKind::Undefined;

		if (type.isPointer())
		{
			return "null";
		}
		else if (type == rfk::getType<std::string>())
		{
			return type.isCArray() ? "[]" : "''";
		}
		else if (kind == rfk::EEntityKind::Class)
		{
			rfk::Class const* classType= rfk::classCast(archetype);
			std::string cppType= classType->getName();

			if (cppType == "String")
			{
				return type.isCArray() ? "[]" : "''";
			}
			else if (cppType == "PolymorphicObjectPtr")
			{
				return "new PolymorphicObject()";
			}
			else if (classType->getClassKind() == rfk::EClassKind::TemplateInstantiation)
			{
				const auto* templateClassInstanceType= rfk::classTemplateInstantiationCast(classType);
				std::string templateTypeName= templateClassInstanceType->getClassTemplate().getName();

				if (templateTypeName == "List")
				{
					return "[]";
				}
				else if (templateTypeName == "Map")
				{
					return "{}";
				}
			}
			else
			{
				return "new " + cppType + "()";
			}
		}
		else if (kind == rfk::EEntityKind::Struct)
		{
			rfk::Struct const* structType= rfk::structCast(archetype);
			std::string structTypeName= structType->getName();
			return type.isCArray() ? "[]" : "new " + structTypeName + "()";
		}
		else if (kind == rfk::EEntityKind::Enum)
		{
			if (type.isCArray())
			{
				return "[]";
			}
			else
			{
				// Get the first enum value as the default
				rfk::Enum const* enumType= rfk::enumCast(archetype);
				if (enumType && enumType->getEnumValuesCount() > 0)
				{
					std::string enumName= enumType->getName();

					const rfk::EnumValue& enumValue= enumType->getEnumValueAt(0);
					auto const* property= enumValue.getProperty<Serialization::EnumStringValue>();
					std::string firstValueName;
					if (property != nullptr)
					{
						firstValueName= property->getValue();
					}
					else
					{
						firstValueName= enumType->getEnumValueAt(0).getName();
					}

					return enumName + "." + firstValueName;
				}

				return "null as any";
			}
		}
		else if (kind == rfk::EEntityKind::FundamentalArchetype)
		{
			if (type == rfk::getType<bool>())
			{
				return type.isCArray() ? "[]" : "false";
			}
			else if (type == rfk::getType<uint64_t>() || type == rfk::getType<int64_t>())
			{
				return type.isCArray() ? "[]" : "0n";
			}
			else if (type == rfk::getType<uint8_t>() || type == rfk::getType<int8_t>()
					 || type == rfk::getType<uint16_t>() || type == rfk::getType<int16_t>()
					 || type == rfk::getType<uint32_t>() || type == rfk::getType<int32_t>()
					 || type == rfk::getType<float>() || type == rfk::getType<double>())
			{
				return type.isCArray() ? "[]" : "0";
			}
		}

		return "null";
	}

	static std::string getTypeScriptType(rfk::Field const& field)
	{
		rfk::Type const& fieldType= field.getType();
		return getTypeScriptType(fieldType);
	}

	static std::string getTypeScriptType(rfk::Type const& type)
	{
		rfk::Archetype const* archetype= type.getArchetype();
		rfk::EEntityKind fieldArchetypeKind= archetype ? archetype->getKind() : rfk::EEntityKind::Undefined;

		if (type.isPointer())
		{
			return "any"; // Pointers become any in TypeScript
		}
		else if (type == rfk::getType<std::string>())
		{
			return type.isCArray() ? "string[]" : "string";
		}
		else if (fieldArchetypeKind == rfk::EEntityKind::Class)
		{
			rfk::Class const* classType= rfk::classCast(archetype);
			rfk::EClassKind classKind= classType->getClassKind();
			std::string cppType= classType->getName();

			if (cppType == "String")
			{
				return type.isCArray() ? "string[]" : "string";
			}
			else if (cppType == "PolymorphicObjectPtr")
			{
				return "PolymorphicObject";
			}
			else if (classKind == rfk::EClassKind::TemplateInstantiation)
			{
				const auto* templateClassInstanceType= rfk::classTemplateInstantiationCast(classType);
				std::string templateTypeName= templateClassInstanceType->getClassTemplate().getName();

				if (templateTypeName == "List" && templateClassInstanceType->getTemplateArgumentsCount() == 1)
				{
					auto const& templateArg= static_cast<rfk::TypeTemplateArgument const&>(
						templateClassInstanceType->getTemplateArgumentAt(0));
					rfk::Type const& elementType= templateArg.getType();
					std::string elementTypeString= getTypeScriptType(elementType);

					return elementTypeString + "[]";
				}
				else if (templateTypeName == "Map" && templateClassInstanceType->getTemplateArgumentsCount() == 2)
				{
					auto const& templateKeyArg= static_cast<rfk::TypeTemplateArgument const&>(
						templateClassInstanceType->getTemplateArgumentAt(0));
					rfk::Type const& keyType= templateKeyArg.getType();
					std::string keyTypeString= getTypeScriptType(keyType);

					auto const& templateValueArg= static_cast<rfk::TypeTemplateArgument const&>(
						templateClassInstanceType->getTemplateArgumentAt(1));
					rfk::Type const& valueType= templateValueArg.getType();
					std::string valueTypeString= getTypeScriptType(valueType);

					return "Record<" + keyTypeString + ", " + valueTypeString + ">";
				}
			}
			else
			{
				return type.isCArray() ? cppType + "[]" : cppType;
			}
		}
		else if (fieldArchetypeKind == rfk::EEntityKind::Struct)
		{
			rfk::Struct const* structType= rfk::structCast(archetype);
			std::string structTypeName= structType->getName();

			return type.isCArray() ? structTypeName + "[]" : structTypeName;
		}
		else if (fieldArchetypeKind == rfk::EEntityKind::Enum)
		{
			rfk::Enum const* enumType= rfk::enumCast(archetype);
			std::string enumTypeName= enumType->getName();

			return type.isCArray() ? enumTypeName + "[]" : enumTypeName;
		}
		else if (fieldArchetypeKind == rfk::EEntityKind::FundamentalArchetype)
		{
			std::string tsType;

			if (type == rfk::getType<bool>())
			{
				tsType= "boolean";
			}
			else if (type == rfk::getType<uint8_t>() || type == rfk::getType<int8_t>()
					 || type == rfk::getType<uint16_t>() || type == rfk::getType<int16_t>()
					 || type == rfk::getType<uint32_t>() || type == rfk::getType<int32_t>()
					 || type == rfk::getType<float>() || type == rfk::getType<double>())
			{
				tsType= "number";
			}
			else if (type == rfk::getType<uint64_t>() || type == rfk::getType<int64_t>())
			{
				tsType= "bigint";
			}

			if (!tsType.empty())
			{
				return type.isCArray() ? tsType + "[]" : tsType;
			}
			else
			{
				tsType= "any";
			}
		}

		return "any";
	}

	// Find which module a type belongs to
	std::string findModuleForType(const std::string& typeName)
	{
		// Special case: PolymorphicObject and PolymorphicStruct are imported from parent module
		if (typeName == "PolymorphicObject" || typeName == "PolymorphicStruct")
		{
			return "../PolymorphicObject";
		}

		if (!m_codeGenDatabase)
			return "";

		// Search through all modules to find which one contains this type
		for (const auto& modulePair : m_codeGenDatabase->modules)
		{
			const ClientModulePtr& module= modulePair.second;

			// Check enums
			for (const rfk::Enum* enumPtr : module->enums)
			{
				if (enumPtr->getName() == typeName)
				{
					return module->name;
				}
			}

			// Check structs
			for (const rfk::Struct* structPtr : module->serializableStructs)
			{
				if (structPtr->getName() == typeName)
				{
					return module->name;
				}
			}
		}

		return "";
	}

private:
	std::filesystem::path m_outputPath;
	TargetLanguage m_targetLanguage= TargetLanguage::CSharp;
	CodeGenDatabase* m_codeGenDatabase= nullptr;
};

//-- entry point -----
int main(int argc, char* argv[])
{
	IMikanAPIPtr apiPtr= IMikanAPI::createMikanAPI();
	MikanClientCodeGen app;

	return app.exec(argc, argv);
}
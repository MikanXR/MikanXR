#include "MikanClientLanguageGen.h"

#include "Logger.h"

#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

// The TypeScript side of the client bindings. A TypeScript interface cannot extend one declared
// later in the same file, so this generator orders a module's structs by inheritance, and it
// writes four files that describe the whole output rather than any one module.
class MikanClientTypeScriptGen : public MikanClientLanguageGen
{
public:
	MikanClientTypeScriptGen(ClientCodeGenContext const& context)
		: MikanClientLanguageGen(context)
	{
	}

	virtual bool generateModuleFile(ClientModulePtr const& module) override
	{
		std::string moduleFileName= module->name + ".ts";
		std::filesystem::path modulePath= m_context.outputPath / moduleFileName;

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

	// SerializationTypes.ts carries the field descriptor every generated interface refers to,
	// the two registration files wire the generated types into the client runtime's registries,
	// and index.ts is the barrel the package exports.
	virtual bool generateWholeOutputFiles() override
	{
		return generateSerializationTypesFile() && generateEnumRegistrationFile() && generateTypeRegistrationFile()
			   && generateIndexFile();
	}

	bool generateSerializationTypesFile()
	{
		std::filesystem::path filePath= m_context.outputPath / "SerializationTypes.ts";

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

	bool generateIndexFile()
	{
		std::filesystem::path indexFilePath= m_context.outputPath / "index.ts";

		try
		{
			std::ofstream moduleFile(indexFilePath);
			moduleFile << "// This file is auto generated. DO NO EDIT." << std::endl;
			moduleFile << "// Re-export all generated type modules" << std::endl;
			moduleFile << std::endl;

			for (auto const& module : m_context.database->modules)
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

	bool generateEnumRegistrationFile()
	{
		std::filesystem::path filePath= m_context.outputPath / "EnumRegistration.ts";

		// Build (moduleName -> [enumNames]) map
		std::map<std::string, std::vector<std::string>> moduleToEnums;
		std::vector<std::string> allEnumNames; // ordered for registration calls

		for (auto const& [moduleName, modulePtr] : m_context.database->modules)
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

	bool generateTypeRegistrationFile()
	{
		std::filesystem::path filePath= m_context.outputPath / "TypeRegistration.ts";

		// Build (moduleName -> [classNames]) map, preserving module iteration order
		std::map<std::string, std::vector<std::string>> moduleToClasses;
		std::vector<std::string> allClassNames; // ordered for registration calls

		for (auto const& [moduleName, modulePtr] : m_context.database->modules)
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
			std::vector<rfk::Struct const*> sortedStructs= sortStructsByInheritance(module->serializableStructs);

			for (rfk::Struct const* structRef : sortedStructs)
			{
				emitTypeScriptInterface(moduleFile, *structRef);
				moduleFile << std::endl;
			}
		}
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
};

MikanClientLanguageGenPtr createTypeScriptClientGen(ClientCodeGenContext const& context)
{
	return std::make_unique<MikanClientTypeScriptGen>(context);
}

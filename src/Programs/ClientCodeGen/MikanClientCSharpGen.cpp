#include "MikanClientLanguageGen.h"

#include "Logger.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <string>
#include <vector>

// The C# side of the client bindings. A C# class may reference another declared later in the
// same namespace, so entities go out in the name order CodeGenDatabase established.
//
// Each class reads and writes its own fields rather than leaving that to reflection, so the
// runtime compiles into hosts that forbid System.Reflection. The one whole-output file is the
// type registry, which creates an event, response or polymorphic value from its wire type name.
class MikanClientCSharpGen : public MikanClientLanguageGen
{
public:
	MikanClientCSharpGen(ClientCodeGenContext const& context)
		: MikanClientLanguageGen(context)
	{
	}

	virtual bool generateModuleFile(ClientModulePtr const& module) override
	{
		std::string moduleFileName= module->name + ".cs";
		std::filesystem::path modulePath= m_context.outputPath / moduleFileName;
		bool success= true;

		try
		{
			std::ofstream moduleFile(modulePath);
			moduleFile << "// This file is auto generated. DO NO EDIT." << std::endl;
			moduleFile << "using Newtonsoft.Json.Linq;" << std::endl;
			moduleFile << "using System;" << std::endl;
			moduleFile << "using System.Collections.Generic;" << std::endl;
			moduleFile << std::endl;
			moduleFile << "namespace MikanXR" << std::endl;
			moduleFile << "{" << std::endl;

			success= emitCSharpModuleEntities(moduleFile, module);

			moduleFile << "}" << std::endl;

			moduleFile.close();
		}
		catch (std::exception* e)
		{
			MIKAN_LOG_ERROR("MikanClientCodeGen") << "Failed to write module file: " << modulePath;
			return false;
		}

		return success;
	}

	virtual bool generateWholeOutputFiles() override { return generateTypeRegistryFile(); }

private:
	bool generateTypeRegistryFile()
	{
		std::vector<std::string> classNames;
		for (auto const& [moduleName, module] : m_context.database->modules)
		{
			for (rfk::Struct const* structRef : module->serializableStructs)
			{
				classNames.push_back(structRef->getName());
			}
		}
		std::sort(classNames.begin(), classNames.end(),
				  [](std::string const& a, std::string const& b) { return stricmp(a.c_str(), b.c_str()) < 0; });

		std::filesystem::path registryPath= m_context.outputPath / "MikanTypeRegistry.cs";

		try
		{
			std::ofstream registryFile(registryPath);
			registryFile << "// This file is auto generated. DO NO EDIT." << std::endl;
			registryFile << std::endl;
			registryFile << "namespace MikanXR" << std::endl;
			registryFile << "{" << std::endl;
			registryFile << "\tpublic static partial class MikanTypeRegistry" << std::endl;
			registryFile << "\t{" << std::endl;
			registryFile << "\t\tstatic partial void RegisterGeneratedTypes()" << std::endl;
			registryFile << "\t\t{" << std::endl;
			for (std::string const& className : classNames)
			{
				registryFile << "\t\t\tRegister(\"" << className << "\", () => new " << className << "());"
							 << std::endl;
			}
			registryFile << "\t\t}" << std::endl;
			registryFile << "\t}" << std::endl;
			registryFile << "}" << std::endl;

			registryFile.close();
		}
		catch (std::exception* e)
		{
			MIKAN_LOG_ERROR("MikanClientCodeGen") << "Failed to write type registry file: " << registryPath;
			return false;
		}

		return true;
	}

	bool emitCSharpModuleEntities(std::ofstream& moduleFile, ClientModulePtr const& module)
	{
		bool success= true;

		// Both lists arrive in name order from CodeGenDatabase::sortEntities. A C# class can
		// reference one declared later in the same namespace, so name order is all this needs.
		for (rfk::Enum const* enumRef : module->enums)
		{
			emitCSharpEnum(moduleFile, *enumRef);
			moduleFile << std::endl;
		}

		for (rfk::Struct const* structRef : module->serializableStructs)
		{
			success&= emitCSharpSerializableClass(moduleFile, *structRef);
			moduleFile << std::endl;
		}

		return success;
	}

	bool emitCSharpSerializableClass(std::ofstream& moduleFile, rfk::Struct const& structRef)
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

		// Generate struct inheritance string. A struct with no parent derives from the runtime's
		// serializable root, so every class overrides the same field members.
		std::string structInheritance= " : ";
		if (parentStructNames.size() > 0)
		{
			for (size_t i= 0; i < parentStructNames.size(); ++i)
			{
				if (i > 0)
				{
					structInheritance+= ", ";
				}
				structInheritance+= parentStructNames[i];
			}
		}
		else
		{
			structInheritance+= "MikanSerializableObject";
		}

		// Start of the struct definition
		const std::string& className= structRef.getName();
		moduleFile << "\tpublic class " << className << structInheritance << std::endl;
		moduleFile << "\t{" << std::endl;

		// The names the property requests address this struct's component class or object
		// system by, so a client never spells them
		if (auto const* names= structRef.getProperty<Serialization::ClientComponentNames>())
		{
			moduleFile << "\t\tpublic const string k_componentClassName = \"" << names->getComponentClassName() << "\";"
					   << std::endl;
			moduleFile << "\t\tpublic const string k_ownerSystemName = \"" << names->getOwnerSystemName() << "\";"
					   << std::endl;
			moduleFile << std::endl;
		}
		if (auto const* systemName= structRef.getProperty<Serialization::ClientSystemName>())
		{
			moduleFile << "\t\tpublic const string k_systemName = \"" << systemName->getSystemName() << "\";"
					   << std::endl;
			moduleFile << std::endl;
		}

		// The order these go out in is the wire order, so it has to be the serializer's. The
		// generated WriteFields and ReadFields walk them in this order too.
		const Serialization::FieldList sortedFields= Serialization::getStructFieldsInWireOrder(&structRef);
		for (rfk::Field const* field : sortedFields)
		{
			moduleFile << "\t\tpublic " << getCSharpType(*field) << " " << field->getName() << ";" << std::endl;
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

		// A polymorphic value names itself for the wire's class_name
		auto& polymorphicStructClass= Serialization::PolymorphicStruct::staticGetArchetype();
		if (structRef.isSubclassOf(polymorphicStructClass))
		{
			moduleFile << std::endl;
			moduleFile << "\t\tpublic override string RuntimeClassName => \"" << className << "\";" << std::endl;
		}

		bool success= true;
		if (!sortedFields.empty())
		{
			success= emitCSharpFieldMembers(moduleFile, className, sortedFields);
		}

		// End of the struct definition
		moduleFile << "\t};" << std::endl;

		return success;
	}

	// WriteFields, ReadFields, TryGetField and TrySetField over this struct's own fields. Each
	// calls its base first, so parent fields come first. Fields are named through this, since a
	// wire field may share its name with a member parameter (the variant wrappers' value).
	bool emitCSharpFieldMembers(std::ofstream& moduleFile, std::string const& className,
								Serialization::FieldList const& sortedFields)
	{
		bool success= true;

		moduleFile << std::endl;
		moduleFile << "\t\tpublic override void WriteFields(JObject json)" << std::endl;
		moduleFile << "\t\t{" << std::endl;
		moduleFile << "\t\t\tbase.WriteFields(json);" << std::endl;
		for (rfk::Field const* field : sortedFields)
		{
			std::string writeExpr;
			if (buildWriteExpr(field->getType(), "this." + std::string(field->getName()), 0, writeExpr))
			{
				moduleFile << "\t\t\tjson.Add(\"" << field->getName() << "\", " << writeExpr << ");" << std::endl;
			}
			else
			{
				MIKAN_LOG_ERROR("MikanClientCodeGen") << "Field " << className << "::" << field->getName()
													  << " has a type the C# serializer cannot write";
				success= false;
			}
		}
		moduleFile << "\t\t}" << std::endl;

		moduleFile << std::endl;
		moduleFile << "\t\tpublic override void ReadFields(JObject json)" << std::endl;
		moduleFile << "\t\t{" << std::endl;
		moduleFile << "\t\t\tbase.ReadFields(json);" << std::endl;
		for (rfk::Field const* field : sortedFields)
		{
			std::string fieldName= field->getName();
			std::string tokenExpr= "MikanJson.Field(json, \"" + fieldName + "\")";
			std::string readExpr;
			if (buildReadExpr(field->getType(), tokenExpr, "this." + fieldName, 0, readExpr))
			{
				moduleFile << "\t\t\tthis." << fieldName << " = " << readExpr << ";" << std::endl;
			}
			else
			{
				MIKAN_LOG_ERROR("MikanClientCodeGen")
					<< "Field " << className << "::" << fieldName << " has a type the C# serializer cannot read";
				success= false;
			}
		}
		moduleFile << "\t\t}" << std::endl;

		moduleFile << std::endl;
		moduleFile << "\t\tpublic override bool TryGetField(string fieldName, out object value)" << std::endl;
		moduleFile << "\t\t{" << std::endl;
		moduleFile << "\t\t\tswitch (fieldName)" << std::endl;
		moduleFile << "\t\t\t{" << std::endl;
		for (rfk::Field const* field : sortedFields)
		{
			moduleFile << "\t\t\t\tcase \"" << field->getName() << "\": value = this." << field->getName()
					   << "; return true;" << std::endl;
		}
		moduleFile << "\t\t\t}" << std::endl;
		moduleFile << "\t\t\treturn base.TryGetField(fieldName, out value);" << std::endl;
		moduleFile << "\t\t}" << std::endl;

		moduleFile << std::endl;
		moduleFile << "\t\tpublic override bool TrySetField(string fieldName, object value)" << std::endl;
		moduleFile << "\t\t{" << std::endl;
		moduleFile << "\t\t\tswitch (fieldName)" << std::endl;
		moduleFile << "\t\t\t{" << std::endl;
		for (rfk::Field const* field : sortedFields)
		{
			moduleFile << "\t\t\t\tcase \"" << field->getName() << "\": return MikanJson.TryAssign(value, ref this."
					   << field->getName() << ");" << std::endl;
		}
		moduleFile << "\t\t\t}" << std::endl;
		moduleFile << "\t\t\treturn base.TrySetField(fieldName, value);" << std::endl;
		moduleFile << "\t\t}" << std::endl;

		return success;
	}

	// The kinds of value the wire carries, as the serializer sees them
	enum class ValueKind
	{
		Unsupported,
		Pointer,
		String,
		Polymorphic,
		List,
		Map,
		Object,
		Enum,
		Fundamental
	};

	static ValueKind classifyType(rfk::Type const& type)
	{
		rfk::Archetype const* archetype= type.getArchetype();
		rfk::EEntityKind archetypeKind= archetype ? archetype->getKind() : rfk::EEntityKind::Undefined;

		// No field crosses the wire as a C array, so the serializer has no rule for one
		if (type.isCArray())
		{
			return ValueKind::Unsupported;
		}

		if (type.isPointer())
		{
			return ValueKind::Pointer;
		}
		else if (type == rfk::getType<std::string>())
		{
			return ValueKind::String;
		}
		else if (archetypeKind == rfk::EEntityKind::Class)
		{
			rfk::Class const* classType= rfk::classCast(archetype);
			std::string cppType= classType->getName();

			if (cppType == "String")
			{
				return ValueKind::String;
			}
			else if (cppType == "PolymorphicObjectPtr")
			{
				return ValueKind::Polymorphic;
			}
			else if (classType->getClassKind() == rfk::EClassKind::TemplateInstantiation)
			{
				const auto* templateClassInstanceType= rfk::classTemplateInstantiationCast(classType);
				std::string templateTypeName= templateClassInstanceType->getClassTemplate().getName();
				size_t argumentCount= templateClassInstanceType->getTemplateArgumentsCount();

				if (templateTypeName == "List" && argumentCount == 1)
				{
					return ValueKind::List;
				}
				else if (templateTypeName == "Map" && argumentCount == 2)
				{
					return ValueKind::Map;
				}

				return ValueKind::Unsupported;
			}

			return ValueKind::Object;
		}
		else if (archetypeKind == rfk::EEntityKind::Struct)
		{
			return ValueKind::Object;
		}
		else if (archetypeKind == rfk::EEntityKind::Enum)
		{
			return ValueKind::Enum;
		}
		else if (archetypeKind == rfk::EEntityKind::FundamentalArchetype)
		{
			return getFundamentalSuffix(type).empty() ? ValueKind::Unsupported : ValueKind::Fundamental;
		}

		return ValueKind::Unsupported;
	}

	// The MikanJson Read/Write suffix for a fundamental type, or empty when it has none
	static std::string getFundamentalSuffix(rfk::Type const& type)
	{
		if (type == rfk::getType<bool>())
			return "Bool";
		else if (type == rfk::getType<uint8_t>())
			return "Byte";
		else if (type == rfk::getType<int8_t>())
			return "SByte";
		else if (type == rfk::getType<uint16_t>())
			return "UShort";
		else if (type == rfk::getType<int16_t>())
			return "Short";
		else if (type == rfk::getType<uint32_t>())
			return "UInt";
		else if (type == rfk::getType<int32_t>() || type == rfk::getType<int>())
			return "Int";
		else if (type == rfk::getType<uint64_t>())
			return "ULong";
		else if (type == rfk::getType<int64_t>() || type == rfk::getType<long>())
			return "Long";
		else if (type == rfk::getType<float>())
			return "Float";
		else if (type == rfk::getType<double>())
			return "Double";

		return "";
	}

	static rfk::Type const& getTemplateArgumentType(rfk::Type const& type, size_t argumentIndex)
	{
		rfk::Class const* classType= rfk::classCast(type.getArchetype());
		const auto* templateClassInstanceType= rfk::classTemplateInstantiationCast(classType);
		auto const& templateArg= static_cast<rfk::TypeTemplateArgument const&>(
			templateClassInstanceType->getTemplateArgumentAt(argumentIndex));

		return templateArg.getType();
	}

	// A C# expression turning valueExpr into a JToken. depth keeps nested lambda parameters apart.
	static bool buildWriteExpr(rfk::Type const& type, std::string const& valueExpr, int depth, std::string& outExpr)
	{
		switch (classifyType(type))
		{
		case ValueKind::Pointer:
			outExpr= "MikanJson.WritePointer(" + valueExpr + ")";
			return true;
		case ValueKind::String:
			outExpr= "MikanJson.WriteString(" + valueExpr + ")";
			return true;
		case ValueKind::Polymorphic:
			outExpr= "MikanJson.WritePolymorphic(" + valueExpr + ")";
			return true;
		case ValueKind::Object:
			outExpr= "MikanJson.WriteObject(" + valueExpr + ")";
			return true;
		case ValueKind::Enum:
			outExpr= std::string(type.getArchetype()->getName()) + "Json.ToJson(" + valueExpr + ")";
			return true;
		case ValueKind::Fundamental:
			outExpr= "MikanJson.Write" + getFundamentalSuffix(type) + "(" + valueExpr + ")";
			return true;
		case ValueKind::List:
		{
			std::string element= "e" + std::to_string(depth);
			std::string elementExpr;
			if (!buildWriteExpr(getTemplateArgumentType(type, 0), element, depth + 1, elementExpr))
				return false;

			outExpr= "MikanJson.WriteList(" + valueExpr + ", " + element + " => " + elementExpr + ")";
			return true;
		}
		case ValueKind::Map:
		{
			std::string key= "k" + std::to_string(depth);
			std::string value= "v" + std::to_string(depth);
			std::string keyExpr, valueWriteExpr;
			if (!buildWriteExpr(getTemplateArgumentType(type, 0), key, depth + 1, keyExpr)
				|| !buildWriteExpr(getTemplateArgumentType(type, 1), value, depth + 1, valueWriteExpr))
				return false;

			outExpr= "MikanJson.WriteMap(" + valueExpr + ", " + key + " => " + keyExpr + ", " + value + " => "
					 + valueWriteExpr + ")";
			return true;
		}
		default:
			return false;
		}
	}

	// A C# expression reading a value of type from tokenExpr. existingExpr is the field being
	// read into, whose object or container is refilled in place, or empty for a container element.
	static bool buildReadExpr(rfk::Type const& type, std::string const& tokenExpr, std::string const& existingExpr,
							  int depth, std::string& outExpr)
	{
		switch (classifyType(type))
		{
		case ValueKind::Pointer:
			outExpr= "MikanJson.ReadPointer(" + tokenExpr + ")";
			return true;
		case ValueKind::String:
			outExpr= "MikanJson.ReadString(" + tokenExpr + ")";
			return true;
		case ValueKind::Polymorphic:
			outExpr=
				"MikanJson.ReadPolymorphic(" + tokenExpr + ", " + (existingExpr.empty() ? "null" : existingExpr) + ")";
			return true;
		case ValueKind::Object:
		{
			std::string newInstance= "new " + std::string(type.getArchetype()->getName()) + "()";
			std::string instanceExpr= existingExpr.empty() ? newInstance : existingExpr + " ?? " + newInstance;

			outExpr= "MikanJson.ReadObject(" + tokenExpr + ", " + instanceExpr + ")";
			return true;
		}
		case ValueKind::Enum:
			outExpr= std::string(type.getArchetype()->getName()) + "Json.FromJson(" + tokenExpr + ")";
			return true;
		case ValueKind::Fundamental:
			outExpr= "MikanJson.Read" + getFundamentalSuffix(type) + "(" + tokenExpr + ")";
			return true;
		case ValueKind::List:
		{
			std::string element= "e" + std::to_string(depth);
			std::string elementExpr;
			if (!buildReadExpr(getTemplateArgumentType(type, 0), element, "", depth + 1, elementExpr))
				return false;

			outExpr= "MikanJson.ReadList(" + tokenExpr + ", " + (existingExpr.empty() ? "null" : existingExpr) + ", "
					 + element + " => " + elementExpr + ")";
			return true;
		}
		case ValueKind::Map:
		{
			std::string key= "k" + std::to_string(depth);
			std::string value= "v" + std::to_string(depth);
			std::string keyExpr, valueReadExpr;
			if (!buildReadExpr(getTemplateArgumentType(type, 0), key, "", depth + 1, keyExpr)
				|| !buildReadExpr(getTemplateArgumentType(type, 1), value, "", depth + 1, valueReadExpr))
				return false;

			outExpr= "MikanJson.ReadMap(" + tokenExpr + ", " + (existingExpr.empty() ? "null" : existingExpr) + ", "
					 + key + " => " + keyExpr + ", " + value + " => " + valueReadExpr + ")";
			return true;
		}
		default:
			return false;
		}
	}

	// An enum value that crosses the wire: its C# member name and its integer value
	struct WireEnumValue
	{
		std::string name;
		int64_t value;
	};

	void emitCSharpEnum(std::ofstream& moduleFile, rfk::Enum const& enumRef)
	{
		// Only enum values with a string property cross the wire
		std::vector<WireEnumValue> wireValues;
		enumRef.foreachEnumValue(
			[](rfk::EnumValue const& enumValue, void* userData) -> bool
			{
				auto* wireValuesPtr= reinterpret_cast<std::vector<WireEnumValue>*>(userData);

				auto const* property= enumValue.getProperty<Serialization::EnumStringValue>();
				if (property != nullptr)
				{
					wireValuesPtr->push_back({property->getValue(), enumValue.getValue()});
				}

				return true;
			},
			&wireValues);

		const std::string enumName= enumRef.getName();

		moduleFile << "\tpublic enum " << enumName << std::endl;
		moduleFile << "\t{" << std::endl;
		for (WireEnumValue const& wireValue : wireValues)
		{
			moduleFile << "\t\t" << wireValue.name << "= " << wireValue.value << "," << std::endl;
		}
		moduleFile << "\t};" << std::endl;
		moduleFile << std::endl;

		// Several names can share one value. The C++ serializer writes the name of whichever
		// value getEnumValue finds for the integer, so the writer here asks the same question.
		std::map<int64_t, std::string> writtenNames;
		for (WireEnumValue const& wireValue : wireValues)
		{
			if (writtenNames.find(wireValue.value) != writtenNames.end())
				continue;

			std::string writtenName= wireValue.name;
			rfk::EnumValue const* foundValue= enumRef.getEnumValue(wireValue.value);
			if (foundValue != nullptr)
			{
				if (auto const* property= foundValue->getProperty<Serialization::EnumStringValue>())
				{
					writtenName= property->getValue();
				}
			}

			writtenNames[wireValue.value]= writtenName;
		}

		moduleFile << "\tinternal static class " << enumName << "Json" << std::endl;
		moduleFile << "\t{" << std::endl;
		moduleFile << "\t\tpublic static JToken ToJson(" << enumName << " value)" << std::endl;
		moduleFile << "\t\t{" << std::endl;
		moduleFile << "\t\t\tswitch (value)" << std::endl;
		moduleFile << "\t\t\t{" << std::endl;
		for (WireEnumValue const& wireValue : wireValues)
		{
			auto it= writtenNames.find(wireValue.value);
			if (it == writtenNames.end() || it->second != wireValue.name)
				continue;

			moduleFile << "\t\t\t\tcase " << enumName << "." << wireValue.name << ": return MikanJson.WriteString(\""
					   << wireValue.name << "\");" << std::endl;
		}
		moduleFile << "\t\t\t}" << std::endl;
		moduleFile << "\t\t\tthrow new ArgumentOutOfRangeException(nameof(value), \"" << enumName
				   << " has no wire name for \" + (long)value);" << std::endl;
		moduleFile << "\t\t}" << std::endl;
		moduleFile << std::endl;
		moduleFile << "\t\tpublic static " << enumName << " FromJson(JToken token)" << std::endl;
		moduleFile << "\t\t{" << std::endl;
		moduleFile << "\t\t\tif (token.Type == JTokenType.Integer)" << std::endl;
		moduleFile << "\t\t\t{" << std::endl;
		moduleFile << "\t\t\t\treturn (" << enumName << ")(long)token;" << std::endl;
		moduleFile << "\t\t\t}" << std::endl;
		moduleFile << std::endl;
		moduleFile << "\t\t\tstring name = MikanJson.ReadEnumName(token);" << std::endl;
		moduleFile << "\t\t\tswitch (name)" << std::endl;
		moduleFile << "\t\t\t{" << std::endl;
		for (WireEnumValue const& wireValue : wireValues)
		{
			moduleFile << "\t\t\t\tcase \"" << wireValue.name << "\": return " << enumName << "." << wireValue.name
					   << ";" << std::endl;
		}
		moduleFile << "\t\t\t}" << std::endl;
		moduleFile << "\t\t\tthrow new Exception(\"" << enumName << " has no value named \" + name);" << std::endl;
		moduleFile << "\t\t}" << std::endl;
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
					rfk::Type const& elementType= getTemplateArgumentType(type, 0);
					std::string elementTypeString= getCSharpType(elementType);

					return "List<" + elementTypeString + ">";
				}
				else if (templateTypeName == "Map" && templateClassInstanceType->getTemplateArgumentsCount() == 2)
				{
					std::string keyTypeString= getCSharpType(getTemplateArgumentType(type, 0));
					std::string valueTypeString= getCSharpType(getTemplateArgumentType(type, 1));

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
			static const std::map<std::string, std::string> k_fundamentalCSharpTypes= {
				{"Bool", "bool"},   {"Byte", "byte"},   {"SByte", "sbyte"},  {"UShort", "ushort"},
				{"Short", "short"}, {"UInt", "uint"},   {"Int", "int"},      {"ULong", "ulong"},
				{"Long", "long"},   {"Float", "float"}, {"Double", "double"}};

			auto it= k_fundamentalCSharpTypes.find(getFundamentalSuffix(type));
			if (it != k_fundamentalCSharpTypes.end())
			{
				return type.isCArray() ? it->second + "[]" : it->second;
			}
		}

		return "UNKNOWN_TYPE";
	}
};

MikanClientLanguageGenPtr createCSharpClientGen(ClientCodeGenContext const& context)
{
	return std::make_unique<MikanClientCSharpGen>(context);
}

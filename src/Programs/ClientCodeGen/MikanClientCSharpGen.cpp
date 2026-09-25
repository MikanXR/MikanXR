#include "MikanClientLanguageGen.h"

#include "Logger.h"

#include <fstream>
#include <string>

// The C# side of the client bindings. A C# class may reference another declared later in the
// same namespace, so entities go out in the name order CodeGenDatabase established and this
// generator has no whole-output files to write.
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

		// The order these go out in is the binary wire order, so it has to be the serializer's
		const Serialization::FieldList sortedFields= Serialization::getStructFieldsInWireOrder(&structRef);

		// Each field states its own position rather than leaving it to declaration order, since
		// the C# runtime recovers it through Type.GetFields, which promises no ordering at all
		for (size_t fieldIndex= 0; fieldIndex < sortedFields.size(); ++fieldIndex)
		{
			rfk::Field const* field= sortedFields[fieldIndex];
			std::string csharpType= getCSharpType(*field);

			moduleFile << "\t\t[MikanFieldOrder(" << fieldIndex << ")] public " << csharpType << " " << field->getName()
					   << ";" << std::endl;
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
};

MikanClientLanguageGenPtr createCSharpClientGen(ClientCodeGenContext const& context)
{
	return std::make_unique<MikanClientCSharpGen>(context);
}

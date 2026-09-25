//-- includes -----
#include <stdio.h>
#include <stdlib.h>

#include "MikanCoreTypes.h"
#include "SerializationVisitor.h"

#include "csharp_interop_unit_tests.h"
#include "unit_test.h"

#include <cstddef>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

// MikanRenderTargetDescriptor is the one wire struct that also crosses the core C API by value.
// Every other client-facing type reaches C# as generated code driven by reflection, but this one
// is marshalled through P/Invoke against a hand-written mirror, MikanRenderTargetDescriptor_Native
// in bindings/csharp/MikanCoreNative.cs. Sequential layout carries no field names, so a mirror
// that is missing a field silently shifts every field after it, and because
// Mikan_GetCameraRenderTargetDescriptor takes the struct as an out parameter, a short mirror is
// also written past its end. Neither the generated bindings nor the property schema guard test
// sees any of that, which is what these tests are for.

// The layout this file's expectations are written against. A field added, removed or retyped on
// the C++ side breaks this first, as a reminder that the C# mirror has to move with it.
static_assert(sizeof(MikanRenderTargetDescriptor) == 32,
			  "MikanRenderTargetDescriptor changed size. Update MikanRenderTargetDescriptor_Native in "
			  "bindings/csharp/MikanCoreNative.cs and both conversions in bindings/csharp/MikanRenderTargetAPI.cs "
			  "to match, then update this assert.");

// Pull the field names, in declaration order, out of a C# struct in a source file. The mirror is
// a plain sequence of "public <type> <name>;" lines by necessity, since that is all P/Invoke can
// marshal, so it stays readable with a line-wise scan.
static bool readCSharpStructFieldNames(const std::string& filePath, const std::string& structName,
									   std::vector<std::string>& outFieldNames, std::string& outError)
{
	std::ifstream file(filePath);
	if (!file.is_open())
	{
		outError= "could not open " + filePath;
		return false;
	}

	const std::regex structPattern("\\bstruct\\s+" + structName + "\\b");
	const std::regex fieldPattern("^\\s*public\\s+[A-Za-z_][A-Za-z0-9_]*\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*;");

	std::string line;
	bool bFoundStruct= false;
	bool bInBody= false;

	while (std::getline(file, line))
	{
		if (!bFoundStruct)
		{
			bFoundStruct= std::regex_search(line, structPattern);
			continue;
		}

		if (!bInBody)
		{
			// Skip the opening brace on its own line
			if (line.find('{') != std::string::npos)
			{
				bInBody= true;
			}
			continue;
		}

		if (line.find('}') != std::string::npos)
		{
			break;
		}

		std::smatch match;
		if (std::regex_search(line, match, fieldPattern))
		{
			outFieldNames.push_back(match[1].str());
		}
	}

	if (!bFoundStruct)
	{
		outError= "no struct named " + structName + " in " + filePath;
		return false;
	}

	if (outFieldNames.empty())
	{
		outError= structName + " in " + filePath + " parsed with no fields";
		return false;
	}

	return true;
}

bool csharp_interop_test_render_target_descriptor_mirror()
{
	UNIT_TEST_BEGIN("the C# render target descriptor mirrors the native one field for field")

	const std::string nativeFilePath= std::string(MIKAN_REPO_ROOT_DIR) + "/bindings/csharp/MikanCoreNative.cs";

	std::vector<std::string> csharpFieldNames;
	std::string parseError;
	if (!readCSharpStructFieldNames(nativeFilePath, "MikanRenderTargetDescriptor_Native", csharpFieldNames, parseError))
	{
		fprintf(stdout, "      %s\n", parseError.c_str());
		success= false;
		UNIT_TEST_COMPLETE()
	}

	// The same field list, and the same order, the serializer and the bindings generator use
	const Serialization::FieldList nativeFields=
		Serialization::getStructFieldsInWireOrder(&MikanRenderTargetDescriptor::staticGetArchetype());

	if (nativeFields.size() != csharpFieldNames.size())
	{
		fprintf(stdout, "      native has %zu fields, C# mirror has %zu\n", nativeFields.size(),
				csharpFieldNames.size());
		success= false;
	}

	const size_t comparedCount= std::min(nativeFields.size(), csharpFieldNames.size());
	for (size_t fieldIndex= 0; fieldIndex < comparedCount; ++fieldIndex)
	{
		const std::string nativeName= nativeFields[fieldIndex]->getName();

		if (nativeName != csharpFieldNames[fieldIndex])
		{
			fprintf(stdout, "      field %zu: native '%s', C# mirror '%s'\n", fieldIndex, nativeName.c_str(),
					csharpFieldNames[fieldIndex].c_str());
			success= false;
		}
	}

	if (!success)
	{
		fprintf(stdout, "      MikanRenderTargetDescriptor_Native in bindings/csharp/MikanCoreNative.cs must list "
						"every field of MikanRenderTargetDescriptor in the same order, and both conversions in "
						"bindings/csharp/MikanRenderTargetAPI.cs must copy them.\n");
	}

	UNIT_TEST_COMPLETE()
}

//-- public interface -----
bool run_csharp_interop_unit_tests()
{
	UNIT_TEST_MODULE_BEGIN("csharp_interop")
	UNIT_TEST_MODULE_CALL_TEST(csharp_interop_test_render_target_descriptor_mirror);
	UNIT_TEST_MODULE_END()
}

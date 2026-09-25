#pragma once

#include "CodeGenDatabase.h"

#include "MikanAPI.h"
#include "SerializableObjectPtr.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// -- Client language generators -----
// One target language's emitters live in their own file behind this base class, so the app that
// owns them neither names a language nor branches on one, and a new language is a file rather
// than another arm of each dispatch. The base carries what every language needs from the
// reflection data; how any of it turns into source text belongs to the derived generator.

// What a generator reads from the app that owns it. The app owns both and outlives the
// generator, so this is held by value but the database by pointer.
struct ClientCodeGenContext
{
	// Absolute, already emptied and recreated by the app
	std::filesystem::path outputPath;
	CodeGenDatabase const* database= nullptr;
};

// Holds a live default-constructed instance of a struct so a generator can read actual field
// default values. One of the holders keeps the shared_ptr alive; rawPtr points into the same
// allocation.
struct AllocatedInstance
{
	void* rawPtr= nullptr;
	std::shared_ptr<MikanEvent> eventHolder;
	std::shared_ptr<MikanRequest> requestHolder;
	std::shared_ptr<MikanResponse> responseHolder;
	std::shared_ptr<Serialization::PolymorphicStruct> polymorphicHolder;

	bool isValid() const { return rawPtr != nullptr; }
};

class MikanClientLanguageGen
{
public:
	MikanClientLanguageGen(ClientCodeGenContext const& context)
		: m_context(context)
	{
	}
	virtual ~MikanClientLanguageGen()= default;

	// Emit the one source file that carries this module's entities.
	virtual bool generateModuleFile(ClientModulePtr const& module)= 0;

	// Emit the files that describe the whole output rather than one module: barrels,
	// registries, a package marker. A language with none keeps this default.
	virtual bool generateWholeOutputFiles() { return true; }

protected:
	// The name of the field a request, response or event stamps its wire type name into, or an
	// empty string for every other struct.
	static std::string getTypeNameFieldForStruct(rfk::Struct const& structRef);

	// A default-constructed instance of structRef, or an invalid one when no known base class
	// is found or allocation fails.
	static AllocatedInstance tryAllocateStructInstance(rfk::Struct const& structRef);

	// Reorders structs so every base comes before the types deriving from it, for a language
	// whose file cannot reference a type declared later in it.
	std::vector<rfk::Struct const*> sortStructsByInheritance(std::vector<rfk::Struct const*> const& structs) const;

	// Which module owns a type, for a language that emits per-module imports. Empty when no
	// module declares it.
	std::string findModuleForType(std::string const& typeName) const;

	ClientCodeGenContext m_context;
};
using MikanClientLanguageGenPtr= std::unique_ptr<MikanClientLanguageGen>;

// One per language, each defined beside the generator it builds.
MikanClientLanguageGenPtr createCSharpClientGen(ClientCodeGenContext const& context);
MikanClientLanguageGenPtr createTypeScriptClientGen(ClientCodeGenContext const& context);

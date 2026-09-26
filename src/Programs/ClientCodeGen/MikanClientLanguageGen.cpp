#include "MikanClientLanguageGen.h"

#include <functional>
#include <map>
#include <set>

// Shared across every language generator. Each of these reads the reflection data; none of them
// decides how it is written out, which is the derived generator's business.

std::string MikanClientLanguageGen::getTypeNameFieldForStruct(rfk::Struct const& structRef)
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

AllocatedInstance MikanClientLanguageGen::tryAllocateStructInstance(rfk::Struct const& structRef)
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

std::vector<rfk::Struct const*> MikanClientLanguageGen::sortStructsByInheritance(
	std::vector<rfk::Struct const*> const& structs) const
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

std::string MikanClientLanguageGen::findModuleForType(std::string const& typeName) const
{
	// Special case: PolymorphicObject and PolymorphicStruct are imported from parent module
	if (typeName == "PolymorphicObject" || typeName == "PolymorphicStruct")
	{
		return "../PolymorphicObject";
	}

	if (!m_context.database)
		return "";

	// Search through all modules to find which one contains this type
	for (const auto& modulePair : m_context.database->modules)
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

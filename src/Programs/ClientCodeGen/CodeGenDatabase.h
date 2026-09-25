#pragma once

#include "SerializationProperty.h"

#include "Refureku/Refureku.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string.h>
#include <string>
#include <vector>

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
#pragma once

#include "ComponentFwd.h"
#include "MikanTypedObjectSystem.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectSystemFwd.h"
#include "SpoutOutputComponent.h"

#include <string>

class SpoutOutputSystemDefinition
	: public MikanTypedObjectSystemDefinition<SpoutOutputComponent, SpoutOutputDefinition, MikanOutputID>
{
public:
	using Super= MikanTypedObjectSystemDefinition<SpoutOutputComponent, SpoutOutputDefinition, MikanOutputID>;

	SpoutOutputSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator);
};

class SpoutOutputSystem : public MikanTypedObjectSystem<SpoutOutputComponent, SpoutOutputDefinition, MikanOutputID,
														SpoutOutputSystem, SpoutOutputSystemDefinition>
{
public:
	using Super= MikanTypedObjectSystem<SpoutOutputComponent, SpoutOutputDefinition, MikanOutputID, SpoutOutputSystem,
										SpoutOutputSystemDefinition>;

	SpoutOutputSystem(ProjectManagerPtr ownerObjectSystem);

	inline static const std::string k_objectSystemClassName= "SpoutOutputSystem";
	virtual std::string getObjectSystemClassName() const { return k_objectSystemClassName; }

	inline SpoutOutputComponentPtr getOutputById(MikanOutputID outputId) const
	{
		return Super::getTypedComponentById(outputId);
	}
	inline SpoutOutputComponentPtr getOutputByName(const std::string& outputName) const
	{
		return Super::getTypedComponentByName(outputName);
	}

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);
};

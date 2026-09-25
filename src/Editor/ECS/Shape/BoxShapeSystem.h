#pragma once

#include "BoxShapeComponent.h"
#include "ComponentFwd.h"
#include "MikanTypedObjectSystem.h"
#include "MikanTypeFwd.h"
#include "ObjectSystemConfigFwd.h"

#include <memory>
#include <string>
#include <vector>

class BoxShapeSystemDefinition
	: public MikanTypedObjectSystemDefinition<BoxShapeComponent, BoxShapeDefinition, MikanShapeID>
{
public:
	using Super= MikanTypedObjectSystemDefinition<BoxShapeComponent, BoxShapeDefinition, MikanShapeID>;

	BoxShapeSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
};

class BoxShapeSystem : public MikanTypedObjectSystem<BoxShapeComponent, BoxShapeDefinition, MikanShapeID,
													 BoxShapeSystem, BoxShapeSystemDefinition>
{
public:
	using Super= MikanTypedObjectSystem<BoxShapeComponent, BoxShapeDefinition, MikanShapeID, BoxShapeSystem,
										BoxShapeSystemDefinition>;

	BoxShapeSystem(ProjectManagerPtr ownerObjectSystem);

	inline static const std::string k_objectSystemClassName= "BoxShapeSystem";
	virtual std::string getObjectSystemClassName() const { return k_objectSystemClassName; }

	inline BoxShapeComponentPtr getBoxShapeById(MikanShapeID shapeId) const
	{
		return Super::getTypedComponentById(shapeId);
	}

	inline BoxShapeComponentPtr getBoxShapeByName(const std::string& shapeName) const
	{
		return Super::getTypedComponentByName(shapeName);
	}

	void getBoxShapeComponentList(std::vector<BoxShapeComponentPtr>& outList) const;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

protected:
	virtual void additionalComponentFactory(MikanObjectPtr ownerComponentObject,
											ComponentDefinitionPtr componentDefinition) override;
};

using BoxShapeSystemPtr= std::shared_ptr<BoxShapeSystem>;
using BoxShapeSystemWeakPtr= std::weak_ptr<BoxShapeSystem>;

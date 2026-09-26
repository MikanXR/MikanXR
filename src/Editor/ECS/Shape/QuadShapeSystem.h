#pragma once

#include "ComponentFwd.h"
#include "MikanTypedObjectSystem.h"
#include "MikanTypeFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "QuadShapeComponent.h"

#include <memory>
#include <string>
#include <vector>

class QuadShapeSystemDefinition
	: public MikanTypedObjectSystemDefinition<QuadShapeComponent, QuadShapeDefinition, MikanShapeID>
{
public:
	using Super= MikanTypedObjectSystemDefinition<QuadShapeComponent, QuadShapeDefinition, MikanShapeID>;

	QuadShapeSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
};

class QuadShapeSystem : public MikanTypedObjectSystem<QuadShapeComponent, QuadShapeDefinition, MikanShapeID,
													  QuadShapeSystem, QuadShapeSystemDefinition>
{
public:
	using Super= MikanTypedObjectSystem<QuadShapeComponent, QuadShapeDefinition, MikanShapeID, QuadShapeSystem,
										QuadShapeSystemDefinition>;

	QuadShapeSystem(ProjectManagerPtr ownerObjectSystem);

	inline static const std::string k_objectSystemClassName= "QuadShapeSystem";
	virtual std::string getObjectSystemClassName() const { return k_objectSystemClassName; }

	inline QuadShapeComponentPtr getQuadShapeById(MikanShapeID shapeId) const
	{
		return Super::getTypedComponentById(shapeId);
	}

	inline QuadShapeComponentPtr getQuadShapeByName(const std::string& shapeName) const
	{
		return Super::getTypedComponentByName(shapeName);
	}

	void getQuadShapeComponentList(std::vector<QuadShapeComponentPtr>& outList) const;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

protected:
	virtual void additionalComponentFactory(MikanObjectPtr ownerComponentObject,
											ComponentDefinitionPtr componentDefinition) override;
};

using QuadShapeSystemPtr= std::shared_ptr<QuadShapeSystem>;
using QuadShapeSystemWeakPtr= std::weak_ptr<QuadShapeSystem>;

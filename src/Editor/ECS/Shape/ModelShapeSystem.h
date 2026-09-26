#pragma once

#include "ComponentFwd.h"
#include "MikanTypedObjectSystem.h"
#include "MikanTypeFwd.h"
#include "ModelShapeComponent.h"
#include "ObjectSystemConfigFwd.h"

#include <memory>
#include <string>
#include <vector>

class ModelShapeSystemDefinition
	: public MikanTypedObjectSystemDefinition<ModelShapeComponent, ModelShapeDefinition, MikanShapeID>
{
public:
	using Super= MikanTypedObjectSystemDefinition<ModelShapeComponent, ModelShapeDefinition, MikanShapeID>;

	ModelShapeSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
};

class ModelShapeSystem : public MikanTypedObjectSystem<ModelShapeComponent, ModelShapeDefinition, MikanShapeID,
													   ModelShapeSystem, ModelShapeSystemDefinition>
{
public:
	using Super= MikanTypedObjectSystem<ModelShapeComponent, ModelShapeDefinition, MikanShapeID, ModelShapeSystem,
										ModelShapeSystemDefinition>;

	ModelShapeSystem(ProjectManagerPtr ownerObjectSystem);

	inline static const std::string k_objectSystemClassName= "ModelShapeSystem";
	virtual std::string getObjectSystemClassName() const { return k_objectSystemClassName; }

	inline ModelShapeComponentPtr getModelShapeById(MikanShapeID shapeId) const
	{
		return Super::getTypedComponentById(shapeId);
	}

	inline ModelShapeComponentPtr getModelShapeByName(const std::string& shapeName) const
	{
		return Super::getTypedComponentByName(shapeName);
	}

	void getModelShapeComponentList(std::vector<ModelShapeComponentPtr>& outList) const;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

protected:
	virtual void additionalComponentFactory(MikanObjectPtr ownerComponentObject,
											ComponentDefinitionPtr componentDefinition) override;
};

using ModelShapeSystemPtr= std::shared_ptr<ModelShapeSystem>;
using ModelShapeSystemWeakPtr= std::weak_ptr<ModelShapeSystem>;

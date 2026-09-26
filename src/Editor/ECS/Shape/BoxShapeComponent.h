#pragma once

#include "ComponentFwd.h"
#include "MikanShapeTypes.h"
#include "MikanTypeFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "ShapeComponent.h"

#include <string>

class BoxShapeDefinition : public ShapeComponentDefinition
{
public:
	BoxShapeDefinition();
	BoxShapeDefinition(MikanShapeID shapeId);

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;

	static const std::string k_boxXSizePropertyId;
	float getBoxXSize() const { return m_xSize; }
	void setBoxXSize(float size);

	static const std::string k_boxYSizePropertyId;
	float getBoxYSize() const { return m_ySize; }
	void setBoxYSize(float size);

	static const std::string k_boxZSizePropertyId;
	float getBoxZSize() const { return m_zSize; }
	void setBoxZSize(float size);

protected:
	float m_xSize= 1.0f;
	float m_ySize= 1.0f;
	float m_zSize= 1.0f;
};

class BoxShapeComponent : public ShapeComponent
{
public:
	BoxShapeComponent(MikanObjectWeakPtr owner);

	inline BoxShapeDefinitionPtr getBoxShapeDefinition() const
	{
		return std::static_pointer_cast<BoxShapeDefinition>(m_definition);
	}

	inline static const std::string k_componentClassName= "BoxShapeComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	virtual void init() override;
	virtual void update(float deltaSeconds) override;
	virtual void dispose() override;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
	{
		ShapeComponent::getFunctionDescriptors(outDescriptors);
	}

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

	virtual glm::mat4 computeSceneRenderableModelMatrix() const override;

	void openShape();
	void closeShape();

protected:
	virtual void onDefinitionMarkedDirty(CommonConfigPtr configPtr,
										 const ConfigPropertyChangeSet& changedPropertySet) override;

	void updateBoxColliderExtents();

	SelectionComponentWeakPtr m_selectionComponent;
	BoxColliderComponentWeakPtr m_boxCollider;
};

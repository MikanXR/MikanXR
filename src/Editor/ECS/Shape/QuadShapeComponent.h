#pragma once

#include "ComponentFwd.h"
#include "MikanShapeTypes.h"
#include "MikanTypeFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "ShapeComponent.h"

#include <string>

class QuadShapeDefinition : public ShapeComponentDefinition
{
public:
	QuadShapeDefinition();
	QuadShapeDefinition(MikanShapeID shapeId);

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;

	static const std::string k_quadWidthPropertyId;
	float getQuadWidth() const { return m_quadWidth; }
	void setQuadWidth(float width);

	static const std::string k_quadHeightPropertyId;
	float getQuadHeight() const { return m_quadHeight; }
	void setQuadHeight(float height);

	static const std::string k_quadDoubleSidedPropertyId;
	bool getIsDoubleSided() const { return m_bIsDoubleSided; }
	void setIsDoubleSided(bool flag);

protected:
	float m_quadWidth= 1.0f;
	float m_quadHeight= 1.0f;
	bool m_bIsDoubleSided= true;
};

class QuadShapeComponent : public ShapeComponent
{
public:
	QuadShapeComponent(MikanObjectWeakPtr owner);

	inline QuadShapeDefinitionPtr getQuadShapeDefinition() const
	{
		return std::static_pointer_cast<QuadShapeDefinition>(m_definition);
	}

	inline static const std::string k_componentClassName= "QuadShapeComponent";
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

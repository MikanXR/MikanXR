#pragma once

#include "MikanStencilTypes.h"
#include "MikanMathTypes.h"
#include "ObjectSystemConfigFwd.h"
#include "StencilComponent.h"
#include "Transform.h"

#include <string>

#include "glm/ext/vector_float3.hpp"

class QuadStencilDefinition : public StencilComponentDefinition
{
public:
	QuadStencilDefinition();
	QuadStencilDefinition(MikanStencilID stencilId);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	static const std::string k_quadStencilWidthPropertyId;
	float getQuadWidth() const { return m_quadWidth; }
	void setQuadWidth(float width);

	static const std::string k_quadStencilHeightPropertyId;
	float getQuadHeight() const { return m_quadHeight; }
	void setQuadHeight(float height);

	void setQuadSize(float width, float height);

	static const std::string k_quadStencilDoubleSidedPropertyId;
	bool getIsDoubleSided() const { return m_bIsDoubleSided; }
	void setIsDoubleSided(bool flag);

protected:
	float m_quadWidth;
	float m_quadHeight;
	bool m_bIsDoubleSided;
};

class QuadStencilComponent : public StencilComponent
{
public:
	QuadStencilComponent(MikanObjectWeakPtr owner);
	virtual void init() override;
	virtual void customRender(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera) const override;

	inline QuadStencilDefinitionPtr getQuadStencilDefinition() const
	{
		return std::static_pointer_cast<QuadStencilDefinition>(m_definition);
	}

	inline static const std::string k_componentClassName= "QuadStencilComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
	{
		StencilComponent::getFunctionDescriptors(outDescriptors);
	}

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

protected:
	void updateBoxColliderExtents();

	SelectionComponentWeakPtr m_selectionComponent;
	BoxColliderComponentWeakPtr m_boxCollider;
};
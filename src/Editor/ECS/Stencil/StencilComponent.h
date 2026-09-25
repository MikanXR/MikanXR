#pragma once

#include "MikanComponent.h"
#include "MikanStencilTypes.h"
#include "CompositorConstants.h"
#include "TransformComponent.h"

#include <string>

#include "glm/ext/vector_float3.hpp"
#include "glm/ext/matrix_float4x4.hpp"

class StencilComponentDefinition : public TransformComponentDefinition
{
public:
	StencilComponentDefinition();
	StencilComponentDefinition(MikanStencilID stencilId, const std::string& componentName, const MikanTransform& xform);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	static const std::string k_stencilDisabledPropertyId;
	bool getIsDisabled() const { return m_bIsDisabled; }
	void setIsDisabled(bool flag);

	static const std::string k_stencilCullModePropertyId;
	eStencilCullMode getCullMode() const { return m_cullMode; }
	void setCullMode(eStencilCullMode mode);

protected:
	bool m_bIsDisabled= false;
	eStencilCullMode m_cullMode= eStencilCullMode::none;
};

class StencilComponent : public TransformComponent
{
public:
	StencilComponent(MikanObjectWeakPtr owner);

	inline StencilComponentConfigPtr getStencilComponentDefinition() const
	{
		return std::static_pointer_cast<StencilComponentDefinition>(m_definition);
	}

	inline static const std::string k_componentClassName= "StencilComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	// -- TransformComponent ----
	virtual bool canAttachToParent(TransformComponentConstPtr newParentComponent) const override;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);
};
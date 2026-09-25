#pragma once

#include "CommonConfig.h"
#include "StencilComponent.h"
#include "MikanStencilTypes.h"
#include "ComponentFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectFwd.h"
#include "Transform.h"

#include <memory>
#include <string>

#include "glm/ext/vector_float3.hpp"

class BoxStencilDefinition : public StencilComponentDefinition
{
public:
	BoxStencilDefinition();
	BoxStencilDefinition(MikanStencilID stencilId);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	static const std::string k_boxStencilXSizePropertyId;
	float getBoxXSize() const { return m_boxSize.x; }
	void setBoxXSize(float size);

	static const std::string k_boxStencilYSizePropertyId;
	float getBoxYSize() const { return m_boxSize.y; }
	void setBoxYSize(float size);

	static const std::string k_boxStencilZSizePropertyId;
	float getBoxZSize() const { return m_boxSize.z; }
	void setBoxZSize(float size);

	void setBoxSize(float xSize, float ySize, float zSize);

private:
	MikanVector3f m_boxSize;
};

class BoxStencilComponent : public StencilComponent
{
public:
	BoxStencilComponent(MikanObjectWeakPtr owner);
	virtual void init() override;
	virtual void customRender(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera) const override;

	inline BoxStencilDefinitionPtr getBoxStencilDefinition() const
	{
		return std::static_pointer_cast<BoxStencilDefinition>(m_definition);
	}

	inline static const std::string k_componentClassName= "BoxStencilComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outPropertyNames)
	{
		StencilComponent::getFunctionDescriptors(outPropertyNames);
	}

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

protected:
	void updateBoxColliderExtents();

	SelectionComponentWeakPtr m_selectionComponent;
	BoxColliderComponentWeakPtr m_boxCollider;
};
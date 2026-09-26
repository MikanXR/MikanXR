#pragma once

#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "TransformComponent.h"
#include "MikanTypeFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectFwd.h"
#include "SceneFwd.h"
#include "Transform.h"

#include <memory>
#include <string>

#include "glm/ext/matrix_float4x4.hpp"

class AnchorDefinition : public TransformComponentDefinition
{
public:
	AnchorDefinition();
	AnchorDefinition(MikanSpatialAnchorID anchorId);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

private:
};

class AnchorComponent : public TransformComponent
{
public:
	AnchorComponent(MikanObjectWeakPtr owner);
	virtual void init() override;
	virtual void customRender(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera) const override;

	inline static const std::string k_componentClassName= "AnchorComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	inline AnchorDefinitionPtr getAnchorDefinition() const
	{
		return std::static_pointer_cast<AnchorDefinition>(m_definition);
	}

	// -- TransformComponent ----
	virtual bool canAttachToParent(TransformComponentConstPtr newParentComponent) const override;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IFunctionInterface ----
	static const std::string k_editAnchorFunctionId;
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors);
	virtual bool invokeFunction(const std::string& functionName) override;

	void editAnchor();

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

protected:
	SelectionComponentWeakPtr m_selectionComponent;
};
#pragma once

#include "AssetFwd.h"
#include "ComponentFwd.h"
#include "MikanRendererFwd.h"
#include "MikanShapeTypes.h"
#include "MikanStencilTypes.h"
#include "MikanTypeFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "ShapeComponent.h"

#include <memory>
#include <string>
#include <vector>

class ModelShapeDefinition : public ShapeComponentDefinition
{
public:
	ModelShapeDefinition();
	ModelShapeDefinition(MikanShapeID shapeId);

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;

	static const std::string k_modelPathPropertyId;
	bool hasModelPath() const;
	const std::filesystem::path getModelPath() const;
	void setModelPath(const std::filesystem::path& path, bool bForceDirty= false);

private:
	AssetReferenceConfigPtr m_modelAssetRefConfig;
};

class ModelShapeComponent : public ShapeComponent
{
public:
	ModelShapeComponent(MikanObjectWeakPtr owner);
	virtual void init() override;
	virtual void update(float deltaSeconds) override;
	virtual void dispose() override;

	inline ModelShapeDefinitionPtr getModelShapeDefinition() const
	{
		return std::static_pointer_cast<ModelShapeDefinition>(m_definition);
	}

	inline static const std::string k_componentClassName= "ModelShapeComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	inline const std::vector<StaticMeshComponentPtr>& getTriangulatedMeshes() const { return m_triMeshComponents; }
	inline MikanRenderModelResourcePtr getRenderModelResource() const { return m_modelResource; }

	void setModelPath(const std::filesystem::path& path);
	void disposeMeshComponents();
	void rebuildMeshComponents();

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

protected:
	virtual void onDefinitionMarkedDirty(CommonConfigPtr configPtr,
										 const ConfigPropertyChangeSet& changedPropertySet) override;

protected:
	AssetReferencePtr m_modelAssetRef;
	MikanRenderModelResourcePtr m_modelResource;
	SelectionComponentWeakPtr m_selectionComponentWeakPtr;
	std::vector<TransformComponentPtr> m_meshComponents;
	std::vector<StaticMeshComponentPtr> m_triMeshComponents;
	std::vector<MeshColliderComponentPtr> m_colliderComponents;
};

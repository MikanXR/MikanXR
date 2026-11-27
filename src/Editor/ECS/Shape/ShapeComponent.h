#pragma once

#include "AssetFwd.h"
#include "ComponentFwd.h"
#include "Graphs/NodeError.h"
#include "MikanCoreTypes.h"
#include "MikanTypeFwd.h"
#include "MkRendererFwd.h"
#include "NodeFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "TransformComponent.h"

#include <filesystem>
#include <string>
#include <vector>

#include "glm/ext/matrix_float4x4.hpp"

class ShapeComponentDefinition : public TransformComponentDefinition
{
public:
	ShapeComponentDefinition();
	ShapeComponentDefinition(MikanShapeID shapeId);

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;

	static const std::string k_shapeGraphPathPropertyId;
	bool hasShapeGraphPath() const;
	std::filesystem::path getShapeGraphPath() const;
	void setShapeGraphPath(const std::filesystem::path& graphPath);

protected:
	AssetReferenceConfigPtr m_nodeGraphAssetRef;
};

class ShapeComponent : public TransformComponent
{
public:
	ShapeComponent(MikanObjectWeakPtr owner);

	inline ShapeComponentDefinitionPtr getShapeComponentDefinition() const
	{
		return std::static_pointer_cast<ShapeComponentDefinition>(m_definition);
	}

	inline static const std::string k_componentClassName= "ShapeComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	// -- TransformComponent ----
	virtual bool canAttachToParent(TransformComponentConstPtr newParentComponent) const override;

	virtual void init() override;
	virtual void postInit() override;
	virtual void dispose() override;

	// Returns the missing texture from the texture cache (used as fallback when no ShapeGraph is set)
	IMkTexturePtr getColorTexture() const;

	// -- Shape Node Graph ----
	bool hasValidShapeGraph() const;
	void setEditorShapeNodeGraph(ShapeNodeGraphPtr editorNodeGraph);
	// The graph the shape renders with: an open editor window's ahead of the saved one
	ShapeNodeGraphPtr getActiveShapeNodeGraph() const;
	void addNewShapeGraph();
	void editShapeGraph();
	void removeShapeGraph();
	// Draws with the caller's blend and depth state
	void renderShapeGraph(const glm::mat4& vpMatrix, class IMkGraphicsContext* graphicsContext);
	const std::vector<NodeEvaluationError>& getLastNodeEvalErrors() const { return m_lastNodeEvalErrors; }

	std::filesystem::path getShapeGraphAssetResolvedPath() const;
	void setShapeGraphAssetPath(const std::filesystem::path& assetRefPath);

	// Function ID constants for GUI button identifiers
	static const std::string k_addNewShapeGraphFunctionId;
	static const std::string k_editShapeGraphFunctionId;
	static const std::string k_removeShapeGraphFunctionId;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors);
	virtual bool invokeFunction(const std::string& functionName) override;

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

protected:
	virtual void onDefinitionMarkedDirty(CommonConfigPtr configPtr,
										 const ConfigPropertyChangeSet& changedPropertySet) override;

	void onDefinitionChanged(CommonConfigPtr configPtr, const ConfigPropertyChangeSet& changedPropertySet);
	void handleShapeNodeGraphChanged(const std::filesystem::path& newAssetRefPath);

	// Shape Node Graph
	ShapeGraphAssetReferencePtr m_nodeGraphAssetRef;
	ShapeNodeGraphPtr m_nodeGraph;           // asset-based (runtime default)
	ShapeNodeGraphWeakPtr m_editorNodeGraph; // editor override (priority)
	std::vector<NodeEvaluationError> m_lastNodeEvalErrors;
};

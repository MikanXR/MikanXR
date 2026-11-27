#pragma once

#include "NodeGraph.h"
#include "ComponentFwd.h"

#include <set>
#include <string>

#include "glm/ext/matrix_float4x4.hpp"

class ShapeNodeGraph : public NodeGraph
{
public:
	ShapeNodeGraph();

	static const std::string k_renderShapeEventName;

	inline static const std::string k_graphClassName= "ShapeNodeGraph";
	virtual std::string getClassName() const override { return k_graphClassName; }
	virtual bool loadFromConfig(const NodeGraphConfig& config) override;

	// Evaluation — sets the transient VP matrix, evaluates the OnRenderShape chain
	bool renderShape(const glm::mat4& vpMatrix, NodeEvaluator& evaluator);

	// The texture sources the OnRenderShape chain reads, by component id, found by
	// walking the graph rather than evaluating it
	void gatherReferencedTextureSources(std::set<int>& outTextureSourceIds) const;

	// Binding
	bool bindToShapeComponent(ShapeComponentPtr shapeComponent);
	void unbindFromShapeComponent(ShapeComponentPtr shapeComponent);
	ShapeComponentPtr getBoundShapeComponent() const;

	// Transient VP matrix — set before evaluation, cleared after
	const glm::mat4& getRenderVPMatrix() const { return m_vpMatrix; }

protected:
	bool bindEventNodes();

	ShapeComponentWeakPtr m_boundShapeComponent;
	glm::mat4 m_vpMatrix= glm::mat4(1.f);
	NodePtr m_renderShapeEventNode;

	friend class ShapeNodeGraphFactory;
};

class ShapeNodeGraphFactory : public TypedNodeGraphFactory<ShapeNodeGraph>
{
public:
	ShapeNodeGraphFactory()= default;

	virtual NodeGraphPtr initialCreateNodeGraph(class IEditorWindow* ownerWindow) const override;
};

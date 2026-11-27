#include "ShapeNodeGraph.h"
#include "Logger.h"
#include "NodeEditorState.h"
#include "TextureSourceComponent.h"

// Asset References
#include "ShapeMaterialAssetReference.h"
#include "TextureAssetReference.h"

// Properties
#include "Properties/GraphBoolProperty.h"
#include "Properties/GraphMaterialProperty.h"
#include "Properties/GraphTextureProperty.h"
#include "Properties/GraphTextureSourceProperty.h"

// Nodes
#include "Nodes/ArrayNode.h"
#include "Nodes/DrawShapeMeshNode.h"
#include "Nodes/EventNode.h"
#include "Nodes/MaterialNode.h"
#include "Nodes/MousePosNode.h"
#include "Nodes/TextureNode.h"
#include "Nodes/TextureSourceNode.h"
#include "Nodes/TimeNode.h"

// Graph
#include "NodeEditorState.h"
#include "Graphs/NodeEvaluator.h"

// -- ShapeNodeGraph -----
const std::string ShapeNodeGraph::k_renderShapeEventName= "OnRenderShape";

ShapeNodeGraph::ShapeNodeGraph()
	: NodeGraph()
{
	// Assets this graph can reference
	addAssetReferenceFactory<ShapeMaterialAssetReferenceFactory>();
	addAssetReferenceFactory<TextureAssetReferenceFactory>();

	// Add property types this graph can use
	addPropertyFactory<GraphBoolPropertyFactory>();
	addPropertyFactory<GraphMaterialPropertyFactory>();
	addPropertyFactory<GraphTexturePropertyFactory>();
	addPropertyFactory<GraphTextureSourcePropertyFactory>();

	// Nodes this graph can spawn
	addNodeFactory<ArrayNodeFactory>();
	addNodeFactory<EventNodeFactory>();
	addNodeFactory<MousePosNodeFactory>();
	addNodeFactory<MaterialNodeFactory>();
	addNodeFactory<TextureNodeFactory>();
	addNodeFactory<TextureSourceNodeFactory>();
	addNodeFactory<TimeNodeFactory>();
	addNodeFactory<DrawShapeMeshNodeFactory>();
}

bool ShapeNodeGraph::loadFromConfig(const NodeGraphConfig& config)
{
	bool bSuccess= true;

	if (NodeGraph::loadFromConfig(config))
	{
		bSuccess= bindEventNodes();
	}
	else
	{
		MIKAN_LOG_ERROR("ShapeNodeGraph::loadFromConfig") << "Failed to parse node graph config";
		bSuccess= false;
	}

	return bSuccess;
}

bool ShapeNodeGraph::bindEventNodes()
{
	bool bSuccess= true;

	m_renderShapeEventNode= getEventNodeByName(k_renderShapeEventName);
	if (!m_renderShapeEventNode)
	{
		MIKAN_LOG_ERROR("ShapeNodeGraph::bindEventNodes") << "Failed to find event node: " << k_renderShapeEventName;
		bSuccess= false;
	}

	return bSuccess;
}

bool ShapeNodeGraph::renderShape(const glm::mat4& vpMatrix, NodeEvaluator& evaluator)
{
	if (m_renderShapeEventNode)
	{
		m_vpMatrix= vpMatrix;
		evaluator.evaluateFlowPinChain(m_renderShapeEventNode);
		m_vpMatrix= glm::mat4(1.f);
	}
	else
	{
		evaluator.addError(
			NodeEvaluationError(eNodeEvaluationErrorCode::invalidNode, "Missing OnRenderShape event node"));
	}

	return !evaluator.hasErrors();
}

void ShapeNodeGraph::gatherReferencedTextureSources(std::set<int>& outTextureSourceIds) const
{
	visitNodesReachableFromEvent(m_renderShapeEventNode,
								 [&outTextureSourceIds](NodeConstPtr node)
								 {
									 auto textureSourceNode= std::dynamic_pointer_cast<const TextureSourceNode>(node);
									 if (!textureSourceNode)
										 return;

									 if (TextureSourceComponentPtr textureSource=
											 textureSourceNode->getTextureSourceComponent())
									 {
										 outTextureSourceIds.insert(textureSource->getComponentId());
									 }
								 });
}

bool ShapeNodeGraph::bindToShapeComponent(ShapeComponentPtr shapeComponent)
{
	if (shapeComponent)
	{
		m_boundShapeComponent= shapeComponent;
		return true;
	}

	return false;
}

void ShapeNodeGraph::unbindFromShapeComponent(ShapeComponentPtr shapeComponent)
{
	if (getBoundShapeComponent() == shapeComponent)
	{
		m_boundShapeComponent.reset();
	}
}

ShapeComponentPtr ShapeNodeGraph::getBoundShapeComponent() const { return m_boundShapeComponent.lock(); }

// -- ShapeNodeGraphFactory -----
NodeGraphPtr ShapeNodeGraphFactory::initialCreateNodeGraph(IEditorWindow* ownerWindow) const
{
	auto nodeGraph= NodeGraphFactory::initialCreateNodeGraph(ownerWindow);
	if (!nodeGraph)
		return NodeGraphPtr();

	auto shapeNodeGraph= std::static_pointer_cast<ShapeNodeGraph>(nodeGraph);

	// Add the default OnRenderShape event node
	NodeEditorState editorState;
	editorState.nodeGraph= shapeNodeGraph;

	auto renderShapeNode= shapeNodeGraph->createTypedNode<EventNode>(editorState);
	renderShapeNode->setName(ShapeNodeGraph::k_renderShapeEventName);

	// Store off pointer to the event node we manually created
	shapeNodeGraph->bindEventNodes();

	return shapeNodeGraph;
}

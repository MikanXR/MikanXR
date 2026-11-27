#include "CompositorGraphReferencesTests.h"
#include "unit_test.h"

#include "NodeEditorState.h"
#include "QuadStencilComponent.h"
#include "Graphs/CompositorNodeGraph.h"
#include "Graphs/NodeGraph.h"
#include "Nodes/ArrayNode.h"
#include "Nodes/DepthTextureSourceNode.h"
#include "Nodes/DrawLayerNode.h"
#include "Nodes/EventNode.h"
#include "Nodes/StencilNode.h"
#include "Nodes/WriteDepthNode.h"
#include "Pins/NodePin.h"
#include "Properties/GraphStencilProperty.h"

#include <cstdio>
#include <string>

// The walk reads only nodes, links and the components nodes hold, so every graph
// here is built in memory with no window, no project and no compositor. Loading
// a graph file would create its GL resources, which needs a window. Without a
// project the select nodes answer nothing, so they are left to the editor drive.

namespace
{
// Exposes the event binding that loading a graph file performs
class CompositorGraphUnderTest : public CompositorNodeGraph
{
public:
	using CompositorNodeGraph::bindEventNodes;
};
using CompositorGraphUnderTestPtr= std::shared_ptr<CompositorGraphUnderTest>;

template <class t_node_class>
std::shared_ptr<t_node_class> makeNode(NodeGraphPtr graph)
{
	NodeEditorState editorState;
	editorState.nodeGraph= graph;

	return graph->createTypedNode<t_node_class>(editorState);
}

NodePinPtr findPin(NodePtr node, eNodePinDirection direction, const std::string& pinName)
{
	const std::vector<NodePinPtr>& pins=
		direction == eNodePinDirection::INPUT ? node->getInputPins() : node->getOutputPins();
	for (NodePinPtr pin : pins)
	{
		if (pin->getName() == pinName)
			return pin;
	}

	fprintf(stdout, "    FAILED: no pin %s on %s\n", pinName.c_str(), node->getClassName().c_str());
	return NodePinPtr();
}

bool link(NodeGraphPtr graph, NodePtr fromNode, const std::string& outputPin, NodePtr toNode,
		  const std::string& inputPin)
{
	NodePinPtr from= findPin(fromNode, eNodePinDirection::OUTPUT, outputPin);
	NodePinPtr to= findPin(toNode, eNodePinDirection::INPUT, inputPin);
	if (!from || !to)
		return false;

	return graph->createLink(from->getId(), to->getId()) != nullptr;
}

// A graph whose OnCompositeFrame chain runs one DrawLayerNode
CompositorGraphUnderTestPtr makeLayerGraph(NodePtr& outDrawLayerNode)
{
	auto graph= std::make_shared<CompositorGraphUnderTest>();

	EventNodePtr eventNode= makeNode<EventNode>(graph);
	eventNode->setName(CompositorNodeGraph::k_compositeFrameEventName);
	graph->bindEventNodes();

	outDrawLayerNode= makeNode<DrawLayerNode>(graph);
	link(graph, eventNode, "flowOut", outDrawLayerNode, "flowIn");

	return graph;
}

StencilNodePtr makeStencilNode(NodeGraphPtr graph, MikanStencilID stencilId)
{
	auto stencil= std::make_shared<QuadStencilComponent>(MikanObjectWeakPtr());
	stencil->setDefinition(std::make_shared<QuadStencilDefinition>(stencilId));

	auto stencilProperty= std::make_shared<GraphStencilProperty>();
	stencilProperty->setStencilComponent(stencil);

	StencilNodePtr stencilNode= makeNode<StencilNode>(graph);
	stencilNode->setStencilSource(stencilProperty);

	return stencilNode;
}

// An OnCompositeFrame chain writing the depth of one DepthTextureSourceNode, whose
// config is loaded the way a graph file loads it. Saving drops a source that does
// not resolve, so a stored one only ever arrives through a load.
CompositorGraphUnderTestPtr makeDepthSourceGraph(MikanTextureSourceID storedTextureSourceId,
												 bool bUseFirstClientTextureSource)
{
	CompositorGraphUnderTestPtr graph= std::make_shared<CompositorGraphUnderTest>();

	EventNodePtr eventNode= makeNode<EventNode>(graph);
	eventNode->setName(CompositorNodeGraph::k_compositeFrameEventName);
	graph->bindEventNodes();

	NodePtr writeDepthNode= makeNode<WriteDepthNode>(graph);
	std::shared_ptr<DepthTextureSourceNode> depthSourceNode= makeNode<DepthTextureSourceNode>(graph);
	link(graph, eventNode, "flowOut", writeDepthNode, "flowIn");
	link(graph, depthSourceNode, "texture", writeDepthNode, "depthTexture");

	// No pins listed, so the node keeps the ones its factory made. With no project
	// the stored source cannot resolve.
	auto sourceConfig= std::make_shared<DepthTextureSourceNodeConfig>();
	sourceConfig->id= depthSourceNode->getId();
	sourceConfig->pos= {0.f, 0.f};
	sourceConfig->textureSourceColorType= eTextureSourceDepthType::depthPackRGBA;
	sourceConfig->bVerticalFlip= false;
	sourceConfig->textureVideoSourceId= storedTextureSourceId;
	sourceConfig->bUseFirstClientTextureSource= bUseFirstClientTextureSource;
	depthSourceNode->loadFromConfig(sourceConfig);

	return graph;
}
} // namespace

// -- tests -----

bool compositor_graph_references_test_unlinked_stencil_contributes_nothing()
{
	UNIT_TEST_BEGIN("a stencil node linked to nothing the chain runs contributes nothing")

	NodePtr drawLayerNode;
	CompositorGraphUnderTestPtr graph= makeLayerGraph(drawLayerNode);
	makeStencilNode(graph, 501);

	CompositorGraphReferences references;
	graph->gatherReferencedSceneObjects(references);
	success&= references.stencilIds.empty();
	success&= references.shapeIds.empty();
	success&= references.textureSourceIds.empty();

	UNIT_TEST_COMPLETE()
}

bool compositor_graph_references_test_linked_stencil_contributes_its_id()
{
	UNIT_TEST_BEGIN("a stencil node feeding the chain through an array contributes its id")

	NodePtr drawLayerNode;
	CompositorGraphUnderTestPtr graph= makeLayerGraph(drawLayerNode);
	StencilNodePtr linkedNode= makeStencilNode(graph, 501);
	makeStencilNode(graph, 502);

	NodePtr arrayNode= makeNode<ArrayNode>(graph);
	success&= link(graph, linkedNode, "Stencil", arrayNode, "[0]");
	success&= link(graph, arrayNode, "array", drawLayerNode, "stencils");

	CompositorGraphReferences references;
	graph->gatherReferencedSceneObjects(references);
	success&= references.stencilIds.size() == 1;
	success&= references.stencilIds.count(501) == 1;

	UNIT_TEST_COMPLETE()
}

bool compositor_graph_references_test_unreached_layer_contributes_nothing()
{
	UNIT_TEST_BEGIN("a stencil feeding a layer the chain never runs contributes nothing")

	NodePtr drawLayerNode;
	CompositorGraphUnderTestPtr graph= makeLayerGraph(drawLayerNode);

	// A second layer off the chain, its flow input left open
	NodePtr offChainLayerNode= makeNode<DrawLayerNode>(graph);
	StencilNodePtr stencilNode= makeStencilNode(graph, 501);
	NodePtr arrayNode= makeNode<ArrayNode>(graph);
	success&= link(graph, stencilNode, "Stencil", arrayNode, "[0]");
	success&= link(graph, arrayNode, "array", offChainLayerNode, "stencils");

	CompositorGraphReferences references;
	graph->gatherReferencedSceneObjects(references);
	success&= references.stencilIds.empty();

	UNIT_TEST_COMPLETE()
}

bool compositor_graph_references_test_unresolved_texture_source_counts()
{
	UNIT_TEST_BEGIN("a texture source node naming a source the project lacks counts as unresolved")

	CompositorGraphUnderTestPtr graph= makeDepthSourceGraph(4242, false);

	CompositorGraphReferences references;
	graph->gatherReferencedSceneObjects(references);
	success&= references.unresolvedTextureSourceCount == 1;
	success&= references.textureSourceIds.empty();
	success&= !references.bUsesFirstClientSource;

	// A node never given a source names nothing to miss
	CompositorGraphUnderTestPtr unassignedGraph= makeDepthSourceGraph(INVALID_MIKAN_ID, false);

	CompositorGraphReferences unassignedReferences;
	unassignedGraph->gatherReferencedSceneObjects(unassignedReferences);
	success&= unassignedReferences.unresolvedTextureSourceCount == 0;

	UNIT_TEST_COMPLETE()
}

bool compositor_graph_references_test_first_client_source_is_no_reference()
{
	UNIT_TEST_BEGIN("a texture source node reading the first client names no source of its own")

	// The stored source is ignored while the option is on, so it is not missing either
	CompositorGraphUnderTestPtr graph= makeDepthSourceGraph(4242, true);

	CompositorGraphReferences references;
	graph->gatherReferencedSceneObjects(references);
	success&= references.bUsesFirstClientSource;
	success&= references.unresolvedTextureSourceCount == 0;
	success&= references.textureSourceIds.empty();

	UNIT_TEST_COMPLETE()
}

// -- test module -----

bool run_compositor_graph_references_tests()
{
	UNIT_TEST_MODULE_BEGIN("compositor_graph_references")
	UNIT_TEST_MODULE_CALL_TEST(compositor_graph_references_test_unlinked_stencil_contributes_nothing);
	UNIT_TEST_MODULE_CALL_TEST(compositor_graph_references_test_linked_stencil_contributes_its_id);
	UNIT_TEST_MODULE_CALL_TEST(compositor_graph_references_test_unreached_layer_contributes_nothing);
	UNIT_TEST_MODULE_CALL_TEST(compositor_graph_references_test_unresolved_texture_source_counts);
	UNIT_TEST_MODULE_CALL_TEST(compositor_graph_references_test_first_client_source_is_no_reference);
	UNIT_TEST_MODULE_END()
}

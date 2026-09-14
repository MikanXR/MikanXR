#include "NodeGraphPropertyNameTests.h"
#include "unit_test.h"

#include "Graphs/MaterialNodeGraph.h"
#include "Graphs/NodeGraph.h"
#include "MaterialCompiler/MaterialDomain.h"
#include "Properties/GraphBoolProperty.h"

#include <memory>
#include <stdio.h>
#include <string>

namespace
{
// The naming rules never touch the owner window, so the graph is built with none
NodeGraphPtr makeGraphWithTwoVariables(GraphPropertyPtr& outAlpha, GraphPropertyPtr& outBeta)
{
	MaterialNodeGraphFactory factory;
	NodeGraphPtr graph= factory.initialCreateMaterialGraph(nullptr, eMaterialDomain::compositor);

	outAlpha= graph->createTypedProperty<GraphBoolProperty>();
	outAlpha->setName("alpha");
	outBeta= graph->createTypedProperty<GraphBoolProperty>();
	outBeta->setName("beta");

	return graph;
}
} // namespace

bool run_node_graph_property_name_tests()
{
	UNIT_TEST_MODULE_BEGIN("node_graph_property_name")
	UNIT_TEST_MODULE_CALL_TEST(node_graph_property_name_test_unique_name);
	UNIT_TEST_MODULE_CALL_TEST(node_graph_property_name_test_rename);
	UNIT_TEST_MODULE_END()
}

bool node_graph_property_name_test_unique_name()
{
	UNIT_TEST_BEGIN("a taken name suffixes, a free or own name is kept")

	GraphPropertyPtr alpha;
	GraphPropertyPtr beta;
	NodeGraphPtr graph= makeGraphWithTwoVariables(alpha, beta);

	success&= graph->makeUniquePropertyName("alpha") == "alpha1";
	success&= graph->makeUniquePropertyName("gamma") == "gamma";
	success&= graph->makeUniquePropertyName("alpha", alpha->getId()) == "alpha";
	success&= graph->makeUniquePropertyName("alpha", beta->getId()) == "alpha1";
	success&= graph->makeUniquePropertyName("").empty();

	UNIT_TEST_COMPLETE()
}

bool node_graph_property_name_test_rename()
{
	UNIT_TEST_BEGIN("renameProperty applies the rule and reports real changes only")

	GraphPropertyPtr alpha;
	GraphPropertyPtr beta;
	NodeGraphPtr graph= makeGraphWithTwoVariables(alpha, beta);

	// A duplicate lands suffixed, and the other variable is untouched
	success&= graph->renameProperty(beta->getId(), "alpha");
	success&= beta->getName() == "alpha1";
	success&= alpha->getName() == "alpha";

	// Renaming to the name already held is not a change
	success&= !graph->renameProperty(beta->getId(), "alpha1");
	success&= !graph->renameProperty(alpha->getId(), "alpha");

	// Empty and unknown ids are refused
	success&= !graph->renameProperty(alpha->getId(), "");
	success&= alpha->getName() == "alpha";
	success&= !graph->renameProperty(-42, "delta");

	// A fresh name is kept as typed
	success&= graph->renameProperty(beta->getId(), "gamma");
	success&= beta->getName() == "gamma";

	UNIT_TEST_COMPLETE()
}

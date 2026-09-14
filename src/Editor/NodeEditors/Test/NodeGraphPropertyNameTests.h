#pragma once

// Covers the graph variable naming rules every rename surface shares:
//   - a taken name gets a numeric suffix, a free one is kept
//   - a property renamed to its own name is not suffixed against itself
//   - NodeGraph::renameProperty applies the rule and reports only real changes
bool run_node_graph_property_name_tests();

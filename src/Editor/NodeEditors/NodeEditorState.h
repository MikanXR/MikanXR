#pragma once

#include "NodeFwd.h"
#include "imgui.h"

#include <filesystem>

class NodeEditorState
{
public:
	NodeEditorState()= default;

	NodeGraphPtr nodeGraph;
	std::filesystem::path nodeGraphPath;
	// The page the canvas shows and new nodes land on (0 is the root page)
	t_graph_page_id currentPageId= 0;
	int startedLinkPinId= -1;
	// A link picked up by Ctrl+drag: hidden while carried, and only deleted or
	// rewired when the drag ends
	t_node_link_id detachedLinkId= -1;
	bool bLinkHanged= false;
	ImVec2 hangPosGridSpace= {};
	class MkGuiStyleManager* styleManager= nullptr;
};

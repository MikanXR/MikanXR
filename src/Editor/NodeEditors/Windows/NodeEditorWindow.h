#pragma once

//-- includes -----
#include "AssetFwd.h"
#include "EditorWindow.h"
#include "NodeEditorFwd.h"
#include "NodeFwd.h"
#include "NodeEditorState.h"

#include "Graphs/GraphObjectSelection.h"
#include "MkGuiDrawUtils.h"
#include "Graphs/NodeError.h"
#include "Graphs/NodeGraphHistory.h"
#include "Graphs/NodeGraphLogWriter.h"

#include "MaterialCompiler/MaterialDomain.h"

#include "Properties/GraphArrayProperty.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "glm/ext/vector_float2.hpp"

namespace ax
{
namespace NodeEditor
{
struct EditorContext;
}
} // namespace ax

//-- definitions -----
class NodeEditorWindow : public EditorWindow
{
public:
	NodeEditorWindow(App* ownerApp);
	~NodeEditorWindow();

	inline NodeGraphPtr getNodeGraph() const { return m_editorState.nodeGraph; }
	inline const std::filesystem::path& getNodeGraphPath() const { return m_editorState.nodeGraphPath; }
	virtual NodeGraphFactoryPtr getNodeGraphFactory() const;

	virtual void newGraph();
	virtual bool loadGraph(const std::filesystem::path& path);
	virtual bool saveGraph(bool bShowFileDialog);

	// Schedule undo/redo steps, applied at the top of the next update outside
	// the ImGui frame
	virtual void undo();
	virtual void redo();
	bool canUndo() const { return m_history.canUndo(); }
	bool canRedo() const { return m_history.canRedo(); }
	const NodeGraphHistory& getHistory() const { return m_history; }
	const std::filesystem::path& getGraphLogFilePath() const { return m_logWriter.getLogFilePath(); }

	/// Apply undo (negative) or redo (positive) steps immediately.
	/// Safe only outside the window's ImGui frame; UI paths schedule through
	/// undo()/redo() instead. @returns true if the graph was restored
	bool stepHistory(int steps);

	// -- Pages ----
	// Show the given page on the canvas, dropping the selection and framing the
	// page's nodes. Ignored for the current page and for ids the graph lacks.
	void setCurrentPage(t_graph_page_id pageId);
	inline t_graph_page_id getCurrentPageId() const { return m_editorState.currentPageId; }

	// -- Automation ----
	// Parked work from the automation server, run at the top of the next
	// update where this window's GL and gui contexts can be made current
	void enqueueAutomationTask(std::function<void()>&& task);

	// Graph mutations for automation tasks (push the needed contexts internally)
	bool automationCreateNode(const std::string& nodeClassName, const glm::vec2& gridPos, t_node_id& outNodeId,
							  std::string& outError);
	bool automationDeleteNode(t_node_id nodeId, std::string& outError);
	bool automationCreateLink(t_node_pin_id startPinId, t_node_pin_id endPinId, t_node_link_id& outLinkId,
							  std::string& outError);
	bool automationDeleteLink(t_node_link_id linkId, std::string& outError);
	bool automationSetCurrentPage(t_graph_page_id pageId, std::string& outError);
	bool automationCreatePage(const std::string& pageClassName, t_graph_page_id& outPageId, std::string& outError);
	bool automationDeletePage(t_graph_page_id pageId, std::string& outError);

	// Ask the app to tear this window down at the end of the frame
	void requestClose() { m_bCloseRequested= true; }

	// -- IEditorWindow ----
	virtual bool startup() override;
	virtual void update(float deltaSeconds) override;
	virtual void render() override;
	virtual void shutdown() override;

	virtual bool getIsRenderingStage() const override { return false; }
	virtual IMkViewportPtr getRenderingViewport() const override { return nullptr; }
	virtual bool wantsDestroy() const override;

	// -- IMkWindowEventListener
	virtual bool onWindowEvent(const MkWindowEvent& event) override;

protected:
	virtual void updateUI();

	virtual void renderMainFrame();
	virtual void renderNodeEvalErrors(const std::vector<ImVec2>& errorScreenPositions);
	virtual void handleGraphVariablesDragDrop(const class NodeEditorState& editorState) {}
	virtual void handleMainFrameDragDrop(const class NodeEditorState& editorState) {}
	virtual void renderMainFrameContextMenu(const class NodeEditorState& editorState);
	// The create-node list of the background context menu: a filter box over
	// the valid factories, grouped by category when the filter is empty
	void renderCreateNodeMenu(const class NodeEditorState& editorState);
	// The canvas holds node positions and comment box sizes in scaled pixels, so
	// a UI scale change (the window moved to a display with a different scale, or
	// the scale preference) has to push every stored one again
	void refreshCanvasUiScale();
	void renderMenuBar();
	// Extra menus appended after File/Edit/View (the compositor window's Compositor menu)
	virtual void renderMenuBarExtras() {}
	// Extra items appended to the View menu's panel toggles
	virtual void renderViewMenuExtras() {}
	void buildDefaultDockLayout(unsigned int dockspaceId);
	// Dock subclass panels before the base panels claim their nodes. The right
	// column id is passed by reference so a split can leave its remainder to Details.
	virtual void dockExtraPanels(unsigned int& rightId) {}
	virtual void renderGraphVariablesPanel();
	virtual void renderNewGraphVariablesContextMenu(const NodeEditorState& editorState);
	// The page list (root first) with the page factories' add buttons below it
	virtual void renderPagesPanel();
	virtual void renderAssetsPanel();
	virtual void renderSelectedObjectPanel();
	void renderVariableNameField(GraphPropertyPtr property);
	// F2: the selected variable or page gets its inline rename field
	void beginSelectedObjectRename();

	virtual void deleteSelectedItem();

	virtual void onNodeGraphCreated();
	virtual void onNodeGraphDeleted();
	virtual void onNodeCreated(t_node_id id);
	virtual void onNodeDeleted(t_node_id id);
	virtual void onLinkCreated(t_node_link_id id);
	virtual void onLinkDeleted(t_node_link_id id);
	virtual void onPinCreated(t_node_pin_id id);
	virtual void onPinDeleted(t_node_pin_id id);
	virtual void onGraphPropertyCreated(t_graph_property_id id);
	virtual void onGraphPropertyModified(t_graph_property_id id);
	virtual void onGraphPropertyDeleted(t_graph_property_id id);

	virtual void onAssetReferenceCreated(AssetReferencePtr assetRef);
	virtual void onAssetReferenceDeleted(AssetReferencePtr assetRef);
	virtual void onPageCreated(t_graph_page_id id);
	virtual void onPageModified(t_graph_page_id id);
	virtual void onPageDeleted(t_graph_page_id id);

	// Rebind the restored graph to its owning component after an undo/redo
	// rebuilds the graph instance
	virtual void onGraphRestored() {}

	// Called once an edit has settled into a committed history checkpoint
	virtual void onGraphEdited() {}

	// The file kind this editor saves: its extension, and the localization keys
	// of the save dialog title and filter description. Directory the save dialog
	// opens in when the graph has no path yet.
	virtual const char* getGraphFileExtension() const= 0;
	virtual const char* getSaveDialogTitleKey() const= 0;
	virtual const char* getGraphFilterDescriptionKey() const= 0;
	virtual std::filesystem::path getDefaultGraphDirectory() const;

	// The window title's localization key, and the ini the window's dock
	// layout persists to (one per editor kind, so each keeps its own layout)
	virtual const char* getWindowTitleKey() const { return "windows.nodeEditor"; }
	virtual std::string getGuiIniName() const { return "node_editor"; }
	// Whether this editor kind shows a Pages panel. Decided per window rather
	// than per loaded graph so the panel is submitted from the first frame: a
	// dock node left empty while a graph loads would be dropped by ImGui.
	virtual bool hasPagesPanel() const { return false; }
	// The Pages panel's title key (a subclass names its pages for what they hold)
	virtual const char* getPagesPanelTitleKey() const { return "windows.nodePagesPanel"; }

	// Install a freshly created graph (the creator runs with this window's GL
	// context current), then reset the history baseline
	void createNewGraph(const std::function<NodeGraphPtr()>& createGraph);

	// Fall back to the root page when the installed graph lacks the current one
	// (an undo inside a page that survives the step stays on that page)
	void syncCurrentPageToGraph();

	// -- Undo history ----
	void markHistoryCheckpoint() { m_bCheckpointPending= true; }
	void updateHistoryCapture();
	bool restoreGraphSnapshot(const std::string& snapshot);
	void flushAutomationTasks();

	// Open the session log (first graph only) and record the graph baseline
	void logGraphBaseline();

	// Clear the canvas node/link selection from outside the canvas frame scope
	void clearCanvasSelection();

protected:
	NodeEditorState m_editorState;

	// The canvas view state (pan/zoom/selection) for this window's graph
	ax::NodeEditor::EditorContext* m_canvasContext= nullptr;
	// The UI scale the canvas positions were last pushed at
	float m_appliedCanvasUiScale= 0.f;
	// Frame the current page's nodes on the next canvas frame (navigation needs
	// the editor current and the page's nodes submitted)
	bool m_bNavigateToContentPending= false;

	GraphObjectSelection m_objectSelection;

	// Ctrl+drag link detach in flight: the pin the link was picked up from and
	// the far pin the drag now hangs off. The link itself is in the editor state.
	t_node_pin_id m_detachHeldPinId= -1;
	t_node_pin_id m_detachAnchorPinId= -1;
	void clearLinkDetach();

	// The pin whose right-click menu is open
	t_node_pin_id m_contextPinId= -1;

	// Errors that occurred during the last graph evaluation
	std::vector<NodeEvaluationError> m_lastNodeEvalErrors;

	// Undo history: whole-graph snapshots committed once per quiescent frame
	NodeGraphHistory m_history;
	bool m_bCheckpointPending= false;
	bool m_bAnyItemActiveLastFrame= false;
	int m_pendingHistorySteps= 0;
	bool m_bApplyingHistory= false;

	// JSONL session log of the graph edits, for post-session diagnosis
	NodeGraphLogWriter m_logWriter;
	int64_t m_nextLogSequenceNumber= 1;

	// Parked automation work (see enqueueAutomationTask)
	std::vector<std::function<void()>> m_automationTasks;

	bool m_bCloseRequested= false;

	// Rename field state for the selected graph variable
	char m_variableNameBuffer[256]= {};
	t_graph_property_id m_variableNameBufferId= -1;
	bool m_bVariableNameFieldActive= false;

	// Inline rename of a Variables row and of a Pages row, with the row a
	// rename click was pressed on
	MkGui::InlineRenameState m_variableRename;
	t_graph_property_id m_variableRenamePressedId= -1;
	MkGui::InlineRenameState m_pageRename;
	t_graph_page_id m_pageRenamePressedId= -1;

	// Filter text of the create-node context menu, cleared each time it opens
	char m_nodeSearchBuffer[64]= {};

	// Dockable panel visibility, toggled from the View menu (not persisted;
	// the dock geometry itself persists via the window's imgui ini)
	bool m_bShowVariablesPanel= true;
	bool m_bShowPagesPanel= true;
	bool m_bShowAssetsPanel= true;
	bool m_bShowDetailsPanel= true;
	bool m_bShowIdStackTool= false; // localization diagnostic, off by default
	// View > Reset Layout: rebuild the default dock layout on the next frame
	bool m_bResetLayoutRequested= false;
};

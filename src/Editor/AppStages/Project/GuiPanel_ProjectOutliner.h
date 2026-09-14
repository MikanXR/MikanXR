#pragma once

#include "CommonConfigFwd.h"
#include "ComponentFwd.h"
#include "IMkGuiStyle.h"
#include "MkGuiDrawUtils.h"
#include "ObjectFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectSystemFwd.h"
#include "ProjectOutlinerModel.h"
#include "Shared/GuiPanel.h"

#include <string>
#include <vector>

class ConfigPropertyChangeSet;

// The unified project panel: a tree over every project object, and for the
// selected node a delete button, the valid add-child buttons, and the bound
// component property panel.
class GuiPanel_ProjectOutliner : public GuiPanel
{
public:
	GuiPanel_ProjectOutliner(class AppStage* ownerAppStage);

	bool init(class ProjectGuiPanelContext* context);
	virtual void onGui() override;
	virtual void dispose() override;

	// Drives the viewport render mode and selection filter in the app stage
	eOutlinerNodeKind getSelectedNodeKind() const { return m_selectedKind; }

	// Select a component's row from another panel, scrolling the tree to it.
	// Call outside the ImGui frame (from a deferred gui event).
	void selectComponent(int componentId);

private:
	void rebuildIfDirty();
	void markTreeDirty() { m_bTreeDirty= true; }

	// Tree drawing
	void drawTree();
	// The drag bar between the tree and the action strip, which sets the tree height
	void drawTreeSplitter(EditorObjectSystemDefinitionPtr editorConfig, float maxTreeHeight);
	void drawNode(ProjectOutlinerNodePtr node);
	void handleNodeDragDrop(ProjectOutlinerNodePtr node);

	// Selected-node action strip and bound component panel
	void drawSelectedNodeActions(ProjectOutlinerNodePtr selectedNode);
	void drawSceneActorAddButtons(ProjectOutlinerNodePtr selectedNode);
	// One add button per row: the glyph of the type being created, with its
	// name beside it
	bool drawAddButton(const char* fieldName, const char* glyph, const char* labelKey);
	// Whether the row deletes: a real component that is not a camera-owned probe
	bool canDeleteNode(ProjectOutlinerNodePtr node) const;
	void requestDeleteNode(ProjectOutlinerNodePtr node);
	// Delete and Backspace: polled from ImGui while it holds the keyboard, and
	// raised by the editor system when it does not, so exactly one path fires
	void handleDeleteShortcut();
	// F2 opens the inline rename field on the selected component row
	void handleRenameShortcut();
	void beginRename(MikanComponentPtr component);
	void onDeleteSelectionRequested();
	int getAddParentTransformId(ProjectOutlinerNodePtr selectedNode) const;
	void deferAddAction(std::function<int(ProjectManagerPtr)> addAction);

	// Selection plumbing
	ProjectOutlinerNodePtr getSelectedNode() const;
	void setSelectedNode(ProjectOutlinerNodePtr node, bool bPushViewportSelection);
	class GuiPanel_MikanComponent* getPanelForComponentClass(const std::string& componentClassName) const;
	void clearComponentPanels();
	void drawComponentPanelForNode(ProjectOutlinerNodePtr node);
	// Open the path above the node and scroll it into view on the next draw
	void scrollToNode(ProjectOutlinerNodePtr node);
	// The project's editor settings, which hold the tree's open state
	EditorObjectSystemDefinitionPtr getEditorConfig() const;

	// Active scene and active display compositor highlighting
	bool isActiveHighlightNode(ProjectOutlinerNodePtr node) const;

	// Object system delegate handlers
	void onObjectInitialized(MikanObjectSystemPtr objectSystemPtr, MikanObjectPtr objectPtr);
	void onObjectDisposed(MikanObjectSystemPtr objectSystemPtr, MikanObjectConstPtr objectPtr);
	void onSystemConfigChanged(CommonConfigPtr configPtr, const ConfigPropertyChangeSet& changedPropertySet);
	void onSelectionChanged();
	void subscribeToSystems();
	void unsubscribeFromSystems();

	class ProjectGuiPanelContext* m_context= nullptr;
	ProjectManagerWeakPtr m_projectManager;
	EditorObjectSystemWeakPtr m_editorSystem;
	SceneObjectSystemWeakPtr m_sceneSystem;
	std::vector<MikanObjectSystemWeakPtr> m_subscribedSystems;

	ProjectOutlinerModel m_model;
	bool m_bTreeDirty= true;

	// Selection survives rebuilds by component id, not by node pointer. A
	// selected folder row has no component id and keys on kind plus owner id.
	int m_selectedComponentId= -1;
	eOutlinerNodeKind m_selectedKind= eOutlinerNodeKind::projectRoot;
	int m_selectedOwnerId= -1;
	int m_pendingSelectComponentId= -1;

	// Viewport pick to tree sync: scroll to the row on the next draw
	bool m_bScrollToSelection= false;

	// Inline rename of a component row, keyed by component id since a rename
	// rebuilds the tree under it
	MkGui::InlineRenameState m_rename;
	int m_renamePressedComponentId= -1;

	// The tree height follows the project setting except while the splitter
	// is held, when the drag drives it and the release writes it back
	float m_treeHeight= 300.f;
	bool m_bTreeHeightDragging= false;

	MkGuiStyleConstPtr m_outlinerGuiStyle;
	MkGuiStyleConstPtr m_deleteButtonGuiStyle;
};

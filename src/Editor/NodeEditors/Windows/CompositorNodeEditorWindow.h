#pragma once

#include "ComponentFwd.h"
#include "Graphs/NodeGraphFileTypes.h"
#include "Windows/NodeEditorWindow.h"

class CompositorNodeEditorWindow : public NodeEditorWindow
{
public:
	CompositorNodeEditorWindow(class App* ownerApp);

	// -- IEditorWindow ----
	virtual bool startup() override;
	virtual void update(float deltaSeconds) override;
	virtual void shutdown() override;

	// -- NodeEditorWindow ----
	virtual NodeGraphFactoryPtr getNodeGraphFactory() const override;
	virtual bool saveGraph(bool bShowFileDialog) override;

	virtual void handleGraphVariablesDragDrop(const class NodeEditorState& editorState) override;
	virtual void handleMainFrameDragDrop(const class NodeEditorState& editorState) override;
	virtual const char* getGraphFileExtension() const override
	{
		return NodeGraphFileTypes::k_compositorGraphExtension;
	}
	virtual const char* getSaveDialogTitleKey() const override { return "nodeEditor.saveCompositorGraphDialogTitle"; }
	virtual const char* getGraphFilterDescriptionKey() const override
	{
		return "nodeEditor.compositorGraphFilesFilterDescription";
	}
	virtual std::filesystem::path getDefaultGraphDirectory() const override;

	// -- CompositorNodeEditorWindow ----
	bool bindCompositorComponent(CompositorComponentPtr compositorComponent);
	void unbindCompositorComponent();
	// Shows the graph a compositor drives, or a graph file no compositor drives
	// (edited on its own, with nothing evaluating it)
	bool openCompositorComponent(CompositorComponentPtr compositorComponent);
	bool openGraphFile(const std::filesystem::path& graphPath);

	// Editor pause of compositor evaluation (the Compositor menu's Run item)
	bool setCompositorRunning(bool bRunning);
	bool isCompositorRunning() const;

protected:
	virtual void onGraphRestored() override;
	virtual void renderMenuBarExtras() override;

protected:
	CompositorComponentPtr m_compositorComponent;
};
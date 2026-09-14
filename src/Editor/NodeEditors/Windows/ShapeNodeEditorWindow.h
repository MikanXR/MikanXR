#pragma once

#include "ComponentFwd.h"
#include "Graphs/NodeGraphFileTypes.h"
#include "Windows/NodeEditorWindow.h"

class ShapeNodeEditorWindow : public NodeEditorWindow
{
public:
	ShapeNodeEditorWindow(class App* ownerApp);

	// -- IEditorWindow ----
	virtual bool startup() override;
	virtual void update(float deltaSeconds) override;
	virtual void shutdown() override;

	// -- NodeEditorWindow ----
	virtual NodeGraphFactoryPtr getNodeGraphFactory() const override;
	virtual bool saveGraph(bool bShowFileDialog) override;

	virtual void handleGraphVariablesDragDrop(const class NodeEditorState& editorState) override;
	virtual void handleMainFrameDragDrop(const class NodeEditorState& editorState) override;
	virtual const char* getGraphFileExtension() const override { return NodeGraphFileTypes::k_shapeGraphExtension; }
	virtual const char* getSaveDialogTitleKey() const override { return "nodeEditor.saveShapeGraphDialogTitle"; }
	virtual const char* getGraphFilterDescriptionKey() const override
	{
		return "nodeEditor.shapeGraphFilesFilterDescription";
	}
	virtual std::filesystem::path getDefaultGraphDirectory() const override;

	// -- ShapeNodeEditorWindow ----
	bool bindShapeComponent(ShapeComponentPtr shapeComponent);
	void unbindShapeComponent();
	// Shows the graph a shape drives, or a graph file no shape drives (edited on
	// its own, with nothing evaluating it)
	bool openShapeComponent(ShapeComponentPtr shapeComponent);
	bool openGraphFile(const std::filesystem::path& graphPath);

protected:
	virtual void onGraphRestored() override;

	ShapeComponentPtr m_shapeComponent;
};

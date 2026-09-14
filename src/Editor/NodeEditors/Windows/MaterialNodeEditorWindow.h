#pragma once

#include "Windows/NodeEditorWindow.h"
#include "Windows/MaterialPreviewPanel.h"
#include "Graphs/MaterialNodeGraph.h"

#include <filesystem>
#include <functional>

// Editor window for a material graph. Unlike the compositor and shape windows
// it binds to no component: the graph compiles to shader source and a material
// file beside the graph file, and those files load through the ordinary .mat path.
class MaterialNodeEditorWindow : public NodeEditorWindow
{
public:
	MaterialNodeEditorWindow(class App* ownerApp);

	// -- IEditorWindow ----
	virtual void update(float deltaSeconds) override;
	virtual void render() override;
	virtual void shutdown() override;

	// -- NodeEditorWindow ----
	virtual NodeGraphFactoryPtr getNodeGraphFactory() const override;
	virtual void newGraph() override;
	virtual bool saveGraph(bool bShowFileDialog) override;

	virtual void handleGraphVariablesDragDrop(const class NodeEditorState& editorState) override;
	virtual void handleMainFrameDragDrop(const class NodeEditorState& editorState) override;

	// -- MaterialNodeEditorWindow ----
	MaterialNodeGraphPtr getMaterialNodeGraph() const;

	void newMaterialGraph(eMaterialDomain domain);
	bool openMaterialGraph(const std::filesystem::path& graphPath);
	// The one material editor window, created or brought forward, on a fresh
	// graph of the domain (the Assets panel's New material buttons)
	static MaterialNodeEditorWindow* openOnNewMaterial(class App* app, eMaterialDomain domain);

	// Compile the graph and refresh the error overlay and preview, writing nothing
	MaterialCompileResult compileGraph();
	// Compile, then write the shader and material files beside the graph file
	// when the graph has a path and compiled without errors
	bool compileAndWriteOutputs();
	// Whether the graph's file sits under its domain's material folder, which is
	// where its compiled material file is allowed to land
	bool isGraphInDomainFolder(const std::filesystem::path& graphPath) const;

protected:
	virtual void updateUI() override;
	virtual void renderMenuBarExtras() override;
	virtual void renderViewMenuExtras() override;
	virtual void dockExtraPanels(unsigned int& rightId) override;
	virtual void onNodeGraphCreated() override;
	virtual void onNodeGraphDeleted() override;
	virtual void onGraphRestored() override;
	virtual void onGraphEdited() override;
	virtual const char* getGraphFileExtension() const override;
	virtual const char* getSaveDialogTitleKey() const override { return "nodeEditor.saveMaterialGraphDialogTitle"; }
	virtual const char* getGraphFilterDescriptionKey() const override
	{
		return "nodeEditor.materialGraphFilesFilterDescription";
	}
	virtual std::filesystem::path getDefaultGraphDirectory() const override;
	virtual const char* getWindowTitleKey() const override { return "windows.materialEditor"; }
	virtual std::string getGuiIniName() const override { return "material_editor"; }
	virtual bool hasPagesPanel() const override { return true; }
	virtual const char* getPagesPanelTitleKey() const override { return "windows.materialFunctionsPanel"; }

	void onDomainChanged();
	// A function page came, went, or changed its signature under the compiled program
	void onPageEdited(t_graph_page_id id);

protected:
	MaterialPreviewPanel m_previewPanel;
	bool m_bShowPreviewPanel= true;
	// The update delta, carried into render() where the preview animates
	float m_lastDeltaSeconds= 0.f;
};

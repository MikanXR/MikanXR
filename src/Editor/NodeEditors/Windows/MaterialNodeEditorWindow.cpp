//-- includes -----
#include "App.h"
#include "AssetReference.h"
#include "Logger.h"
#include "LocText.h"
#include "MaterialNodeEditorWindow.h"
#include "MikanModelResourceManager.h"
#include "MikanShaderCache.h"
#include "MkGuiDockspace.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiScopedStyle.h"
#include "MkGuiScopedWindow.h"
#include "MkGuiStyleManager.h"
#include "PathUtils.h"
#include "ProjectAssetCatalog.h"

#include "Graphs/NodeGraphFileTypes.h"
#include "MaterialCompiler/GlslShaderWriter.h"
#include "MaterialCompiler/MaterialCompiler.h"
#include "Nodes/Material/ShaderNodeUtils.h"
#include "Nodes/Material/ShaderTextureParameterNode.h"
#include "TextureAssetReference.h"

#include "imgui.h"

MaterialNodeEditorWindow::MaterialNodeEditorWindow(App* ownerApp)
	: NodeEditorWindow(ownerApp)
{
}

// -- IEditorWindow ----
void MaterialNodeEditorWindow::update(float deltaSeconds)
{
	m_lastDeltaSeconds= deltaSeconds;

	NodeEditorWindow::update(deltaSeconds);
}

void MaterialNodeEditorWindow::render()
{
	// The preview draws into its own framebuffer before the ImGui pass samples it
	m_previewPanel.render(m_graphicsContext.get(), m_textureCache.get(), m_modelResourceManager.get(),
						  m_lastDeltaSeconds);

	NodeEditorWindow::render();
}

void MaterialNodeEditorWindow::shutdown()
{
	// The preview holds meshes and a model owned by the resource manager the base tears down
	m_previewPanel.dispose();

	NodeEditorWindow::shutdown();
}

// -- NodeEditorWindow ----
NodeGraphFactoryPtr MaterialNodeEditorWindow::getNodeGraphFactory() const
{
	return std::make_shared<MaterialNodeGraphFactory>();
}

void MaterialNodeEditorWindow::newGraph() { newMaterialGraph(eMaterialDomain::compositor); }

bool MaterialNodeEditorWindow::saveGraph(bool bShowFileDialog)
{
	// A graph whose domain no longer matches the folder it was saved in (the
	// domain flipped, or a legacy location) saves as a new material under the
	// domain's folder rather than writing the other domain's file beside it
	const std::filesystem::path resolvedPath= PathUtils::resolveProjectResource(getNodeGraphPath());
	if (!resolvedPath.empty() && !isGraphInDomainFolder(resolvedPath))
	{
		bShowFileDialog= true;
	}

	if (!NodeEditorWindow::saveGraph(bShowFileDialog))
	{
		return false;
	}

	// The written graph is only useful once its shaders and material file sit beside it
	return compileAndWriteOutputs();
}

bool MaterialNodeEditorWindow::isGraphInDomainFolder(const std::filesystem::path& graphPath) const
{
	MaterialNodeGraphPtr materialGraph= getMaterialNodeGraph();
	if (!materialGraph)
	{
		return true;
	}

	// <root>/<domain folder>/<material>/<material>.matgraph
	const std::filesystem::path materialFolder= graphPath.lexically_normal().parent_path();
	return materialFolder.parent_path().filename()
		   == MaterialDomainUtils::materialFolderName(materialGraph->getDomain());
}

void MaterialNodeEditorWindow::handleGraphVariablesDragDrop(const NodeEditorState& editorState)
{
	// A material graph has no graph variables: its inputs are parameter nodes on the canvas
}

void MaterialNodeEditorWindow::handleMainFrameDragDrop(const NodeEditorState& editorState)
{
	// A texture asset dropped on the canvas becomes a texture parameter with that asset as its
	// default. The asset's own drop handler builds the shape graph's texture property and node,
	// neither of which exists here.
	if (auto assetRef= MkGui::receiveTypedDragDropPayload<AssetReference>(TextureAssetReference::k_assetClassName))
	{
		auto parameterNode= editorState.nodeGraph->createTypedNode<ShaderTextureParameterNode>(editorState);
		if (parameterNode)
		{
			parameterNode->setDefaultTexturePath(PathUtils::makeStoredProjectPath(assetRef->getInternalAssetPath()));

			const std::string stem= assetRef->getInternalAssetPath().stem().string();
			if (ShaderNodeUtils::isValidIdentifier(stem))
			{
				parameterNode->setParameterName(stem);
			}
		}
	}
}

// -- MaterialNodeEditorWindow ----
MaterialNodeGraphPtr MaterialNodeEditorWindow::getMaterialNodeGraph() const
{
	NodeGraphPtr nodeGraph= getNodeGraph();
	if (nodeGraph && nodeGraph->getClassName() == MaterialNodeGraph::k_graphClassName)
	{
		return std::static_pointer_cast<MaterialNodeGraph>(nodeGraph);
	}

	return MaterialNodeGraphPtr();
}

MaterialNodeEditorWindow* MaterialNodeEditorWindow::openOnNewMaterial(App* app, eMaterialDomain domain)
{
	MaterialNodeEditorWindow* materialWindow= app->getWindowOfType<MaterialNodeEditorWindow>();
	if (materialWindow != nullptr)
	{
		materialWindow->getMkWindowContext()->raiseWindow();
	}
	else
	{
		materialWindow= app->createAppWindow<MaterialNodeEditorWindow>();
	}

	if (materialWindow != nullptr)
	{
		materialWindow->newMaterialGraph(domain);
	}

	return materialWindow;
}

void MaterialNodeEditorWindow::newMaterialGraph(eMaterialDomain domain)
{
	createNewGraph([this, domain]() { return MaterialNodeGraphFactory().initialCreateMaterialGraph(this, domain); });

	compileGraph();
}

bool MaterialNodeEditorWindow::openMaterialGraph(const std::filesystem::path& graphPath)
{
	// The loader keys on the file's class name, so a graph of another kind can arrive here
	const bool bLoaded= loadGraph(graphPath);
	if (bLoaded && !getMaterialNodeGraph())
	{
		MIKAN_LOG_ERROR("MaterialNodeEditorWindow::openMaterialGraph")
			<< "Not a material graph: " << graphPath.string();
	}

	// The window always shows a material graph, so a failed open falls back to an empty one
	if (!getMaterialNodeGraph())
	{
		newMaterialGraph(eMaterialDomain::compositor);
		return false;
	}

	compileGraph();

	return true;
}

MaterialCompileResult MaterialNodeEditorWindow::compileGraph()
{
	MaterialNodeGraphPtr materialGraph= getMaterialNodeGraph();
	if (!materialGraph)
	{
		m_lastNodeEvalErrors.clear();
		return MaterialCompileResult();
	}

	GlslShaderWriter writer;
	MaterialCompileResult result= materialGraph->compile(writer);

	// The compile errors stand in for eval errors on the canvas overlay until the next compile
	m_lastNodeEvalErrors= result.errors;

	m_previewPanel.setCompileResult(result, materialGraph->getDomain(), materialGraph->getVertexPreset());

	return result;
}

bool MaterialNodeEditorWindow::compileAndWriteOutputs()
{
	MaterialCompileResult result= compileGraph();
	if (result.hasErrors())
	{
		return false;
	}

	// An unsaved graph compiles for its errors but has nowhere to write
	const std::filesystem::path graphPath= PathUtils::resolveProjectResource(getNodeGraphPath());
	if (!getMaterialNodeGraph() || graphPath.empty())
	{
		return false;
	}

	// The outputs land beside the graph, so a read-only bundled graph has to be
	// saved into the project first (Save redirects there on its own)
	ProjectAssetCatalog* catalog= getAssetCatalog();
	if (catalog != nullptr && catalog->isReadOnlyPath(graphPath))
	{
		MIKAN_LOG_INFO("MaterialNodeEditorWindow::compileAndWriteOutputs")
			<< "Bundled material is read only, save it into the project first: " << graphPath.string();
		return false;
	}

	// The material file's extension names its domain, so it only belongs under
	// that domain's folder (Save moves the graph there on its own)
	if (!isGraphInDomainFolder(graphPath))
	{
		MIKAN_LOG_INFO("MaterialNodeEditorWindow::compileAndWriteOutputs")
			<< "Material graph is not under its domain's folder, save it there first: " << graphPath.string();
		return false;
	}

	std::string error;
	if (!MaterialCompiler::writeOutputs(result, graphPath, error))
	{
		MIKAN_LOG_ERROR("MaterialNodeEditorWindow::compileAndWriteOutputs")
			<< "Failed to write material outputs for " << graphPath.string() << ": " << error;
		return false;
	}

	// Every window caches materials separately, so each one reloads the rewritten material
	const std::filesystem::path materialPath=
		MaterialCompiler::getMaterialPathForGraph(graphPath, getMaterialNodeGraph()->getDomain());
	for (EditorWindow* window : App::getInstance()->getAppWindows())
	{
		MikanModelResourceManager* modelResourceManager= window->getModelResourceManager();
		MikanShaderCache* shaderCache= modelResourceManager ? modelResourceManager->getShaderCache() : nullptr;
		if (shaderCache)
		{
			shaderCache->reloadMaterialByPath(materialPath);
		}
	}

	// The written material may be one the project did not have before
	if (ProjectAssetCatalog* catalog= getAssetCatalog())
	{
		catalog->refresh();
	}

	return true;
}

void MaterialNodeEditorWindow::updateUI()
{
	NodeEditorWindow::updateUI();

	// The dockspace was submitted by the base pass, so the preview docks into
	// the node the layout assigned it
	if (m_bShowPreviewPanel)
	{
		MkGuiScopedStyle baseStyle(m_styleManager->getStyle("node_editor_base"));
		MkGuiScopedWindow previewWindow(locWindowTitle("windows.materialPreviewPanel"), &m_bShowPreviewPanel);
		if (previewWindow)
		{
			m_previewPanel.renderUi(m_styleManager.get());
		}
	}

	// Ctrl+B compiles, mirroring the base window's Ctrl+S handling
	const ImGuiIO& io= ImGui::GetIO();
	if (ImGui::IsKeyPressed(ImGuiKey_B, false) && io.KeyCtrl && !ImGui::IsAnyItemActive())
	{
		compileAndWriteOutputs();
	}
}

void MaterialNodeEditorWindow::renderMenuBarExtras()
{
	if (ImGui::BeginMenu(locLabel("materialEditor.materialMenu")))
	{
		if (ImGui::MenuItem(locLabel("materialEditor.compile"), "Ctrl+B"))
		{
			compileAndWriteOutputs();
		}

		MaterialNodeGraphPtr materialGraph= getMaterialNodeGraph();
		if (ImGui::BeginMenu(locLabel("materialEditor.domainMenu"), materialGraph != nullptr))
		{
			const eMaterialDomain currentDomain= materialGraph->getDomain();

			if (ImGui::MenuItem(locLabel("materialEditor.domainCompositor"), nullptr,
								currentDomain == eMaterialDomain::compositor))
			{
				materialGraph->setDomain(eMaterialDomain::compositor);
			}
			if (ImGui::MenuItem(locLabel("materialEditor.domainShape"), nullptr,
								currentDomain == eMaterialDomain::shape))
			{
				materialGraph->setDomain(eMaterialDomain::shape);
			}

			ImGui::EndMenu();
		}

		ImGui::EndMenu();
	}
}

void MaterialNodeEditorWindow::renderViewMenuExtras()
{
	ImGui::MenuItem(locLabel("nodeEditor.preview"), nullptr, &m_bShowPreviewPanel);
}

void MaterialNodeEditorWindow::dockExtraPanels(unsigned int& rightId)
{
	// The preview sits above the Details panel in the right column
	ImGuiID remaining= (ImGuiID)rightId;
	const ImGuiID previewId= MkGui::dockBuilderSplit(remaining, ImGuiDir_Up, 0.45f, remaining);

	MkGui::dockBuilderDockWindow(locWindowTitle("windows.materialPreviewPanel"), previewId);
	rightId= (unsigned int)remaining;
}

void MaterialNodeEditorWindow::onNodeGraphCreated()
{
	NodeEditorWindow::onNodeGraphCreated();

	MaterialNodeGraphPtr materialGraph= getMaterialNodeGraph();
	if (materialGraph)
	{
		materialGraph->OnDomainChanged+= MakeDelegate(this, &MaterialNodeEditorWindow::onDomainChanged);
		materialGraph->OnPageCreated+= MakeDelegate(this, &MaterialNodeEditorWindow::onPageEdited);
		materialGraph->OnPageModified+= MakeDelegate(this, &MaterialNodeEditorWindow::onPageEdited);
		materialGraph->OnPageDeleted+= MakeDelegate(this, &MaterialNodeEditorWindow::onPageEdited);
	}
}

void MaterialNodeEditorWindow::onNodeGraphDeleted()
{
	MaterialNodeGraphPtr materialGraph= getMaterialNodeGraph();
	if (materialGraph)
	{
		materialGraph->OnDomainChanged-= MakeDelegate(this, &MaterialNodeEditorWindow::onDomainChanged);
		materialGraph->OnPageCreated-= MakeDelegate(this, &MaterialNodeEditorWindow::onPageEdited);
		materialGraph->OnPageModified-= MakeDelegate(this, &MaterialNodeEditorWindow::onPageEdited);
		materialGraph->OnPageDeleted-= MakeDelegate(this, &MaterialNodeEditorWindow::onPageEdited);
	}

	NodeEditorWindow::onNodeGraphDeleted();
}

void MaterialNodeEditorWindow::onDomainChanged()
{
	// The output node and preset changed under the compiled program
	compileGraph();
}

void MaterialNodeEditorWindow::onPageEdited(t_graph_page_id id)
{
	// The error overlay follows function edits without waiting for the checkpoint
	compileGraph();
}

void MaterialNodeEditorWindow::onGraphRestored()
{
	// The overlay must describe the restored graph, not the one it replaced
	compileGraph();
}

void MaterialNodeEditorWindow::onGraphEdited() { compileGraph(); }

const char* MaterialNodeEditorWindow::getGraphFileExtension() const
{
	return NodeGraphFileTypes::k_materialGraphExtension;
}

std::filesystem::path MaterialNodeEditorWindow::getDefaultGraphDirectory() const
{
	MaterialNodeGraphPtr materialGraph= getMaterialNodeGraph();
	if (!materialGraph)
	{
		return NodeEditorWindow::getDefaultGraphDirectory();
	}

	return PathUtils::getProjectDirectory() / MaterialDomainUtils::materialFolderName(materialGraph->getDomain());
}

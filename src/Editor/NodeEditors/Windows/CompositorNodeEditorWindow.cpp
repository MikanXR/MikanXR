//-- includes -----
#include "App.h"
#include "AssetReference.h"
#include "CompositorComponent.h"
#include "CompositorObjectSystem.h"
#include "CompositorNodeEditorWindow.h"
#include "PathUtils.h"
#include "Logger.h"
#include "LocText.h"
#include "MkGuiScopedChild.h"
#include "MkGuiScopedStyle.h"
#include "MkGuiStyleManager.h"
#include "MkGuiDrawUtils.h"

#include "Graphs/CompositorNodeGraph.h"
#include "Graphs/NodeEvaluator.h"

#include "IconsForkAwesome.h"

CompositorNodeEditorWindow::CompositorNodeEditorWindow(App* ownerApp)
	: NodeEditorWindow(ownerApp)
{
}

// -- IEditorWindow ----
bool CompositorNodeEditorWindow::startup()
{
	// Start the node editor window
	if (!NodeEditorWindow::startup())
	{
		return false;
	}

	return true;
}

bool CompositorNodeEditorWindow::bindCompositorComponent(CompositorComponentPtr compositorComponent)
{
	assert(compositorComponent);
	m_compositorComponent= compositorComponent;

	// Load the graph from the asset path on the main window's frame compositor (if any)
	auto graphAssetPath= m_compositorComponent->getCompositorDefinition()->getCompositorGraphPath();
	if (!graphAssetPath.empty() && !loadGraph(graphAssetPath))
	{
		return false;
	}

	// Create a new graph if none was loaded
	if (!m_editorState.nodeGraph)
	{
		newGraph();
	}

	// Tell the new node graph about the compositor component it's bound to
	auto compositorNodeGraph= std::static_pointer_cast<CompositorNodeGraph>(m_editorState.nodeGraph);
	compositorNodeGraph->bindToCompositorComponent(m_compositorComponent);

	// Tell the compositor component about the node graph it's bound to
	m_compositorComponent->setEditorCompositorNodeGraph(compositorNodeGraph);

	// Keep it running while this window shows its graph
	m_compositorComponent->setEditorHeld(true);

	return true;
}

void CompositorNodeEditorWindow::unbindCompositorComponent()
{
	if (m_compositorComponent)
	{
		m_compositorComponent->setEditorHeld(false);
		m_compositorComponent->setEditorCompositorNodeGraph(nullptr);
		m_compositorComponent= nullptr;
	}
}

bool CompositorNodeEditorWindow::openCompositorComponent(CompositorComponentPtr compositorComponent)
{
	if (m_compositorComponent == compositorComponent)
	{
		return true;
	}

	unbindCompositorComponent();
	return bindCompositorComponent(compositorComponent);
}

bool CompositorNodeEditorWindow::openGraphFile(const std::filesystem::path& graphPath)
{
	// A failed load keeps the graph and compositor the window already shows. A
	// window that has none yet gets an empty graph, since the editor always draws one.
	if (!loadGraph(graphPath))
	{
		if (!m_editorState.nodeGraph)
		{
			newGraph();
		}
		return false;
	}

	unbindCompositorComponent();
	return true;
}

void CompositorNodeEditorWindow::onGraphRestored()
{
	if (m_compositorComponent == nullptr)
	{
		return;
	}

	// Rebind the rebuilt graph instance so the compositor evaluates it
	auto compositorNodeGraph= std::static_pointer_cast<CompositorNodeGraph>(m_editorState.nodeGraph);
	compositorNodeGraph->bindToCompositorComponent(m_compositorComponent);
	m_compositorComponent->setEditorCompositorNodeGraph(compositorNodeGraph);
}

void CompositorNodeEditorWindow::update(float deltaSeconds)
{
	m_lastNodeEvalErrors.clear();

	if (m_compositorComponent != nullptr)
	{
		m_lastNodeEvalErrors= m_compositorComponent->getLastNodeEvalErrors();
	}

	NodeEditorWindow::update(deltaSeconds);
}

void CompositorNodeEditorWindow::shutdown()
{
	// Frees the editor compositor texture and releases the running hold
	unbindCompositorComponent();

	NodeEditorWindow::shutdown();
}

// -- CompositorNodeEditorWindow ----
NodeGraphFactoryPtr CompositorNodeEditorWindow::getNodeGraphFactory() const
{
	return std::make_shared<CompositorNodeGraphFactory>();
}

bool CompositorNodeEditorWindow::saveGraph(bool bShowFileDialog)
{
	if (NodeEditorWindow::saveGraph(bShowFileDialog))
	{
		// A graph opened on its own has no compositor to follow the saved path
		if (m_compositorComponent)
		{
			m_compositorComponent->setCompositorGraphAssetPath(m_editorState.nodeGraphPath);
		}
		return true;
	}

	return false;
}

void CompositorNodeEditorWindow::handleGraphVariablesDragDrop(const NodeEditorState& editorState)
{
	// The window may hold no graph for a frame after a failed open
	if (!getNodeGraph())
	{
		return;
	}

	std::vector<AssetReferenceFactoryPtr> validAssetRefFactories=
		getNodeGraph()->editorGetValidAssetRefFactories(editorState);
	for (auto factory : validAssetRefFactories)
	{
		if (auto assetRef= MkGui::receiveTypedDragDropPayload<AssetReference>(factory->getAssetRefClassName()))
		{
			// The payload carries the project catalog's instance, while graph
			// properties bind to the graph's own reference by index
			AssetReferencePtr graphAssetRef=
				getNodeGraph()->findOrAddAssetReference(assetRef->getClassName(), assetRef->getInternalAssetPath());
			if (graphAssetRef)
			{
				graphAssetRef->editorHandleGraphVariablesDragDrop(editorState);
			}
			return;
		}
	}
}

void CompositorNodeEditorWindow::handleMainFrameDragDrop(const NodeEditorState& editorState)
{
	// The window may hold no graph for a frame after a failed open
	if (!getNodeGraph())
	{
		return;
	}

	std::vector<GraphPropertyFactoryPtr> validPropertyFactories=
		getNodeGraph()->editorGetValidPropertyFactories(editorState);
	for (auto factory : validPropertyFactories)
	{
		if (auto property= MkGui::receiveTypedDragDropPayload<GraphProperty>(factory->getGraphPropertyClassName()))
		{
			property->editorHandleMainFrameDragDrop(editorState);
			return;
		}
	}

	std::vector<AssetReferenceFactoryPtr> validAssetRefFactories=
		getNodeGraph()->editorGetValidAssetRefFactories(editorState);
	for (auto factory : validAssetRefFactories)
	{
		if (auto assetRef= MkGui::receiveTypedDragDropPayload<AssetReference>(factory->getAssetRefClassName()))
		{
			// The payload carries the project catalog's instance, while graph
			// properties bind to the graph's own reference by index
			AssetReferencePtr graphAssetRef=
				getNodeGraph()->findOrAddAssetReference(assetRef->getClassName(), assetRef->getInternalAssetPath());
			if (graphAssetRef)
			{
				graphAssetRef->editorHandleMainFrameDragDrop(editorState);
			}
			return;
		}
	}
}

void CompositorNodeEditorWindow::renderMenuBarExtras()
{
	if (ImGui::BeginMenu(locLabel("nodeEditor.compositorMenu")))
	{
		// The bound compositor's name heads the menu (data, so drawn raw)
		const std::string boundName=
			m_compositorComponent ? m_compositorComponent->getName() : std::string(locText("nodeEditor.noCompositor"));
		ImGui::MenuItem(boundName.c_str(), nullptr, false, false);
		ImGui::Separator();

		bool bRunning= m_compositorComponent ? !m_compositorComponent->getEditorEvaluationPaused() : true;
		if (ImGui::MenuItem(locLabel("nodeEditor.runCompositor"), nullptr, &bRunning) && m_compositorComponent)
		{
			m_compositorComponent->setEditorEvaluationPaused(!bRunning);
		}
		ImGui::EndMenu();
	}
}

bool CompositorNodeEditorWindow::setCompositorRunning(bool bRunning)
{
	if (m_compositorComponent == nullptr)
	{
		return false;
	}

	m_compositorComponent->setEditorEvaluationPaused(!bRunning);
	return true;
}

bool CompositorNodeEditorWindow::isCompositorRunning() const
{
	return m_compositorComponent != nullptr && !m_compositorComponent->getEditorEvaluationPaused();
}

std::filesystem::path CompositorNodeEditorWindow::getDefaultGraphDirectory() const
{
	return PathUtils::getProjectDirectory() / "compositors";
}

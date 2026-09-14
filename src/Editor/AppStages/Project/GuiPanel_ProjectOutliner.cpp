#include "GuiPanel_ProjectOutliner.h"
#include "AnchorComponent.h"
#include "ARKitVideoSourceComponent.h"
#include "BoxShapeComponent.h"
#include "BoxStencilComponent.h"
#include "CameraComponent.h"
#include "CEFTextureSourceComponent.h"
#include "ClientTextureSourceComponent.h"
#include "CommonConfig.h"
#include "CompositorComponent.h"
#include "DMXFixtureComponent.h"
#include "DMXFixtureGroupComponent.h"
#include "DMXPresetComponent.h"
#include "DMXSequenceComponent.h"
#include "EditorObjectSystem.h"
#include "IconsForkAwesome.h"
#include "IEditorWindow.h"
#include "LightEnvironmentComponent.h"
#include "LocText.h"
#include "MarkerComponent.h"
#include "MarkerTrackingVolumeComponent.h"
#include "MikanCoreTypes.h"
#include "MikanObject.h"
#include "MikanObjectSystem.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiScopedDragDropSource.h"
#include "MkGuiScopedStyle.h"
#include "MkGuiStyleManager.h"
#include "ModalConfirm/ModalDialog_Confirm.h"
#include "ModelShapeComponent.h"
#include "ModelStencilComponent.h"
#include "NetworkVideoSourceComponent.h"
#include "Project/AppStage_Project.h"
#include "Project/ProjectGuiPanelContext.h"
#include "Project/ProjectOutlinerActions.h"
#include "ProjectManager.h"
#include "QuadShapeComponent.h"
#include "QuadStencilComponent.h"
#include "RGBPixelGridComponent.h"
#include "RGBSpotLightComponent.h"
#include "SceneComponent.h"
#include "SceneObjectSystem.h"
#include "ScriptComponent.h"
#include "SelectionComponent.h"
#include "Shared/GuiPanel_AnchorComponent.h"
#include "Shared/GuiPanel_ARKitVideoSourceComponent.h"
#include "Shared/GuiPanel_CameraComponent.h"
#include "Shared/GuiPanel_CEFTextureSourceComponent.h"
#include "Shared/GuiPanel_ClientTextureSourceComponent.h"
#include "Shared/GuiPanel_CompositorComponent.h"
#include "Shared/GuiPanel_DMXFixtureGroupComponent.h"
#include "Shared/GuiPanel_DMXPresetComponent.h"
#include "Shared/GuiPanel_DMXSequenceComponent.h"
#include "Shared/GuiPanel_LightEnvironmentComponent.h"
#include "Shared/GuiPanel_MarkerComponent.h"
#include "Shared/GuiPanel_MarkerTrackingVolumeComponent.h"
#include "Shared/GuiPanel_NetworkVideoSourceComponent.h"
#include "Shared/GuiPanel_RGBPixelGridComponent.h"
#include "Shared/GuiPanel_RGBSpotLightComponent.h"
#include "Shared/GuiPanel_SceneComponent.h"
#include "Shared/GuiPanel_ScriptComponent.h"
#include "Shared/GuiPanel_ShapeComponent.h"
#include "Shared/GuiPanel_SpoutTextureSourceComponent.h"
#include "Shared/GuiPanel_StageComponent.h"
#include "Shared/GuiPanel_StencilComponent.h"
#include "Shared/GuiPanel_TrackingMountComponent.h"
#include "Shared/GuiPanel_USBVideoSourceComponent.h"
#include "Shared/GuiPanel_VRTrackingVolumeComponent.h"
#include "SpoutTextureSourceComponent.h"
#include "StageComponent.h"
#include "TrackingMountComponent.h"
#include "TransactionHistory.h"
#include "TransformComponent.h"
#include "USBVideoSourceComponent.h"
#include "VRTrackingVolumeComponent.h"

#include "imgui.h"

#include <algorithm>

// The outliner tree never shrinks below this, and the action strip below it
// always keeps at least its minimum
static constexpr float k_minOutlinerTreeHeight= 100.f;
static constexpr float k_minOutlinerActionsHeight= 120.f;
static constexpr float k_outlinerSplitterHeight= 6.f;

// One payload type for all seven scene actor classes: the payload itself is
// always a TransformComponentPtr
static const char* k_outlinerActorPayloadType= "OutlinerSceneActor";

GuiPanel_ProjectOutliner::GuiPanel_ProjectOutliner(AppStage* ownerAppStage)
	: GuiPanel(ownerAppStage)
{
}

bool GuiPanel_ProjectOutliner::init(ProjectGuiPanelContext* context)
{
	m_context= context;
	m_projectManager= m_ownerAppStage->getProjectManager();
	m_editorSystem= m_ownerAppStage->getObjectSystemOfType<EditorObjectSystem>();
	m_sceneSystem= m_ownerAppStage->getObjectSystemOfType<SceneObjectSystem>();
	m_outlinerGuiStyle= getGuiStyleManager()->getStyle("project_outliner");
	m_deleteButtonGuiStyle= getGuiStyleManager()->getStyle("delete_button");

	if (EditorObjectSystemPtr editorSystem= m_editorSystem.lock())
	{
		editorSystem->OnSelectionChanged+= MakeDelegate(this, &GuiPanel_ProjectOutliner::onSelectionChanged);
		editorSystem->OnDeleteSelectionRequested+=
			MakeDelegate(this, &GuiPanel_ProjectOutliner::onDeleteSelectionRequested);
	}

	subscribeToSystems();
	m_bTreeDirty= true;

	return true;
}

void GuiPanel_ProjectOutliner::dispose()
{
	if (EditorObjectSystemPtr editorSystem= m_editorSystem.lock())
	{
		editorSystem->OnSelectionChanged-= MakeDelegate(this, &GuiPanel_ProjectOutliner::onSelectionChanged);
		editorSystem->OnDeleteSelectionRequested-=
			MakeDelegate(this, &GuiPanel_ProjectOutliner::onDeleteSelectionRequested);
	}

	unsubscribeFromSystems();
	m_model.clear();

	GuiPanel::dispose();
}

void GuiPanel_ProjectOutliner::onGui()
{
	rebuildIfDirty();

	drawTree();

	drawSelectedNodeActions(getSelectedNode());

	handleDeleteShortcut();
}

void GuiPanel_ProjectOutliner::rebuildIfDirty()
{
	if (!m_bTreeDirty)
		return;

	m_bTreeDirty= false;
	m_model.rebuild(m_projectManager.lock());

	if (m_pendingSelectComponentId != INVALID_MIKAN_ID)
	{
		ProjectOutlinerNodePtr newNode= m_model.findNodeByComponentId(m_pendingSelectComponentId);
		m_pendingSelectComponentId= INVALID_MIKAN_ID;
		if (newNode)
		{
			setSelectedNode(newNode, true);
			scrollToNode(newNode);
			return;
		}
	}

	// Re-resolve the selection against the rebuilt tree: rebind the panel to the
	// fresh component, or fall back to the root when the object is gone
	if (m_selectedComponentId != INVALID_MIKAN_ID)
	{
		ProjectOutlinerNodePtr selectedNode= m_model.findNodeByComponentId(m_selectedComponentId);
		setSelectedNode(selectedNode, false);
	}
	else if (m_selectedKind != eOutlinerNodeKind::projectRoot
			 && !m_model.findFolderNode(m_selectedKind, m_selectedOwnerId))
	{
		// A selected per-stage folder whose stage was deleted falls back to the root
		setSelectedNode(nullptr, false);
	}
}

// -- Tree drawing ----
void GuiPanel_ProjectOutliner::drawTree()
{
	// A fixed height the splitter below adjusts, clamped so the tree never
	// collapses and the action strip under it always keeps some room
	EditorObjectSystemDefinitionPtr editorConfig= getEditorConfig();
	if (!m_bTreeHeightDragging && editorConfig)
	{
		m_treeHeight= editorConfig->getOutlinerTreeHeight();
	}
	const float maxTreeHeight=
		std::max(k_minOutlinerTreeHeight, ImGui::GetContentRegionAvail().y - k_minOutlinerActionsHeight);
	m_treeHeight= std::clamp(m_treeHeight, k_minOutlinerTreeHeight, maxTreeHeight);

	if (ImGui::BeginChild("##OutlinerTree", ImVec2(0.f, m_treeHeight)))
	{
		if (ProjectOutlinerNodePtr rootNode= m_model.getRoot())
		{
			drawNode(rootNode);
		}
	}
	ImGui::EndChild();

	drawTreeSplitter(editorConfig, maxTreeHeight);
}

void GuiPanel_ProjectOutliner::drawTreeSplitter(EditorObjectSystemDefinitionPtr editorConfig, float maxTreeHeight)
{
	const float width= std::max(ImGui::GetContentRegionAvail().x, 1.f);
	ImGui::InvisibleButton("##OutlinerSplitter", ImVec2(width, k_outlinerSplitterHeight));
	const bool bHovered= ImGui::IsItemHovered();
	const bool bActive= ImGui::IsItemActive();

	if (bHovered || bActive)
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	}

	// The drag moves the local height frame by frame; the release commits it
	// to the project once, so a drag lands as one config change
	if (bActive)
	{
		m_bTreeHeightDragging= true;
		m_treeHeight+= ImGui::GetIO().MouseDelta.y;
	}
	else if (m_bTreeHeightDragging)
	{
		m_bTreeHeightDragging= false;
		const float treeHeight= std::clamp(m_treeHeight, k_minOutlinerTreeHeight, maxTreeHeight);
		if (editorConfig)
		{
			addDeferredGuiEvent([editorConfig, treeHeight]() { editorConfig->setOutlinerTreeHeight(treeHeight); });
		}
	}

	// Drawn as a separator line that brightens under the mouse, so the bar
	// reads as a handle without taking more room than the separator it replaces
	const ImVec2 rectMin= ImGui::GetItemRectMin();
	const ImVec2 rectMax= ImGui::GetItemRectMax();
	const float lineY= (rectMin.y + rectMax.y) * 0.5f;
	const ImU32 lineColor= ImGui::GetColorU32(bActive    ? ImGuiCol_SeparatorActive
											  : bHovered ? ImGuiCol_SeparatorHovered
														 : ImGuiCol_Separator);
	ImGui::GetWindowDrawList()->AddLine(ImVec2(rectMin.x, lineY), ImVec2(rectMax.x, lineY), lineColor);
}

void GuiPanel_ProjectOutliner::drawNode(ProjectOutlinerNodePtr node)
{
	ImGuiTreeNodeFlags flags=
		ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
	const bool bIsLeaf= node->children.empty();
	if (bIsLeaf)
		flags|= ImGuiTreeNodeFlags_Leaf;

	// The project holds the open state, so a rebuild cannot reset it and it
	// comes back on the next load. ImGui is told the state every frame; a click
	// on the arrow shows up as a toggle below and writes the new state back.
	const std::string stateKey= ProjectOutlinerModel::getNodeStateKey(*node);
	const bool bDefaultOpen= ProjectOutlinerModel::getNodeDefaultOpen(node->kind);
	EditorObjectSystemDefinitionPtr editorConfig= getEditorConfig();
	if (!bIsLeaf)
	{
		const bool bStoredOpen= editorConfig ? editorConfig->getOutlinerNodeOpen(stateKey, bDefaultOpen) : bDefaultOpen;
		ImGui::SetNextItemOpen(bStoredOpen, ImGuiCond_Always);
	}

	// Synthetic rows (root and folders) carry no component id, so they select
	// by kind plus owner id
	const bool bIsSyntheticSelectable=
		node->kind == eOutlinerNodeKind::projectRoot || node->kind == eOutlinerNodeKind::folderSources
		|| node->kind == eOutlinerNodeKind::folderMarkers || node->kind == eOutlinerNodeKind::folderTrackingVolumes
		|| node->kind == eOutlinerNodeKind::folderCameras || node->kind == eOutlinerNodeKind::folderLights
		|| node->kind == eOutlinerNodeKind::folderLightGroups || node->kind == eOutlinerNodeKind::folderScenes
		|| node->kind == eOutlinerNodeKind::folderScripts;
	const bool bIsSelected= (node->componentId != INVALID_MIKAN_ID && node->componentId == m_selectedComponentId)
							|| (bIsSyntheticSelectable && m_selectedComponentId == INVALID_MIKAN_ID
								&& node->kind == m_selectedKind && node->ownerId == m_selectedOwnerId);
	if (bIsSelected)
		flags|= ImGuiTreeNodeFlags_Selected;

	const std::string label= node->icon + " " + node->displayName + "##node" + std::to_string((int)node->kind) + "_"
							 + std::to_string(node->componentId);

	bool bOpen= false;
	if (isActiveHighlightNode(node))
	{
		MkGuiScopedStyle activeStyle(m_outlinerGuiStyle);
		bOpen= ImGui::TreeNodeEx(label.c_str(), flags);
	}
	else
	{
		bOpen= ImGui::TreeNodeEx(label.c_str(), flags);
	}

	if (!bIsLeaf && ImGui::IsItemToggledOpen() && editorConfig)
	{
		addDeferredGuiEvent([editorConfig, stateKey, bOpen, bDefaultOpen]()
							{ editorConfig->setOutlinerNodeOpen(stateKey, bOpen, bDefaultOpen); });
	}

	if (bIsSelected && m_bScrollToSelection)
	{
		ImGui::SetScrollHereY(0.5f);
		m_bScrollToSelection= false;
	}

	// The unparented tray is the one row that is not selectable
	const bool bIsSelectable= node->componentId != INVALID_MIKAN_ID || bIsSyntheticSelectable;
	if (bIsSelectable && ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
	{
		ProjectOutlinerNodePtr clickedNode= node;
		addDeferredGuiEvent([this, clickedNode]() { setSelectedNode(clickedNode, true); });
	}

	handleNodeDragDrop(node);

	if (bOpen)
	{
		for (ProjectOutlinerNodePtr childNode : node->children)
		{
			drawNode(childNode);
		}
		ImGui::TreePop();
	}
}

void GuiPanel_ProjectOutliner::handleNodeDragDrop(ProjectOutlinerNodePtr node)
{
	// Drag sources: scene actors only. Nothing above a scene moves by drag.
	if (node->kind == eOutlinerNodeKind::sceneActor)
	{
		TransformComponentPtr transformComponent= std::dynamic_pointer_cast<TransformComponent>(node->component.lock());
		if (transformComponent)
		{
			MkGuiScopedDragDropSource dragSource;
			if (dragSource)
			{
				ImGui::SetDragDropPayload(k_outlinerActorPayloadType, &transformComponent,
										  sizeof(TransformComponentPtr));
				ImGui::TextUnformatted(node->displayName.c_str());
			}
		}
	}

	// Drop targets: a scene (cross-scene moves included) or another scene actor
	if (node->kind == eOutlinerNodeKind::scene || node->kind == eOutlinerNodeKind::sceneActor)
	{
		TransformComponentPtr droppedComponent=
			MkGui::receiveTypedDragDropPayload<TransformComponent>(k_outlinerActorPayloadType);
		if (droppedComponent)
		{
			TransformComponentPtr newParent= std::dynamic_pointer_cast<TransformComponent>(node->component.lock());
			addDeferredGuiEvent(
				[this, droppedComponent, newParent]()
				{
					TransactionHistory* transactionHistory= m_ownerAppStage->getOwnerWindow()->getTransactionHistory();
					if (transactionHistory)
						transactionHistory->beginGesture("outliner_reparent");

					ProjectOutlinerActions::reparentSceneActor(droppedComponent, newParent);

					if (transactionHistory)
						transactionHistory->endGesture();
				});
		}
	}
}

// -- Selected-node action strip ----
void GuiPanel_ProjectOutliner::drawSelectedNodeActions(ProjectOutlinerNodePtr selectedNode)
{
	if (!selectedNode)
		return;

	switch (selectedNode->kind)
	{
	case eOutlinerNodeKind::folderSources:
		if (drawAddButton("outlinerAddUSBSource", ICON_FK_USB, "project.outlinerAddUSBSource"))
			deferAddAction([](ProjectManagerPtr pm) { return ProjectOutlinerActions::addUSBVideoSource(pm); });
		if (drawAddButton("outlinerAddNetworkSource", ICON_FK_WIFI, "project.outlinerAddNetworkSource"))
			deferAddAction([](ProjectManagerPtr pm) { return ProjectOutlinerActions::addNetworkVideoSource(pm); });
		if (drawAddButton("outlinerAddARKitSource", ICON_FK_MOBILE, "project.outlinerAddARKitSource"))
			deferAddAction([](ProjectManagerPtr pm) { return ProjectOutlinerActions::addARKitVideoSource(pm); });

		if (drawAddButton("outlinerAddClientSource", ICON_FK_PLUG, "project.outlinerAddClientSource"))
			deferAddAction([](ProjectManagerPtr pm) { return ProjectOutlinerActions::addClientTextureSource(pm); });
		if (drawAddButton("outlinerAddSpoutSource", ICON_FK_SHARE_ALT, "project.outlinerAddSpoutSource"))
			deferAddAction([](ProjectManagerPtr pm) { return ProjectOutlinerActions::addSpoutTextureSource(pm); });
		if (drawAddButton("outlinerAddCEFSource", ICON_FK_CHROME, "project.outlinerAddCEFSource"))
			deferAddAction([](ProjectManagerPtr pm) { return ProjectOutlinerActions::addCEFTextureSource(pm); });
		break;
	case eOutlinerNodeKind::folderMarkers:
		if (drawAddButton("outlinerAddMarker", ICON_FK_QRCODE, "project.outlinerAddMarker"))
			deferAddAction([](ProjectManagerPtr pm) { return ProjectOutlinerActions::addMarker(pm); });
		break;
	case eOutlinerNodeKind::folderTrackingVolumes:
		if (drawAddButton("outlinerAddMarkerTracking", ICON_FK_BULLSEYE, "project.outlinerAddMarkerTracking"))
			deferAddAction([](ProjectManagerPtr pm) { return ProjectOutlinerActions::addMarkerTrackingVolume(pm); });
		if (drawAddButton("outlinerAddVRTracking", ICON_FK_BULLSEYE, "project.outlinerAddVRTracking"))
			deferAddAction([](ProjectManagerPtr pm) { return ProjectOutlinerActions::addVRTrackingVolume(pm); });
		break;
	case eOutlinerNodeKind::folderScripts:
		if (drawAddButton("outlinerAddScript", ICON_FK_FILE_CODE_O, "project.outlinerAddScript"))
			deferAddAction([](ProjectManagerPtr pm) { return ProjectOutlinerActions::addScript(pm); });
		break;
	case eOutlinerNodeKind::trackingVolume:
	{
		const int volumeId= selectedNode->componentId;
		if (drawAddButton("outlinerAddStage", ICON_FK_MAP_O, "project.outlinerAddStage"))
		{
			deferAddAction([volumeId](ProjectManagerPtr pm) { return ProjectOutlinerActions::addStage(pm, volumeId); });
		}
		if (selectedNode->componentClassName == VRTrackingVolumeComponent::k_componentClassName)
		{
			if (drawAddButton("outlinerAddMount", ICON_FK_CROSSHAIRS, "project.outlinerAddMount"))
			{
				deferAddAction([volumeId](ProjectManagerPtr pm)
							   { return ProjectOutlinerActions::addTrackingMount(pm, volumeId); });
			}
		}
		break;
	}
	case eOutlinerNodeKind::folderCameras:
	{
		const int stageId= selectedNode->ownerId;
		if (drawAddButton("outlinerAddCamera", ICON_FK_VIDEO_CAMERA, "project.outlinerAddCamera"))
		{
			deferAddAction([stageId](ProjectManagerPtr pm) { return ProjectOutlinerActions::addCamera(pm, stageId); });
		}
		break;
	}
	case eOutlinerNodeKind::folderLights:
	{
		const int stageId= selectedNode->ownerId;
		if (drawAddButton("outlinerAddSpotLight", ICON_FK_LIGHTBULB_O, "project.outlinerAddSpotLight"))
		{
			deferAddAction([stageId](ProjectManagerPtr pm)
						   { return ProjectOutlinerActions::addSpotLight(pm, stageId); });
		}
		if (drawAddButton("outlinerAddPixelGrid", ICON_FK_TH, "project.outlinerAddPixelGrid"))
		{
			deferAddAction([stageId](ProjectManagerPtr pm)
						   { return ProjectOutlinerActions::addPixelGrid(pm, stageId); });
		}
		break;
	}
	case eOutlinerNodeKind::folderLightGroups:
	{
		const int stageId= selectedNode->ownerId;
		if (drawAddButton("outlinerAddLightGroup", ICON_FK_OBJECT_GROUP, "project.outlinerAddLightGroup"))
		{
			deferAddAction([stageId](ProjectManagerPtr pm)
						   { return ProjectOutlinerActions::addLightGroup(pm, stageId); });
		}
		break;
	}
	case eOutlinerNodeKind::lightGroup:
	{
		const int groupId= selectedNode->componentId;
		if (drawAddButton("outlinerAddPreset", ICON_FK_SLIDERS, "project.outlinerAddPreset"))
		{
			deferAddAction([groupId](ProjectManagerPtr pm) { return ProjectOutlinerActions::addPreset(pm, groupId); });
		}
		if (drawAddButton("outlinerAddSequence", ICON_FK_PLAY_CIRCLE, "project.outlinerAddSequence"))
		{
			deferAddAction([groupId](ProjectManagerPtr pm)
						   { return ProjectOutlinerActions::addSequence(pm, groupId); });
		}
		break;
	}
	case eOutlinerNodeKind::folderScenes:
	{
		const int stageId= selectedNode->ownerId;
		if (drawAddButton("outlinerAddScene", ICON_FK_SITEMAP, "project.outlinerAddScene"))
		{
			deferAddAction([stageId](ProjectManagerPtr pm) { return ProjectOutlinerActions::addScene(pm, stageId); });
		}
		break;
	}
	case eOutlinerNodeKind::scene:
	{
		const int sceneId= selectedNode->componentId;
		if (drawAddButton("outlinerAddCompositor", ICON_FK_CLONE, "project.outlinerAddCompositor"))
		{
			deferAddAction([sceneId](ProjectManagerPtr pm)
						   { return ProjectOutlinerActions::addCompositor(pm, sceneId); });
		}
		// Its own row: the compositor is a scene-level output, and its button is
		// sized for the glyph set rather than the taller actor icons
		drawSceneActorAddButtons(selectedNode);
		break;
	}
	case eOutlinerNodeKind::sceneActor:
		drawSceneActorAddButtons(selectedNode);
		break;
	default:
		break;
	}

	drawComponentPanelForNode(selectedNode);

	// Delete closes out the panel, below the component's own function buttons,
	// so the destructive action is not the first thing under the cursor
	if (canDeleteNode(selectedNode))
	{
		MkGuiScopedStyle deleteButtonStyle(m_deleteButtonGuiStyle);
		if (ImGui::Button(locLabel("project.outlinerDeleteComponent")))
		{
			requestDeleteNode(selectedNode);
		}
	}
}

bool GuiPanel_ProjectOutliner::canDeleteNode(ProjectOutlinerNodePtr node) const
{
	// Environment probes are camera-owned and are deleted with their camera
	return node && node->componentId != INVALID_MIKAN_ID
		   && node->componentClassName != LightEnvironmentComponent::k_componentClassName;
}

void GuiPanel_ProjectOutliner::handleDeleteShortcut()
{
	// Only while ImGui holds the keyboard: otherwise the key reaches the editor
	// system's binding instead. A widget being edited keeps its own Delete and
	// Backspace, and an open modal (the confirm itself) must not stack another.
	const ImGuiIO& io= ImGui::GetIO();
	if (!io.WantCaptureKeyboard || ImGui::IsAnyItemActive() || m_ownerAppStage->getCurrentModalDialog() != nullptr)
		return;

	if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
	{
		ProjectOutlinerNodePtr selectedNode= getSelectedNode();
		if (canDeleteNode(selectedNode))
			requestDeleteNode(selectedNode);
	}
}

void GuiPanel_ProjectOutliner::onDeleteSelectionRequested()
{
	if (m_ownerAppStage->getCurrentModalDialog() != nullptr)
		return;

	ProjectOutlinerNodePtr selectedNode= getSelectedNode();
	if (canDeleteNode(selectedNode))
		requestDeleteNode(selectedNode);
}

bool GuiPanel_ProjectOutliner::drawAddButton(const char* fieldName, const char* glyph, const char* labelKey)
{
	// Larger than the property panels' buttons: these are the primary way to
	// build a project out. The glyph is the one the created object will carry
	// in the tree.
	static constexpr float k_buttonSize= 44.f;
	static constexpr float k_glyphSize= 28.f;

	return MkGui::drawGlyphButtonWithLabel(fieldName, glyph, locText(labelKey), k_buttonSize, k_glyphSize);
}

void GuiPanel_ProjectOutliner::drawSceneActorAddButtons(ProjectOutlinerNodePtr selectedNode)
{
	const int parentTransformId= getAddParentTransformId(selectedNode);
	if (parentTransformId == INVALID_MIKAN_ID)
		return;

	if (drawAddButton("outlinerAddAnchor", ICON_FK_ANCHOR, "project.outlinerAddAnchor"))
	{
		deferAddAction([parentTransformId](ProjectManagerPtr pm)
					   { return ProjectOutlinerActions::addAnchor(pm, parentTransformId); });
	}
	if (drawAddButton("outlinerAddQuadStencil", ICON_FK_CROP, "project.outlinerAddQuadStencil"))
	{
		deferAddAction([parentTransformId](ProjectManagerPtr pm)
					   { return ProjectOutlinerActions::addStencil(pm, eStencilType::quad, parentTransformId); });
	}
	if (drawAddButton("outlinerAddBoxStencil", ICON_FK_CROP, "project.outlinerAddBoxStencil"))
	{
		deferAddAction([parentTransformId](ProjectManagerPtr pm)
					   { return ProjectOutlinerActions::addStencil(pm, eStencilType::box, parentTransformId); });
	}
	if (drawAddButton("outlinerAddModelStencil", ICON_FK_CROP, "project.outlinerAddModelStencil"))
	{
		deferAddAction([parentTransformId](ProjectManagerPtr pm)
					   { return ProjectOutlinerActions::addStencil(pm, eStencilType::model, parentTransformId); });
	}

	if (drawAddButton("outlinerAddQuadShape", ICON_FK_SQUARE, "project.outlinerAddQuadShape"))
	{
		deferAddAction([parentTransformId](ProjectManagerPtr pm)
					   { return ProjectOutlinerActions::addShape(pm, eShapeType::quad, parentTransformId); });
	}
	if (drawAddButton("outlinerAddBoxShape", ICON_FK_CUBE, "project.outlinerAddBoxShape"))
	{
		deferAddAction([parentTransformId](ProjectManagerPtr pm)
					   { return ProjectOutlinerActions::addShape(pm, eShapeType::box, parentTransformId); });
	}
	if (drawAddButton("outlinerAddModelShape", ICON_FK_CUBES, "project.outlinerAddModelShape"))
	{
		deferAddAction([parentTransformId](ProjectManagerPtr pm)
					   { return ProjectOutlinerActions::addShape(pm, eShapeType::model, parentTransformId); });
	}
}

int GuiPanel_ProjectOutliner::getAddParentTransformId(ProjectOutlinerNodePtr selectedNode) const
{
	if (!selectedNode)
		return INVALID_MIKAN_ID;

	if (selectedNode->kind == eOutlinerNodeKind::scene || selectedNode->kind == eOutlinerNodeKind::sceneActor)
		return selectedNode->componentId;

	return INVALID_MIKAN_ID;
}

void GuiPanel_ProjectOutliner::deferAddAction(std::function<int(ProjectManagerPtr)> addAction)
{
	addDeferredGuiEvent(
		[this, addAction]()
		{
			ProjectManagerPtr projectManager= m_projectManager.lock();
			if (!projectManager)
				return;

			const int newComponentId= addAction(projectManager);
			if (newComponentId != INVALID_MIKAN_ID)
			{
				m_pendingSelectComponentId= newComponentId;
				markTreeDirty();
			}
		});
}

void GuiPanel_ProjectOutliner::requestDeleteNode(ProjectOutlinerNodePtr node)
{
	const int objectCount= ProjectOutlinerActions::countSubtreeObjects(node);
	const std::string question=
		objectCount > 1 ? locFormat("project.outlinerDeleteManyFmt", node->displayName.c_str(), objectCount - 1)
						: locFormat("project.outlinerDeleteOneFmt", node->displayName.c_str());

	// The captured node keeps its subtree snapshot alive across the modal
	ModalDialog_Confirm::confirmQuestion(m_ownerAppStage, locText("project.outlinerDeleteConfirmTitle"), question,
										 [this, node]()
										 {
											 addDeferredGuiEvent(
												 [this, node]()
												 {
													 if (ProjectManagerPtr projectManager= m_projectManager.lock())
													 {
														 ProjectOutlinerActions::deleteSubtree(projectManager, node);
													 }
												 });
										 });
}

// -- Selection plumbing ----
ProjectOutlinerNodePtr GuiPanel_ProjectOutliner::getSelectedNode() const
{
	if (m_selectedComponentId != INVALID_MIKAN_ID)
	{
		if (ProjectOutlinerNodePtr node= m_model.findNodeByComponentId(m_selectedComponentId))
			return node;
	}

	if (ProjectOutlinerNodePtr folderNode= m_model.findFolderNode(m_selectedKind, m_selectedOwnerId))
		return folderNode;

	return m_model.getRoot();
}

void GuiPanel_ProjectOutliner::setSelectedNode(ProjectOutlinerNodePtr node, bool bPushViewportSelection)
{
	m_selectedComponentId= node ? node->componentId : INVALID_MIKAN_ID;
	m_selectedKind= node ? node->kind : eOutlinerNodeKind::projectRoot;
	m_selectedOwnerId= node ? node->ownerId : INVALID_MIKAN_ID;

	clearComponentPanels();
	if (node)
	{
		if (MikanComponentPtr component= node->component.lock())
		{
			if (GuiPanel_MikanComponent* panel= getPanelForComponentClass(node->componentClassName))
				panel->setComponent(component);
		}
	}

	if (bPushViewportSelection)
	{
		if (EditorObjectSystemPtr editorSystem= m_editorSystem.lock())
			editorSystem->setSelection(node ? node->selection.lock() : nullptr);
	}
}

GuiPanel_MikanComponent* GuiPanel_ProjectOutliner::getPanelForComponentClass(
	const std::string& componentClassName) const
{
	if (componentClassName == USBVideoSourceComponent::k_componentClassName)
		return m_context->getUSBVideoSourcePanel();
	if (componentClassName == NetworkVideoSourceComponent::k_componentClassName)
		return m_context->getNetworkVideoSourcePanel();
	if (componentClassName == ARKitVideoSourceComponent::k_componentClassName)
		return m_context->getARKitVideoSourcePanel();
	if (componentClassName == ClientTextureSourceComponent::k_componentClassName)
		return m_context->getClientTextureSourcePanel();
	if (componentClassName == SpoutTextureSourceComponent::k_componentClassName)
		return m_context->getSpoutTextureSourcePanel();
	if (componentClassName == CEFTextureSourceComponent::k_componentClassName)
		return m_context->getCEFTextureSourcePanel();
	if (componentClassName == MarkerComponent::k_componentClassName)
		return m_context->getMarkerPanel();
	if (componentClassName == VRTrackingVolumeComponent::k_componentClassName)
		return m_context->getVRTrackingVolumePanel();
	if (componentClassName == MarkerTrackingVolumeComponent::k_componentClassName)
		return m_context->getMarkerTrackingVolumePanel();
	if (componentClassName == TrackingMountComponent::k_componentClassName)
		return m_context->getTrackingMountPanel();
	if (componentClassName == StageComponent::k_componentClassName)
		return m_context->getStagePanel();
	if (componentClassName == CameraComponent::k_componentClassName)
		return m_context->getCameraPanel();
	if (componentClassName == CompositorComponent::k_componentClassName)
		return m_context->getCompositorPanel();
	if (componentClassName == SceneComponent::k_componentClassName)
		return m_context->getScenePanel();
	if (componentClassName == LightEnvironmentComponent::k_componentClassName)
		return m_context->getLightEnvironmentPanel();
	if (componentClassName == RGBSpotLightComponent::k_componentClassName)
		return m_context->getSpotLightPanel();
	if (componentClassName == RGBPixelGridComponent::k_componentClassName)
		return m_context->getPixelGridPanel();
	if (componentClassName == DMXFixtureGroupComponent::k_componentClassName)
		return m_context->getFixtureGroupPanel();
	if (componentClassName == DMXPresetComponent::k_componentClassName)
		return m_context->getPresetPanel();
	if (componentClassName == DMXSequenceComponent::k_componentClassName)
		return m_context->getSequencePanel();
	if (componentClassName == AnchorComponent::k_componentClassName)
		return m_context->getAnchorPanel();
	if (componentClassName == QuadStencilComponent::k_componentClassName)
		return m_context->getQuadStencilPanel();
	if (componentClassName == BoxStencilComponent::k_componentClassName)
		return m_context->getBoxStencilPanel();
	if (componentClassName == ModelStencilComponent::k_componentClassName)
		return m_context->getModelStencilPanel();
	if (componentClassName == QuadShapeComponent::k_componentClassName)
		return m_context->getQuadShapePanel();
	if (componentClassName == BoxShapeComponent::k_componentClassName)
		return m_context->getBoxShapePanel();
	if (componentClassName == ModelShapeComponent::k_componentClassName)
		return m_context->getModelShapePanel();
	if (componentClassName == ScriptComponent::k_componentClassName)
		return m_context->getScriptPanel();

	return nullptr;
}

void GuiPanel_ProjectOutliner::clearComponentPanels()
{
	m_context->getUSBVideoSourcePanel()->setComponent(nullptr);
	m_context->getNetworkVideoSourcePanel()->setComponent(nullptr);
	m_context->getARKitVideoSourcePanel()->setComponent(nullptr);
	m_context->getClientTextureSourcePanel()->setComponent(nullptr);
	m_context->getSpoutTextureSourcePanel()->setComponent(nullptr);
	m_context->getCEFTextureSourcePanel()->setComponent(nullptr);
	m_context->getMarkerPanel()->setComponent(nullptr);
	m_context->getVRTrackingVolumePanel()->setComponent(nullptr);
	m_context->getMarkerTrackingVolumePanel()->setComponent(nullptr);
	m_context->getTrackingMountPanel()->setComponent(nullptr);
	m_context->getStagePanel()->setComponent(nullptr);
	m_context->getCameraPanel()->setComponent(nullptr);
	m_context->getCompositorPanel()->setComponent(nullptr);
	m_context->getScenePanel()->setComponent(nullptr);
	m_context->getLightEnvironmentPanel()->setComponent(nullptr);
	m_context->getSpotLightPanel()->setComponent(nullptr);
	m_context->getPixelGridPanel()->setComponent(nullptr);
	m_context->getFixtureGroupPanel()->setComponent(nullptr);
	m_context->getPresetPanel()->setComponent(nullptr);
	m_context->getSequencePanel()->setComponent(nullptr);
	m_context->getAnchorPanel()->setComponent(nullptr);
	m_context->getQuadStencilPanel()->setComponent(nullptr);
	m_context->getBoxStencilPanel()->setComponent(nullptr);
	m_context->getModelStencilPanel()->setComponent(nullptr);
	m_context->getQuadShapePanel()->setComponent(nullptr);
	m_context->getBoxShapePanel()->setComponent(nullptr);
	m_context->getModelShapePanel()->setComponent(nullptr);
	m_context->getScriptPanel()->setComponent(nullptr);
}

void GuiPanel_ProjectOutliner::drawComponentPanelForNode(ProjectOutlinerNodePtr node)
{
	if (!node || node->componentId == INVALID_MIKAN_ID)
		return;

	// Video sources use their compact form, matching the old Sources panel
	if (node->componentClassName == USBVideoSourceComponent::k_componentClassName)
	{
		m_context->getUSBVideoSourcePanel()->drawCompactGui();
		return;
	}
	if (node->componentClassName == NetworkVideoSourceComponent::k_componentClassName)
	{
		m_context->getNetworkVideoSourcePanel()->drawCompactGui();
		return;
	}
	if (node->componentClassName == ARKitVideoSourceComponent::k_componentClassName)
	{
		m_context->getARKitVideoSourcePanel()->drawCompactGui();
		return;
	}

	if (GuiPanel_MikanComponent* panel= getPanelForComponentClass(node->componentClassName))
	{
		panel->onGui();
	}
}

bool GuiPanel_ProjectOutliner::isActiveHighlightNode(ProjectOutlinerNodePtr node) const
{
	SceneObjectSystemPtr sceneSystem= m_sceneSystem.lock();
	if (!sceneSystem)
		return false;

	if (node->kind == eOutlinerNodeKind::scene)
	{
		return sceneSystem->getCurrentSceneId() == node->componentId;
	}

	if (node->kind == eOutlinerNodeKind::compositor)
	{
		SceneComponentPtr currentScene= sceneSystem->getCurrentScene();
		return currentScene
			   && currentScene->getSceneComponentDefinition()->getDisplayCompositorId() == node->componentId;
	}

	return false;
}

// -- Object system delegate handlers ----
void GuiPanel_ProjectOutliner::onObjectInitialized(MikanObjectSystemPtr objectSystemPtr, MikanObjectPtr objectPtr)
{
	markTreeDirty();
}

void GuiPanel_ProjectOutliner::onObjectDisposed(MikanObjectSystemPtr objectSystemPtr, MikanObjectConstPtr objectPtr)
{
	markTreeDirty();
}

void GuiPanel_ProjectOutliner::onSystemConfigChanged(CommonConfigPtr configPtr,
													 const ConfigPropertyChangeSet& changedPropertySet)
{
	// Only changes that move a node, rename it, or rewire an ownership edge
	// reshape the tree
	if (changedPropertySet.hasPropertyName(MikanComponentDefinition::k_componentNamePropertyId)
		|| changedPropertySet.hasPropertyName(TransformComponentDefinition::k_parentTransformIdPropertyId)
		|| changedPropertySet.hasPropertyName(StageComponentDefinition::k_trackingVolumeIdPropertyId)
		|| changedPropertySet.hasPropertyName(CameraDefinition::k_ownerStageIdPropertyId)
		|| changedPropertySet.hasPropertyName(CameraDefinition::k_lightEnvironmentIdPropertyId)
		|| changedPropertySet.hasPropertyName(CompositorDefinition::k_ownerScenePropertyId)
		|| changedPropertySet.hasPropertyName(DMXFixtureComponentDefinition::k_ownerStageIdPropertyId)
		|| changedPropertySet.hasPropertyName(DMXPresetDefinition::k_groupIdPropertyId)
		|| changedPropertySet.hasPropertyName(VRTrackingVolumeDefinition::k_trackingMountIdsPropertyId))
	{
		markTreeDirty();
	}
}

void GuiPanel_ProjectOutliner::onSelectionChanged()
{
	// A viewport pick selects the matching tree row; a deselect leaves the tree
	// selection alone
	EditorObjectSystemPtr editorSystem= m_editorSystem.lock();
	SelectionComponentPtr selection= editorSystem ? editorSystem->getSelectedSceneActor() : nullptr;
	if (!selection)
		return;

	ProjectOutlinerNodePtr node= m_model.findNodeBySelection(selection);
	if (!node || node->componentId == m_selectedComponentId)
		return;

	setSelectedNode(node, false);
	scrollToNode(node);
}

void GuiPanel_ProjectOutliner::selectComponent(int componentId)
{
	ProjectOutlinerNodePtr node= m_bTreeDirty ? nullptr : m_model.findNodeByComponentId(componentId);
	if (!node)
	{
		// Not in the tree yet: the next rebuild resolves and selects it
		m_pendingSelectComponentId= componentId;
		markTreeDirty();
		return;
	}

	setSelectedNode(node, true);
	scrollToNode(node);
}

void GuiPanel_ProjectOutliner::scrollToNode(ProjectOutlinerNodePtr node)
{
	// Open the path above the row through the project state, since that is
	// what the tree draws from
	if (EditorObjectSystemDefinitionPtr editorConfig= getEditorConfig())
	{
		for (ProjectOutlinerNodePtr pathNode= node->parent.lock(); pathNode; pathNode= pathNode->parent.lock())
		{
			editorConfig->setOutlinerNodeOpen(ProjectOutlinerModel::getNodeStateKey(*pathNode), true,
											  ProjectOutlinerModel::getNodeDefaultOpen(pathNode->kind));
		}
	}
	m_bScrollToSelection= true;
}

EditorObjectSystemDefinitionPtr GuiPanel_ProjectOutliner::getEditorConfig() const
{
	EditorObjectSystemPtr editorSystem= m_editorSystem.lock();
	return editorSystem ? editorSystem->getEditorSystemConfig() : nullptr;
}

void GuiPanel_ProjectOutliner::subscribeToSystems()
{
	ProjectManagerPtr projectManager= m_projectManager.lock();
	if (!projectManager)
		return;

	// Subscribe to every system: the property-name filter above keeps systems
	// with no tree presence from triggering rebuilds
	for (MikanObjectSystemPtr system : projectManager->getSystems())
	{
		system->OnNewObjectFinalized+= MakeDelegate(this, &GuiPanel_ProjectOutliner::onObjectInitialized);
		system->OnObjectDisposed+= MakeDelegate(this, &GuiPanel_ProjectOutliner::onObjectDisposed);
		if (CommonConfigPtr definition= system->getDefinition())
		{
			definition->OnPropertyChanged+= MakeDelegate(this, &GuiPanel_ProjectOutliner::onSystemConfigChanged);
		}

		m_subscribedSystems.push_back(system);
	}
}

void GuiPanel_ProjectOutliner::unsubscribeFromSystems()
{
	for (MikanObjectSystemWeakPtr systemWeakPtr : m_subscribedSystems)
	{
		MikanObjectSystemPtr system= systemWeakPtr.lock();
		if (!system)
			continue;

		system->OnNewObjectFinalized-= MakeDelegate(this, &GuiPanel_ProjectOutliner::onObjectInitialized);
		system->OnObjectDisposed-= MakeDelegate(this, &GuiPanel_ProjectOutliner::onObjectDisposed);
		if (CommonConfigPtr definition= system->getDefinition())
		{
			definition->OnPropertyChanged-= MakeDelegate(this, &GuiPanel_ProjectOutliner::onSystemConfigChanged);
		}
	}
	m_subscribedSystems.clear();
}

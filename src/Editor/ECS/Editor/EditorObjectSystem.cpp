#include "AnchorObjectSystem.h"
#include "App.h"
#include "Project/AppStage_Project.h"
#include "BoxColliderComponent.h"
#include "DiskColliderComponent.h"
#include "EditorObjectSystem.h"
#include "GizmoRotateComponent.h"
#include "GizmoScaleComponent.h"
#include "GizmoTransformComponent.h"
#include "GizmoTranslateComponent.h"
#include "MikanViewport.h"
#include "MikanCamera.h"
#include "MikanLineRenderer.h"
#include "MikanTextRenderer.h"
#include "TextStyle.h"
#include "MikanPropertyDatabase.h"
#include "MikanFunctionDatabase.h"
#include "ObjectSystemColliderQueries.h"
#include "InputManager.h"
#include "IEditorWindow.h"
#include "ProjectManager.h"
#include "MathTypeConversion.h"
#include "MathUtility.h"
#include "MikanObject.h"
#include "MikanEditorTypes.h"
#include "SceneObjectSystem.h"
#include "SceneComponent.h"
#include "StageComponent.h"
#include "ProjectConfig.h"
#include "SelectionComponent.h"
#include "TransactionHistory.h"
#include "QuadStencilSystem.h"
#include "BoxStencilSystem.h"
#include "ModelStencilSystem.h"
#include "Transform.h"

#include <cmath>

#include "glm/geometric.hpp"

// -- AnchorObjectSystemConfig -----
const std::string EditorObjectSystemDefinition::k_renderOriginFlagPropertyId= "render_origin";
const std::string EditorObjectSystemDefinition::k_renderAnchorsPropertyId= "render_anchors";
const std::string EditorObjectSystemDefinition::k_renderQuadStencilsPropertyId= "render_quad_stencils";
const std::string EditorObjectSystemDefinition::k_renderBoxStencilsPropertyId= "render_box_stencils";
const std::string EditorObjectSystemDefinition::k_renderModelStencilsPropertyId= "render_model_stencils";
const std::string EditorObjectSystemDefinition::k_renderQuadShapesPropertyId= "render_quad_shapes";
const std::string EditorObjectSystemDefinition::k_renderBoxShapesPropertyId= "render_box_shapes";
const std::string EditorObjectSystemDefinition::k_renderModelShapesPropertyId= "render_model_shapes";
const std::string EditorObjectSystemDefinition::k_cameraSpeedPropertyId= "camera_speed";
const std::string EditorObjectSystemDefinition::k_gridExtentPropertyId= "grid_extent";
const std::string EditorObjectSystemDefinition::k_gridCellSizePropertyId= "grid_cell_size";
const std::string EditorObjectSystemDefinition::k_snapIncrementPropertyId= "snap_increment";
const std::string EditorObjectSystemDefinition::k_snapEnabledPropertyId= "snap_enabled";
const std::string EditorObjectSystemDefinition::k_rulerDisplayUnitsPropertyId= "ruler_display_units";
const std::string EditorObjectSystemDefinition::k_debugCameraAlignmentPropertyId= "debug_camera_alignment";
const std::string EditorObjectSystemDefinition::k_modelStencilDisplayModePropertyId= "model_stencil_display_mode";
const std::string EditorObjectSystemDefinition::k_debugRenderInCompositorPropertyId= "debug_render_in_compositor";
const std::string EditorObjectSystemDefinition::k_renderFrameRatePropertyId= "render_frame_rate";
const std::string EditorObjectSystemDefinition::k_renderComponentNamesPropertyId= "render_component_names";
const std::string EditorObjectSystemDefinition::k_editorCameraStatePropertyId= "editor_camera_state";
const std::string EditorObjectSystemDefinition::k_outlinerOpenStatePropertyId= "outliner_open_state";
const std::string EditorObjectSystemDefinition::k_outlinerTreeHeightPropertyId= "outliner_tree_height";

// Viewpoint names used as the keys of the saved ortho views, in eCameraViewpoint order
static const char* k_orthoViewpointStrings[EditorCameraState::k_orthoViewCount]= {"top",  "bottom", "front",
																				  "back", "left",   "right"};

// Keys within the saved camera state block
static const char* k_cameraStatePerspectiveKey= "perspective";
static const char* k_cameraStateOrthoViewsKey= "ortho_views";
static const char* k_cameraStateActiveViewKey= "active_view";
static const char* k_cameraStatePositionKey= "position";
static const char* k_cameraStateYawKey= "yaw_degrees";
static const char* k_cameraStatePitchKey= "pitch_degrees";
static const char* k_cameraStateTargetKey= "target";
static const char* k_cameraStateExtentKey= "extent";
static const char* k_cameraStatePerspectiveViewName= "perspective";

// The index of a saved view name, or -1 for the perspective view
static int findCameraViewIndex(const std::string& viewName)
{
	for (int viewIndex= 0; viewIndex < EditorCameraState::k_orthoViewCount; ++viewIndex)
	{
		if (viewName == k_orthoViewpointStrings[viewIndex])
			return viewIndex;
	}

	return -1;
}

static configuru::Config writeCameraStateToJSON(const EditorCameraState& cameraState)
{
	configuru::Config cameraPt= configuru::Config::object();

	configuru::Config perspectivePt= configuru::Config::object();
	CommonConfig::writeVector3f(perspectivePt, k_cameraStatePositionKey,
								glm_vec3_to_MikanVector3f(cameraState.perspectivePosition));
	perspectivePt[k_cameraStateYawKey]= cameraState.perspectiveYawDegrees;
	perspectivePt[k_cameraStatePitchKey]= cameraState.perspectivePitchDegrees;
	cameraPt[k_cameraStatePerspectiveKey]= perspectivePt;

	configuru::Config orthoViewsPt= configuru::Config::object();
	for (int viewIndex= 0; viewIndex < EditorCameraState::k_orthoViewCount; ++viewIndex)
	{
		configuru::Config viewPt= configuru::Config::object();
		CommonConfig::writeVector3f(viewPt, k_cameraStateTargetKey,
									glm_vec3_to_MikanVector3f(cameraState.orthoTargets[viewIndex]));
		viewPt[k_cameraStateExtentKey]= cameraState.orthoExtents[viewIndex];
		orthoViewsPt[k_orthoViewpointStrings[viewIndex]]= viewPt;
	}
	cameraPt[k_cameraStateOrthoViewsKey]= orthoViewsPt;

	const bool bIsOrthoView=
		cameraState.activeView >= 0 && cameraState.activeView < EditorCameraState::k_orthoViewCount;
	cameraPt[k_cameraStateActiveViewKey]=
		bIsOrthoView ? k_orthoViewpointStrings[cameraState.activeView] : k_cameraStatePerspectiveViewName;

	return cameraPt;
}

static void readCameraStateFromJSON(const configuru::Config& cameraPt, EditorCameraState& outCameraState)
{
	if (cameraPt.has_key(k_cameraStatePerspectiveKey))
	{
		const configuru::Config& perspectivePt= cameraPt[k_cameraStatePerspectiveKey];

		MikanVector3f position= glm_vec3_to_MikanVector3f(outCameraState.perspectivePosition);
		CommonConfig::readVector3f(perspectivePt, k_cameraStatePositionKey, position);
		outCameraState.perspectivePosition= MikanVector3f_to_glm_vec3(position);

		outCameraState.perspectiveYawDegrees=
			perspectivePt.get_or<float>(k_cameraStateYawKey, outCameraState.perspectiveYawDegrees);
		outCameraState.perspectivePitchDegrees=
			perspectivePt.get_or<float>(k_cameraStatePitchKey, outCameraState.perspectivePitchDegrees);
	}

	if (cameraPt.has_key(k_cameraStateOrthoViewsKey))
	{
		const configuru::Config& orthoViewsPt= cameraPt[k_cameraStateOrthoViewsKey];

		for (int viewIndex= 0; viewIndex < EditorCameraState::k_orthoViewCount; ++viewIndex)
		{
			const char* viewName= k_orthoViewpointStrings[viewIndex];
			if (!orthoViewsPt.has_key(viewName))
				continue;

			const configuru::Config& viewPt= orthoViewsPt[viewName];

			MikanVector3f target= glm_vec3_to_MikanVector3f(outCameraState.orthoTargets[viewIndex]);
			CommonConfig::readVector3f(viewPt, k_cameraStateTargetKey, target);
			outCameraState.orthoTargets[viewIndex]= MikanVector3f_to_glm_vec3(target);

			outCameraState.orthoExtents[viewIndex]=
				viewPt.get_or<float>(k_cameraStateExtentKey, outCameraState.orthoExtents[viewIndex]);
		}
	}

	const std::string activeViewName=
		cameraPt.get_or<std::string>(k_cameraStateActiveViewKey, k_cameraStatePerspectiveViewName);
	outCameraState.activeView= findCameraViewIndex(activeViewName);
}

bool EditorCameraState::operator==(const EditorCameraState& other) const
{
	return perspectivePosition == other.perspectivePosition && perspectiveYawDegrees == other.perspectiveYawDegrees
		   && perspectivePitchDegrees == other.perspectivePitchDegrees && orthoTargets == other.orthoTargets
		   && orthoExtents == other.orthoExtents && activeView == other.activeView;
}

configuru::Config EditorObjectSystemDefinition::writeToJSON()
{
	configuru::Config pt= MikanObjectSystemDefinition::writeToJSON();

	pt[k_renderOriginFlagPropertyId]= m_editorSettings.bRenderOrigin;
	pt[k_renderAnchorsPropertyId]= m_editorSettings.bDebugRenderAnchors;
	pt[k_renderQuadStencilsPropertyId]= m_editorSettings.bDebugRenderQuadStencils;
	pt[k_renderBoxStencilsPropertyId]= m_editorSettings.bDebugRenderBoxStencils;
	pt[k_renderModelStencilsPropertyId]= m_editorSettings.bDebugRenderModelStencils;
	pt[k_renderQuadShapesPropertyId]= m_editorSettings.bDebugRenderQuadShapes;
	pt[k_renderBoxShapesPropertyId]= m_editorSettings.bDebugRenderBoxShapes;
	pt[k_renderModelShapesPropertyId]= m_editorSettings.bDebugRenderModelShapes;
	pt[k_cameraSpeedPropertyId]= m_editorSettings.cameraSpeed;
	pt[k_gridExtentPropertyId]= m_editorSettings.gridExtent;
	pt[k_gridCellSizePropertyId]= m_editorSettings.gridCellSize;
	pt[k_snapIncrementPropertyId]= m_editorSettings.snapIncrement;
	pt[k_snapEnabledPropertyId]= m_editorSettings.bSnapEnabled;
	pt[k_rulerDisplayUnitsPropertyId]= (int)m_editorSettings.rulerDisplayUnits;
	pt[k_debugCameraAlignmentPropertyId]= m_editorSettings.bDebugCameraAlignment;
	pt[k_modelStencilDisplayModePropertyId]= (int)m_editorSettings.modelStencilDisplayMode;
	pt[k_debugRenderInCompositorPropertyId]= m_editorSettings.bDebugRenderInCompositor;
	pt[k_renderFrameRatePropertyId]= m_editorSettings.bRenderFrameRate;
	pt[k_renderComponentNamesPropertyId]= m_editorSettings.bRenderComponentNames;
	pt[k_editorCameraStatePropertyId]= writeCameraStateToJSON(m_editorSettings.cameraState);

	configuru::Config outlinerPt= configuru::Config::object();
	for (const auto& [nodeKey, bOpen] : m_editorSettings.outlinerOpenState)
	{
		outlinerPt[nodeKey]= bOpen;
	}
	pt[k_outlinerOpenStatePropertyId]= outlinerPt;
	pt[k_outlinerTreeHeightPropertyId]= m_editorSettings.outlinerTreeHeight;

	return pt;
}

void EditorObjectSystemDefinition::readFromJSON(const configuru::Config& pt)
{
	MikanObjectSystemDefinition::readFromJSON(pt);

	m_editorSettings.bRenderOrigin= pt.get_or<bool>(k_renderOriginFlagPropertyId, m_editorSettings.bRenderOrigin);
	m_editorSettings.bDebugRenderAnchors=
		pt.get_or<bool>(k_renderAnchorsPropertyId, m_editorSettings.bDebugRenderAnchors);
	m_editorSettings.bDebugRenderQuadStencils=
		pt.get_or<bool>(k_renderQuadStencilsPropertyId, m_editorSettings.bDebugRenderQuadStencils);
	m_editorSettings.bDebugRenderBoxStencils=
		pt.get_or<bool>(k_renderBoxStencilsPropertyId, m_editorSettings.bDebugRenderBoxStencils);
	m_editorSettings.bDebugRenderModelStencils=
		pt.get_or<bool>(k_renderModelStencilsPropertyId, m_editorSettings.bDebugRenderModelStencils);
	m_editorSettings.bDebugRenderQuadShapes=
		pt.get_or<bool>(k_renderQuadShapesPropertyId, m_editorSettings.bDebugRenderQuadShapes);
	m_editorSettings.bDebugRenderBoxShapes=
		pt.get_or<bool>(k_renderBoxShapesPropertyId, m_editorSettings.bDebugRenderBoxShapes);
	m_editorSettings.bDebugRenderModelShapes=
		pt.get_or<bool>(k_renderModelShapesPropertyId, m_editorSettings.bDebugRenderModelShapes);
	m_editorSettings.cameraSpeed= pt.get_or<float>(k_cameraSpeedPropertyId, m_editorSettings.cameraSpeed);
	m_editorSettings.gridExtent= pt.get_or<float>(k_gridExtentPropertyId, m_editorSettings.gridExtent);
	m_editorSettings.gridCellSize= pt.get_or<float>(k_gridCellSizePropertyId, m_editorSettings.gridCellSize);
	m_editorSettings.snapIncrement= pt.get_or<float>(k_snapIncrementPropertyId, m_editorSettings.snapIncrement);
	m_editorSettings.bSnapEnabled= pt.get_or<bool>(k_snapEnabledPropertyId, m_editorSettings.bSnapEnabled);
	m_editorSettings.rulerDisplayUnits=
		(eRulerDisplayUnits)pt.get_or<int>(k_rulerDisplayUnitsPropertyId, (int)m_editorSettings.rulerDisplayUnits);
	m_editorSettings.bDebugCameraAlignment=
		pt.get_or<bool>(k_debugCameraAlignmentPropertyId, m_editorSettings.bDebugCameraAlignment);
	m_editorSettings.modelStencilDisplayMode= (eStencilDisplayMode)pt.get_or<int>(
		k_modelStencilDisplayModePropertyId, (int)m_editorSettings.modelStencilDisplayMode);
	m_editorSettings.bDebugRenderInCompositor=
		pt.get_or<bool>(k_debugRenderInCompositorPropertyId, m_editorSettings.bDebugRenderInCompositor);
	m_editorSettings.bRenderFrameRate= pt.get_or<bool>(k_renderFrameRatePropertyId, m_editorSettings.bRenderFrameRate);
	m_editorSettings.bRenderComponentNames=
		pt.get_or<bool>(k_renderComponentNamesPropertyId, m_editorSettings.bRenderComponentNames);

	// Projects saved before the camera poses were persisted keep the defaults
	if (pt.has_key(k_editorCameraStatePropertyId))
	{
		readCameraStateFromJSON(pt[k_editorCameraStatePropertyId], m_editorSettings.cameraState);
	}

	m_editorSettings.outlinerOpenState.clear();
	if (pt.has_key(k_outlinerOpenStatePropertyId))
	{
		for (const auto& entry : pt[k_outlinerOpenStatePropertyId].as_object())
		{
			if (entry.value().is_bool())
				m_editorSettings.outlinerOpenState[entry.key()]= (bool)entry.value();
		}
	}

	m_editorSettings.outlinerTreeHeight=
		pt.get_or<float>(k_outlinerTreeHeightPropertyId, m_editorSettings.outlinerTreeHeight);
}

void EditorObjectSystemDefinition::setRenderOriginFlag(bool flag)
{
	if (m_editorSettings.bRenderOrigin != flag)
	{
		m_editorSettings.bRenderOrigin= flag;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_renderOriginFlagPropertyId));
	}
}

void EditorObjectSystemDefinition::setRenderAnchorsFlag(bool flag)
{
	if (m_editorSettings.bDebugRenderAnchors != flag)
	{
		m_editorSettings.bDebugRenderAnchors= flag;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_renderAnchorsPropertyId));
	}
}

void EditorObjectSystemDefinition::setRenderQuadStencilsFlag(bool flag)
{
	if (m_editorSettings.bDebugRenderQuadStencils != flag)
	{
		m_editorSettings.bDebugRenderQuadStencils= flag;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_renderQuadStencilsPropertyId));
	}
}

void EditorObjectSystemDefinition::setRenderBoxStencilsFlag(bool flag)
{
	if (m_editorSettings.bDebugRenderBoxStencils != flag)
	{
		m_editorSettings.bDebugRenderBoxStencils= flag;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_renderBoxStencilsPropertyId));
	}
}

void EditorObjectSystemDefinition::setRenderModelStencilsFlag(bool flag)
{
	if (m_editorSettings.bDebugRenderModelStencils != flag)
	{
		m_editorSettings.bDebugRenderModelStencils= flag;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_renderModelStencilsPropertyId));
	}
}

void EditorObjectSystemDefinition::setRenderQuadShapesFlag(bool flag)
{
	if (m_editorSettings.bDebugRenderQuadShapes != flag)
	{
		m_editorSettings.bDebugRenderQuadShapes= flag;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_renderQuadShapesPropertyId));
	}
}

void EditorObjectSystemDefinition::setRenderBoxShapesFlag(bool flag)
{
	if (m_editorSettings.bDebugRenderBoxShapes != flag)
	{
		m_editorSettings.bDebugRenderBoxShapes= flag;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_renderBoxShapesPropertyId));
	}
}

void EditorObjectSystemDefinition::setRenderModelShapesFlag(bool flag)
{
	if (m_editorSettings.bDebugRenderModelShapes != flag)
	{
		m_editorSettings.bDebugRenderModelShapes= flag;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_renderModelShapesPropertyId));
	}
}

void EditorObjectSystemDefinition::setCameraSpeed(float speed)
{
	if (m_editorSettings.cameraSpeed != speed)
	{
		m_editorSettings.cameraSpeed= speed;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_cameraSpeedPropertyId));
	}
}

void EditorObjectSystemDefinition::setGridExtent(float extent)
{
	if (m_editorSettings.gridExtent != extent)
	{
		m_editorSettings.gridExtent= extent;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_gridExtentPropertyId));
	}
}

void EditorObjectSystemDefinition::setGridCellSize(float cellSize)
{
	if (m_editorSettings.gridCellSize != cellSize)
	{
		m_editorSettings.gridCellSize= cellSize;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_gridCellSizePropertyId));
	}
}

void EditorObjectSystemDefinition::setSnapIncrement(float increment)
{
	if (m_editorSettings.snapIncrement != increment)
	{
		m_editorSettings.snapIncrement= increment;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_snapIncrementPropertyId));
	}
}

void EditorObjectSystemDefinition::setSnapEnabled(bool enabled)
{
	if (m_editorSettings.bSnapEnabled != enabled)
	{
		m_editorSettings.bSnapEnabled= enabled;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_snapEnabledPropertyId));
	}
}

void EditorObjectSystemDefinition::setRulerDisplayUnits(eRulerDisplayUnits units)
{
	if (m_editorSettings.rulerDisplayUnits != units)
	{
		m_editorSettings.rulerDisplayUnits= units;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_rulerDisplayUnitsPropertyId));
	}
}

void EditorObjectSystemDefinition::setDebugCameraAlignment(bool enabled)
{
	if (m_editorSettings.bDebugCameraAlignment != enabled)
	{
		m_editorSettings.bDebugCameraAlignment= enabled;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_debugCameraAlignmentPropertyId));
	}
}

void EditorObjectSystemDefinition::setModelStencilDisplayMode(eStencilDisplayMode mode)
{
	if (m_editorSettings.modelStencilDisplayMode != mode)
	{
		m_editorSettings.modelStencilDisplayMode= mode;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_modelStencilDisplayModePropertyId));
	}
}

void EditorObjectSystemDefinition::setDebugRenderInCompositor(bool enabled)
{
	if (m_editorSettings.bDebugRenderInCompositor != enabled)
	{
		m_editorSettings.bDebugRenderInCompositor= enabled;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_debugRenderInCompositorPropertyId));
	}
}

void EditorObjectSystemDefinition::setRenderFrameRate(bool enabled)
{
	if (m_editorSettings.bRenderFrameRate != enabled)
	{
		m_editorSettings.bRenderFrameRate= enabled;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_renderFrameRatePropertyId));
	}
}

void EditorObjectSystemDefinition::setRenderComponentNames(bool enabled)
{
	if (m_editorSettings.bRenderComponentNames != enabled)
	{
		m_editorSettings.bRenderComponentNames= enabled;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_renderComponentNamesPropertyId));
	}
}

void EditorObjectSystemDefinition::setEditorCameraState(const EditorCameraState& cameraState)
{
	if (m_editorSettings.cameraState != cameraState)
	{
		m_editorSettings.cameraState= cameraState;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_editorCameraStatePropertyId));
	}
}

bool EditorObjectSystemDefinition::getOutlinerNodeOpen(const std::string& nodeKey, bool bDefaultOpen) const
{
	const auto it= m_editorSettings.outlinerOpenState.find(nodeKey);
	return it != m_editorSettings.outlinerOpenState.end() ? it->second : bDefaultOpen;
}

void EditorObjectSystemDefinition::setOutlinerNodeOpen(const std::string& nodeKey, bool bOpen, bool bDefaultOpen)
{
	if (nodeKey.empty())
		return;

	std::map<std::string, bool>& openState= m_editorSettings.outlinerOpenState;
	const auto it= openState.find(nodeKey);
	bool bChanged= false;

	if (bOpen == bDefaultOpen)
	{
		if (it != openState.end())
		{
			openState.erase(it);
			bChanged= true;
		}
	}
	else if (it == openState.end() || it->second != bOpen)
	{
		openState[nodeKey]= bOpen;
		bChanged= true;
	}

	if (bChanged)
	{
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_outlinerOpenStatePropertyId));
	}
}

void EditorObjectSystemDefinition::setOutlinerTreeHeight(float height)
{
	if (m_editorSettings.outlinerTreeHeight != height)
	{
		m_editorSettings.outlinerTreeHeight= height;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_outlinerTreeHeightPropertyId));
	}
}

// -- EditorObjectSystem -----
bool EditorObjectSystem::init(MikanObjectSystemDefinitionPtr definitionPtr)
{
	MikanObjectSystem::init(definitionPtr);

	auto editorConfig= getEditorSystemConfig();

	IEditorWindow* ownerWindow= getOwnerProjectManager()->getOwnerWindow();
	ownerWindow->OnAppStageEntered+= MakeDelegate(this, &EditorObjectSystem::onAppStageEntered);

	SceneObjectSystemPtr sceneObjectSystem= getObjectSystemOfType<SceneObjectSystem>();
	sceneObjectSystem->OnSceneActivated+= MakeDelegate(this, &EditorObjectSystem::onSceneActivated);
	sceneObjectSystem->OnSceneDeactivated+= MakeDelegate(this, &EditorObjectSystem::onSceneDeactivated);

	AnchorObjectSystemPtr anchorObjectSystem= getObjectSystemOfType<AnchorObjectSystem>();
	anchorObjectSystem->OnComponentDisposed+= MakeDelegate(this, &EditorObjectSystem::onActorDisposed);

	QuadStencilSystemPtr quadStencilSystem= getObjectSystemOfType<QuadStencilSystem>();
	quadStencilSystem->OnComponentDisposed+= MakeDelegate(this, &EditorObjectSystem::onActorDisposed);

	BoxStencilSystemPtr boxStencilSystem= getObjectSystemOfType<BoxStencilSystem>();
	boxStencilSystem->OnComponentDisposed+= MakeDelegate(this, &EditorObjectSystem::onActorDisposed);

	ModelStencilSystemPtr modelStencilSystem= getObjectSystemOfType<ModelStencilSystem>();
	modelStencilSystem->OnComponentDisposed+= MakeDelegate(this, &EditorObjectSystem::onActorDisposed);

	m_lastestRaycastResult= ColliderRaycastHitResult();
	m_hoverComponentWeakPtr.reset();
	m_selectedComponentWeakPtr.reset();

	return true;
}

void EditorObjectSystem::createSceneTransformGizmo(SceneComponentPtr ownerScene)
{
	disposeSceneTransformGizmo();

	m_gizmoObjectWeakPtr= newEmptyObject();
	MikanObjectPtr gizmoObjectPtr= m_gizmoObjectWeakPtr.lock();
	gizmoObjectPtr->setName("Gizmo");

	GizmoTransformComponentPtr transformGizmoPtr= gizmoObjectPtr->addComponent<GizmoTransformComponent>();
	transformGizmoPtr->setName("Gizmo");
	gizmoObjectPtr->setRootComponent(transformGizmoPtr);
	m_gizmoComponentWeakPtr= transformGizmoPtr;

	gizmoObjectPtr->addComponent<SelectionComponent>();

	const float W= GizmoTransformComponent::k_gizmoBaseWidth;
	const float R= GizmoTransformComponent::k_gizmoBaseRadius;
	const float P= R * 0.1f;

	GizmoTranslateComponentPtr translateComponentPtr= gizmoObjectPtr->addComponent<GizmoTranslateComponent>();
	createGizmoBoxCollider(gizmoObjectPtr, "xyTranslateHandle", glm::vec3(P, P, 0.f), glm::vec3(P, P, W * 0.1f), 3);
	createGizmoBoxCollider(gizmoObjectPtr, "xzTranslateHandle", glm::vec3(P, 0.f, P), glm::vec3(P, W * 0.1f, P), 3);
	createGizmoBoxCollider(gizmoObjectPtr, "yzTranslateHandle", glm::vec3(0.f, P, P), glm::vec3(W * 0.1f, P, P), 3);
	createGizmoDiskCollider(gizmoObjectPtr, "viewPlaneTranslateHandle", glm::vec3(0.f), glm::vec3(0.f, 0.f, 1.f),
							W * 2.5f, 2);
	createGizmoBoxCollider(gizmoObjectPtr, "xAxisTranslateHandle", glm::vec3(R / 2.f, 0.f, 0.f),
						   glm::vec3(R / 2.f, W, W), 1);
	createGizmoBoxCollider(gizmoObjectPtr, "yAxisTranslateHandle", glm::vec3(0.f, R / 2.f, 0.f),
						   glm::vec3(W, R / 2.f, W), 1);
	createGizmoBoxCollider(gizmoObjectPtr, "zAxisTranslateHandle", glm::vec3(0.f, 0.f, R / 2.f),
						   glm::vec3(W, W, R / 2.f), 1);

	GizmoRotateComponentPtr rotateComponentPtr= gizmoObjectPtr->addComponent<GizmoRotateComponent>();
	createGizmoDiskCollider(gizmoObjectPtr, "xAxisRotateHandle", glm::vec3(0.f), glm::vec3(1.f, 0.f, 0.f), R, 1);
	createGizmoDiskCollider(gizmoObjectPtr, "yAxisRotateHandle", glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f), R, 1);
	createGizmoDiskCollider(gizmoObjectPtr, "zAxisRotateHandle", glm::vec3(0.f), glm::vec3(0.f, 0.f, 1.f), R, 1);

	GizmoScaleComponentPtr scaleComponentPtr= gizmoObjectPtr->addComponent<GizmoScaleComponent>();
	createGizmoBoxCollider(gizmoObjectPtr, "centerScaleHandle", glm::vec3(0.f, 0.f, 0.f), glm::vec3(W, W, W), 1);
	createGizmoBoxCollider(gizmoObjectPtr, "xAxisScaleHandle", glm::vec3(R, 0.f, 0.f), glm::vec3(W, W, W), 1);
	createGizmoBoxCollider(gizmoObjectPtr, "yAxisScaleHandle", glm::vec3(0.f, R, 0.f), glm::vec3(W, W, W), 1);
	createGizmoBoxCollider(gizmoObjectPtr, "zAxisScaleHandle", glm::vec3(0.f, 0.f, R), glm::vec3(W, W, W), 1);

	gizmoObjectPtr->init();

	// Attach the gizmo to the scene
	gizmoObjectPtr->getRootComponent()->attachToComponent(ownerScene);
}

void EditorObjectSystem::disposeSceneTransformGizmo()
{
	MikanObjectPtr gizmoObjectPtr= m_gizmoObjectWeakPtr.lock();
	if (gizmoObjectPtr)
	{
		// deleteObject, NOT gizmoObjectPtr->dispose(): the gizmo comes from
		// newEmptyObject(), so the system's object list holds the only strong
		// reference to it. Disposing it without also removing it from that list
		// leaves a disposed object behind, which deleteAllObjects() then
		// disposes a second time - tripping MikanObject::dispose's
		// assert(!m_bIsDisposed) on shutdown - and leaks one object per scene
		// switch, since a fresh gizmo is created on every scene activation.
		deleteObject(gizmoObjectPtr);
	}

	m_gizmoObjectWeakPtr.reset();
	m_gizmoComponentWeakPtr.reset();
}

void EditorObjectSystem::createGizmoBoxCollider(MikanObjectPtr gizmoObjectPtr, const std::string& name,
												const glm::vec3& center, const glm::vec3& halfExtents,
												const int priority)
{
	BoxColliderComponentPtr colliderPtr= gizmoObjectPtr->addComponent<BoxColliderComponent>(name);

	colliderPtr->setName(name);
	colliderPtr->setHalfExtents(halfExtents);
	colliderPtr->setRelativeTransform(GlmTransform(center));
	colliderPtr->attachToComponent(gizmoObjectPtr->getRootComponent());
	colliderPtr->setEnabled(false);
	colliderPtr->setPriority(priority);
}

void EditorObjectSystem::createGizmoDiskCollider(MikanObjectPtr gizmoObjectPtr, const std::string& name,
												 const glm::vec3& center, const glm::vec3& normal, const float radius,
												 const int priority)
{
	DiskColliderComponentPtr colliderPtr= gizmoObjectPtr->addComponent<DiskColliderComponent>(name);

	glm::quat orientation= glm::quat(glm::vec3(0.f, 1.f, 0.f), normal);
	colliderPtr->setRelativeTransform(GlmTransform(center, orientation));
	colliderPtr->attachToComponent(gizmoObjectPtr->getRootComponent());
	colliderPtr->setRadius(radius);
	colliderPtr->setName(name);
	colliderPtr->setEnabled(false);
	colliderPtr->setPriority(priority);
}

void EditorObjectSystem::dispose()
{
	SceneObjectSystemPtr sceneObjectSystem= getObjectSystemOfType<SceneObjectSystem>();
	sceneObjectSystem->OnSceneActivated-= MakeDelegate(this, &EditorObjectSystem::onSceneActivated);
	sceneObjectSystem->OnSceneDeactivated-= MakeDelegate(this, &EditorObjectSystem::onSceneDeactivated);

	AnchorObjectSystemPtr anchorObjectSystem= getObjectSystemOfType<AnchorObjectSystem>();
	anchorObjectSystem->OnComponentDisposed-= MakeDelegate(this, &EditorObjectSystem::onActorDisposed);

	QuadStencilSystemPtr quadStencilSystem= getObjectSystemOfType<QuadStencilSystem>();
	quadStencilSystem->OnComponentDisposed-= MakeDelegate(this, &EditorObjectSystem::onActorDisposed);

	BoxStencilSystemPtr boxStencilSystem= getObjectSystemOfType<BoxStencilSystem>();
	boxStencilSystem->OnComponentDisposed-= MakeDelegate(this, &EditorObjectSystem::onActorDisposed);

	ModelStencilSystemPtr modelStencilSystem= getObjectSystemOfType<ModelStencilSystem>();
	modelStencilSystem->OnComponentDisposed-= MakeDelegate(this, &EditorObjectSystem::onActorDisposed);

	disposeSceneTransformGizmo();

	MikanObjectSystem::dispose();
}

void EditorObjectSystem::renderGizmo(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera)
{
	GizmoTransformComponentPtr gizmoComponent= m_gizmoComponentWeakPtr.lock();
	if (gizmoComponent)
	{
		gizmoComponent->customRender(graphicsContext, viewportCamera);
	}
}

bool EditorObjectSystem::isRulerSnapActive(MikanViewportPtr targetViewport) const
{
	// Hold Shift while measuring to invert the configured snap baseline
	// (default off: Shift = snap, release = free).
	const bool shiftHeld= targetViewport ? targetViewport->getIsShiftPressed() : false;
	return getEditorSettings().bSnapEnabled != shiftHeld;
}

glm::vec3 EditorObjectSystem::projectAndSnapMeasurePoint(const glm::vec3& worldPoint) const
{
	glm::vec3 point= worldPoint;

	// The orthographic ray origin lives on the near plane, far from the scene along the
	// view axis. Project it onto the plane through the ortho target so the ruler line
	// renders among the geometry (parallel projection keeps the on-screen result identical).
	MikanViewportPtr viewport= getPrimaryViewport();
	MikanCameraPtr camera= viewport ? viewport->getCurrentMikanCamera() : nullptr;
	if (camera && camera->isOrthographic())
	{
		const glm::vec3 forward= camera->getCameraForwardFromViewMatrix();
		const glm::vec3 target= camera->getOrthoTargetPosition();
		point= point - forward * glm::dot(point - target, forward);
	}

	// Snap to the grid increment if snapping is currently active
	const EditorSettings& settings= getEditorSettings();
	if (isRulerSnapActive(viewport) && settings.snapIncrement > 0.f)
	{
		const float inc= settings.snapIncrement;
		point.x= std::round(point.x / inc) * inc;
		point.y= std::round(point.y / inc) * inc;
		point.z= std::round(point.z / inc) * inc;
	}

	return point;
}

void EditorObjectSystem::renderRuler(IMkGraphicsContext* graphicsContext, MikanViewportPtr targetViewport)
{
	MikanCameraPtr viewportCamera= targetViewport->getCurrentMikanCamera();

	if (!m_hasMeasurement || !viewportCamera || !viewportCamera->isOrthographic())
		return;

	const glm::vec3 color(1.f, 1.f, 0.f); // yellow

	drawSegment(graphicsContext, glm::mat4(1.f), m_measureStart, m_measureEnd, color);
	drawPoint(graphicsContext, glm::mat4(1.f), m_measureStart, color, 6.f);
	drawPoint(graphicsContext, glm::mat4(1.f), m_measureEnd, color, 6.f);

	const float distanceMeters= glm::length(m_measureEnd - m_measureStart);
	const glm::vec3 midPoint= (m_measureStart + m_measureEnd) * 0.5f;

	// Convert the world-space distance (meters) into the configured display units
	float displayValue= distanceMeters;
	const wchar_t* fmt= L"%.3f m";
	const wchar_t* fmtSnap= L"%.3f m (snap)";
	switch (getEditorSettings().rulerDisplayUnits)
	{
	case eRulerDisplayUnits::meters:
		displayValue= distanceMeters;
		fmt= L"%.3f m";
		fmtSnap= L"%.3f m (snap)";
		break;
	case eRulerDisplayUnits::centimeters:
		displayValue= distanceMeters * 100.f;
		fmt= L"%.2f cm";
		fmtSnap= L"%.2f cm (snap)";
		break;
	case eRulerDisplayUnits::millimeters:
	default:
		displayValue= distanceMeters * 1000.f;
		fmt= L"%.1f mm";
		fmtSnap= L"%.1f mm (snap)";
		break;
	}

	TextStyle style= getDefaultTextStyle();
	drawTextAtWorldPosition(graphicsContext, style, midPoint, isRulerSnapActive(targetViewport) ? fmtSnap : fmt,
							displayValue);
}

EditorObjectSystemDefinitionConstPtr EditorObjectSystem::getEditorSystemConfigConst() const
{
	auto projectConfig= getProjectConfig();

	return projectConfig ? projectConfig->editorConfig : EditorObjectSystemDefinitionConstPtr();
}

EditorObjectSystemDefinitionPtr EditorObjectSystem::getEditorSystemConfig()
{
	return std::const_pointer_cast<EditorObjectSystemDefinition>(getEditorSystemConfigConst());
}

MikanComponentPtr EditorObjectSystem::getComponentById(int componentId) const
{
	// EditorObjectSystem doesn't manage components by ID
	return MikanComponentPtr();
}

bool EditorObjectSystem::getComponentList(const std::string& componentClassName,
										  std::vector<MikanComponentPtr>& outComponentList) const
{
	// EditorObjectSystem doesn't manage ownership of components
	return false;
}

bool EditorObjectSystem::getComponentIdList(const std::string& componentClassName,
											std::vector<int>& outComponentIdList) const
{
	// EditorObjectSystem doesn't manage ownership of components
	return false;
}

MikanViewportPtr EditorObjectSystem::getPrimaryViewport() const
{
	if (!m_viewports.empty())
		if (auto viewport= m_viewports[0].lock())
			return viewport;
	return nullptr;
}

MikanCameraPtr EditorObjectSystem::getPrimaryCamera() const
{
	if (auto viewport= getPrimaryViewport())
		return viewport->getCurrentMikanCamera();
	return nullptr;
}

void EditorObjectSystem::bindViewport(MikanViewportWeakPtr viewportWeakPtr)
{
	MikanViewportPtr viewportPtr= viewportWeakPtr.lock();
	if (viewportPtr)
	{
		const auto it=
			std::find_if(m_viewports.begin(), m_viewports.end(),
						 [viewportPtr](const MikanViewportWeakPtr& entry) { return entry.lock() == viewportPtr; });
		if (it == m_viewports.end())
		{
			viewportPtr->OnMouseExited+= MakeDelegate(this, &EditorObjectSystem::onMouseExited);
			viewportPtr->OnMouseRayChanged+= MakeDelegate(this, &EditorObjectSystem::onMouseRayChanged);
			viewportPtr->OnMouseRayButtonUp+= MakeDelegate(this, &EditorObjectSystem::onMouseRayButtonUp);
			viewportPtr->OnMouseRayButtonDown+= MakeDelegate(this, &EditorObjectSystem::onMouseRayButtonDown);

			m_viewports.push_back(viewportWeakPtr);
		}
	}
}

void EditorObjectSystem::unbindViewport(MikanViewportWeakPtr viewportWeakPtr)
{
	MikanViewportPtr viewportPtr= viewportWeakPtr.lock();
	if (viewportPtr)
	{
		const auto it=
			std::find_if(m_viewports.begin(), m_viewports.end(),
						 [viewportPtr](const MikanViewportWeakPtr& entry) { return entry.lock() == viewportPtr; });
		if (it != m_viewports.end())
		{
			viewportPtr->OnMouseExited-= MakeDelegate(this, &EditorObjectSystem::onMouseExited);
			viewportPtr->OnMouseRayChanged-= MakeDelegate(this, &EditorObjectSystem::onMouseRayChanged);
			viewportPtr->OnMouseRayButtonUp-= MakeDelegate(this, &EditorObjectSystem::onMouseRayButtonUp);
			viewportPtr->OnMouseRayButtonDown-= MakeDelegate(this, &EditorObjectSystem::onMouseRayButtonDown);

			m_viewports.erase(it);
		}
	}
}

void EditorObjectSystem::clearViewports()
{
	for (MikanViewportWeakPtr& viewportWeakPtr : m_viewports)
	{
		MikanViewportPtr viewportPtr= viewportWeakPtr.lock();
		if (viewportPtr)
		{
			viewportPtr->OnMouseExited-= MakeDelegate(this, &EditorObjectSystem::onMouseExited);
			viewportPtr->OnMouseRayChanged-= MakeDelegate(this, &EditorObjectSystem::onMouseRayChanged);
			viewportPtr->OnMouseRayButtonUp-= MakeDelegate(this, &EditorObjectSystem::onMouseRayButtonUp);
			viewportPtr->OnMouseRayButtonDown-= MakeDelegate(this, &EditorObjectSystem::onMouseRayButtonDown);
		}
	}
	m_viewports.clear();
}

// App Events
void EditorObjectSystem::onAppStageEntered(class AppStage* oldAppStage, class AppStage* newAppStage)
{
	if (newAppStage->getAppStageName() == AppStage_Project::APP_STAGE_NAME)
	{
		InputManager* inputManager= getOwnerWindow()->getInputManager();
		inputManager->fetchOrAddKeyBindings(MkKey::DELETE_KEYCODE)->OnKeyPressed+=
			MakeDelegate(this, &EditorObjectSystem::onDeletePressed);
		inputManager->fetchOrAddKeyBindings(MkKey::BACKSPACE)->OnKeyPressed+=
			MakeDelegate(this, &EditorObjectSystem::onDeletePressed);

		auto gizmoComponent= m_gizmoComponentWeakPtr.lock();
		if (gizmoComponent)
		{
			gizmoComponent->bindInput();
		}
	}
}

// Keyboard Events
void EditorObjectSystem::onDeletePressed()
{
	if (OnDeleteSelectionRequested)
		OnDeleteSelectionRequested();
}

// Object System Events
void EditorObjectSystem::onSceneActivated(SceneComponentPtr newScene) { createSceneTransformGizmo(newScene); }

void EditorObjectSystem::onSceneDeactivated(SceneComponentPtr oldScene)
{
	clearSelectedComponent();
	clearHoveredComponent();

	disposeSceneTransformGizmo();
}

void EditorObjectSystem::onActorDisposed(MikanObjectSystemPtr system, MikanComponentConstPtr component)
{
	SelectionComponentPtr hoverComponentPtr= m_hoverComponentWeakPtr.lock();
	if (hoverComponentPtr == component)
	{
		clearHoveredComponent();
	}

	SelectionComponentPtr selectedComponent= m_selectedComponentWeakPtr.lock();
	if (selectedComponent == component)
	{
		clearSelectedComponent();
	}
}

void EditorObjectSystem::onMouseExited()
{
	SelectionComponentPtr oldHoverComponentPtr= m_hoverComponentWeakPtr.lock();

	if (oldHoverComponentPtr)
	{
		oldHoverComponentPtr->notifyHoverExit(m_lastestRaycastResult);

		m_hoverComponentWeakPtr.reset();
		m_hoverColliderWeakPtr.reset();
		m_lastestRaycastResult= ColliderRaycastHitResult();
	}
}

void EditorObjectSystem::onMouseRayButtonDown(const glm::vec3& rayOrigin, const glm::vec3& rayDir, int button)
{
	SelectionComponentPtr currentHoverPtr= m_hoverComponentWeakPtr.lock();

	if (button == MkMouseButton::LEFT)
	{
		// See if the current selection is changing
		SelectionComponentPtr oldSelectedComponentPtr= m_selectedComponentWeakPtr.lock();
		SelectionComponentPtr newSelectedComponentPtr= currentHoverPtr;
		if (oldSelectedComponentPtr != newSelectedComponentPtr)
		{
			// Update the selection component weak ptr
			m_selectedComponentWeakPtr= newSelectedComponentPtr;

			// Send notification of selection change
			onSelectionChanged(oldSelectedComponentPtr, newSelectedComponentPtr);
		}

		// Send notification of selection grab
		if (newSelectedComponentPtr)
		{
			newSelectedComponentPtr->notifyGrab(m_lastestRaycastResult);

			// Group all transform edits of this drag into one transaction
			TransactionHistory* transactionHistory= getOwnerWindow()->getTransactionHistory();
			if (transactionHistory != nullptr)
			{
				transactionHistory->beginGesture("mouse_drag");
			}
		}
	}
	else if (button == MkMouseButton::MIDDLE)
	{
		// Middle-mouse drag starts a ruler measurement (orthographic views only, where
		// the near-plane hit point is an unambiguous in-plane world position).
		MikanCameraPtr camera= getPrimaryCamera();
		if (camera && camera->isOrthographic())
		{
			m_isMeasuring= true;
			m_hasMeasurement= true;
			m_measureStart= projectAndSnapMeasurePoint(rayOrigin);
			m_measureEnd= m_measureStart;
		}
	}
}

void EditorObjectSystem::onMouseRayChanged(const glm::vec3& rayOrigin, const glm::vec3& rayDir)
{
	// Update the active ruler measurement, if any
	if (m_isMeasuring)
	{
		m_measureEnd= projectAndSnapMeasurePoint(rayOrigin);
	}

	// While a grab is held the drag owns the cursor: nothing else takes hover,
	// so the handle keeps its highlight and other objects do not flash theirs
	SelectionComponentPtr grabbedComponentPtr= m_selectedComponentWeakPtr.lock();
	if (grabbedComponentPtr && grabbedComponentPtr->getIsGrabbed())
	{
		grabbedComponentPtr->notifyMove(rayOrigin, rayDir);
		return;
	}

	ColliderRaycastHitResult prevRaycastResult= m_lastestRaycastResult;
	m_lastestRaycastResult= ColliderRaycastHitResult();

	SelectionComponentPtr oldHoverComponentPtr= m_hoverComponentWeakPtr.lock();
	SelectionComponentPtr newHoverComponentPtr= findClosestSelectionTarget(rayOrigin, rayDir, m_lastestRaycastResult);

	ColliderComponentPtr oldHoverColliderPtr= m_hoverColliderWeakPtr.lock();
	ColliderComponentPtr newHoverColliderPtr= m_lastestRaycastResult.hitComponent.lock();

	if (newHoverComponentPtr != oldHoverComponentPtr)
	{
		if (oldHoverComponentPtr)
			oldHoverComponentPtr->notifyHoverExit(prevRaycastResult);
		if (newHoverComponentPtr)
			newHoverComponentPtr->notifyHoverEnter(m_lastestRaycastResult);
		m_hoverComponentWeakPtr= newHoverComponentPtr;
	}
	else if (newHoverColliderPtr != oldHoverColliderPtr && oldHoverComponentPtr)
	{
		// Same SelectionComponent but different collider — fire exit+enter so
		// per-handle highlighting works (e.g. moving between gizmo handles).
		oldHoverComponentPtr->notifyHoverExit(prevRaycastResult);
		if (newHoverColliderPtr)
			oldHoverComponentPtr->notifyHoverEnter(m_lastestRaycastResult);
	}

	m_hoverColliderWeakPtr= newHoverColliderPtr;

	SelectionComponentPtr selectedComponentPtr= m_selectedComponentWeakPtr.lock();
	if (selectedComponentPtr)
	{
		selectedComponentPtr->notifyMove(rayOrigin, rayDir);
	}
}

void EditorObjectSystem::onMouseRayButtonUp(const glm::vec3& rayOrigin, const glm::vec3& rayDir, int button)
{
	if (button == MkMouseButton::MIDDLE)
	{
		// Finish the ruler drag; the measurement stays displayed until the next one begins.
		m_isMeasuring= false;
	}

	SelectionComponentPtr currentSelectedPtr= m_selectedComponentWeakPtr.lock();
	if (currentSelectedPtr)
	{
		currentSelectedPtr->notifyRelease();
	}

	if (button == MkMouseButton::LEFT)
	{
		TransactionHistory* transactionHistory= getOwnerWindow()->getTransactionHistory();
		if (transactionHistory != nullptr)
		{
			transactionHistory->endGesture();
		}
	}
}

void EditorObjectSystem::onSelectionChanged(SelectionComponentPtr oldSelectedComponentPtr,
											SelectionComponentPtr newSelectedComponentPtr)
{
	GizmoTransformComponentPtr gizmoComponentPtr= m_gizmoComponentWeakPtr.lock();

	// Tell the old selection that it's getting unselected
	if (oldSelectedComponentPtr)
	{
		oldSelectedComponentPtr->notifyUnselected();
	}

	// Handle the new selection
	if (newSelectedComponentPtr)
	{
		// Tell the new selection that it's getting selected
		newSelectedComponentPtr->notifySelected();

		// Is the component selected not owned by the gizmo object?
		if (newSelectedComponentPtr->getOwnerObject() != gizmoComponentPtr->getOwnerObject())
		{
			SelectionComponentPtr oldGizmoTargetPtr= gizmoComponentPtr->getSelectionTarget();
			SelectionComponentPtr newGizmoTargetPtr= newSelectedComponentPtr;

			// Is the newly selected component not the one the transform gizmo is currently attached to?
			if (newGizmoTargetPtr && oldGizmoTargetPtr != newGizmoTargetPtr)
			{
				// Snap gizmo to the newly selected component
				gizmoComponentPtr->setSelectionTarget(newGizmoTargetPtr);
			}
		}
	}
	else
	{
		// Clean up the gizmo
		gizmoComponentPtr->clearSelectionTarget();
	}

	// Send an event for the selection changing
	if (OnSelectionChanged)
		OnSelectionChanged();
}

void EditorObjectSystem::setObjectSystemSelectionFilter(const std::set<const MikanObjectSystem*>& objectSystemFilter)
{
	if (m_objectSystemSelectionFilter != objectSystemFilter)
	{
		// Hover is recomputed on the next mouse move. The selection only goes if
		// its system left the filter: picking a stage object from the scene view
		// flips the view mode, and the object is still pickable in the new one.
		clearHoveredComponent();

		SelectionComponentPtr selectedActor= getSelectedSceneActor();
		MikanObjectPtr selectedObject= selectedActor ? selectedActor->getOwnerObject() : nullptr;
		MikanObjectSystemPtr selectedSystem= selectedObject ? selectedObject->getOwnerSystem() : nullptr;
		if (!selectedSystem || objectSystemFilter.find(selectedSystem.get()) == objectSystemFilter.end())
		{
			clearSelectedComponent();
		}

		m_objectSystemSelectionFilter= objectSystemFilter;
	}
}

SelectionComponentPtr EditorObjectSystem::getSelectedSceneActor() const
{
	SelectionComponentPtr currentSelection= m_selectedComponentWeakPtr.lock();
	if (!currentSelection)
		return nullptr;

	// If the selected component belongs to the gizmo, return what the gizmo is manipulating instead
	MikanObjectPtr gizmoObject= m_gizmoObjectWeakPtr.lock();
	if (gizmoObject && currentSelection->getOwnerObject() == gizmoObject)
	{
		GizmoTransformComponentPtr gizmoComponent= m_gizmoComponentWeakPtr.lock();
		if (gizmoComponent)
			return gizmoComponent->getSelectionTarget();
		return nullptr;
	}

	return currentSelection;
}

void EditorObjectSystem::setSelection(SelectionComponentPtr newSelectedComponentPtr)
{
	// See if the current selection is changing
	SelectionComponentPtr oldSelectedComponentPtr= m_selectedComponentWeakPtr.lock();
	if (oldSelectedComponentPtr != newSelectedComponentPtr)
	{
		// Update the selection component weak ptr
		m_selectedComponentWeakPtr= newSelectedComponentPtr;

		// Send notification of selection change
		onSelectionChanged(oldSelectedComponentPtr, newSelectedComponentPtr);
	}
}

SelectionComponentPtr EditorObjectSystem::findClosestSelectionTarget(const glm::vec3& rayOrigin,
																	 const glm::vec3& rayDir,
																	 ColliderRaycastHitResult& outRaycastResult) const
{
	ColliderRaycastHitRequest request= {};
	request.rayOrigin= rayOrigin;
	request.rayDirection= rayDir;

	// Find the closest collision result in the
	outRaycastResult= findClosestCollisionAlongRay(m_objectSystemSelectionFilter, request);

	SelectionComponentPtr closestSelectionComponent;
	ColliderComponentPtr hitCollider= outRaycastResult.hitComponent.lock();
	if (hitCollider)
	{
		MikanObjectPtr hitObject= hitCollider->getOwnerObject();

		closestSelectionComponent= hitObject->getComponentOfType<SelectionComponent>();
	}

	return closestSelectionComponent;
}

void EditorObjectSystem::clearHoveredComponent()
{
	SelectionComponentPtr hoverComponentPtr= m_hoverComponentWeakPtr.lock();
	if (hoverComponentPtr)
	{
		hoverComponentPtr->notifyHoverExit(m_lastestRaycastResult);
		m_hoverComponentWeakPtr.reset();
		m_hoverColliderWeakPtr.reset();
	}
}

void EditorObjectSystem::clearSelectedComponent()
{
	SelectionComponentPtr selectedComponent= m_selectedComponentWeakPtr.lock();
	if (selectedComponent)
	{
		// Clear the currently selected component first in case selection handler asks
		m_selectedComponentWeakPtr.reset();

		// Signal that the selection changed to nothing
		onSelectionChanged(selectedComponent, nullptr);
	}
}

void EditorObjectSystem::registerPropertyDescriptors(MikanPropertyDatabasePtr propertyDatabase)
{
	propertyDatabase->registerPropertiesForSystem<EditorObjectSystem>();
}

void EditorObjectSystem::registerFunctionDescriptors(MikanFunctionDatabasePtr functionDatabase)
{
	functionDatabase->registerFunctionsForSystem<EditorObjectSystem>();
}

// -- IEntityAccessor ----
rfk::Struct const* EditorObjectSystem::getClientAPIValuesStructType() const
{
	return &MikanEditorSystemValues::staticGetArchetype();
}

// -- IPropertyInterface ----
const std::string EditorObjectSystem::k_selectedLanguagePropertyId= "selected_language";
const std::string EditorObjectSystem::k_availableLanguageListPropertyId= "available_language_list";

void EditorObjectSystem::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	MikanObjectSystem::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
		EditorObjectSystemDefinition::k_renderOriginFlagPropertyId, MikanVariantType::BOOL));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
		EditorObjectSystemDefinition::k_renderAnchorsPropertyId, MikanVariantType::BOOL));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
		EditorObjectSystemDefinition::k_renderQuadStencilsPropertyId, MikanVariantType::BOOL));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
		EditorObjectSystemDefinition::k_renderBoxStencilsPropertyId, MikanVariantType::BOOL));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
		EditorObjectSystemDefinition::k_renderModelStencilsPropertyId, MikanVariantType::BOOL));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(EditorObjectSystemDefinition::k_cameraSpeedPropertyId,
																  MikanVariantType::FLOAT));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(EditorObjectSystem::k_selectedLanguagePropertyId,
																  MikanVariantType::STRING));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(EditorObjectSystem::k_availableLanguageListPropertyId,
																  MikanVariantType::STRING_ARRAY)
								 ->setReadOnly());
}

bool EditorObjectSystem::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == EditorObjectSystemDefinition::k_renderOriginFlagPropertyId)
	{
		EditorObjectSystemDefinitionConstPtr definition= getEditorSystemConfigConst();
		outValue= definition->getRenderOriginFlag();
		return true;
	}
	else if (propertyName == EditorObjectSystemDefinition::k_renderAnchorsPropertyId)
	{
		EditorObjectSystemDefinitionConstPtr definition= getEditorSystemConfigConst();
		outValue= definition->getRenderAnchorsFlag();
		return true;
	}
	else if (propertyName == EditorObjectSystemDefinition::k_renderQuadStencilsPropertyId)
	{
		EditorObjectSystemDefinitionConstPtr definition= getEditorSystemConfigConst();
		outValue= definition->getRenderQuadStencilsFlag();
		return true;
	}
	else if (propertyName == EditorObjectSystemDefinition::k_renderBoxStencilsPropertyId)
	{
		EditorObjectSystemDefinitionConstPtr definition= getEditorSystemConfigConst();
		outValue= definition->getRenderBoxStencilsFlag();
		return true;
	}
	else if (propertyName == EditorObjectSystemDefinition::k_renderModelStencilsPropertyId)
	{
		EditorObjectSystemDefinitionConstPtr definition= getEditorSystemConfigConst();
		outValue= definition->getRenderModelStencilsFlag();
		return true;
	}
	else if (propertyName == EditorObjectSystemDefinition::k_cameraSpeedPropertyId)
	{
		EditorObjectSystemDefinitionConstPtr definition= getEditorSystemConfigConst();
		outValue= definition->getCameraSpeed();
		return true;
	}
	else if (propertyName == EditorObjectSystem::k_selectedLanguagePropertyId)
	{
		outValue= getOwnerWindow()->getLocalizationManager()->getLanguage();
		return true;
	}
	else if (propertyName == EditorObjectSystem::k_availableLanguageListPropertyId)
	{
		outValue= getOwnerWindow()->getLocalizationManager()->getSupportedLanguages();
		return true;
	}

	return MikanObjectSystem::getPropertyValue(propertyName, outValue);
}

bool EditorObjectSystem::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == EditorObjectSystemDefinition::k_renderOriginFlagPropertyId)
	{
		EditorObjectSystemDefinitionPtr definition= getEditorSystemConfig();
		definition->setRenderOriginFlag(inValue.getBoolValue());
		return true;
	}
	else if (propertyName == EditorObjectSystemDefinition::k_renderAnchorsPropertyId)
	{
		EditorObjectSystemDefinitionPtr definition= getEditorSystemConfig();
		definition->setRenderAnchorsFlag(inValue.getBoolValue());
		return true;
	}
	else if (propertyName == EditorObjectSystemDefinition::k_renderQuadStencilsPropertyId)
	{
		EditorObjectSystemDefinitionPtr definition= getEditorSystemConfig();
		definition->setRenderQuadStencilsFlag(inValue.getBoolValue());
		return true;
	}
	else if (propertyName == EditorObjectSystemDefinition::k_renderBoxStencilsPropertyId)
	{
		EditorObjectSystemDefinitionPtr definition= getEditorSystemConfig();
		definition->setRenderBoxStencilsFlag(inValue.getBoolValue());
		return true;
	}
	else if (propertyName == EditorObjectSystemDefinition::k_renderModelStencilsPropertyId)
	{
		EditorObjectSystemDefinitionPtr definition= getEditorSystemConfig();
		definition->setRenderModelStencilsFlag(inValue.getBoolValue());
		return true;
	}
	else if (propertyName == EditorObjectSystemDefinition::k_cameraSpeedPropertyId)
	{
		EditorObjectSystemDefinitionPtr definition= getEditorSystemConfig();
		definition->setCameraSpeed(inValue.getFloatValue());
		return true;
	}
	else if (propertyName == EditorObjectSystem::k_selectedLanguagePropertyId)
	{
		std::string langCode= inValue.getUtf8Value();
		getOwnerWindow()->getLocalizationManager()->setLanguage(langCode);
		return true;
	}

	return MikanObjectSystem::setPropertyValue(propertyName, inValue);
}
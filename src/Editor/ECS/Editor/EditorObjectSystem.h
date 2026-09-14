#pragma once

#include "MikanObjectSystem.h"
#include "ColliderQuery.h"
#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "GizmoFwd.h"
#include "MulticastDelegate.h"
#include "ObjectSystemFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "MikanRendererFwd.h"
#include "SceneFwd.h"

#include <array>
#include <map>
#include <string>
#include <vector>

#include "glm/ext/vector_float3.hpp"
#include "glm/ext/quaternion_float.hpp"

// Display units for the ruler/measurement readout (scene is natively meters)
enum class eRulerDisplayUnits : int
{
	meters,
	centimeters,
	millimeters,
};

// How model stencils are drawn in the editor debug view
enum class eStencilDisplayMode : int
{
	solid,     // opaque textured mesh only
	wireframe, // wireframe edges only (lets the background video show through)
	both,      // solid mesh with wireframe overlaid
};

// Saved editor viewport camera poses: the perspective fly pose plus a pan and zoom
// for each axis-aligned orthographic viewpoint, indexed in eCameraViewpoint order.
// The defaults mirror the ones MikanCamera starts at, so a project with no saved
// state opens exactly as it did before the poses were persisted.
struct EditorCameraState
{
	static constexpr int k_orthoViewCount= 6;

	glm::vec3 perspectivePosition= glm::vec3(0.f);
	float perspectiveYawDegrees= 0.f;
	float perspectivePitchDegrees= 0.f;

	std::array<glm::vec3, k_orthoViewCount> orthoTargets= {};
	std::array<float, k_orthoViewCount> orthoExtents= {5.f, 5.f, 5.f, 5.f, 5.f, 5.f};

	// -1 for the perspective view, otherwise an eCameraViewpoint index
	int activeView= -1;

	bool operator==(const EditorCameraState& other) const;
	bool operator!=(const EditorCameraState& other) const { return !(*this == other); }
};

struct EditorSettings
{
	bool bRenderOrigin= true;
	bool bDebugRenderAnchors= true;
	bool bDebugRenderQuadStencils= true;
	bool bDebugRenderBoxStencils= true;
	bool bDebugRenderModelStencils= true;
	bool bDebugRenderQuadShapes= true;
	bool bDebugRenderBoxShapes= true;
	bool bDebugRenderModelShapes= true;
	float cameraSpeed= 1.f;

	// Floor grid + measurement snapping (world units are meters)
	float gridExtent= 10.f;    // total width/depth of the floor grid
	float gridCellSize= 0.5f;  // size of a single grid cell
	float snapIncrement= 0.1f; // ruler / measurement snap increment
	bool bSnapEnabled= false;  // ruler snap baseline; hold Shift while measuring to invert it
	eRulerDisplayUnits rulerDisplayUnits= eRulerDisplayUnits::millimeters; // ruler readout units
	bool bDebugCameraAlignment= false; // draw the MR camera-alignment debug overlay in the compositor view
	eStencilDisplayMode modelStencilDisplayMode= eStencilDisplayMode::both; // how model stencils are drawn

	// Master gate for the editor debug overlay in the compositor output window. The per-object
	// "Render <X>" flags above still apply on top of this, but they are authoring aids for the
	// editor viewport - the compositor window shows what the shot looks like, so it stays clean
	// by default.
	bool bDebugRenderInCompositor= false;

	// Frame rate readout drawn in the corner of the scene view
	bool bRenderFrameRate= true;

	// Name labels drawn at each component's position in the scene view
	bool bRenderComponentNames= true;

	// Where the editor viewport camera was left in each view
	EditorCameraState cameraState;

	// Outliner rows whose open state differs from their default, keyed by
	// ProjectOutlinerModel::getNodeStateKey. An absent row is at its default.
	std::map<std::string, bool> outlinerOpenState;

	// Height of the outliner tree above its action strip, in pixels, set by
	// dragging the splitter between them
	float outlinerTreeHeight= 300.f;
};

class EditorObjectSystemDefinition : public MikanObjectSystemDefinition
{
public:
	EditorObjectSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator)
		: MikanObjectSystemDefinition(configName, idAllocator)
		, m_editorSettings()
	{
	}

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);

	inline const EditorSettings& getEditorSettings() const { return m_editorSettings; }

	static const std::string k_renderOriginFlagPropertyId;
	inline bool getRenderOriginFlag() const { return m_editorSettings.bRenderOrigin; }
	void setRenderOriginFlag(bool flag);

	static const std::string k_renderAnchorsPropertyId;
	inline bool getRenderAnchorsFlag() const { return m_editorSettings.bDebugRenderAnchors; }
	void setRenderAnchorsFlag(bool flag);

	static const std::string k_renderQuadStencilsPropertyId;
	inline bool getRenderQuadStencilsFlag() const { return m_editorSettings.bDebugRenderQuadStencils; }
	void setRenderQuadStencilsFlag(bool flag);

	static const std::string k_renderBoxStencilsPropertyId;
	inline bool getRenderBoxStencilsFlag() const { return m_editorSettings.bDebugRenderBoxStencils; }
	void setRenderBoxStencilsFlag(bool flag);

	static const std::string k_renderModelStencilsPropertyId;
	inline bool getRenderModelStencilsFlag() const { return m_editorSettings.bDebugRenderModelStencils; }
	void setRenderModelStencilsFlag(bool flag);

	static const std::string k_renderQuadShapesPropertyId;
	inline bool getRenderQuadShapesFlag() const { return m_editorSettings.bDebugRenderQuadShapes; }
	void setRenderQuadShapesFlag(bool flag);

	static const std::string k_renderBoxShapesPropertyId;
	inline bool getRenderBoxShapesFlag() const { return m_editorSettings.bDebugRenderBoxShapes; }
	void setRenderBoxShapesFlag(bool flag);

	static const std::string k_renderModelShapesPropertyId;
	inline bool getRenderModelShapesFlag() const { return m_editorSettings.bDebugRenderModelShapes; }
	void setRenderModelShapesFlag(bool flag);

	static const std::string k_cameraSpeedPropertyId;
	float getCameraSpeed() const { return m_editorSettings.cameraSpeed; }
	void setCameraSpeed(float speed);

	static const std::string k_gridExtentPropertyId;
	float getGridExtent() const { return m_editorSettings.gridExtent; }
	void setGridExtent(float extent);

	static const std::string k_gridCellSizePropertyId;
	float getGridCellSize() const { return m_editorSettings.gridCellSize; }
	void setGridCellSize(float cellSize);

	static const std::string k_snapIncrementPropertyId;
	float getSnapIncrement() const { return m_editorSettings.snapIncrement; }
	void setSnapIncrement(float increment);

	static const std::string k_snapEnabledPropertyId;
	bool getSnapEnabled() const { return m_editorSettings.bSnapEnabled; }
	void setSnapEnabled(bool enabled);

	static const std::string k_rulerDisplayUnitsPropertyId;
	eRulerDisplayUnits getRulerDisplayUnits() const { return m_editorSettings.rulerDisplayUnits; }
	void setRulerDisplayUnits(eRulerDisplayUnits units);

	static const std::string k_debugCameraAlignmentPropertyId;
	bool getDebugCameraAlignment() const { return m_editorSettings.bDebugCameraAlignment; }
	void setDebugCameraAlignment(bool enabled);

	static const std::string k_modelStencilDisplayModePropertyId;
	eStencilDisplayMode getModelStencilDisplayMode() const { return m_editorSettings.modelStencilDisplayMode; }
	void setModelStencilDisplayMode(eStencilDisplayMode mode);

	static const std::string k_debugRenderInCompositorPropertyId;
	bool getDebugRenderInCompositor() const { return m_editorSettings.bDebugRenderInCompositor; }
	void setDebugRenderInCompositor(bool enabled);

	static const std::string k_renderFrameRatePropertyId;
	bool getRenderFrameRate() const { return m_editorSettings.bRenderFrameRate; }
	void setRenderFrameRate(bool enabled);

	static const std::string k_renderComponentNamesPropertyId;
	bool getRenderComponentNames() const { return m_editorSettings.bRenderComponentNames; }
	void setRenderComponentNames(bool enabled);

	// The camera state notifies under one name: the viewport writes it as a block
	// once the camera comes to rest, rather than property by property while it moves
	static const std::string k_editorCameraStatePropertyId;
	const EditorCameraState& getEditorCameraState() const { return m_editorSettings.cameraState; }
	void setEditorCameraState(const EditorCameraState& cameraState);

	static const std::string k_outlinerOpenStatePropertyId;
	bool getOutlinerNodeOpen(const std::string& nodeKey, bool bDefaultOpen) const;
	// Stores only a departure from the default, and notifies only when the
	// stored state actually changed
	void setOutlinerNodeOpen(const std::string& nodeKey, bool bOpen, bool bDefaultOpen);

	static const std::string k_outlinerTreeHeightPropertyId;
	float getOutlinerTreeHeight() const { return m_editorSettings.outlinerTreeHeight; }
	void setOutlinerTreeHeight(float height);

private:
	EditorSettings m_editorSettings;
};

class EditorObjectSystem : public MikanObjectSystem
{
public:
	EditorObjectSystem(ProjectManagerPtr ownerObjectSystem)
		: MikanObjectSystem(ownerObjectSystem)
	{
	}

	inline static const std::string k_objectSystemClassName= "EditorObjectSystem";
	virtual std::string getObjectSystemClassName() const { return k_objectSystemClassName; }

	virtual bool init(MikanObjectSystemDefinitionPtr definitionPtr) override;
	virtual void dispose() override;

	// Draws the transform gizmo. Call after the scene geometry: the gizmo's
	// occluded pass has to read through a finished depth buffer.
	void renderGizmo(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera);

	// Draws the orthographic ruler/measurement overlay (no-op unless a measurement
	// exists and the supplied camera is orthographic).
	void renderRuler(IMkGraphicsContext* graphicsContext, MikanViewportPtr targetViewport);

	EditorObjectSystemDefinitionConstPtr getEditorSystemConfigConst() const;
	EditorObjectSystemDefinitionPtr getEditorSystemConfig();
	const EditorSettings& getEditorSettings() const { return getEditorSystemConfigConst()->getEditorSettings(); }

	virtual MikanComponentPtr getComponentById(int componentId) const override;
	virtual bool getComponentList(const std::string& componentClassName,
								  std::vector<MikanComponentPtr>& outComponentList) const override;
	virtual bool getComponentIdList(const std::string& componentClassName,
									std::vector<int>& outComponentIdList) const override;

	void bindViewport(MikanViewportWeakPtr viewportWeakPtr);
	void unbindViewport(MikanViewportWeakPtr viewportWeakPtr);
	void clearViewports();

	void setObjectSystemSelectionFilter(const std::set<const MikanObjectSystem*>& objectSystemFilter);
	SelectionComponentPtr getSelection() const { return m_selectedComponentWeakPtr.lock(); }
	SelectionComponentPtr getSelectedSceneActor() const;
	void setSelection(SelectionComponentPtr newComponentPtr);
	MulticastDelegate<void()> OnSelectionChanged;
	// Delete or Backspace pressed while no ImGui window holds the keyboard. The
	// outliner owns the delete itself (confirmation and cascade rules), since its
	// selected row covers objects that have no selection component.
	MulticastDelegate<void()> OnDeleteSelectionRequested;

	inline MikanObjectPtr getGizmoObject() const { return m_gizmoObjectWeakPtr.lock(); }
	MikanViewportPtr getPrimaryViewport() const;
	MikanCameraPtr getPrimaryCamera() const;

	virtual void registerPropertyDescriptors(MikanPropertyDatabasePtr propertyDatabase) override;
	virtual void registerFunctionDescriptors(MikanFunctionDatabasePtr functionDatabase) override;

	// -- IEntityAccessor ----
	virtual rfk::Struct const* getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static const std::string k_selectedLanguagePropertyId;
	static const std::string k_availableLanguageListPropertyId;
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

protected:
	std::vector<MikanViewportWeakPtr> m_viewports;

	std::set<const MikanObjectSystem*> m_objectSystemSelectionFilter;
	ColliderRaycastHitResult m_lastestRaycastResult;
	SelectionComponentWeakPtr m_hoverComponentWeakPtr;
	ColliderComponentWeakPtr m_hoverColliderWeakPtr;
	SelectionComponentWeakPtr m_selectedComponentWeakPtr;

	MikanObjectWeakPtr m_gizmoObjectWeakPtr;
	GizmoTransformComponentWeakPtr m_gizmoComponentWeakPtr;

	// App Events
	void onAppStageEntered(class AppStage* oldAppStage, class AppStage* newAppStage);

	// Object System Events
	void onSceneActivated(SceneComponentPtr newScene);
	void onSceneDeactivated(SceneComponentPtr oldScene);
	void onActorDisposed(MikanObjectSystemPtr system, MikanComponentConstPtr component);

	// Keyboard Events
	void onDeletePressed();

	// Viewport Events
	void onMouseExited();
	void onMouseRayButtonDown(const glm::vec3& rayOrigin, const glm::vec3& rayDir, int button);
	void onMouseRayChanged(const glm::vec3& rayOrigin, const glm::vec3& rayDir);
	void onMouseRayButtonUp(const glm::vec3& rayOrigin, const glm::vec3& rayDir, int button);
	void onSelectionChanged(SelectionComponentPtr oldComponentPtr, SelectionComponentPtr newComponentPtr);

	// Helpers
	void createSceneTransformGizmo(SceneComponentPtr ownerScene);
	void disposeSceneTransformGizmo();
	void createGizmoBoxCollider(MikanObjectPtr gizmoObjectPtr, const std::string& name, const glm::vec3& center,
								const glm::vec3& halfExtents, const int priority);
	void createGizmoDiskCollider(MikanObjectPtr gizmoObjectPtr, const std::string& name, const glm::vec3& center,
								 const glm::vec3& normal, const float radius, const int priority);
	SelectionComponentPtr findClosestSelectionTarget(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
													 ColliderRaycastHitResult& outRaycastResult) const;
	void clearHoveredComponent();
	void clearSelectedComponent();

	// Ruler / measurement tool (orthographic only)
	glm::vec3 projectAndSnapMeasurePoint(const glm::vec3& worldPoint) const;
	// Effective snap state: the configured baseline, inverted while Shift is held.
	bool isRulerSnapActive(MikanViewportPtr targetViewport) const;

	bool m_hasMeasurement= false;
	bool m_isMeasuring= false;
	glm::vec3 m_measureStart= glm::vec3(0.f);
	glm::vec3 m_measureEnd= glm::vec3(0.f);
};
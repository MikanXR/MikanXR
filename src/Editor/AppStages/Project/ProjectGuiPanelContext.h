#pragma once

#include "ObjectSystemFwd.h"

// Forward declare all GuiPanel component types
class GuiPanel_DMXObjectSystem;
class GuiPanel_MarkerObjectSystem;
class GuiPanel_AnchorComponent;
class GuiPanel_ARKitVideoSourceComponent;
class GuiPanel_FileVideoSourceComponent;
class GuiPanel_BoxShapeComponent;
class GuiPanel_BoxStencilComponent;
class GuiPanel_CameraComponent;
class GuiPanel_ClientTextureSourceComponent;
class GuiPanel_CompositorComponent;
class GuiPanel_DMXFixtureGroupComponent;
class GuiPanel_DMXPresetComponent;
class GuiPanel_DMXSequenceComponent;
class GuiPanel_MarkerComponent;
class GuiPanel_LightEnvironmentComponent;
class GuiPanel_MarkerTrackingVolumeComponent;
class GuiPanel_ModelShapeComponent;
class GuiPanel_ModelStencilComponent;
class GuiPanel_NetworkVideoSourceComponent;
class GuiPanel_QuadShapeComponent;
class GuiPanel_QuadStencilComponent;
class GuiPanel_RGBPixelGridComponent;
class GuiPanel_RGBSpotLightComponent;
class GuiPanel_SceneComponent;
class GuiPanel_ScriptComponent;
class GuiPanel_CEFTextureSourceComponent;
class GuiPanel_SpoutTextureSourceComponent;
class GuiPanel_StageComponent;
class GuiPanel_TrackingMountComponent;
class GuiPanel_USBVideoSourceComponent;
class GuiPanel_VRTrackingVolumeComponent;

class ProjectGuiPanelContext
{
public:
	ProjectGuiPanelContext(class AppStage_Project* ownerAppStage);

	bool init();
	void update(float deltaSeconds);

	inline AppStage_Project* getOwnerAppStage() const { return m_ownerAppStage; }

	// System Panels
	inline GuiPanel_DMXObjectSystem* getDMXSystemPanel() const { return m_dmxSystemPanel; }
	inline GuiPanel_MarkerObjectSystem* getMarkerSystemPanel() const { return m_markerSystemPanel; }

	// Component Panels
	inline GuiPanel_AnchorComponent* getAnchorPanel() const { return m_anchorPanel; }
	inline GuiPanel_ARKitVideoSourceComponent* getARKitVideoSourcePanel() const { return m_arkitVideoSourcePanel; }
	inline GuiPanel_FileVideoSourceComponent* getFileVideoSourcePanel() const { return m_fileVideoSourcePanel; }
	inline GuiPanel_BoxShapeComponent* getBoxShapePanel() const { return m_boxShapePanel; }
	inline GuiPanel_BoxStencilComponent* getBoxStencilPanel() const { return m_boxStencilPanel; }
	inline GuiPanel_CameraComponent* getCameraPanel() const { return m_cameraPanel; }
	inline GuiPanel_ClientTextureSourceComponent* getClientTextureSourcePanel() const
	{
		return m_clientTextureSourcePanel;
	}
	inline GuiPanel_CompositorComponent* getCompositorPanel() const { return m_compositorPanel; }
	inline GuiPanel_DMXFixtureGroupComponent* getFixtureGroupPanel() const { return m_fixtureGroupPanel; }
	inline GuiPanel_DMXPresetComponent* getPresetPanel() const { return m_presetPanel; }
	inline GuiPanel_DMXSequenceComponent* getSequencePanel() const { return m_sequencePanel; }
	inline GuiPanel_LightEnvironmentComponent* getLightEnvironmentPanel() const { return m_lightEnvironmentPanel; }
	inline GuiPanel_MarkerComponent* getMarkerPanel() const { return m_markerPanel; }
	inline GuiPanel_MarkerTrackingVolumeComponent* getMarkerTrackingVolumePanel() const
	{
		return m_markerTrackingVolumePanel;
	}
	inline GuiPanel_ModelShapeComponent* getModelShapePanel() const { return m_modelShapePanel; }
	inline GuiPanel_ModelStencilComponent* getModelStencilPanel() const { return m_modelStencilPanel; }
	inline GuiPanel_NetworkVideoSourceComponent* getNetworkVideoSourcePanel() const
	{
		return m_networkVideoSourcePanel;
	}
	inline GuiPanel_QuadShapeComponent* getQuadShapePanel() const { return m_quadShapePanel; }
	inline GuiPanel_QuadStencilComponent* getQuadStencilPanel() const { return m_quadStencilPanel; }
	inline GuiPanel_RGBPixelGridComponent* getPixelGridPanel() const { return m_pixelGridPanel; }
	inline GuiPanel_RGBSpotLightComponent* getSpotLightPanel() const { return m_spotLightPanel; }
	inline GuiPanel_SceneComponent* getScenePanel() const { return m_scenePanel; }
	inline GuiPanel_ScriptComponent* getScriptPanel() const { return m_scriptPanel; }
	inline GuiPanel_CEFTextureSourceComponent* getCEFTextureSourcePanel() const { return m_cefTextureSourcePanel; }
	inline GuiPanel_SpoutTextureSourceComponent* getSpoutTextureSourcePanel() const
	{
		return m_spoutTextureSourcePanel;
	}
	inline GuiPanel_StageComponent* getStagePanel() const { return m_stagePanel; }
	inline GuiPanel_TrackingMountComponent* getTrackingMountPanel() const { return m_trackingMountPanel; }
	inline GuiPanel_USBVideoSourceComponent* getUSBVideoSourcePanel() const { return m_usbVideoSourcePanel; }
	inline GuiPanel_VRTrackingVolumeComponent* getVRTrackingVolumePanel() const { return m_vrTrackingVolumePanel; }

private:
	class AppStage_Project* m_ownerAppStage= nullptr;

	// System Panels
	GuiPanel_DMXObjectSystem* m_dmxSystemPanel= nullptr;
	GuiPanel_MarkerObjectSystem* m_markerSystemPanel= nullptr;

	// Component Panels
	GuiPanel_AnchorComponent* m_anchorPanel= nullptr;
	GuiPanel_ARKitVideoSourceComponent* m_arkitVideoSourcePanel= nullptr;
	GuiPanel_FileVideoSourceComponent* m_fileVideoSourcePanel= nullptr;
	GuiPanel_BoxShapeComponent* m_boxShapePanel= nullptr;
	GuiPanel_BoxStencilComponent* m_boxStencilPanel= nullptr;
	GuiPanel_CameraComponent* m_cameraPanel= nullptr;
	GuiPanel_ClientTextureSourceComponent* m_clientTextureSourcePanel= nullptr;
	GuiPanel_CompositorComponent* m_compositorPanel= nullptr;
	GuiPanel_DMXFixtureGroupComponent* m_fixtureGroupPanel= nullptr;
	GuiPanel_DMXPresetComponent* m_presetPanel= nullptr;
	GuiPanel_DMXSequenceComponent* m_sequencePanel= nullptr;
	GuiPanel_LightEnvironmentComponent* m_lightEnvironmentPanel= nullptr;
	GuiPanel_MarkerComponent* m_markerPanel= nullptr;
	GuiPanel_MarkerTrackingVolumeComponent* m_markerTrackingVolumePanel= nullptr;
	GuiPanel_ModelShapeComponent* m_modelShapePanel= nullptr;
	GuiPanel_ModelStencilComponent* m_modelStencilPanel= nullptr;
	GuiPanel_NetworkVideoSourceComponent* m_networkVideoSourcePanel= nullptr;
	GuiPanel_QuadShapeComponent* m_quadShapePanel= nullptr;
	GuiPanel_QuadStencilComponent* m_quadStencilPanel= nullptr;
	GuiPanel_RGBPixelGridComponent* m_pixelGridPanel= nullptr;
	GuiPanel_RGBSpotLightComponent* m_spotLightPanel= nullptr;
	GuiPanel_SceneComponent* m_scenePanel= nullptr;
	GuiPanel_ScriptComponent* m_scriptPanel= nullptr;
	GuiPanel_CEFTextureSourceComponent* m_cefTextureSourcePanel= nullptr;
	GuiPanel_SpoutTextureSourceComponent* m_spoutTextureSourcePanel= nullptr;
	GuiPanel_StageComponent* m_stagePanel= nullptr;
	GuiPanel_TrackingMountComponent* m_trackingMountPanel= nullptr;
	GuiPanel_USBVideoSourceComponent* m_usbVideoSourcePanel= nullptr;
	GuiPanel_VRTrackingVolumeComponent* m_vrTrackingVolumePanel= nullptr;
};

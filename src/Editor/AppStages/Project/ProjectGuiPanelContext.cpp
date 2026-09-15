#include "AppStage_Project.h"
#include "DMXObjectSystem.h"
#include "MarkerObjectSystem.h"
#include "ProjectGuiPanelContext.h"
#include "Shared/GuiPanel_AnchorComponent.h"
#include "Shared/GuiPanel_ARKitVideoSourceComponent.h"
#include "Shared/GuiPanel_FileVideoSourceComponent.h"
#include "Shared/GuiPanel_CameraComponent.h"
#include "Shared/GuiPanel_ClientTextureSourceComponent.h"
#include "Shared/GuiPanel_CompositorComponent.h"
#include "Shared/GuiPanel_DMXFixtureGroupComponent.h"
#include "Shared/GuiPanel_DMXObjectSystem.h"
#include "Shared/GuiPanel_DMXPresetComponent.h"
#include "Shared/GuiPanel_DMXSequenceComponent.h"
#include "Shared/GuiPanel_MarkerObjectSystem.h"
#include "Shared/GuiPanel_MarkerTrackingVolumeComponent.h"
#include "Shared/GuiPanel_LightEnvironmentComponent.h"
#include "Shared/GuiPanel_MarkerComponent.h"
#include "Shared/GuiPanel_NetworkVideoSourceComponent.h"
#include "Shared/GuiPanel_SceneComponent.h"
#include "Shared/GuiPanel_ScriptComponent.h"
#include "Shared/GuiPanel_CEFTextureSourceComponent.h"
#include "Shared/GuiPanel_SpoutTextureSourceComponent.h"
#include "Shared/GuiPanel_RGBPixelGridComponent.h"
#include "Shared/GuiPanel_RGBSpotLightComponent.h"
#include "Shared/GuiPanel_ShapeComponent.h"
#include "Shared/GuiPanel_StencilComponent.h"
#include "Shared/GuiPanel_StageComponent.h"
#include "Shared/GuiPanel_TrackingMountComponent.h"
#include "Shared/GuiPanel_USBVideoSourceComponent.h"
#include "Shared/GuiPanel_VRTrackingVolumeComponent.h"

ProjectGuiPanelContext::ProjectGuiPanelContext(AppStage_Project* ownerAppStage)
	: m_ownerAppStage(ownerAppStage)
{
}

bool ProjectGuiPanelContext::init()
{
	// System Panels
	m_dmxSystemPanel= m_ownerAppStage->addGuiPanel<GuiPanel_DMXObjectSystem>();
	m_dmxSystemPanel->init();
	m_dmxSystemPanel->setObjectSystem(m_ownerAppStage->getObjectSystemOfType<DMXObjectSystem>());

	m_markerSystemPanel= m_ownerAppStage->addGuiPanel<GuiPanel_MarkerObjectSystem>();
	m_markerSystemPanel->init();
	m_markerSystemPanel->setObjectSystem(m_ownerAppStage->getObjectSystemOfType<MarkerObjectSystem>());

	// Component Panels
	m_anchorPanel= m_ownerAppStage->addGuiPanel<GuiPanel_AnchorComponent>();
	m_anchorPanel->init();

	m_arkitVideoSourcePanel= m_ownerAppStage->addGuiPanel<GuiPanel_ARKitVideoSourceComponent>();
	m_arkitVideoSourcePanel->init();

	m_fileVideoSourcePanel= m_ownerAppStage->addGuiPanel<GuiPanel_FileVideoSourceComponent>();
	m_fileVideoSourcePanel->init();

	m_boxShapePanel= m_ownerAppStage->addGuiPanel<GuiPanel_BoxShapeComponent>();
	m_boxShapePanel->init();

	m_boxStencilPanel= m_ownerAppStage->addGuiPanel<GuiPanel_BoxStencilComponent>();
	m_boxStencilPanel->init();

	m_cameraPanel= m_ownerAppStage->addGuiPanel<GuiPanel_CameraComponent>();
	m_cameraPanel->init();

	m_clientTextureSourcePanel= m_ownerAppStage->addGuiPanel<GuiPanel_ClientTextureSourceComponent>();
	m_clientTextureSourcePanel->init();

	m_compositorPanel= m_ownerAppStage->addGuiPanel<GuiPanel_CompositorComponent>();
	m_compositorPanel->init();

	m_fixtureGroupPanel= m_ownerAppStage->addGuiPanel<GuiPanel_DMXFixtureGroupComponent>();
	m_fixtureGroupPanel->init();

	m_presetPanel= m_ownerAppStage->addGuiPanel<GuiPanel_DMXPresetComponent>();
	m_presetPanel->init();

	m_sequencePanel= m_ownerAppStage->addGuiPanel<GuiPanel_DMXSequenceComponent>();
	m_sequencePanel->init();

	m_markerPanel= m_ownerAppStage->addGuiPanel<GuiPanel_MarkerComponent>();
	m_markerPanel->init();

	m_markerTrackingVolumePanel= m_ownerAppStage->addGuiPanel<GuiPanel_MarkerTrackingVolumeComponent>();
	m_markerTrackingVolumePanel->init();

	m_modelShapePanel= m_ownerAppStage->addGuiPanel<GuiPanel_ModelShapeComponent>();
	m_modelShapePanel->init();

	m_lightEnvironmentPanel= m_ownerAppStage->addGuiPanel<GuiPanel_LightEnvironmentComponent>();
	m_lightEnvironmentPanel->init();

	m_modelStencilPanel= m_ownerAppStage->addGuiPanel<GuiPanel_ModelStencilComponent>();
	m_modelStencilPanel->init();

	m_networkVideoSourcePanel= m_ownerAppStage->addGuiPanel<GuiPanel_NetworkVideoSourceComponent>();
	m_networkVideoSourcePanel->init();

	m_pixelGridPanel= m_ownerAppStage->addGuiPanel<GuiPanel_RGBPixelGridComponent>();
	m_pixelGridPanel->init();

	m_quadShapePanel= m_ownerAppStage->addGuiPanel<GuiPanel_QuadShapeComponent>();
	m_quadShapePanel->init();

	m_quadStencilPanel= m_ownerAppStage->addGuiPanel<GuiPanel_QuadStencilComponent>();
	m_quadStencilPanel->init();

	m_spotLightPanel= m_ownerAppStage->addGuiPanel<GuiPanel_RGBSpotLightComponent>();
	m_spotLightPanel->init();

	m_scenePanel= m_ownerAppStage->addGuiPanel<GuiPanel_SceneComponent>();
	m_scenePanel->init();

	m_cefTextureSourcePanel= m_ownerAppStage->addGuiPanel<GuiPanel_CEFTextureSourceComponent>();
	m_cefTextureSourcePanel->init();

	m_spoutTextureSourcePanel= m_ownerAppStage->addGuiPanel<GuiPanel_SpoutTextureSourceComponent>();
	m_spoutTextureSourcePanel->init();

	m_stagePanel= m_ownerAppStage->addGuiPanel<GuiPanel_StageComponent>();
	m_stagePanel->init();

	m_trackingMountPanel= m_ownerAppStage->addGuiPanel<GuiPanel_TrackingMountComponent>();
	m_trackingMountPanel->init();

	m_scriptPanel= m_ownerAppStage->addGuiPanel<GuiPanel_ScriptComponent>();
	m_scriptPanel->init();

	m_usbVideoSourcePanel= m_ownerAppStage->addGuiPanel<GuiPanel_USBVideoSourceComponent>();
	m_usbVideoSourcePanel->init();

	m_vrTrackingVolumePanel= m_ownerAppStage->addGuiPanel<GuiPanel_VRTrackingVolumeComponent>();
	m_vrTrackingVolumePanel->init();

	return true;
}

void ProjectGuiPanelContext::update(float deltaSeconds) { m_spoutTextureSourcePanel->update(deltaSeconds); }
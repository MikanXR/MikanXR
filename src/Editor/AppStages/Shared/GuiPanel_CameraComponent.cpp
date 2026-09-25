#include "AppStage.h"
#include "Shared/GuiPanel_CameraComponent.h"
#include "LocText.h"
#include "Shared/PickerPropertyGui.h"
#include "MkGuiDrawUtils.h"
#include "ARKitVideoSourceComponent.h"
#include "ARKitVideoSourceSystem.h"
#include "FileVideoSourceComponent.h"
#include "FileVideoSourceSystem.h"
#include "NetworkVideoSourceComponent.h"
#include "NetworkVideoSourceSystem.h"
#include "TrackingMountComponent.h"
#include "TrackingMountObjectSystem.h"
#include "USBVideoSourceComponent.h"
#include "USBVideoSourceSystem.h"
#include "StringUtils.h"

#include <cmath>
#include <iterator>

GuiPanel_CameraComponent::GuiPanel_CameraComponent(AppStage* ownerAppStage)
	: GuiPanel_MikanComponent(ownerAppStage)
	, m_videoSourceDataSource(
		  ownerAppStage->getProjectManager(),
		  {{USBVideoSourceSystem::k_objectSystemClassName, USBVideoSourceComponent::k_componentClassName},
		   {NetworkVideoSourceSystem::k_objectSystemClassName, NetworkVideoSourceComponent::k_componentClassName},
		   {ARKitVideoSourceSystem::k_objectSystemClassName, ARKitVideoSourceComponent::k_componentClassName},
		   {FileVideoSourceSystem::k_objectSystemClassName, FileVideoSourceComponent::k_componentClassName}})
	, m_trackingMountDataSource(
		  ownerAppStage->getProjectManager(),
		  {{TrackingMountObjectSystem::k_objectSystemClassName, TrackingMountComponent::k_componentClassName}})
{
}

bool GuiPanel_CameraComponent::init() { return initTypedPropertyInterface<CameraComponent>(); }

void GuiPanel_CameraComponent::onConstruct()
{
	GuiPanel_MikanComponent::onConstruct();

	m_entityAccessor->setPropertyRenderer(
		CameraDefinition::k_videoSourceIdPropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			CameraComponentPtr cameraComp= getCameraComponent();
			if (!cameraComp)
				return false;

			m_videoSourceDataSource.refreshEntries();
			if (PickerPropertyGui::drawEmptyPlaceholder(m_defaultGuiStyle, m_videoSourceDataSource,
														"componentPanel.videoSource", "componentPanel.noVideoSources"))
				return true;

			const MikanVideoSourceID currentVideoSourceId= cameraComp->getCameraDefinition()->getVideoSourceId();
			int selectedIndex= m_videoSourceDataSource.getEntryIndexByComponentId(currentVideoSourceId);

			if (MkGui::drawComboBoxProperty(
					m_defaultGuiStyle,
					cameraComp->makePropertyUIIdentifier(CameraDefinition::k_videoSourceIdPropertyId),
					locText("componentPanel.videoSource"), &m_videoSourceDataSource, selectedIndex))
			{
				MikanComponentPtr newVideoSource= m_videoSourceDataSource.getEntryAtIndex(selectedIndex);
				if (newVideoSource)
				{
					addDeferredGuiEvent(
						[cameraComp, newVideoSource]()
						{ cameraComp->getCameraDefinition()->setVideoSourceId(newVideoSource->getComponentId()); });
				}
			}
			return true;
		});

	m_entityAccessor->setPropertyRenderer(
		CameraDefinition::k_trackingMountIdPropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			CameraComponentPtr cameraComp= getCameraComponent();
			if (!cameraComp)
				return false;

			m_trackingMountDataSource.refreshEntries();
			if (PickerPropertyGui::drawEmptyPlaceholder(m_defaultGuiStyle, m_trackingMountDataSource,
														"componentPanel.trackingMount",
														"componentPanel.noTrackingMounts"))
				return true;

			const MikanTrackingMountID currentMountId= cameraComp->getCameraDefinition()->getTrackingMountId();
			int selectedIndex= m_trackingMountDataSource.getEntryIndexByComponentId(currentMountId);

			if (MkGui::drawComboBoxProperty(
					m_defaultGuiStyle,
					cameraComp->makePropertyUIIdentifier(CameraDefinition::k_trackingMountIdPropertyId),
					locText("componentPanel.trackingMount"), &m_trackingMountDataSource, selectedIndex))
			{
				MikanComponentPtr newMount= m_trackingMountDataSource.getEntryAtIndex(selectedIndex);
				if (newMount)
				{
					addDeferredGuiEvent(
						[cameraComp, newMount]()
						{ cameraComp->getCameraDefinition()->setTrackingMountId(newMount->getComponentId()); });
				}
			}
			return true;
		});

	m_entityAccessor->setPropertyRenderer(
		CameraDefinition::k_clientColorRenderScalePropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			CameraComponentPtr cameraComp= getCameraComponent();
			if (!cameraComp)
				return false;

			return drawClientRenderScaleProperty(
				CameraDefinition::k_clientColorRenderScalePropertyId, "properties.client_color_render_scale",
				cameraComp->getCameraDefinition()->getClientColorRenderScale(), [](CameraComponentPtr comp, float scale)
				{ comp->getCameraDefinition()->setClientColorRenderScale(scale); });
		});

	m_entityAccessor->setPropertyRenderer(
		CameraDefinition::k_clientAuxRenderScalePropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			CameraComponentPtr cameraComp= getCameraComponent();
			if (!cameraComp)
				return false;

			return drawClientRenderScaleProperty(
				CameraDefinition::k_clientAuxRenderScalePropertyId, "properties.client_aux_render_scale",
				cameraComp->getCameraDefinition()->getClientAuxRenderScale(), [](CameraComponentPtr comp, float scale)
				{ comp->getCameraDefinition()->setClientAuxRenderScale(scale); });
		});

	// client_max_buffer_dimension needs no renderer of its own: it carries
	// EnumPropertyMetaData, so the generic widget already draws it as a combo.
}

void GuiPanel_CameraComponent::drawClientRenderSizeRow(CameraComponentPtr cameraComp, const std::string& propertyId,
													   float scale)
{
	int videoWidth= 0, videoHeight= 0;
	if (!cameraComp->getAperturePixelDimensions(videoWidth, videoHeight) || videoWidth <= 0 || videoHeight <= 0)
	{
		// Nothing to multiply yet, so report that rather than a size derived from nowhere
		MkGui::drawStaticTextProperty(m_defaultGuiStyle, locText("componentPanel.scaledSize"), "-");
		return;
	}

	int outWidth= 0, outHeight= 0;
	const bool bWasClamped= CameraDefinition::computeClientRenderSize(
		videoWidth, videoHeight, scale, cameraComp->getCameraDefinition()->getClientMaxBufferDimensionPixels(),
		outWidth, outHeight);

	MkGui::drawStaticTextProperty(m_defaultGuiStyle,
								  locText(bWasClamped ? "componentPanel.clampedSize" : "componentPanel.scaledSize"),
								  StringUtils::stringify(outWidth, "x", outHeight));
}

bool GuiPanel_CameraComponent::drawClientRenderScaleProperty(
	const std::string& propertyId, const char* labelKey, float currentScale,
	const std::function<void(CameraComponentPtr, float)>& applyScale)
{
	CameraComponentPtr cameraComp= getCameraComponent();
	if (!cameraComp)
		return false;

	float scale= currentScale;
	if (MkGui::drawFloatSliderProperty(
			m_defaultGuiStyle, cameraComp->makePropertyUIIdentifier(propertyId), locText(labelKey), scale,
			CameraDefinition::k_minClientRenderScale, CameraDefinition::k_maxClientRenderScale,
			CameraDefinition::k_minClientRenderScale, CameraDefinition::k_maxClientRenderScale, "%.2f"))
	{
		// The step is a slider affordance rather than a property rule, so the quantizing
		// happens here and not in the setter a client or a drive script goes through
		const float step= CameraDefinition::k_clientRenderScaleStep;
		const float quantizedScale= std::round(scale / step) * step;

		addDeferredGuiEvent([cameraComp, applyScale, quantizedScale]() { applyScale(cameraComp, quantizedScale); });
	}

	drawClientRenderSizeRow(cameraComp, propertyId, currentScale);

	return true;
}

CameraComponentPtr GuiPanel_CameraComponent::getCameraComponent() const
{
	MikanComponentPtr component= m_component.lock();
	if (component)
	{
		return std::static_pointer_cast<CameraComponent>(component);
	}
	return nullptr;
}

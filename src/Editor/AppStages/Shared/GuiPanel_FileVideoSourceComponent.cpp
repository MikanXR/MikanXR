#include "AppStage.h"
#include "Shared/GuiPanel_FileVideoSourceComponent.h"
#include "FileVideoSourceComponent.h"
#include "LocText.h"
#include "MkGuiDrawUtils.h"

#include "imgui.h"

bool GuiPanel_FileVideoSourceComponent::init() { return initTypedPropertyInterface<FileVideoSourceComponent>(); }

void GuiPanel_FileVideoSourceComponent::onConstruct()
{
	GuiPanel_MikanComponent::onConstruct();

	// Playback status as one line: state, which media is showing, and the
	// scrub slider below it. A still has no timeline, so the slider is left
	// out rather than drawn locked at zero.
	m_entityAccessor->setPropertyRenderer(
		FileVideoSourceComponent::k_playbackStatePropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			FileVideoSourceComponentPtr videoSourceComp= getFileVideoSourceComponent();
			if (!videoSourceComp)
				return false;

			const char* stateText= "";
			switch (videoSourceComp->getPlaybackState())
			{
			case eFilePlaybackState::stopped:
				stateText= locText("propertyValues.playback_stopped");
				break;
			case eFilePlaybackState::playing:
				stateText= locText("propertyValues.playback_playing");
				break;
			case eFilePlaybackState::paused:
				stateText= locText("propertyValues.playback_paused");
				break;
			}
			ImGui::Text(locText("componentPanel.filePlaybackStatusFmt"), stateText, videoSourceComp->getPlaybackTime(),
						videoSourceComp->getDurationSeconds());

			if (videoSourceComp->isMarkerReferenceActive())
			{
				ImGui::TextUnformatted(locText("componentPanel.fileMarkerReferenceActive"));
			}

			if (!videoSourceComp->isStill() && videoSourceComp->getDurationSeconds() > 0.f)
			{
				const float duration= videoSourceComp->getDurationSeconds();
				float playbackTime= videoSourceComp->getPlaybackTime();
				if (MkGui::drawFloatSliderProperty(
						m_defaultGuiStyle,
						videoSourceComp->makePropertyUIIdentifier(FileVideoSourceComponent::k_playbackTimePropertyId),
						locText("properties.playback_time"), playbackTime, 0.f, duration, 0.f, duration))
				{
					addDeferredGuiEvent([videoSourceComp, playbackTime]() { videoSourceComp->seekTo(playbackTime); });
				}
			}

			return true;
		});
}

FileVideoSourceComponentPtr GuiPanel_FileVideoSourceComponent::getFileVideoSourceComponent() const
{
	MikanComponentPtr component= m_component.lock();
	if (component)
	{
		return std::static_pointer_cast<FileVideoSourceComponent>(component);
	}
	return nullptr;
}

void GuiPanel_FileVideoSourceComponent::drawCompactGui()
{
	GuiPanel_EntityAccessorPtr entityAccessor= getPropertyInterface();

	static const std::set<std::string> compactProperties= {
		MikanComponentDefinition::k_componentNamePropertyId,
		FileVideoSourceDefinition::k_mediaPathPropertyId,
		FileVideoSourceComponent::k_playbackStatePropertyId,
	};
	static const std::set<std::string> compactFunctions= {
		FileVideoSourceComponent::k_playFunctionId,
		FileVideoSourceComponent::k_pauseFunctionId,
		FileVideoSourceComponent::k_stopFunctionId,
		FileVideoSourceComponent::k_showVideoSourceSettingsFunctionId,
	};
	entityAccessor->drawPropertiesGui(compactProperties);
	entityAccessor->drawFunctionsGui(compactFunctions);
}

#include "GuiPanel_CompositorList.h"
#include "AppStage.h"
#include "CompositorComponent.h"
#include "IconsForkAwesome.h"
#include "LocText.h"
#include "MikanCoreTypes.h"
#include "MkGuiScopedStyle.h"
#include "MkGuiStyleManager.h"
#include "Project/GuiPanel_ProjectOutliner.h"
#include "Project/ProjectGuiPanelContext.h"
#include "SceneComponent.h"
#include "SceneObjectSystem.h"

#include "imgui.h"

bool GuiPanel_CompositorList::init(ProjectGuiPanelContext* context, GuiPanel_ProjectOutliner* outliner)
{
	m_context= context;
	m_outliner= outliner;
	m_sceneSystem= m_ownerAppStage->getObjectSystemOfType<SceneObjectSystem>();
	m_activeRowStyle= getGuiStyleManager()->getStyle("project_outliner");

	return true;
}

void GuiPanel_CompositorList::onGui()
{
	SceneObjectSystemPtr sceneSystem= m_sceneSystem.lock();
	SceneComponentPtr currentScene= sceneSystem ? sceneSystem->getCurrentScene() : nullptr;
	if (!currentScene)
	{
		ImGui::TextDisabled("%s", locText("project.compositorListNoScene"));
		return;
	}

	const std::vector<CompositorComponentPtr> compositors= currentScene->getOutputCompositors();
	if (compositors.empty())
	{
		ImGui::TextDisabled("%s", locText("project.compositorListEmpty"));
		return;
	}

	const MikanSceneID sceneId= currentScene->getSceneId();
	const MikanCompositorID displayCompositorId= currentScene->getSceneComponentDefinition()->getDisplayCompositorId();

	for (const CompositorComponentPtr& compositor : compositors)
	{
		const MikanCompositorID compositorId= compositor->getComponentId();
		const bool bIsActive= compositorId == displayCompositorId;
		const std::string label=
			std::string(ICON_FK_CLONE) + " " + compositor->getName() + "##compositor" + std::to_string(compositorId);

		bool bClicked= false;
		if (bIsActive)
		{
			MkGuiScopedStyle activeStyle(m_activeRowStyle);
			bClicked= ImGui::Selectable(label.c_str(), true);
		}
		else
		{
			bClicked= ImGui::Selectable(label.c_str(), false);
		}

		if (bClicked)
		{
			// Deferred: the property change swaps the active compositor set out from
			// under this draw. Re-resolve the scene in case it changed since the click.
			addDeferredGuiEvent(
				[this, sceneId, compositorId]()
				{
					SceneObjectSystemPtr system= m_sceneSystem.lock();
					SceneComponentPtr scene= system ? system->getSceneById(sceneId) : nullptr;
					if (scene)
						scene->getSceneComponentDefinition()->setDisplayCompositorId(compositorId);
					if (m_outliner)
						m_outliner->selectComponent(compositorId);
				});
		}
	}
}

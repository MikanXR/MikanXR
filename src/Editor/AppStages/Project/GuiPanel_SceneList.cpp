#include "GuiPanel_SceneList.h"
#include "AppStage.h"
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

bool GuiPanel_SceneList::init(ProjectGuiPanelContext* context, GuiPanel_ProjectOutliner* outliner)
{
	m_context= context;
	m_outliner= outliner;
	m_sceneSystem= m_ownerAppStage->getObjectSystemOfType<SceneObjectSystem>();
	m_activeRowStyle= getGuiStyleManager()->getStyle("project_outliner");

	return true;
}

void GuiPanel_SceneList::onGui()
{
	SceneObjectSystemPtr sceneSystem= m_sceneSystem.lock();
	if (!sceneSystem)
		return;

	const MikanSceneID currentSceneId= sceneSystem->getCurrentSceneId();
	bool bAnyScene= false;

	sceneSystem->visitComponents(
		[&](SceneComponentPtr scene)
		{
			bAnyScene= true;

			const MikanSceneID sceneId= scene->getSceneId();
			const bool bIsActive= sceneId == currentSceneId;
			const std::string label=
				std::string(ICON_FK_SITEMAP) + " " + scene->getName() + "##scene" + std::to_string(sceneId);

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
				// Deferred: activating a scene swaps the compositor set the panels
				// are drawing from
				addDeferredGuiEvent(
					[this, sceneId]()
					{
						if (SceneObjectSystemPtr system= m_sceneSystem.lock())
							system->setCurrentSceneById(sceneId);
						if (m_outliner)
							m_outliner->selectComponent(sceneId);
					});
			}
		});

	if (!bAnyScene)
	{
		ImGui::TextDisabled("%s", locText("project.sceneListEmpty"));
	}
}

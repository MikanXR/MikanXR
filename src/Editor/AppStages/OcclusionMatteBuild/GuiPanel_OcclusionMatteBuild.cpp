#include "OcclusionMatteBuild/GuiPanel_OcclusionMatteBuild.h"

#include "imgui.h"
#include "LocText.h"
#include "MkGuiDrawUtils.h"

#include <algorithm>
#include <cfloat>
#include <string>

void GuiPanel_OcclusionMatteBuild::onGui()
{
	switch (m_menuState)
	{
	case eOcclusionMatteBuildMenuState::verifyTake:
	{
		ImGui::TextWrapped("%s", locFormat("occlusionMatteBuild.takeSummaryFmt", m_takeFrameCount, m_takeImageWidth,
										   m_takeImageHeight, m_takeDurationSeconds, m_takeStencilCount)
									 .c_str());
		ImGui::Spacing();

		int modelIndex= (int)m_model;
		const char* modelLabels[(int)eOcclusionMatteModel::COUNT]= {locText("occlusionMatteBuild.modelSmall"),
																	locText("occlusionMatteBuild.modelTiny")};
		ImGui::SetNextItemWidth(180.f * MkGui::getUiScale());
		if (ImGui::Combo(locLabel("occlusionMatteBuild.model"), &modelIndex, modelLabels,
						 (int)eOcclusionMatteModel::COUNT))
		{
			m_model= (eOcclusionMatteModel)modelIndex;
		}

		ImGui::SetNextItemWidth(180.f * MkGui::getUiScale());
		if (ImGui::InputInt(locLabel("occlusionMatteBuild.bandPx"), &m_bandPx))
		{
			m_bandPx= std::max(m_bandPx, 1);
		}
		ImGui::TextWrapped("%s", locText("occlusionMatteBuild.bandPxHelp"));

		int scaleIndex= m_matteScale == 2 ? 1 : 0;
		const char* scaleLabels[2]= {locText("occlusionMatteBuild.scaleFull"),
									 locText("occlusionMatteBuild.scaleHalf")};
		ImGui::SetNextItemWidth(180.f * MkGui::getUiScale());
		if (ImGui::Combo(locLabel("occlusionMatteBuild.matteScale"), &scaleIndex, scaleLabels, 2))
		{
			m_matteScale= scaleIndex == 1 ? 2 : 1;
		}

		ImGui::SetNextItemWidth(180.f * MkGui::getUiScale());
		if (ImGui::InputInt(locLabel("occlusionMatteBuild.frameStep"), &m_frameStep))
		{
			m_frameStep= std::max(m_frameStep, 1);
		}
		ImGui::TextWrapped("%s", locText("occlusionMatteBuild.frameStepHelp"));

		ImGui::Checkbox(locLabel("occlusionMatteBuild.cropSmallRuns"), &m_bCropSmallRuns);
		ImGui::TextWrapped("%s", locText("occlusionMatteBuild.cropSmallRunsHelp"));

		ImGui::Spacing();
		if (ImGui::Button(locLabel("occlusionMatteBuild.build")))
		{
			if (OnBuildEvent)
				OnBuildEvent();
		}
		ImGui::SameLine();
		if (ImGui::Button(locLabel("common.cancel")))
		{
			if (OnCancelEvent)
				OnCancelEvent();
		}
	}
	break;

	case eOcclusionMatteBuildMenuState::building:
	{
		std::string phaseLabel= locText("occlusionMatteBuild.working");
		float fraction= 0.f;
		if (m_buildPhase == eOcclusionMatteBuildPhase::decoding && m_frameCount > 0)
		{
			phaseLabel= locFormat("occlusionMatteBuild.phaseSegmentingFmt", m_framesDone, m_frameCount);
			fraction= (float)m_framesDone / (float)m_frameCount;
		}
		else if (m_buildPhase == eOcclusionMatteBuildPhase::complete)
		{
			phaseLabel= locText("occlusionMatteBuild.phaseFinishing");
			fraction= 1.f;
		}

		if (m_bCancellingBuild)
			ImGui::TextWrapped("%s", locText("occlusionMatteBuild.cancelling"));
		else
			ImGui::TextWrapped("%s", phaseLabel.c_str());

		ImGui::ProgressBar(fraction, ImVec2(-FLT_MIN, 0.f));
		ImGui::TextWrapped("%s", locFormat("occlusionMatteBuild.elapsedSecondsFmt", m_buildElapsedSeconds).c_str());

		ImGui::Spacing();
		ImGui::BeginDisabled(m_bCancellingBuild);
		if (ImGui::Button(locLabel("occlusionMatteBuild.cancelBuild")))
		{
			if (OnCancelBuildEvent)
				OnCancelBuildEvent();
		}
		ImGui::EndDisabled();

		if (m_bHasPreview)
		{
			ImGui::Spacing();
			ImGui::TextWrapped("%s", locText("occlusionMatteBuild.previewLegend"));
		}
	}
	break;

	case eOcclusionMatteBuildMenuState::failed:
	{
		ImGui::TextWrapped("%s", locText("occlusionMatteBuild.buildFailed"));
		ImGui::Spacing();
		if (!m_failureReason.empty())
			ImGui::TextWrapped("%s", m_failureReason.c_str());
		ImGui::Spacing();
		if (ImGui::Button(locLabel("occlusionMatteBuild.retry")))
		{
			if (OnRedoEvent)
				OnRedoEvent();
		}
		ImGui::SameLine();
		if (ImGui::Button(locLabel("common.cancel")))
		{
			if (OnCancelEvent)
				OnCancelEvent();
		}
	}
	break;

	case eOcclusionMatteBuildMenuState::buildComplete:
	{
		const float meanRuns=
			m_stats.framesSegmented > 0 ? (float)m_stats.runsTotal / (float)m_stats.framesSegmented : 0.f;
		const float segmentSecondsPerFrame=
			m_stats.framesSegmented > 0 ? m_stats.segmentSeconds / (float)m_stats.framesSegmented : 0.f;

		ImGui::TextWrapped("%s", locFormat("occlusionMatteBuild.matteSummaryFmt", m_recordCount, m_stats.matteWidth,
										   m_stats.matteHeight)
									 .c_str());
		ImGui::TextWrapped("%s", locFormat("occlusionMatteBuild.framesSummaryFmt", m_stats.framesSegmented,
										   m_stats.framesHeld, meanRuns)
									 .c_str());
		if (m_stats.framesUnmatched > 0)
		{
			ImGui::TextWrapped("%s",
							   locFormat("occlusionMatteBuild.framesUnmatchedFmt", m_stats.framesUnmatched).c_str());
		}
		ImGui::TextWrapped(
			"%s",
			locFormat("occlusionMatteBuild.timeSummaryFmt", m_stats.totalSeconds, segmentSecondsPerFrame).c_str());
		ImGui::TextWrapped("%s", locFormat("occlusionMatteBuild.sidecarFmt", m_sidecarPath.c_str()).c_str());
		ImGui::Spacing();
		ImGui::TextWrapped("%s", locText("occlusionMatteBuild.completeNote"));
		if (m_bHasPreview)
			ImGui::TextWrapped("%s", locText("occlusionMatteBuild.previewLegend"));
		ImGui::Spacing();

		if (ImGui::Button(locLabel("common.ok")))
		{
			if (OnOkEvent)
				OnOkEvent();
		}
		ImGui::SameLine();
		if (ImGui::Button(locLabel("occlusionMatteBuild.redo")))
		{
			if (OnRedoEvent)
				OnRedoEvent();
		}
	}
	break;

	default:
		break;
	}
}

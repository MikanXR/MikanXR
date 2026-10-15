#include "OcclusionMeshBuild/GuiPanel_OcclusionMeshBuild.h"

#include "imgui.h"
#include "LocText.h"
#include "MkGuiDrawUtils.h"

#include <algorithm>
#include <cfloat>

// Fraction of the bar at the start of the steps around integration, which
// is the only step with a real count to show
static const float k_occlusionMeshBuildPrefixFraction= 0.05f;
static const float k_occlusionMeshBuildIntegrateFraction= 0.85f;

void GuiPanel_OcclusionMeshBuild::onGui()
{
	switch (m_menuState)
	{
	case eOcclusionMeshBuildMenuState::verifyTake:
	{
		ImGui::TextWrapped("%s", locFormat("occlusionMeshBuild.takeSummaryFmt", m_takeFrameCount, m_takeDepthWidth,
										   m_takeDepthHeight, m_takeDurationSeconds)
									 .c_str());
		ImGui::Spacing();

		int presetIndex= (int)m_voxelPreset;
		const char* presetLabels[(int)eOcclusionMeshVoxelPreset::COUNT]= {locText("occlusionMeshBuild.presetRoom"),
																		  locText("occlusionMeshBuild.presetTabletop")};
		ImGui::SetNextItemWidth(180.f * MkGui::getUiScale());
		if (ImGui::Combo(locLabel("occlusionMeshBuild.voxelPreset"), &presetIndex, presetLabels,
						 (int)eOcclusionMeshVoxelPreset::COUNT))
		{
			m_voxelPreset= (eOcclusionMeshVoxelPreset)presetIndex;
		}

		ImGui::SetNextItemWidth(180.f * MkGui::getUiScale());
		if (ImGui::InputInt(locLabel("occlusionMeshBuild.keyframeStride"), &m_keyframeStride))
		{
			m_keyframeStride= std::max(m_keyframeStride, 1);
		}

		ImGui::SetNextItemWidth(180.f * MkGui::getUiScale());
		if (ImGui::InputFloat(locLabel("occlusionMeshBuild.pathRadius"), &m_pathRadius, 0.5f, 1.f, "%.1f m"))
		{
			m_pathRadius= std::max(m_pathRadius, 0.f);
		}
		ImGui::TextWrapped("%s", locText("occlusionMeshBuild.pathRadiusHelp"));

		ImGui::Checkbox(locLabel("occlusionMeshBuild.frameToModel"), &m_bFrameToModel);
		ImGui::TextWrapped("%s", locText("occlusionMeshBuild.frameToModelHelp"));
		ImGui::BeginDisabled(!m_bFrameToModel);
		ImGui::Checkbox(locLabel("occlusionMeshBuild.replacePoseTrack"), &m_bReplacePoseTrack);
		ImGui::EndDisabled();
		ImGui::TextWrapped("%s", locText("occlusionMeshBuild.replacePoseTrackHelp"));

		ImGui::Spacing();
		if (ImGui::Button(locLabel("occlusionMeshBuild.build")))
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

	case eOcclusionMeshBuildMenuState::building:
	{
		std::string phaseLabel= locText("occlusionMeshBuild.working");
		float fraction= k_occlusionMeshBuildPrefixFraction;
		switch (m_buildPhase)
		{
		case eOcclusionMeshBuildPhase::loadingTake:
			phaseLabel= locText("occlusionMeshBuild.phaseLoadingTake");
			break;
		case eOcclusionMeshBuildPhase::computingBounds:
			phaseLabel= locText("occlusionMeshBuild.phaseComputingBounds");
			break;
		case eOcclusionMeshBuildPhase::integrating:
			phaseLabel= locFormat("occlusionMeshBuild.phaseIntegratingFmt", m_framesDone, m_frameCount);
			if (m_frameCount > 0)
			{
				fraction= k_occlusionMeshBuildPrefixFraction
						  + (k_occlusionMeshBuildIntegrateFraction - k_occlusionMeshBuildPrefixFraction)
								* ((float)m_framesDone / (float)m_frameCount);
			}
			break;
		case eOcclusionMeshBuildPhase::extracting:
			phaseLabel= locText("occlusionMeshBuild.phaseExtracting");
			fraction= k_occlusionMeshBuildIntegrateFraction;
			break;
		case eOcclusionMeshBuildPhase::cleaning:
			phaseLabel= locText("occlusionMeshBuild.phaseCleaning");
			fraction= 0.95f;
			break;
		default:
			break;
		}

		if (m_bCancellingBuild)
			ImGui::TextWrapped("%s", locText("occlusionMeshBuild.cancelling"));
		else
			ImGui::TextWrapped("%s", phaseLabel.c_str());

		ImGui::ProgressBar(fraction, ImVec2(-FLT_MIN, 0.f));
		ImGui::TextWrapped("%s", locFormat("occlusionMeshBuild.elapsedSecondsFmt", m_buildElapsedSeconds).c_str());

		ImGui::Spacing();
		ImGui::BeginDisabled(m_bCancellingBuild);
		if (ImGui::Button(locLabel("occlusionMeshBuild.cancelBuild")))
		{
			if (OnCancelBuildEvent)
				OnCancelBuildEvent();
		}
		ImGui::EndDisabled();
	}
	break;

	case eOcclusionMeshBuildMenuState::failed:
	{
		ImGui::TextWrapped("%s", locText("occlusionMeshBuild.buildFailed"));
		ImGui::Spacing();
		if (!m_failureReason.empty())
			ImGui::TextWrapped("%s", m_failureReason.c_str());
		ImGui::Spacing();
		if (ImGui::Button(locLabel("occlusionMeshBuild.retry")))
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

	case eOcclusionMeshBuildMenuState::verifyMesh:
	{
		ImGui::TextWrapped("%s", locFormat("occlusionMeshBuild.meshSummaryFmt", m_stats.trianglesClean,
										   m_stats.trianglesRemovedAsIslands, m_stats.gridDims.x, m_stats.gridDims.y,
										   m_stats.gridDims.z, m_stats.voxelSize * 100.f)
									 .c_str());
		ImGui::TextWrapped("%s", locFormat("occlusionMeshBuild.framesFusedFmt", m_stats.framesIntegrated,
										   m_stats.keyframes, m_stats.totalSeconds)
									 .c_str());
		if (m_bRegistrationRan)
		{
			ImGui::TextWrapped("%s",
							   locFormat("occlusionMeshBuild.registrationSummaryFmt", m_stats.registrationsAccepted,
										 m_stats.registrationsRejected, m_stats.meanStepTranslationMm,
										 m_stats.meanStepRotationDeg, m_stats.meanRmsBeforeMm, m_stats.meanRmsAfterMm,
										 m_stats.finalCorrectionMm, m_stats.finalCorrectionDeg)
								   .c_str());
		}
		else
		{
			ImGui::TextWrapped("%s", locText("occlusionMeshBuild.registrationOff"));
		}
		if (m_stats.voxelsClipped > 0)
		{
			ImGui::TextWrapped(
				"%s", locFormat("occlusionMeshBuild.trimSummaryFmt", (double)m_stats.voxelsClipped * 1e-6).c_str());
		}

		ImGui::Spacing();
		ImGui::TextWrapped("%s", locText("occlusionMeshBuild.previewNote"));
		ImGui::Spacing();

		if (ImGui::Button(locLabel("occlusionMeshBuild.createStencil")))
		{
			if (OnApplyEvent)
				OnApplyEvent();
		}
		ImGui::SameLine();
		if (ImGui::Button(locLabel("occlusionMeshBuild.redo")))
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

	case eOcclusionMeshBuildMenuState::buildComplete:
	{
		ImGui::TextWrapped("%s",
						   locFormat("occlusionMeshBuild.stencilCreatedFmt", m_createdStencilName.c_str()).c_str());
		if (!m_refinedTrackPath.empty())
		{
			ImGui::TextWrapped(
				"%s", locFormat("occlusionMeshBuild.refinedTrackWrittenFmt", m_refinedTrackPath.c_str()).c_str());
			if (m_bReplacedSourceTrack)
				ImGui::TextWrapped("%s", locText("occlusionMeshBuild.poseTrackReplacedNote"));
		}
		ImGui::Spacing();
		if (ImGui::Button(locLabel("common.ok")))
		{
			if (OnOkEvent)
				OnOkEvent();
		}
	}
	break;

	default:
		break;
	}
}

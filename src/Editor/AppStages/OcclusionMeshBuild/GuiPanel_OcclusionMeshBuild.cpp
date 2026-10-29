#include "OcclusionMeshBuild/GuiPanel_OcclusionMeshBuild.h"

#include "OcclusionMeshBuild/OcclusionMeshPreview.h"

#include "imgui.h"
#include "LocText.h"
#include "MkGuiDrawUtils.h"

#include <algorithm>
#include <cfloat>
#include <string>

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

		ImGui::Checkbox(locLabel("occlusionMeshBuild.texture"), &m_bTexture);
		ImGui::TextWrapped("%s", locText("occlusionMeshBuild.textureHelp"));

		if (m_bHasBoundsBox)
		{
			const glm::vec3 extent= m_boundsBoxHi - m_boundsBoxLo;
			ImGui::Spacing();
			ImGui::TextWrapped("%s",
							   locFormat("occlusionMeshBuild.boundsBoxFmt", extent.x, extent.y, extent.z).c_str());
			if (ImGui::Button(locLabel("occlusionMeshBuild.useAutomaticBounds")))
				m_bHasBoundsBox= false;
		}

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
			fraction= 0.92f;
			break;
		case eOcclusionMeshBuildPhase::coverage:
			phaseLabel= locFormat("occlusionMeshBuild.phaseCoverageFmt", m_framesDone, m_frameCount);
			fraction= 0.93f;
			break;
		case eOcclusionMeshBuildPhase::texturing:
			phaseLabel= locFormat("occlusionMeshBuild.phaseTexturingFmt", m_framesDone, m_frameCount);
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
		if (m_stats.bVoxelGrown)
		{
			ImGui::TextWrapped("%s", locFormat("occlusionMeshBuild.voxelGrownFmt", m_stats.configuredVoxelSize * 100.f,
											   m_stats.voxelSize * 100.f)
										 .c_str());
		}
		ImGui::TextWrapped("%s", locFormat("occlusionMeshBuild.coverageSummaryFmt", m_stats.coverageGood * 100.f,
										   m_stats.coverageWeak * 100.f, m_stats.coverageUnseen * 100.f)
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
		if (m_stats.textureViews > 0)
		{
			ImGui::TextWrapped("%s", locFormat("occlusionMeshBuild.textureSummaryFmt", m_stats.textureViewsUsed,
											   m_stats.textureViews, m_stats.atlasWidth, m_stats.atlasHeight,
											   m_stats.trianglesUntextured)
										 .c_str());
		}
		else
		{
			ImGui::TextWrapped("%s", locText("occlusionMeshBuild.textureOff"));
		}

		ImGui::Spacing();
		ImGui::TextWrapped("%s", locText("occlusionMeshBuild.previewNote"));
		ImGui::Spacing();

		// Only the views the built mesh can show are offered
		const char* viewKeys[(int)eOcclusionMeshPreviewView::COUNT]= {
			"occlusionMeshBuild.viewTextured",      "occlusionMeshBuild.viewShaded",
			"occlusionMeshBuild.viewCoverage",      "occlusionMeshBuild.viewObservations",
			"occlusionMeshBuild.viewDirections",    "occlusionMeshBuild.viewAngle",
			"occlusionMeshBuild.viewContradiction", "occlusionMeshBuild.viewDistance"};
		ImGui::SetNextItemWidth(180.f * MkGui::getUiScale());
		if (ImGui::BeginCombo(locLabel("occlusionMeshBuild.previewView"), locText(viewKeys[(int)m_previewView])))
		{
			for (int viewIndex= 0; viewIndex < (int)eOcclusionMeshPreviewView::COUNT; ++viewIndex)
			{
				if ((m_availablePreviewViews & (1u << viewIndex)) == 0)
					continue;
				const bool bSelected= viewIndex == (int)m_previewView;
				if (ImGui::Selectable(locText(viewKeys[viewIndex]), bSelected) && !bSelected)
				{
					m_previewView= (eOcclusionMeshPreviewView)viewIndex;
					if (OnPreviewViewChanged)
						OnPreviewViewChanged();
				}
			}
			ImGui::EndCombo();
		}

		std::string rampLow, rampHigh;
		if (m_previewView == eOcclusionMeshPreviewView::coverage)
		{
			ImGui::TextWrapped("%s", locText("occlusionMeshBuild.coverageLegend"));
		}
		else if (OcclusionMeshPreview::getRampEnds(m_previewView, rampLow, rampHigh))
		{
			ImGui::TextWrapped(
				"%s", locFormat("occlusionMeshBuild.rampLegendFmt", rampLow.c_str(), rampHigh.c_str()).c_str());
		}

		if (ImGui::Button(locLabel("occlusionMeshBuild.frameMesh")))
		{
			if (OnFrameMeshEvent)
				OnFrameMeshEvent();
		}
		ImGui::SameLine();
		ImGui::Checkbox(locLabel("occlusionMeshBuild.cameraPath"), &m_bShowCameraPath);

		ImGui::Spacing();
		ImGui::TextWrapped("%s", locText("occlusionMeshBuild.boundsBoxHelp"));
		if (ImGui::Button(locLabel("occlusionMeshBuild.rebuildWithBox")))
		{
			if (OnRebuildInBoxEvent)
				OnRebuildInBoxEvent();
		}
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

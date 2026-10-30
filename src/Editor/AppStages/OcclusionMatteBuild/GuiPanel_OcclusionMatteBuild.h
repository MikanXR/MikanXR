#pragma once

#include "Constants_OcclusionMatteBuild.h"
#include "OcclusionMatteBuilder.h"
#include "Shared/GuiPanel.h"

#include <functional>
#include <string>

class GuiPanel_OcclusionMatteBuild : public GuiPanel
{
public:
	GuiPanel_OcclusionMatteBuild(class AppStage* ownerAppStage)
		: GuiPanel(ownerAppStage)
	{
	}
	virtual void onGui() override;

	eOcclusionMatteBuildMenuState getMenuState() const { return m_menuState; }
	void setMenuState(eOcclusionMatteBuildMenuState newState) { m_menuState= newState; }

	// -- settings the operator picks before a build --
	eOcclusionMatteModel getModel() const { return m_model; }
	void setModel(eOcclusionMatteModel model) { m_model= model; }
	int getBandPx() const { return m_bandPx; }
	void setBandPx(int bandPx) { m_bandPx= bandPx; }
	int getMatteScale() const { return m_matteScale; }
	void setMatteScale(int scale) { m_matteScale= scale; }
	int getFrameStep() const { return m_frameStep; }
	void setFrameStep(int step) { m_frameStep= step; }
	bool getCropSmallRuns() const { return m_bCropSmallRuns; }
	void setCropSmallRuns(bool bCrop) { m_bCropSmallRuns= bCrop; }

	void setTakeSummary(int frameCount, int imageWidth, int imageHeight, float durationSeconds, int stencilCount)
	{
		m_takeFrameCount= frameCount;
		m_takeImageWidth= imageWidth;
		m_takeImageHeight= imageHeight;
		m_takeDurationSeconds= durationSeconds;
		m_takeStencilCount= stencilCount;
	}

	// Pushed every frame while a build runs
	void setBuildProgress(eOcclusionMatteBuildPhase phase, int framesDone, int frameCount, float elapsedSeconds,
						  bool bCancelling)
	{
		m_buildPhase= phase;
		m_framesDone= framesDone;
		m_frameCount= frameCount;
		m_buildElapsedSeconds= elapsedSeconds;
		m_bCancellingBuild= bCancelling;
	}

	void setBuildSummary(const OcclusionMatteBuildStats& stats, int recordCount, const std::string& sidecarPath)
	{
		m_stats= stats;
		m_recordCount= recordCount;
		m_sidecarPath= sidecarPath;
	}
	void setFailureReason(const std::string& reason) { m_failureReason= reason; }
	const std::string& getFailureReason() const { return m_failureReason; }

	// True while the stage draws a segmented frame's overlay in the viewport,
	// which is when the panel explains its colours
	void setHasPreview(bool bHasPreview) { m_bHasPreview= bHasPreview; }

	std::function<void()> OnBuildEvent;
	std::function<void()> OnCancelBuildEvent;
	std::function<void()> OnRedoEvent;
	std::function<void()> OnCancelEvent;
	std::function<void()> OnOkEvent;

private:
	eOcclusionMatteBuildMenuState m_menuState= eOcclusionMatteBuildMenuState::inactive;

	eOcclusionMatteModel m_model= eOcclusionMatteModel::small;
	int m_bandPx= 64;
	int m_matteScale= 1;
	int m_frameStep= 1;
	bool m_bCropSmallRuns= true;

	int m_takeFrameCount= 0;
	int m_takeImageWidth= 0;
	int m_takeImageHeight= 0;
	float m_takeDurationSeconds= 0.f;
	int m_takeStencilCount= 0;

	eOcclusionMatteBuildPhase m_buildPhase= eOcclusionMatteBuildPhase::idle;
	int m_framesDone= 0;
	int m_frameCount= 0;
	float m_buildElapsedSeconds= 0.f;
	bool m_bCancellingBuild= false;

	OcclusionMatteBuildStats m_stats;
	int m_recordCount= 0;
	std::string m_sidecarPath;
	std::string m_failureReason;

	bool m_bHasPreview= false;
};

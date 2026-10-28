#pragma once

#include "Constants_OcclusionMeshBuild.h"
#include "OcclusionMeshBuilder.h"
#include "Shared/GuiPanel.h"

#include "glm/ext/vector_float3.hpp"

#include <cstdint>
#include <functional>
#include <string>

class GuiPanel_OcclusionMeshBuild : public GuiPanel
{
public:
	GuiPanel_OcclusionMeshBuild(class AppStage* ownerAppStage)
		: GuiPanel(ownerAppStage)
	{
	}
	virtual void onGui() override;

	eOcclusionMeshBuildMenuState getMenuState() const { return m_menuState; }
	void setMenuState(eOcclusionMeshBuildMenuState newState) { m_menuState= newState; }

	// -- settings the operator picks before a build --
	eOcclusionMeshVoxelPreset getVoxelPreset() const { return m_voxelPreset; }
	void setVoxelPreset(eOcclusionMeshVoxelPreset preset) { m_voxelPreset= preset; }
	int getKeyframeStride() const { return m_keyframeStride; }
	void setKeyframeStride(int stride) { m_keyframeStride= stride; }
	bool getFrameToModel() const { return m_bFrameToModel; }
	void setFrameToModel(bool bEnabled) { m_bFrameToModel= bEnabled; }
	bool getReplacePoseTrack() const { return m_bReplacePoseTrack; }
	void setReplacePoseTrack(bool bEnabled) { m_bReplacePoseTrack= bEnabled; }
	float getPathRadius() const { return m_pathRadius; }
	void setPathRadius(float radius) { m_pathRadius= radius; }
	bool getTexture() const { return m_bTexture; }
	void setTexture(bool bEnabled) { m_bTexture= bEnabled; }

	// The box the next build fuses inside, in the take's ARKit world, placed
	// over the previous build's preview. Without one the bounds come from the
	// take's depth.
	bool hasBoundsBox() const { return m_bHasBoundsBox; }
	const glm::vec3& getBoundsBoxLo() const { return m_boundsBoxLo; }
	const glm::vec3& getBoundsBoxHi() const { return m_boundsBoxHi; }
	void setBoundsBox(const glm::vec3& lo, const glm::vec3& hi)
	{
		m_bHasBoundsBox= true;
		m_boundsBoxLo= lo;
		m_boundsBoxHi= hi;
	}
	void clearBoundsBox() { m_bHasBoundsBox= false; }

	// -- the preview after a build --
	eOcclusionMeshPreviewView getPreviewView() const { return m_previewView; }
	void setPreviewView(eOcclusionMeshPreviewView view) { m_previewView= view; }
	// One bit per eOcclusionMeshPreviewView the built mesh can show
	void setAvailablePreviewViews(uint32_t viewMask) { m_availablePreviewViews= viewMask; }
	bool getShowCameraPath() const { return m_bShowCameraPath; }
	void setShowCameraPath(bool bShow) { m_bShowCameraPath= bShow; }

	void setTakeSummary(int frameCount, int depthWidth, int depthHeight, float durationSeconds)
	{
		m_takeFrameCount= frameCount;
		m_takeDepthWidth= depthWidth;
		m_takeDepthHeight= depthHeight;
		m_takeDurationSeconds= durationSeconds;
	}

	// Pushed every frame while a build runs
	void setBuildProgress(eOcclusionMeshBuildPhase phase, int framesDone, int frameCount, float elapsedSeconds,
						  bool bCancelling)
	{
		m_buildPhase= phase;
		m_framesDone= framesDone;
		m_frameCount= frameCount;
		m_buildElapsedSeconds= elapsedSeconds;
		m_bCancellingBuild= bCancelling;
	}

	void setBuildSummary(const OcclusionMeshBuildStats& stats, bool bRegistrationRan)
	{
		m_stats= stats;
		m_bRegistrationRan= bRegistrationRan;
	}
	void setFailureReason(const std::string& reason) { m_failureReason= reason; }
	void setCreatedStencilName(const std::string& name) { m_createdStencilName= name; }
	void setRefinedTrackPath(const std::string& storedPath, bool bReplacedSourceTrack)
	{
		m_refinedTrackPath= storedPath;
		m_bReplacedSourceTrack= bReplacedSourceTrack;
	}

	std::function<void()> OnBuildEvent;
	std::function<void()> OnCancelBuildEvent;
	std::function<void()> OnApplyEvent;
	std::function<void()> OnRedoEvent;
	std::function<void()> OnCancelEvent;
	std::function<void()> OnOkEvent;
	std::function<void()> OnPreviewViewChanged;
	std::function<void()> OnFrameMeshEvent;
	std::function<void()> OnRebuildInBoxEvent;

private:
	eOcclusionMeshBuildMenuState m_menuState= eOcclusionMeshBuildMenuState::inactive;

	eOcclusionMeshVoxelPreset m_voxelPreset= eOcclusionMeshVoxelPreset::room;
	int m_keyframeStride= 3;
	bool m_bFrameToModel= true;
	bool m_bReplacePoseTrack= true;
	float m_pathRadius= 2.f;
	bool m_bTexture= true;
	bool m_bHasBoundsBox= false;
	glm::vec3 m_boundsBoxLo= glm::vec3(0.f);
	glm::vec3 m_boundsBoxHi= glm::vec3(0.f);

	eOcclusionMeshPreviewView m_previewView= eOcclusionMeshPreviewView::shaded;
	uint32_t m_availablePreviewViews= 0;
	bool m_bShowCameraPath= false;

	int m_takeFrameCount= 0;
	int m_takeDepthWidth= 0;
	int m_takeDepthHeight= 0;
	float m_takeDurationSeconds= 0.f;

	eOcclusionMeshBuildPhase m_buildPhase= eOcclusionMeshBuildPhase::idle;
	int m_framesDone= 0;
	int m_frameCount= 0;
	float m_buildElapsedSeconds= 0.f;
	bool m_bCancellingBuild= false;

	OcclusionMeshBuildStats m_stats;
	bool m_bRegistrationRan= false;
	std::string m_failureReason;
	std::string m_createdStencilName;
	std::string m_refinedTrackPath;
	bool m_bReplacedSourceTrack= false;
};

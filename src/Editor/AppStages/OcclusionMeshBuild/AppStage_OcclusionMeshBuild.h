#pragma once

#include "AppStage.h"
#include "CaptureArchiveReader.h"
#include "ComponentFwd.h"
#include "Constants_OcclusionMeshBuild.h"
#include "MikanRendererFwd.h"
#include "OcclusionMeshBuilder.h"
#include "PoseTrack.h"

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class GuiPanel_OcclusionMeshBuild;
class OcclusionMeshPreview;

/// Fuses a recorded take's LiDAR depth into an occlusion mesh and registers
/// it as a model stencil placed through the take's solved world-to-stage
/// offset. The take's recorded poses are refined against the model as it is
/// fused and written back as a second pose track beside the take.
class AppStage_OcclusionMeshBuild : public AppStage
{
public:
	static const char* APP_STAGE_NAME;

	AppStage_OcclusionMeshBuild(class IEditorWindow* ownerWindow);
	virtual ~AppStage_OcclusionMeshBuild();

	/// Checks the take has everything a build needs (a trackcamera capture
	/// with depth members, a solved alignment, a camera on a stage) and pushes
	/// the stage, or shows the operator why not on fromAppStage.
	static bool tryEnterBuild(AppStage* fromAppStage, FileVideoSourceComponentPtr videoSource);

	virtual void enter() override;
	virtual void exit() override;
	virtual void update(float deltaSeconds) override;
	virtual void onGui() override;
	virtual void render(IMkViewportPtr targetViewport) override;

	// -- IRemoteControllable --
	virtual bool handleRemoteControlCommand(const std::string& command, const std::vector<std::string>& parameters,
											std::vector<std::string>& outResults) override;

protected:
	/// Everything the worker needs, gathered on the UI thread: a copy of the
	/// pose track, the capture's depth members it opens itself, and the settings.
	struct BuildRequest
	{
		PoseTrack track;
		std::filesystem::path capturePath;
		CaptureArchiveMember depthMember;
		CaptureArchiveMember confidenceMember;
		CaptureArchiveMember movieMember; // the colour frames the texturing projects
		OcclusionMeshBuildSettings settings;
	};

	struct BuildOutput
	{
		OcclusionMeshBuildResult result;
	};

	/// Relays the builder's progress into the stage's atomics
	class BuildObserver : public IOcclusionMeshBuildObserver
	{
	public:
		BuildObserver(AppStage_OcclusionMeshBuild* stage)
			: m_stage(stage)
		{
		}
		virtual bool isCancelRequested() const override;
		virtual void onProgress(eOcclusionMeshBuildPhase phase, int done, int total) override;

	private:
		AppStage_OcclusionMeshBuild* m_stage;
	};

	void setSourceVideoSource(FileVideoSourceComponentPtr videoSource, CameraComponentPtr camera,
							  StageComponentConstPtr stage, const PoseTrack& track,
							  const std::filesystem::path& capturePath, const CaptureArchiveMember& depthMember,
							  const CaptureArchiveMember& confidenceMember, const CaptureArchiveMember& movieMember);
	void setMenuState(eOcclusionMeshBuildMenuState newState);
	OcclusionMeshBuildSettings gatherSettings() const;

	// -- build worker --
	void startBuildWorker();
	void stopBuildWorker();
	void buildWorkerMain();
	void runBuildRequest(const BuildRequest& request, BuildOutput& outOutput);
	void consumeBuildOutput();

	bool createStencilFromMesh();
	bool writeRefinedPoseTrack(std::string& outStoredPath);

	// -- preview --
	/// Where the mesh's own space, the take's ARKit world, lands in the world:
	/// the stage's transform after the take's solved pose offset
	glm::mat4 getMeshToWorld() const;
	void buildPreview();
	void releasePreview();
	bool setPreviewView(eOcclusionMeshPreviewView view);
	/// Orbit the viewport camera around the mesh, looking the way the take did
	void frameMesh();
	void renderMeshPreview();
	void renderCameraPath();
	void renderBoundsBox();

	// -- bounds box handles, one per face: axis = handle / 2, high face when odd --
	glm::vec3 getBoxHandleCenter(int handle) const;
	float getBoxHandleRadius() const;
	int pickBoxHandle(const glm::vec3& rayOrigin, const glm::vec3& rayDir) const;
	void onMouseRayChanged(const glm::vec3& rayOrigin, const glm::vec3& rayDir);
	void onMouseRayButtonDown(const glm::vec3& rayOrigin, const glm::vec3& rayDir, int button);
	void onMouseRayButtonUp(const glm::vec3& rayOrigin, const glm::vec3& rayDir, int button);

	// GUI button events
	void onBuildEvent();
	void onCancelBuildEvent();
	void onApplyEvent();
	void onRedoEvent();
	void onRebuildInBoxEvent();
	void onCancelEvent();

private:
	class GuiPanel_OcclusionMeshBuild* m_buildPanel= nullptr;

	FileVideoSourceComponentPtr m_videoSource;
	CameraComponentPtr m_camera;
	StageComponentConstPtr m_stage;
	PoseTrack m_track;
	std::filesystem::path m_capturePath;
	CaptureArchiveMember m_depthMember;
	CaptureArchiveMember m_confidenceMember;
	CaptureArchiveMember m_movieMember;

	// -- build worker state --
	std::thread m_workerThread;
	mutable std::mutex m_workerMutex;
	std::condition_variable m_workerSignal;
	bool m_bWorkerShutdownRequested= false; ///< guarded by m_workerMutex
	bool m_bBuildRequested= false;          ///< guarded by m_workerMutex
	bool m_bBuildFinished= false;           ///< guarded by m_workerMutex
	BuildRequest m_pendingRequest;          ///< guarded by m_workerMutex
	BuildOutput m_buildOutput;              ///< guarded by m_workerMutex

	std::atomic<int> m_buildPhase{(int)eOcclusionMeshBuildPhase::idle};
	std::atomic<int> m_framesDone{0};
	std::atomic<int> m_frameCount{0};
	std::atomic<bool> m_bCancelRequested{false};
	float m_buildElapsedSeconds= 0.f; ///< UI thread only

	OcclusionMeshBuildResult m_result;
	bool m_bHasResult= false;
	bool m_bLastBuildRegistered= false;

	MikanCameraPtr m_mkCamera;
	std::unique_ptr<OcclusionMeshPreview> m_preview;

	// The box being fitted over the preview, in the take's ARKit world
	glm::vec3 m_boxLo= glm::vec3(0.f);
	glm::vec3 m_boxHi= glm::vec3(0.f);
	int m_hoveredBoxHandle= -1;
	int m_draggedBoxHandle= -1;
	float m_dragGrabOffset= 0.f; ///< metres from the dragged face to where the ray grabbed it
};

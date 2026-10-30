#pragma once

#include "AppStage.h"
#include "CaptureArchiveReader.h"
#include "ComponentFwd.h"
#include "Constants_OcclusionMatteBuild.h"
#include "MikanRendererFwd.h"
#include "ModelCatalog.h"
#include "OcclusionMatteBuilder.h"
#include "PoseTrack.h"
#include "ReconstructionTypes.h"
#include "SceneFwd.h"

#include "glm/ext/matrix_float4x4.hpp"

#include <opencv2/core.hpp>

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class GuiPanel_OcclusionMatteBuild;
class Sam2Segmenter;

/// Builds a recorded take's occlusion matte: the project's model stencils
/// rendered at the take's recorded poses, their occluding contours prompted
/// into the segmenter, and the labels written as a sidecar beside the capture.
/// The source reopens the sidecar when the build lands, so playback picks it
/// up without reopening the media.
class AppStage_OcclusionMatteBuild : public AppStage
{
public:
	static const char* APP_STAGE_NAME;

	AppStage_OcclusionMatteBuild(class IEditorWindow* ownerWindow);
	virtual ~AppStage_OcclusionMatteBuild();

	/// Checks the take has everything a build needs (a trackcamera capture
	/// with a movie, a solved alignment, a camera on a stage, a model stencil
	/// to render) and pushes the stage, or shows the operator why not on
	/// fromAppStage.
	static bool tryEnterBuild(AppStage* fromAppStage, FileVideoSourceComponentPtr videoSource);

	virtual void enter() override;
	virtual void exit() override;
	virtual void update(float deltaSeconds) override;
	virtual void render(IMkViewportPtr targetViewport) override;
	virtual void onGui() override;

	// -- IRemoteControllable --
	virtual bool handleRemoteControlCommand(const std::string& command, const std::vector<std::string>& parameters,
											std::vector<std::string>& outResults) override;

protected:
	/// One stencil's mesh, read on the UI thread and owned by the request so
	/// the worker never touches a scene object
	struct RequestMesh
	{
		TriangleMesh mesh;
		glm::mat4 meshToTakeWorld= glm::mat4(1.f);
		std::string name;
		std::string modelPath;
	};

	/// Everything the worker needs, gathered on the UI thread: a copy of the
	/// pose track, the capture's movie member it opens itself, the meshes,
	/// the settings, the sidecar path, and the model directory
	struct BuildRequest
	{
		PoseTrack track;
		std::filesystem::path capturePath;
		CaptureArchiveMember movieMember;
		std::vector<RequestMesh> meshes;
		OcclusionMatteSettings settings;
		std::filesystem::path jsonPath;
		std::filesystem::path modelDirectory;
	};

	struct BuildOutput
	{
		OcclusionMatteBuildResult result;
	};

	/// Relays the builder's progress into the stage's atomics and composes
	/// the preview overlay of each segmented frame
	class BuildObserver : public IOcclusionMatteBuildObserver
	{
	public:
		BuildObserver(AppStage_OcclusionMatteBuild* stage)
			: m_stage(stage)
		{
		}
		virtual bool isCancelRequested() const override;
		virtual void onProgress(eOcclusionMatteBuildPhase phase, int done, int total) override;
		virtual void onFrameProcessed(const OcclusionMatteFrame& frame) override;

	private:
		AppStage_OcclusionMatteBuild* m_stage;
	};

	void setSourceVideoSource(FileVideoSourceComponentPtr videoSource, StageComponentConstPtr stage,
							  const PoseTrack& track, const std::filesystem::path& capturePath,
							  const CaptureArchiveMember& movieMember);
	void setMenuState(eOcclusionMatteBuildMenuState newState);
	OcclusionMatteSettings gatherSettings() const;
	eModelId getChosenModelId() const;
	/// Reads every enabled model stencil's glTF into the request. False, with
	/// the reason on the panel, when none could be read.
	bool gatherMeshes(std::vector<RequestMesh>& outMeshes);

	// -- build worker --
	void startBuildWorker();
	void stopBuildWorker();
	void buildWorkerMain();
	/// Runs on the worker thread, which owns the segmenter for its whole
	/// lifetime: the ONNX sessions are created, run and destroyed on one thread
	void runBuildRequest(const BuildRequest& request, BuildOutput& outOutput);
	void consumeBuildOutput();

	// -- preview --
	/// Composes the overlay of a segmented frame on the worker: added pixels
	/// tinted green, removed pixels red, at most k_previewMaxWidth wide
	void composePreviewOverlay(const OcclusionMatteFrame& frame);
	/// Uploads a changed overlay into the preview texture, on the UI thread
	void updatePreviewTexture();
	/// Draws the last segmented frame's overlay over the viewport
	void renderPreview();
	void releasePreview();

	// GUI button events
	void onBuildEvent();
	void onCancelBuildEvent();
	void onRedoEvent();
	void onCancelEvent();

private:
	class GuiPanel_OcclusionMatteBuild* m_buildPanel= nullptr;

	FileVideoSourceComponentPtr m_videoSource;
	StageComponentConstPtr m_stage;
	PoseTrack m_track;
	std::filesystem::path m_capturePath;
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
	// Created, started, run and destroyed on the worker. The UI thread only
	// reaches it under m_workerMutex, and only to call the thread-safe
	// requestCancel().
	std::unique_ptr<Sam2Segmenter> m_segmenter;      ///< guarded by m_workerMutex
	std::filesystem::path m_segmenterModelDirectory; ///< worker thread only

	std::atomic<int> m_buildPhase{(int)eOcclusionMatteBuildPhase::idle};
	std::atomic<int> m_framesDone{0};
	std::atomic<int> m_frameCount{0};
	std::atomic<bool> m_bCancelRequested{false};
	float m_buildElapsedSeconds= 0.f; ///< UI thread only

	OcclusionMatteBuildResult m_result;
	bool m_bHasResult= false;

	// -- preview handover --
	std::mutex m_previewMutex;
	cv::Mat m_pendingOverlay;      ///< guarded by m_previewMutex, CV_8UC3 BGR
	bool m_bOverlayChanged= false; ///< guarded by m_previewMutex
	cv::Mat m_overlay;             ///< UI thread only
	IMkTexturePtr m_previewTexture;
	IMkTriangulatedMeshPtr m_previewQuad;
};

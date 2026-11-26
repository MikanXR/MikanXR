#pragma once

#include "AppStage.h"
#include "Constants_DepthMeshCapture.h"
#include "DepthMeshGenerator.h"
#include "MikanRendererFwd.h"
#include "MikanTypeFwd.h"
#include "MoGeInference.h"
#include "SceneFwd.h"
#include "VideoDisplayConstants.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class GuiPanel_DepthMeshCapture;

/// Captures one video frame, recovers metric scene depth from it, and turns
/// the result into a model stencil (shadow catcher) placed at the capturing
/// camera's pose - replacing hand-placed proxy geometry.
///
/// The mesh is judged before it is committed: the panel shows the mesh
/// statistics and a depth overlay drawn over the live frame, so an operator
/// can check the recovered silhouettes against the image before a stencil is
/// created.
class AppStage_DepthMeshCapture : public AppStage
{
public:
	static const char* APP_STAGE_NAME;

	AppStage_DepthMeshCapture(class IEditorWindow* ownerWindow);
	virtual ~AppStage_DepthMeshCapture();

	/// Camera supplying the video feed, the calibrated FOV the metric depth
	/// recovery needs, and the pose the stencil is placed at. Call before
	/// enter().
	void setSourceCamera(CameraComponentPtr cameraComponent);
	/// Scene the captured stencil is parented to. Call before enter().
	void setTargetScene(SceneComponentPtr sceneComponent);

	virtual void enter() override;
	virtual void exit() override;
	virtual void update(float deltaSeconds) override;
	// Required: AppStage::onGui() only draws the modal dialog stack, so a stage
	// that does not override this renders no panels at all.
	virtual void onGui() override;
	virtual void render(IMkViewportPtr targetViewport) override;

protected:
	/// Everything the capture worker needs, prepared on the UI thread: the
	/// video buffers and the distortion view the marker detector reads are not
	/// safe to touch from another thread, so the worker gets plain data only.
	struct CaptureRequest
	{
		cv::Mat bgrFrame;
		float fovXDegrees= 0.f;
		bool bHasMarker= false;
		std::vector<cv::Point2f> markerCornerPixels;
		std::vector<float> markerCornerDepths; ///< camera-space truth, metres
		float storedScaleCorrection= 1.f;
	};

	struct CaptureOutput
	{
		bool bSucceeded= false;
		bool bCancelled= false;
		std::string failureReason;
		std::string executionProvider;

		MoGeInference::Result geometry;
		DepthMeshGenerator::Mesh mesh;
		DepthMeshGenerator::Stats meshStats;
		eDepthScaleCorrectionSource scaleSource= eDepthScaleCorrectionSource::none;
		float appliedScaleCorrection= 1.f;
		float markerCornerSpread= 0.f;
	};

	void setMenuState(eDepthMeshCaptureMenuState newState);

	bool createStencilFromMesh();

	// -- capture worker --
	void startCaptureWorker();
	void stopCaptureWorker();
	void captureWorkerMain();
	/// Runs on the worker thread. Reports progress through m_capturePhase and
	/// bails between steps (and mid-inference, via MoGeInference::requestCancel)
	/// when m_bCancelRequested is set.
	void runCaptureRequest(const CaptureRequest& request, CaptureOutput& outOutput);
	/// Runs on the UI thread once the worker signals completion.
	void consumeCaptureOutput();

	/// Detects any of the project's ArUco markers in the current video frame and
	/// returns its subpixel corner pixels plus their true camera-space depths
	/// from solvePnP. UI thread only - it reads the distortion view.
	bool tryDetectMarkerCorners(std::vector<cv::Point2f>& outCornerPixels, std::vector<float>& outCornerDepths);

	/// Compares the detected corners' true depths against the model's depths at
	/// those pixels to produce a scale correction. outCornerSpread is the worst
	/// per-corner disagreement with the factor - a consistency check on the
	/// geometry, not just the scale. Pure, so the worker can call it.
	static bool computeScaleFromMarkerCorners(const MoGeInference::Result& geometry,
											  const std::vector<cv::Point2f>& cornerPixels,
											  const std::vector<float>& cornerDepths, float& outFactor,
											  float& outCornerSpread);

	/// Draws the recovered depth as colored points over the video frame so the
	/// silhouette alignment can be judged visually.
	void renderDepthPreview();

	// GUI button events
	void onCaptureEvent();
	void onCancelCaptureEvent();
	void onApplyEvent();
	void onRedoEvent();
	void onCancelEvent();

private:
	class GuiPanel_DepthMeshCapture* m_capturePanel= nullptr;

	CameraComponentPtr m_currentSceneCameraComponent;
	SceneComponentWeakPtr m_targetScene;
	VideoSourceComponentPtr m_videoSourceComponent;
	class VideoFrameDistortionView* m_monoDistortionView= nullptr;

	// -- capture worker state --
	// Inference runs off the UI thread so the progress readout and the cancel
	// button stay live. The worker owns m_inference for its whole lifetime
	// because OnnxSession must be created, run, and destroyed on one thread;
	// the UI thread may only touch it under m_workerMutex, and only to call the
	// thread-safe requestCancel().
	std::thread m_workerThread;
	mutable std::mutex m_workerMutex;
	std::condition_variable m_workerSignal;
	bool m_bWorkerShutdownRequested= false;     ///< guarded by m_workerMutex
	bool m_bCaptureRequested= false;            ///< guarded by m_workerMutex
	bool m_bCaptureFinished= false;             ///< guarded by m_workerMutex
	CaptureRequest m_pendingRequest;            ///< guarded by m_workerMutex
	CaptureOutput m_captureOutput;              ///< guarded by m_workerMutex
	std::unique_ptr<MoGeInference> m_inference; ///< guarded by m_workerMutex

	std::atomic<int> m_capturePhase{(int)eDepthMeshCapturePhase::idle};
	std::atomic<bool> m_bCancelRequested{false};
	float m_captureElapsedSeconds= 0.f; ///< UI thread only

	MoGeInference::Result m_geometry;
	DepthMeshGenerator::Mesh m_mesh;
	DepthMeshGenerator::Stats m_meshStats;
	/// The frame the geometry was inferred from, kept so Create Stencil can
	/// save it as the proxy's projected texture.
	cv::Mat m_capturedFrame;
	bool m_bHasResult= false;

	// Scale calibration state for the current capture
	eDepthScaleCorrectionSource m_scaleCorrectionSource= eDepthScaleCorrectionSource::none;
	float m_appliedScaleCorrection= 1.f;
	float m_markerCornerSpread= 0.f;

	MikanCameraPtr m_mkCamera;
};

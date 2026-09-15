# Calibration

The calibration subsystem in `src/Editor/Calibration`, built on OpenCV, plus the `src/Editor/AppStages` UI flows that drive it. Covers lens intrinsics, camera-to-tracker alignment, marker-based camera and stage alignment, and the triangulation/alignment tools for anchors, stencils, and light fixtures. See [videosources.md](./videosources.md) for the video frame pipeline these flows consume, [conventions.md](./conventions.md) for coordinate conventions, and [objects.md](./objects.md) for the components that store the results.

Two further consumers of a calibrated frame share this directory but not this doc, because their math is inference rather than geometry: `SceneLightingEstimator` with `MarigoldInference` ([scene-lighting.md](./scene-lighting.md)) and `DepthMeshGenerator` with `MoGeInference` ([depth-proxy-mesh.md](./depth-proxy-mesh.md)). They live here because both start from an undistorted frame plus its calibrated intrinsics, and one of them reuses the ArUco detection below.

---

## Shared infrastructure

- `VideoFrameDistortionView` (`src/Editor/Calibration/VideoFrameDistortionView.h`) supplies frames. Calibration flows create it in `eVideoFrameProcessorMode::CALIBRATION`, which runs `CVVideoFrameProcessor` (CPU `cv::remap` undistortion plus grayscale buffers for pattern detection); the compositor uses `COMPOSITOR` mode with `GLVideoFrameProcessor` (GPU shader undistortion). `applyMonoCameraIntrinsics()` rebuilds the distortion maps via `cv::initUndistortRectifyMap`.

- `CalibrationPatternFinder` is the pattern-detection base class with three subclasses: `CalibrationPatternFinder_Chessboard` (`cv::findChessboardCorners` + `cv::cornerSubPix`), `CalibrationPatternFinder_Charuco` (`cv::aruco::CharucoDetector` with `CORNER_REFINE_SUBPIX`), and `CalibrationPatternFinder_Aruco` (`cv::aruco::ArucoDetector` against a predefined dictionary). Pattern parameters (charuco rows/cols, square and marker lengths in mm, dictionary) come from `MarkerObjectSystemDefinition` (`src/Editor/ECS/Marker/MarkerObjectSystem.cpp`) and persist in the project file. The chessboard finder exists but no current flow instantiates it; the intrinsics and alignment calibrators hardcode the ChArUco finder (unverified whether chessboard mode is reachable from the UI).

- `estimateNewCalibrationPatternPose()` on the base finder resolves a camera-to-pattern transform through the shared math in `src/Editor/Math/CameraMath.cpp`: `computeOpenCVCameraRelativePatternTransform()` wraps `cv::solvePnP` on undistorted image points; `computeMonoLensCameraCalibration()` wraps `cv::initCameraMatrix2D` + `cv::calibrateCamera`; `computeOpenGLProjMatFromCameraIntrinsics()` turns intrinsics into a GL projection matrix.

- 2D/3D overlay drawing helpers live in `CalibrationRenderHelpers.cpp`.

---

## Mono lens intrinsics

`AppStage_MonoLensCalibration` drives `MonoLensDistortionCalibrator`. The user shows a ChArUco board; `findNewCalibrationPattern(minSeperationDist)` gates capture on board movement, and the flow accumulates a desired number of board captures across the frame. `computeCameraCalibration()` calls `computeMonoLensCameraCalibration()`, which runs `cv::calibrateCamera` with `CALIB_FIX_ASPECT_RATIO + CALIB_FIX_PRINCIPAL_POINT + CALIB_ZERO_TANGENT_DIST + CALIB_RATIONAL_MODEL + CALIB_FIX_K3 + CALIB_FIX_K4 + CALIB_FIX_K5` (so in practice k1, k2, k6 radial terms are optimized, tangential terms stay zero) and derives the undistorted camera matrix with `cv::getOptimalNewCameraMatrix`. The result is a `MikanMonoIntrinsics` plus reprojection error; the stage applies it with `VideoSourceComponent::setCameraIntrinsics()`, which stores it on `VideoSourceDefinition` and persists it in the project `*.mikanproj` config. The stage also feeds it back to the live view via `VideoFrameDistortionView::applyMonoCameraIntrinsics()` for immediate undistortion preview.

---

## Camera-to-tracker alignment (aperture offset)

`AppStage_AlignmentCalibration` drives `MonoLensTrackerPoseCalibrator`. Setup: the camera rig carries a VR tracking puck (`TrackingMountComponent`), and a second "mat" puck sits at a known millimeter offset from a printed ChArUco board. Each sample reads both puck poses through `VRDevicePoseView`s, solves the camera-to-board transform optically via `computeOpenCVCameraRelativePatternTransform()` (`cv::solvePnP`), and calls `computeCameraPuckToApertureXform()` (`CameraMath.h`) to solve for the fixed tracker-puck-to-lens ("aperture") transform. Samples are averaged (`computeAverageCameraPuckToApertureOffset`, quaternion/vector averaging with `cv::Quatd`); the result is written to `CameraDefinition::setAperturePoseOffset()` and persists with the camera component. `CameraComponent::updateAperturePoseFromTrackingMount()` composes this offset onto the live puck pose every tick thereafter. Offline math tests exist in `Calibration/Test/TrackerPoseCalibratorTests.cpp` (run by the unit test suite).

---

## Marker-based camera and stage alignment

Three flows share `ArucoMarkerPoseSampler`, which detects a single ArUco marker (`CalibrationPatternFinder_Aruco`), computes the aperture-relative marker transform each frame via `cv::solvePnP`, and averages a desired sample count:

- `AppStage_AlignCameraByOriginMarker`: for a camera viewing the stage's designated origin marker (`VRTrackingVolumeDefinition::getOriginMarkerId()`). For a fixed camera the averaged marker pose is inverted into a stage-space aperture pose and applied with `CameraComponent::setRelativeTransform()`. For a frame-coupled source (an ARKit phone, or a recorded take with a pose track) each sample is converted into a world-to-stage offset using that frame's own pose and the average is stored through `IFrameCoupledPoseProvider::setPoseOffset()` instead, see [videosources.md](./videosources.md). A source that offers no pose, such as a recording without a track, takes the fixed camera path. A file source with a marker reference recording is switched to it for the stage's lifetime through `IAlignmentReferenceSource`.

- `AppStage_AlignCameraByUtilityMarker`: same mechanic against an explicit utility `MarkerDefinition`, additionally using a second, already-aligned source camera; the target camera's stage-space aperture transform is set from the shared marker observation.

- `AppStage_VRTrackingRecenter`: pairs `ArucoMarkerPoseSampler` with `VRDevicePoseSampler` (which averages a VR puck pose over the same frames). Given the averaged marker pose in aperture space, the puck pose in VR space, and the calibrated aperture offset, it computes the VR-tracking-space to stage-space transform and stores it with `VRTrackingVolumeComponent::setVRSpaceToStageSpace()`, recentering all VR devices onto the stage origin marker.

A fourth flow uses `CalibrationPatternFinder_Aruco` directly rather than through the sampler. `AppStage_DepthMeshCapture` searches its captured frame for every marker in the project, and a found marker's four solvePnP corner depths are the ground truth that corrects the depth model's metric scale. The correction persists on the camera, because the error is a property of the lens rather than of the frame. See [depth-proxy-mesh.md](./depth-proxy-mesh.md).

---

## Point tools: anchors, light fixtures, stencils

- `AppStage_AnchorTriangulation` / `AnchorTriangulator`: the user clicks the same physical point from two tracked camera poses; `computeCameraRayAtPixel()` builds a stage-space ray per click and the triangulated point is the ray intersection. Three triangulated points (origin, +X, +Y by convention) define the anchor transform, applied via the anchor component/definition `setWorldTransform`/`setRelativeTransform`.

- `AppStage_LightFixtureCalibration` / `LightFixtureTriangulator`: the same two-ray triangulation for a single point, positioning a `DMXFixtureComponent` in the stage.

- `AppStage_StencilAlignment` / `StencilAligner`: the user pairs clicked video pixels with clicked model vertices on a `ModelStencilComponent`; `computeStencilTransform()` solves the model pose with `computeOpenCVCameraRelativePatternTransform()` (`cv::solvePnP` over the pixel/vertex correspondences).

- `AppStage_PointCloudAlignment`: model-to-video alignment without manual correspondences. `NaturalFeatureCloudBuilder` tracks natural features across frames from a tracked camera (`cv::goodFeaturesToTrack` + `cv::calcOpticalFlowPyrLK`), triangulating a stage-space feature cloud with reprojection-error and parallax stats. `ModelPointCloudAligner` then runs trimmed-correspondence ICP (SVD-based rigid fit via `cv::SVD::compute`, optional uniform scale, voxel downsampling, optional dominant-plane removal) to produce an `IcpResult` whose `modelWorldTransform` is applied to the `ModelStencilComponent`.

All of these persist through the component definitions into the project `*.mikanproj` file; none write separate calibration files.

---

## Distortion model and downstream use

Intrinsics are stored as `MikanMonoIntrinsics` (see [videosources.md](./videosources.md) and `src/Libraries/MikanClientAPI/Public/MikanVideoSourceTypes.h`): an 8-coefficient OpenCV rational distortion model (`k1..k6` radial numerator/denominator, `p1`/`p2` tangential) plus separate distorted and undistorted 3x3 camera matrices; hfov/vfov are derived from the undistorted matrix. Stereo intrinsics (`MikanStereoIntrinsics`, rectification/essential/fundamental matrices, `computeOpenCVCameraRectification`) are defined and plumbed through `VideoFrameSection` left/right handling, but no stereo calibration AppStage exists (unverified/incomplete).

Results feed rendering in two ways. For image correction, `VideoFrameDistortionView` bakes the model into X/Y remap maps (`cv::initUndistortRectifyMap`) consumed either by `cv::remap` on the CPU or as a distortion-map texture by `GLVideoFrameProcessor::computeUndistortion` on the GPU. For geometry, `VideoSourceComponent::recomputeCameraProjectionMatrix()` and `CameraComponent::getApertureProjectionMatrix()` build the GL projection from the undistorted camera matrix via `computeOpenGLProjMatFromCameraIntrinsics()`, which is what the compositor and client `MikanCameraNewFrameEvent` consumers see. Coordinate-space conventions (OpenCV camera space vs GL, stage space, VR tracking space) are documented in [conventions.md](./conventions.md).

# Conventions

The math and coordinate conventions every subsystem assumes, in one place: math vocabulary, matrix conventions, world space, camera spaces, tracking spaces, rotations, intrinsics, and image/texture orientation. Mikan bridges OpenCV calibration, OpenGL rendering, and OpenVR tracking, and each disagrees with the others somewhere. This doc names each disagreement and its conversion point in code. See [calibration.md](./calibration.md) for the calibration algorithms themselves, [videosources.md](./videosources.md) for frame delivery, [objects.md](./objects.md) for how transforms attach to scene objects, and [wire-protocol.md](./wire-protocol.md) for the wire math types.

---

## Math vocabulary

- GLM is the working math library everywhere in the editor: `glm::vec3`, `glm::quat`, `glm::mat4` (float), with `glm::dmat4` / `glm::dvec3` / `glm::dquat` doubles in calibration paths.

- `src/Libraries/MikanMath` is a small helper DLL on top of GLM: `MathGLM.h` (axis accessors, ray/triangle/OBB intersection, composition helpers), `MathUtility.h` (scalar helpers, constants like `k_real64_pi`), `Transform.h` (`GlmTransform`, a TRS decomposition wrapper), `MathMikan.h`.

- Wire/config math types live in `src/Libraries/MikanClientAPI/Public/MikanMathTypes.h`: `MikanVector3f/d`, `MikanQuatf/d`, `MikanRotator3f` (Euler), `MikanMatrix4f`, `MikanMatrix3d`, `MikanMatrix4x3d`, `MikanTransform` (scale, rotation quat, position). The matrix structs document their storage as column major in the header.

- All conversions between GLM, OpenCV (`cv::Matx`, `cv::Vec`, `cv::Quatd`), Mikan wire types, and `VRDevicePose` are centralized in `src/Editor/Math/MathTypeConversion.h` (plus `src/Editor/OpenCV/MathOpenCV.h`). Do not hand-roll element copies elsewhere.

## Matrix and multiplication conventions

- GLM matrices are column major and indexed column-first (`m[col][row]`); vectors are column vectors, so transforms apply as `M * v` and chained transforms read right to left.

- OpenCV `cv::Matx` is indexed row-first (`m(row, col)`). Every GLM/OpenCV crossing transposes element order; `convertOpenCVCameraRelativePoseToGLMMat` in `src/Editor/Math/CameraMath.cpp` spells this out ("GLM indexed by column first, OpenCV indexed by row first").

- Composition helpers in `MathGLM.h` use apply-order arguments: `glm_composite_xform(first, second)` returns `second * first` (apply `first`, then `second`), likewise `glm_composite_rotation`. `glm_relative_xform(parentWorldXform, childWorldXform)` returns `inverse(parent) * child`. Prefer these over raw `*` when order matters.

- `GlmTransform::rebuildMat()` composes `translation * (rotation * scale)`; `setMat4` decomposes with `glm::decompose` and drops skew/perspective.

## World space and units

- World space is OpenGL-style right handed: +X right, +Y up, +Z toward the viewer (camera forward is -Z). Comments asserting this: `computeCameraRayAtPixel` ("-Z is forward") and `CameraComponent::makeNewCameraFrameEvent` ("Camera up is along the y-axis") in `src/Editor/ECS/Camera/CameraComponent.cpp`.

- Units are meters. `src/Editor/ECS/Editor/EditorObjectSystem.h` states it twice ("scene is natively meters", "world units are meters"); default clip planes are `DEFAULT_MONO_ZNEAR` 0.1 m / `DEFAULT_MONO_ZFAR` 20 m (`CameraMath.h`).

- Millimeters appear only at physical-measurement boundaries and are suffixed as such: solvePnP object points and translations (`objectPointsMM`, `tvecMM`), marker size (`MarkerDefinition::k_lengthMMPropertyId`), stage bounds (`StageComponentDefinition::getStageBoundsMinMM`), mat-puck offsets. Converters `k_millimeters_to_meters` / `k_meters_to_millimeters` are in `MathTypeConversion.h`.

---

## Camera and view conventions

A camera pose is a camera-to-world `glm::mat4` in the GL convention above (column 0 right, column 1 up, column 2 backward, column 3 position).

- View matrix: `computeGLMCameraViewMatrix(poseXform)` in `src/Editor/Math/CameraMath.cpp` builds the inverse rigid transform directly from the pose columns (adapted from `glm::lookAt`). `CameraComponent::getApertureViewMatrix` feeds it `getStageSpaceAperturePose`.

- Projection matrix: `computeOpenGLProjMatFromCameraIntrinsics` (`CameraMath.cpp`) converts a calibrated OpenCV camera matrix into a GL frustum by composing an NDC ortho matrix with the augmented intrinsic matrix (technique credited in-code to Gregson/Tabb/Simek). This is the single point where pixel-unit intrinsics become GL clip space. The principal point changes frame on the way through: OpenCV measures `cy` down from the image top, the NDC mapping puts y=0 at the viewport bottom, so that conversion uses `pixelHeight - cy`. `cx` needs no equivalent. The flip is invisible for a lens-calibrated camera, whose `cy` sits at the exact image center because calibration runs with `CALIB_FIX_PRINCIPAL_POINT`; it only shows up for a source that reports its own off-center principal point, where getting it wrong slides the whole CG overlay vertically.

- OpenCV extrinsics from a GL pose: `computeOpenCVCameraExtrinsicMatrix` (`CameraMath.cpp`) inverts the stage-space aperture pose and transposes it into a row-major `cv::Matx34f`.

## OpenCV camera space vs OpenGL camera space

OpenCV camera space is right handed with +Z forward and +Y down; OpenGL camera space is right handed with -Z forward and +Y up. The single conversion point for poses coming out of calibration is `convertOpenCVCameraRelativePoseToGLMMat` in `src/Editor/Math/CameraMath.cpp`: it converts mm to meters, negates the Y and Z rows of the rotation/translation ("(x, y, z) -> (x, -y, -z)"), transposes into column-major GLM, and post-multiplies a 180-degree rotation about X. Its callers are `CalibrationPatternFinder.cpp` and `StencilAligner.cpp` in `src/Editor/Calibration`, which is how every solvePnP result (camera-to-pattern, camera-to-stencil) enters GL space.

The ML frame estimators cross the same boundary and have their own single point. Both ONNX models emit geometry in the OpenCV convention, and `MoGeInference::run` negates Y and Z once at its API boundary, so everything `MoGeInference::Result` exposes (points, normals) is already Mikan camera space with visible geometry at negative Z. Do not flip again downstream: the depth mesh generator, the OBJ writer, and the lighting fit all consume the converted values. Details in [depth-proxy-mesh.md](./depth-proxy-mesh.md).

## Spherical harmonic environments do not transform like vectors

A recovered lighting environment is a spherical function stored as 27 coefficients in Mikan world space, not a direction. Unreal's frame is a Y/Z swap away from Mikan's, and a swap is a handedness flip rather than a rotation, so the coefficients cannot be moved between the two frames at all. The consumer instead leaves them in Mikan space and evaluates the basis at the swapped direction. This is why `UMikanLightEnvironmentData::GetSHCoefficients` returns Mikan-space values while `GetKeyLightDirection` returns Unreal-space ones: a direction is a vector and converts directly, a spherical function does not. The editor's own probe sphere needs no swap for the same reason, but it must be drawn unrotated, since its shader treats object-space position as the world-space direction to evaluate along. See [scene-lighting.md](./scene-lighting.md).

## Rotation representations

- Runtime: `glm::quat` / `glm::dquat`. Wire/config: `MikanQuatf` / `MikanQuatd`; `MikanRotator3f` holds Euler angles for UI-facing rotation properties, converted via `MikanRotator3f_to_glm_quat` / `glm_quat_to_MikanRotator3f` (`MathTypeConversion.h`) and the Euler helpers in `MathGLM.h`.

- Rodrigues rotation vectors exist only at the OpenCV boundary: `cv::solvePnP` returns an `rvec` that `computeOpenCVCameraRelativePatternTransform` (`CameraMath.cpp`) immediately converts with `cv::Quatd::createFromRvec`; nothing downstream stores rvecs.

## Intrinsics

`MikanMonoIntrinsics` (wire struct, populated by `computeMonoLensCameraCalibration` in `CameraMath.cpp`) carries `pixel_width`/`pixel_height`, `hfov`/`vfov` (degrees, from `cv::calibrationMatrixValues`), `znear`/`zfar`, a `distorted_camera_matrix` and an `undistorted_camera_matrix` (`MikanMatrix3d`), and `distortion_coefficients`.

- Camera matrices are in pixel units, OpenCV convention: `fx`, `fy` on the diagonal, principal point `(cx, cy)` in pixels from the image top-left (element accessors in `extractCameraIntrinsicMatrixParameters`). The undistorted matrix is `cv::getOptimalNewCameraMatrix` with alpha 0 (border cropped).

- `MikanMatrix3d` stores columns, not rows: `x`, `y`, `z` name the three column vectors and the digit is the row, so an OpenCV camera matrix lands as `x0 = fx`, `y1 = fy`, `z0 = cx`, `z1 = cy`. Populate one through `cv_mat33d_to_MikanMatrix3d` rather than by aggregate initialization, whose nine values read in declaration order and so transpose the matrix silently. A principal point written into `x2`/`y2` reads back as zero, which collapses `cv::remap` onto the image corner and leaves a nearly black undistorted frame.

- Distortion model: OpenCV rational model, 8 coefficients `(k1, k2, p1, p2, k3, k4, k5, k6)`. Calibration runs `cv::calibrateCamera` with `CALIB_RATIONAL_MODEL + CALIB_ZERO_TANGENT_DIST + CALIB_FIX_K3/K4/K5 + CALIB_FIX_ASPECT_RATIO + CALIB_FIX_PRINCIPAL_POINT`, keeps the first 8 of OpenCV's 14 outputs, and stores them as a column vector (`cv_vec8_to_Mikan_distortion`; OpenCV's native row vector is transposed on the way in and back out via `Mikan_distortion_to_cv_vec8`).

- Pose solves against undistorted image points pass zeroed distortion coefficients to `cv::solvePnP` (`computeOpenCVCameraRelativePatternTransform`).

---

## Tracking spaces

- VR tracking space (SteamVR/OpenVR) is right handed, +Y up, meters, -Z forward: the same convention as Mikan world space. `vr_HmdMatrix34_to_glm_mat4` in `src/Plugins/MikanSteamVR/Private/MikanSteamVRMath.cpp` is therefore a pure row/column transpose with no axis flip.

- Stage space is Mikan's calibrated world space. The VR-to-stage transform is stored per tracking volume: `VRTrackingVolumeComponent::getVRSpaceToStageSpace()` (`src/Editor/ECS/TrackingVolume/VRTrackingVolumeComponent.h`), set by the recenter flow (`AppStage_VRTrackingRecenter`). The single application point is `VRDevicePoseView::getPose` (`src/Editor/ECS/VRObject/VRDevicePoseView.cpp`): a pose view built with `eVRDevicePoseSpace::MikanTrackingVolumePose` composes the raw device pose with `vrSpaceToStageSpace`; `VRTrackingSystemPose` returns it raw.

- Camera poses for VR-tracked cameras chain: VR puck pose (`TrackingMountComponent` -> `VRDeviceComponent`) composed with the calibrated puck-to-aperture offset stored in `CameraDefinition` (`CameraComponent::updateAperturePoseFromTrackingMount`). That offset comes from `computeCameraPuckToApertureXform` (`CameraMath.cpp`), which also documents two physical-space quirks: the mat-puck offset measurement swaps Y and Z entering VR tracking space, and the calibration pattern is yawed 180 degrees relative to the mat puck's forward. `CameraComponent::getStageSpaceAperturePose` is the pose everything downstream (rendering, `computeOpenCVCameraExtrinsicMatrix`) consumes.

- Marker-based tracking volumes (`MarkerTrackingVolumeComponent`) anchor stage space to an ArUco origin marker (`TrackingVolumeDefinition::getOriginMarkerId`) instead of a VR-space offset.

- ARKit (iPhone streaming): `ARKitPosePacket` / `ARKitPoseInRTPPayload` (`src/Plugins/MikanARKitVideo/Private/ARKitWireProtocol.h`) carry a row-major `float[16]` camera-to-world transform plus per-frame `fx/fy/cx/cy`. The receiver conversion point is `ARKitVideoSourceComponent::notifyFrameBundleReceived` (`src/Editor/ECS/VideoSource/ARKitVideoSourceComponent.cpp`): a row-major to column-major transpose into `glm::mat4`, deliberately with no axis conversion; ARKit's camera convention (right handed, +Y up, -Z forward) is taken as already matching Mikan's. ARKit reports no distortion, so distorted and undistorted matrices are set equal. The pose is frame-coupled via `IFrameCoupledPoseProvider` rather than polled per tick like VR pucks. That same point is where the marker-solved ARKit-world-to-stage offset is applied, immediately after the transpose and nowhere else ([videosources.md](./videosources.md)).

- Recorded pose tracks (the `*.pose.json` sidecar the phone writes beside a movie or photo) store the same row-major camera-to-world layout in the same ARKit world space. `PoseTrack::readFrame` (`src/Editor/ECS/VideoSource/PoseTrack.cpp`) is their transpose point, again with no axis conversion, and `FileVideoSourceComponent::publishPoseForFrame` is the one place the source's world-to-stage offset is applied to a recorded pose.

---

## Images, textures, and the Y flip

- Video frames flow through the editor as BGR `cv::Mat` buffers with OpenCV's top-left origin (row 0 is the top of the image). `VideoFrameDistortionView::copyOpenCVMatIntoGLTexture` (`src/Editor/Calibration/VideoFrameDistortionView.cpp`) uploads the bytes in row order via `IMkTexture::copyBufferIntoTexture`, so cv row 0 lands at GL texture coordinate v=0.

- GL samples with a bottom-left UV origin, so displaying a cv-ordered texture upright requires a V flip somewhere. Mikan does it in mesh texcoords, chosen per call site: `createFullscreenQuadMesh(ownerContext, vFlipped, ...)` (`src/Libraries/MikanRenderer/Private/GlTriangulatedMesh.cpp`) builds either a flipped or unflipped fullscreen quad, and each app stage picks (calibration stages pass `false`, `AppStage_TextureSourceSettings` passes `true`). There is no single global flip point.

- Every video texture reaches its consumer with image row 0 at v=0, whatever produced it, so no consumer needs to know which source or decode tier a frame came from. A pass that writes a video texture rather than displaying one therefore builds its quad unflipped: `ARKitVideoSourceComponent::processDirectVideoFrame` converts NV12 to RGBA that way, and the V flip stays with the consumer's own display quad. See [videosources.md](./videosources.md).

- Render-to-texture paths can instead flip in projection: `CameraComponent::getApertureProjectionMatrix(outProj, bVerticalFlip)` pre-multiplies `glm::scale(vec3(1, -1, 1))` "to account for OpenGL's inverted Y-axis"; the compositor's `ColorTextureSourceNode` exposes a `vertical_flip` option.

- Geometry drawn into the compositor's working framebuffer takes that flip. The video layers reach that buffer through a v-flipped fullscreen quad, so its rows run opposite to GL's default, and 3d geometry projected the usual way lands mirrored top to bottom against the video. `DrawShapesNode` and `DepthMaskNode` therefore ask for the flipped aperture projection. `DrawLayerNode`'s stencil mask rendering still asks for the unflipped one and is believed to have the same mirroring, unverified because no graph in the project connects a stencil.

- Horizontal mirroring for mirrored sources is separate: `VideoFrameDistortionView::writeVideoFrame` applies `cv::flip(mat, +1)` when the source definition's mirrored flag is set.

- Pixel-to-ray conversion flips image Y once: `computeCameraRayAtPixel` (`CameraMath.cpp`) computes `local_y = (principal_point_y - imagePoint.y) / fy` because image +Y is down while camera-space +Y is up.

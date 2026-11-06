# Proposal: ImGuizmo for the transform gizmo and the view cube

Design decided. All three parts are built. This document is the record of the design: the decisions, what exists today, the pieces in order, and what verifies them. A reader starting from it should not need the conversation that produced it.

## Goal

The project viewport's transform gizmo is built from scene objects: a per-scene `Gizmo` object carrying `GizmoTransformComponent` plus a translate, rotate, and scale component, each hit-tested through generated box and disk colliders and drawn through the line renderer and two arrow meshes in an occluded and a visible pass (`src/Editor/ECS/Editor/Gizmo*`, about 1,700 lines). The view direction is picked from a seven-entry combo over the scene image (Perspective and six axis-aligned orthographic views), and `MikanCamera` keeps a pan and zoom per axis view.

[ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) is an MIT-licensed Dear ImGui extension that draws and drives a translate, rotate, and scale gizmo from a view matrix, a projection matrix, and the manipulated matrix, in both projections, with snapping and a local or world mode. The same library carries a view cube (`ViewManipulate`) that orbits a view matrix by drag and snaps it to a face, edge, or corner by click, and the vector editor that a later curve asset will use. Adopting it replaces the hand-built gizmo with a maintained one, replaces the direction combo with the cube, and gets the library into the build ahead of the curve asset.

## Decisions

- **Upstream submodule.** `thirdparty/ImGuizmo` tracks `CedricGuillemet/ImGuizmo` master, pinned to a commit. No fork until a patch is needed. The repo carries every widget under `src/`, so only `src/ImGuizmo.cpp` compiles now, through an `IMGUIZMO_SOURCE` list in `cmake/ThirdParty.cmake` beside the imgui and imgui-node-editor lists. `ImVectorEditor.cpp` joins the list with the curve asset.

- **Compiled into `MikanGUI`.** The source goes into the DLL the way imgui and imgui-node-editor do, excluded from the unity build. `ImGuizmo.h` decorates its API with `IMGUI_API`, so the DLL's existing dllexport and dllimport defines cover it with no patch. It lives in the same DLL as imgui, so it reads the current ImGui context and `SetImGuiContext` is never called. Its one global context holds the hover and drag flags, which is enough for the one scene view. `ImGuizmo::BeginFrame` runs after every context's `NewFrame`, from `MkGuiScopedUpdate`: it resets the per-frame hover flags and binds the fallback draw list in whichever context is current, so any window could draw a gizmo.

- **Direct calls, no facade.** The scene panel includes `ImGuizmo.h` and calls `Manipulate` and `ViewManipulate` itself, as the node graph files include `imgui_node_editor.h` today. A facade would wrap two calls.

- **The gizmo stops being scene objects.** The `Gizmo` object, the four gizmo components, their generated colliders, the arrow meshes, the handle hit priorities, and the occluded render pass are deleted. A plain `TransformGizmo` class owned by `EditorObjectSystem` keeps the binding: the selection target, the mode, the apply-to-target write, the config listener that resnaps the gizmo when a panel edits the transform, and the bound and unbound notifications on `SelectionComponent` that `RGBSpotLightComponent` and `ModelStencilComponent` listen to. The gizmo special cases in `getSelectedSceneActor` and `getSelectionOutlineTargets` go with the object.

- **Drawn over the scene image, always on top.** `GuiPanel_SceneView::onGui` sets the draw list to its window, the rect to the image, the orthographic flag from the camera, and calls `Manipulate` with the target's world matrix. ImGuizmo draws into the ImGui draw list, so the handles are never occluded and the occluded dimming of the current gizmo is not reproduced. Edits apply per frame through `TransformComponent::setWorldTransform` as today, and the undo gesture brackets on the edges of `ImGuizmo::IsUsing` rather than on the viewport's mouse down and up.

- **One input rule.** While a handle is hovered or dragging (`IsOver`, `IsUsing`) or the cube is hovered or dragging (`IsViewManipulateHovered`, `IsUsingViewManipulate`), `wantsViewportInput` is false, so no pick, ruler, camera drag, or wheel reaches the viewport. The overlay widgets win the way the combo already does.

- **Local and World.** ImGuizmo's `MODE` is exposed through a toggle, default World, which matches today's world-space translate and rotate. Scale is always local. The choice persists in `EditorSettings`.

- **Snapping under the ruler's rule.** Translation snaps to `snapIncrement` when `bSnapEnabled` XOR Shift, the rule `isRulerSnapActive` applies to the ruler. `EditorSettings` gains `rotationSnapDegrees` (15) and `scaleSnapIncrement` (0.1) for the other two modes.

- **A toolbar strip on the scene image.** Icon-font buttons beside the view combo: translate, rotate, scale, Local or World, snap on or off, with localized tooltips. The T, R, and Y keys stay bound through `InputManager`.

- **The combo switches projection only.** Its entries are Perspective and Isometric. Isometric means orthographic; the direction comes from the cube, and a switch keeps the current view direction. The combo stays editor camera only, since compositor cameras carry lens intrinsics.

- **The view cube drives the editor camera.** `ViewManipulate` is drawn in the image's top right for the editor camera in either projection. A face click gives an axis view, an edge or corner click a diagonal, and a drag orbits. Each frame the cube changes the view matrix, the camera takes it back: the fly camera as position plus yaw and pitch (pitch is already clamped to 90 degrees, and the cube keeps +Y up, so the top and bottom faces land at the clamp), the orthographic camera as a forward and up.

- **One orthographic framing.** `MikanCamera` drops `eCameraViewpoint` and the six `MikanOrthoViewState` entries for one orientation (yaw and pitch, +Y up), one target, and one extent. Switching direction keeps target and extent. `EditorCameraState` and the `editor_camera_state` config block change shape to match, and the six `ortho_views` keys of a saved project are ignored on load, leaving the defaults. The ruler, the right-drag pan, and the wheel zoom already read right, up, and forward from the view matrix, so they work at any orientation.

- **The cube's pivot in perspective** is the selected scene actor's world position when there is one, so a face click frames the selection, and otherwise a fixed distance ahead of the camera. In orthographic it is the target.

- **Automation verbs** so the drive and verify loop covers the new state. A `gizmo` namespace reads and sets the mode, the space, and the snap flag. A `camera` namespace reads and sets the projection and the look direction (a face name or a yaw and pitch) and replies the camera state. Handle drags are driven by injected input, which follows cursor position. The cube's drag orbit reads `MouseDelta`, which a warped move reports as zero, so it is checked by hand and the verbs cover the state it would have produced.

- **Three commits, one pull request** on the `imguizmo` branch, each building and passing both test suites and `FormatCheck` on its own.

## What exists today

- `EditorObjectSystem` builds the gizmo per activated scene from `onSceneActivated`, feeds it mouse rays through the collider raycast, and brackets its drags as a `mouse_drag` gesture in `onMouseRayButtonDown` and `onMouseRayButtonUp` ([objects.md](../reference/objects.md), [transactions.md](../reference/transactions.md)).
- `GuiPanel_SceneView` owns the viewport's frame buffer, presents it as an image in ImGui screen space, draws the view combo and the frame rate over it, and claims window input for the viewport through an invisible button while it is hovered or held.
- `AppStage_Project::applyCameraView` maps the combo index onto `MikanCamera::setOrthographicViewpoint` or the perspective fly mode, and `captureCameraState` and `restoreCameraState` round-trip `EditorCameraState` through `EditorObjectSystemDefinition` after 0.75 s of stillness.
- `MikanGUI` compiles imgui and imgui-node-editor from source lists in `cmake/ThirdParty.cmake` ([modules.md](../reference/modules.md), [build.md](../reference/build.md)).

## The pieces, in order

### Part 1: the library in the build

- [x] `thirdparty/ImGuizmo` submodule, `.gitmodules` entry, `IMGUIZMO_DIR` and `IMGUIZMO_SOURCE` in `cmake/ThirdParty.cmake`
- [x] `MikanGUI` compiles the source, adds the include directory, excludes it from the unity build
- [x] `MkGuiScopedUpdate` calls `ImGuizmo::BeginFrame` after `NewFrame`
- [x] layout.md, modules.md

### Part 2: the transform gizmo

- [x] `TransformGizmo` in `src/Editor/ECS/Editor/`, owned by `EditorObjectSystem`, holding the binding and the mode, space, and snap state
- [x] Delete the gizmo components, `GizmoFwd.h`, the collider and mesh creation, the handle priorities, and `renderGizmo`
- [x] `GuiPanel_SceneView` draws the gizmo with `Manipulate` and applies the result to the target each frame, brackets the gesture on `IsUsing`, and reports gizmo capture in `wantsViewportInput`
- [x] Style thicknesses scaled by `MkGui::getUiScale()`, colors from the current gizmo, gizmo size matched by eye to today's
- [x] The toolbar strip and the `EditorSettings` fields for space and the two snap increments, with their localization keys
- [x] The `gizmo` automation namespace
- [x] objects.md selection section, transactions.md, automation.md

### Part 3: the view cube

- [x] `MikanCamera` orthographic mode as one orientation, target, and extent, with `eCameraViewpoint` and the per-view state removed
- [x] `EditorCameraState`, its config keys, `captureCameraState` and `restoreCameraState`
- [x] The combo reduced to Perspective and Isometric, the six view keys removed from the localization sources and the tables regenerated
- [x] `ViewManipulate` in the scene panel, the write-back to the fly and orthographic cameras, the selection pivot, and the cube's share of `wantsViewportInput`
- [x] The `camera` automation namespace
- [x] objects.md camera state paragraph, automation.md

## Verification

- Both test suites and `FormatCheck` per commit.
- A drive through `tools/automate.py`: select a model stencil, set each gizmo mode through the verb, inject a handle drag, read the transform property back, undo once and read it again. Set the projection and each face through the `camera` verbs and read the state back. Tear the Scene panel out into its own OS window and repeat the drag, since the gizmo rect is in ImGui screen space.
- By hand: the cube's drag orbit in both projections, the toolbar, and the Local mode on a rotated object.

## Later

- `ImVectorEditor` for authoring lighting sequences in a new curve asset, which adds its source to `IMGUIZMO_SOURCE` and an editor window around it.

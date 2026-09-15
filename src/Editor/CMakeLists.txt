## Mikan Editor
## =======================
MESSAGE(STATUS "Generating Mikan Editor Project")

# CEF (Chromium Embedded Framework) - set up include path and lib variables
if(DEFINED CEF_ROOT)
  # Normalize path to forward slashes so cef_macros.cmake string expansions don't produce
  # invalid backslash escape sequences (e.g. \G in \Github).
  file(TO_CMAKE_PATH "${CEF_ROOT}" CEF_ROOT)
  list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
  find_package(CEF REQUIRED)
endif()

# Dependencies
set(MIKAN_INCL_DIRS)
set(MIKAN_REQ_LIBS)

list(APPEND MIKAN_REQ_LIBS ${PLATFORM_LIBS})

# AppCore
file(GLOB MIKAN_APP_CORE_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppCore/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppCore/*.h"
)
# The executable entry points are NOT part of the shared editor object library;
# each executable (Mikan / MikanCmd) compiles its own entry point file.
list(REMOVE_ITEM MIKAN_APP_CORE_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppCore/EntryPoint.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppCore/CmdEntryPoint.cpp")
source_group("AppCore" FILES ${MIKAN_APP_CORE_SRC})

# AppStages 
## Root
file(GLOB MIKAN_APP_STAGES_ROOT_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/*.h"
)
source_group("AppStages" FILES ${MIKAN_APP_STAGES_ROOT_SRC})
## AlignmentCalibrationSettings
file(GLOB MIKAN_APP_STAGES_ALIGNMENT_CALIBRATION_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/AlignmentCalibration/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/AlignmentCalibration/*.h"
)
source_group("AppStages\\AlignmentCalibration" FILES ${MIKAN_APP_STAGES_ALIGNMENT_CALIBRATION_SRC})
## AlignCameraByUtilityMarker
file(GLOB MIKAN_APP_STAGES_ALIGN_CAMERA_BY_UTILITY_MARKER_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/AlignCameraByUtilityMarker/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/AlignCameraByUtilityMarker/*.h"
)
source_group("AppStages\\AlignCameraByUtilityMarker" FILES ${MIKAN_APP_STAGES_ALIGN_CAMERA_BY_UTILITY_MARKER_SRC})
## AlignCameraByOriginMarker
file(GLOB MIKAN_APP_STAGES_ALIGN_CAMERA_BY_ORIGIN_MARKER_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/AlignCameraByOriginMarker/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/AlignCameraByOriginMarker/*.h"
)
source_group("AppStages\\AlignCameraByOriginMarker" FILES ${MIKAN_APP_STAGES_ALIGN_CAMERA_BY_ORIGIN_MARKER_SRC})
## AnchorTriangulation
file(GLOB MIKAN_APP_STAGES_ANCHOR_CALIBRATION_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/AnchorTriangulation/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/AnchorTriangulation/*.h"
)
source_group("AppStages\\AnchorTriangulation" FILES ${MIKAN_APP_STAGES_ANCHOR_CALIBRATION_SRC})
## LightFixtureCalibration
file(GLOB MIKAN_APP_STAGES_LIGHT_FIXTURE_CALIBRATION_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/LightFixtureCalibration/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/LightFixtureCalibration/*.h"
)
source_group("AppStages\\LightFixtureCalibration" FILES ${MIKAN_APP_STAGES_LIGHT_FIXTURE_CALIBRATION_SRC})
## SceneLightingCapture
file(GLOB MIKAN_APP_STAGES_SCENE_LIGHTING_CAPTURE_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/SceneLightingCapture/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/SceneLightingCapture/*.h"
)
source_group("AppStages\\SceneLightingCapture" FILES ${MIKAN_APP_STAGES_SCENE_LIGHTING_CAPTURE_SRC})
## DepthMeshCapture
file(GLOB MIKAN_APP_STAGES_DEPTH_MESH_CAPTURE_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/DepthMeshCapture/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/DepthMeshCapture/*.h"
)
source_group("AppStages\\DepthMeshCapture" FILES ${MIKAN_APP_STAGES_DEPTH_MESH_CAPTURE_SRC})
## MainMenu
file(GLOB MIKAN_APP_STAGES_MAIN_MENU_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/MainMenu/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/MainMenu/*.h"
)
source_group("AppStages\\MainMenu" FILES ${MIKAN_APP_STAGES_MAIN_MENU_SRC})
## ModalConfirm
file(GLOB MIKAN_APP_STAGES_MODAL_CONFIRM_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/ModalConfirm/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/ModalConfirm/*.h"
)
## ModalMessageBox
file(GLOB MIKAN_APP_STAGES_MODAL_MESSAGE_BOX_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/ModalMessageBox/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/ModalMessageBox/*.h"
)
source_group("AppStages\\ModalConfirm" FILES ${MIKAN_APP_STAGES_MODAL_CONFIRM_SRC})
source_group("AppStages\\ModalMessageBox" FILES ${MIKAN_APP_STAGES_MODAL_MESSAGE_BOX_SRC})
## ModalSelectCamera
file(GLOB MIKAN_APP_STAGES_MODAL_SELECT_CAMERA_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/ModalSelectCamera/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/ModalSelectCamera/*.h"
)
source_group("AppStages\\ModalSelectCamera" FILES ${MIKAN_APP_STAGES_MODAL_SELECT_CAMERA_SRC})
## ModalSceneAddCompositor
file(GLOB MIKAN_APP_STAGES_MODAL_SCENE_ADD_COMPOSITOR_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/ModalSceneAddCompositor/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/ModalSceneAddCompositor/*.h"
)
source_group("AppStages\\ModalSceneAddCompositor" FILES ${MIKAN_APP_STAGES_MODAL_SCENE_ADD_COMPOSITOR_SRC})
## MonoLensCalibration
file(GLOB MIKAN_APP_STAGES_MONO_LENS_CALIBRATION_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/MonoLensCalibration/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/MonoLensCalibration/*.h"
)
source_group("AppStages\\MonoLensCalibration" FILES ${MIKAN_APP_STAGES_MONO_LENS_CALIBRATION_SRC})
## PointCloudAlignment
file(GLOB MIKAN_APP_STAGES_POINT_CLOUD_ALIGNMENT_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/PointCloudAlignment/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/PointCloudAlignment/*.h"
)
source_group("AppStages\\PointCloudAlignment" FILES ${MIKAN_APP_STAGES_POINT_CLOUD_ALIGNMENT_SRC})
## Project
file(GLOB MIKAN_APP_STAGES_PROJECT_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/Project/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/Project/*.h"
)
source_group("AppStages\\Project" FILES ${MIKAN_APP_STAGES_PROJECT_SRC})
## Shared
file(GLOB MIKAN_APP_STAGES_SHARED_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/Shared/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/Shared/*.h"
)
source_group("AppStages\\Shared" FILES ${MIKAN_APP_STAGES_SHARED_SRC})
## StencilAlignment
file(GLOB MIKAN_APP_STAGES_STENCIL_ALIGNMENT_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/StencilAlignment/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/StencilAlignment/*.h"
)
source_group("AppStages\\StencilAlignment" FILES ${MIKAN_APP_STAGES_STENCIL_ALIGNMENT_SRC})
## TextureSourceSettings
file(GLOB MIKAN_APP_STAGES_TEXTURE_SOURCE_SETTINGS_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/TextureSourceSettings/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/TextureSourceSettings/*.h"
)
source_group("AppStages\\TextureSourceSettings" FILES ${MIKAN_APP_STAGES_TEXTURE_SOURCE_SETTINGS_SRC})
## VideoSourceSettings
file(GLOB MIKAN_APP_STAGES_VIDEO_SOURCE_SETTINGS_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/VideoSourceSettings/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/VideoSourceSettings/*.h"
)
source_group("AppStages\\VideoSourceSettings" FILES ${MIKAN_APP_STAGES_VIDEO_SOURCE_SETTINGS_SRC})
## VRTrackingRecenter
file(GLOB MIKAN_APP_STAGES_VR_TRACKING_RECENTER_SRC
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/VRTrackingRecenter/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/AppStages/VRTrackingRecenter/*.h"
)
source_group("AppStages\\VRTrackingRecenter" FILES ${MIKAN_APP_STAGES_VR_TRACKING_RECENTER_SRC})

## Mikan All AppStage Source
set(MIKAN_APP_STAGES_SRC
    ${MIKAN_APP_STAGES_ROOT_SRC}
    ${MIKAN_APP_STAGES_ALIGNMENT_CALIBRATION_SRC}
    ${MIKAN_APP_STAGES_ALIGN_CAMERA_BY_UTILITY_MARKER_SRC}
    ${MIKAN_APP_STAGES_ALIGN_CAMERA_BY_ORIGIN_MARKER_SRC}
    ${MIKAN_APP_STAGES_ANCHOR_CALIBRATION_SRC}
    ${MIKAN_APP_STAGES_LIGHT_FIXTURE_CALIBRATION_SRC}
    ${MIKAN_APP_STAGES_SCENE_LIGHTING_CAPTURE_SRC}
    ${MIKAN_APP_STAGES_DEPTH_MESH_CAPTURE_SRC}
    ${MIKAN_APP_STAGES_MAIN_MENU_SRC}
    ${MIKAN_APP_STAGES_MODAL_CONFIRM_SRC}
    ${MIKAN_APP_STAGES_MODAL_MESSAGE_BOX_SRC}
    ${MIKAN_APP_STAGES_MODAL_SCENE_ADD_COMPOSITOR_SRC}
    ${MIKAN_APP_STAGES_MODAL_SELECT_CAMERA_SRC}
    ${MIKAN_APP_STAGES_MONO_LENS_CALIBRATION_SRC}
    ${MIKAN_APP_STAGES_POINT_CLOUD_ALIGNMENT_SRC}
    ${MIKAN_APP_STAGES_PROJECT_SRC}
    ${MIKAN_APP_STAGES_SHARED_SRC}
    ${MIKAN_APP_STAGES_STENCIL_ALIGNMENT_SRC}
    ${MIKAN_APP_STAGES_TEXTURE_SOURCE_SETTINGS_SRC}
    ${MIKAN_APP_STAGES_VIDEO_SOURCE_SETTINGS_SRC}
    ${MIKAN_APP_STAGES_VR_TRACKING_RECENTER_SRC}
)

# Asset
file(GLOB MIKAN_ASSET_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Asset/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Asset/*.h"
)
source_group("Asset" FILES ${MIKAN_ASSET_SRC})
## Asset/Import
file(GLOB MIKAN_ASSET_IMPORT_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Asset/Import/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Asset/Import/*.h"
)
source_group("Asset\\Import" FILES ${MIKAN_ASSET_IMPORT_SRC})
## Asset/Export
file(GLOB MIKAN_ASSET_EXPORT_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Asset/Export/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Asset/Export/*.h"
)
source_group("Asset\\Export" FILES ${MIKAN_ASSET_EXPORT_SRC})
## Asset/Test
file(GLOB MIKAN_ASSET_TEST_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Asset/Test/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Asset/Test/*.h"
)
source_group("Asset\\Test" FILES ${MIKAN_ASSET_TEST_SRC})

# Calibration
file(GLOB MIKAN_CALIBRATION_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Calibration/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Calibration/*.h"
)
source_group("Calibration" FILES ${MIKAN_CALIBRATION_SRC})
file(GLOB MIKAN_CALIBRATION_TEST_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Calibration/Test/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Calibration/Test/*.h"
)
source_group("Calibration\\Test" FILES ${MIKAN_CALIBRATION_TEST_SRC})

# Config
file(GLOB MIKAN_CONFIG_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Config/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Config/*.h"
)
source_group("Config" FILES ${MIKAN_CONFIG_SRC})

# Delegates
file(GLOB MIKAN_DELEGATES_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Delegates/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Delegates/*.h"
)
source_group("Delegates" FILES ${MIKAN_DELEGATES_SRC})

# Events
file(GLOB MIKAN_EVENTS_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Events/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Events/*.h"
)
source_group("Events" FILES ${MIKAN_EVENTS_SRC})

# ECS
file(GLOB MIKAN_ECS_ROOT_SRC
    "${CMAKE_CURRENT_LIST_DIR}/ECS/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ECS/*.h"
)
source_group("ECS" FILES ${MIKAN_ECS_ROOT_SRC})
## Anchor
file(GLOB MIKAN_ECS_ANCHOR_SRC
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Anchor/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Anchor/*.h"
)
source_group("ECS\\Anchor" FILES ${MIKAN_ECS_ANCHOR_SRC})
## Camera
file(GLOB MIKAN_ECS_CAMERA_SRC
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Camera/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Camera/*.h"
)
source_group("ECS\\Camera" FILES ${MIKAN_ECS_CAMERA_SRC})
## Collision
file(GLOB MIKAN_ECS_COLLISION_SRC
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Collision/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Collision/*.h"
)
source_group("ECS\\Collision" FILES ${MIKAN_ECS_COLLISION_SRC})
## Compositor
file(GLOB MIKAN_ECS_COMPOSITOR_SRC
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Compositor/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Compositor/*.h"
)
source_group("ECS\\Compositor" FILES ${MIKAN_ECS_COMPOSITOR_SRC})
## Editor
file(GLOB MIKAN_ECS_EDITOR_SRC
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Editor/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Editor/*.h"
)
source_group("ECS\\Editor" FILES ${MIKAN_ECS_EDITOR_SRC})
## Marker
file(GLOB MIKAN_ECS_MARKER_SRC
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Marker/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Marker/*.h"
)
source_group("ECS\\Marker" FILES ${MIKAN_ECS_MARKER_SRC})
## Scene
file(GLOB MIKAN_ECS_SCENE_SRC
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Scene/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Scene/*.h"
)
source_group("ECS\\Scene" FILES ${MIKAN_ECS_SCENE_SRC})
## Stage
file(GLOB MIKAN_ECS_STAGE_SRC
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Stage/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Stage/*.h"
)
source_group("ECS\\Stage" FILES ${MIKAN_ECS_STAGE_SRC})
## Shape
file(GLOB MIKAN_ECS_SHAPE_SRC
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Shape/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Shape/*.h"
)
source_group("ECS\\Shape" FILES ${MIKAN_ECS_SHAPE_SRC})
## Stencil
file(GLOB MIKAN_ECS_STENCIL_SRC
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Stencil/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Stencil/*.h"
)
source_group("ECS\\Stencil" FILES ${MIKAN_ECS_STENCIL_SRC})
## Texture Source
file(GLOB MIKAN_ECS_TEXTURE_SOURCE_SRC
    "${CMAKE_CURRENT_LIST_DIR}/ECS/TextureSource/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ECS/TextureSource/*.h"
)
source_group("ECS\\TextureSource" FILES ${MIKAN_ECS_TEXTURE_SOURCE_SRC})
## Script
file(GLOB MIKAN_ECS_SCRIPT_SRC
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Script/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Script/*.h"
)
source_group("ECS\\Script" FILES ${MIKAN_ECS_SCRIPT_SRC})
## TrackingMount
file(GLOB MIKAN_ECS_TRACKINGMOUNT_SRC
    "${CMAKE_CURRENT_LIST_DIR}/ECS/TrackingMount/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ECS/TrackingMount/*.h"
)
source_group("ECS\\TrackingMount" FILES ${MIKAN_ECS_TRACKINGMOUNT_SRC})
## TrackingVolume
file(GLOB MIKAN_ECS_TRACKINGVOLUME_SRC
    "${CMAKE_CURRENT_LIST_DIR}/ECS/TrackingVolume/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ECS/TrackingVolume/*.h"
)
source_group("ECS\\TrackingVolume" FILES ${MIKAN_ECS_TRACKINGVOLUME_SRC})
## VRObject
file(GLOB MIKAN_ECS_VROBJECT_SRC
    "${CMAKE_CURRENT_LIST_DIR}/ECS/VRObject/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ECS/VRObject/*.h"
)
source_group("ECS\\VRObject" FILES ${MIKAN_ECS_VROBJECT_SRC})
## Light
file(GLOB MIKAN_ECS_LIGHT_SRC
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Light/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ECS/Light/*.h"
)
source_group("ECS\\Light" FILES ${MIKAN_ECS_LIGHT_SRC})
## Video Source
file(GLOB MIKAN_ECS_VIDEO_SOURCE_SRC
    "${CMAKE_CURRENT_LIST_DIR}/ECS/VideoSource/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ECS/VideoSource/*.h"
)
source_group("ECS\\VideoSource" FILES ${MIKAN_ECS_VIDEO_SOURCE_SRC})
## ECS Full Source
set(MIKAN_ECS_SRC
    ${MIKAN_ECS_ROOT_SRC}
    ${MIKAN_ECS_ANCHOR_SRC}
    ${MIKAN_ECS_CAMERA_SRC}
    ${MIKAN_ECS_COLLISION_SRC}
    ${MIKAN_ECS_COMPOSITOR_SRC}
    ${MIKAN_ECS_EDITOR_SRC}
    ${MIKAN_ECS_MARKER_SRC}
    ${MIKAN_ECS_SCENE_SRC}
    ${MIKAN_ECS_SHAPE_SRC}
    ${MIKAN_ECS_STAGE_SRC}
    ${MIKAN_ECS_STENCIL_SRC}
    ${MIKAN_ECS_TEXTURE_SOURCE_SRC}
    ${MIKAN_ECS_SCRIPT_SRC}
    ${MIKAN_ECS_TRACKINGMOUNT_SRC}
    ${MIKAN_ECS_TRACKINGVOLUME_SRC}
    ${MIKAN_ECS_VROBJECT_SRC}
    ${MIKAN_ECS_VIDEO_SOURCE_SRC}
    ${MIKAN_ECS_LIGHT_SRC}
)

# Interprocess
file(GLOB MIKAN_INTERPROCESS_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Interprocess/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Interprocess/*.h"
)
source_group("Interprocess" FILES ${MIKAN_INTERPROCESS_SRC})

# Localization
file(GLOB MIKAN_LOCALIZATION_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Localization/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Localization/*.h"
)
source_group("Localization" FILES ${MIKAN_LOCALIZATION_SRC})

file(GLOB MIKAN_LOCALIZATION_TEST_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Localization/Test/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Localization/Test/*.h"
)
source_group("Localization\\Test" FILES ${MIKAN_LOCALIZATION_TEST_SRC})

# Math
file(GLOB MIKAN_MATH_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Math/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Math/*.h"
)
source_group("Math" FILES ${MIKAN_MATH_SRC})

# Node Editor
## Root
file(GLOB MIKAN_NODE_EDITORS_ROOT_SRC
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/*.h"
)
source_group("NodeEditors" FILES ${MIKAN_NODE_EDITORS_ROOT_SRC})
## DataSources
file(GLOB MIKAN_NODE_EDITORS_DATASOURCES_SRC
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/DataSources/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/DataSources/*.h"
)
source_group("NodeEditors\\DataSources" FILES ${MIKAN_NODE_EDITORS_DATASOURCES_SRC})
## Graphs
file(GLOB MIKAN_NODE_EDITORS_GRAPHS_SRC
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/Graphs/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/Graphs/*.h"
)
source_group("NodeEditors\\Graphs" FILES ${MIKAN_NODE_EDITORS_GRAPHS_SRC})
## Nodes
file(GLOB MIKAN_NODE_EDITORS_NODES_SRC
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/Nodes/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/Nodes/*.h"
)
source_group("NodeEditors\\Nodes" FILES ${MIKAN_NODE_EDITORS_NODES_SRC})
## Material Nodes
file(GLOB MIKAN_NODE_EDITORS_MATERIAL_NODES_SRC
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/Nodes/Material/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/Nodes/Material/*.h"
)
source_group("NodeEditors\\Nodes\\Material" FILES ${MIKAN_NODE_EDITORS_MATERIAL_NODES_SRC})
## Material Compiler
file(GLOB MIKAN_NODE_EDITORS_MATERIAL_COMPILER_SRC
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/MaterialCompiler/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/MaterialCompiler/*.h"
)
source_group("NodeEditors\\MaterialCompiler" FILES ${MIKAN_NODE_EDITORS_MATERIAL_COMPILER_SRC})
## Tests
file(GLOB MIKAN_NODE_EDITORS_TEST_SRC
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/Test/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/Test/*.h"
)
source_group("NodeEditors\\Test" FILES ${MIKAN_NODE_EDITORS_TEST_SRC})
## Pins
file(GLOB MIKAN_NODE_EDITORS_PINS_SRC
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/Pins/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/Pins/*.h"
)
source_group("NodeEditors\\Pins" FILES ${MIKAN_NODE_EDITORS_PINS_SRC})
## Properties
file(GLOB MIKAN_NODE_EDITORS_PROPERTIES_SRC
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/Properties/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/Properties/*.h"
)
source_group("NodeEditors\\Properties" FILES ${MIKAN_NODE_EDITORS_PROPERTIES_SRC})

## Windows
file(GLOB MIKAN_NODE_EDITORS_WINDOWS_SRC
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/Windows/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/NodeEditors/Windows/*.h"
)
source_group("NodeEditors\\Windows" FILES ${MIKAN_NODE_EDITORS_WINDOWS_SRC})
## Node Editor Full Source
set(MIKAN_NODE_EDITORS_SRC
    ${MIKAN_NODE_EDITORS_ROOT_SRC}
    ${MIKAN_NODE_EDITORS_DATASOURCES_SRC}
    ${MIKAN_NODE_EDITORS_GRAPHS_SRC}
    ${MIKAN_NODE_EDITORS_NODES_SRC}
    ${MIKAN_NODE_EDITORS_MATERIAL_NODES_SRC}
    ${MIKAN_NODE_EDITORS_MATERIAL_COMPILER_SRC}
    ${MIKAN_NODE_EDITORS_TEST_SRC}
    ${MIKAN_NODE_EDITORS_PINS_SRC}
    ${MIKAN_NODE_EDITORS_PROPERTIES_SRC}
    ${MIKAN_NODE_EDITORS_WINDOWS_SRC}
)

# OpenCV
file(GLOB MIKAN_OPENCV_SRC
    "${CMAKE_CURRENT_LIST_DIR}/OpenCV/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/OpenCV/*.h"
)
source_group("OpenCV" FILES ${MIKAN_OPENCV_SRC})

# Project
file(GLOB MIKAN_PROJECT_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Project/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Project/*.h"
)
source_group("Project" FILES ${MIKAN_PROJECT_SRC})

# Renderer
file(GLOB MIKAN_RENDERER_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Renderer/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Renderer/*.h"
)
source_group("Renderer" FILES ${MIKAN_RENDERER_SRC})

# Scripting
file(GLOB MIKAN_SCRIPTING_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Scripting/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Scripting/*.h"
)
source_group("Scripting" FILES ${MIKAN_SCRIPTING_SRC})

# Server
file(GLOB MIKAN_SERVER_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Server/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Server/*.h"
)
source_group("Server" FILES ${MIKAN_SERVER_SRC})
file(GLOB MIKAN_SERVER_TEST_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Server/Test/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Server/Test/*.h"
)
## Project/Test
file(GLOB MIKAN_PROJECT_TEST_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Project/Test/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Project/Test/*.h"
)
source_group("Project\\Test" FILES ${MIKAN_PROJECT_TEST_SRC})
source_group("Server\\Test" FILES ${MIKAN_SERVER_TEST_SRC})

# Transactions
file(GLOB MIKAN_TRANSACTIONS_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Transactions/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Transactions/*.h"
)
source_group("Transactions" FILES ${MIKAN_TRANSACTIONS_SRC})

# TinyFileDialogs's source files are included in directly, rather than linking against a lib
source_group("TinyFileDialogs" FILES ${TINYFILEDIALOGS_SOURCE})

# Mikan Executable
set(MIKAN_SRC
    ${MIKAN_APP_CORE_SRC}
    ${MIKAN_APP_STAGES_SRC}
    ${MIKAN_ASSET_SRC}
    ${MIKAN_ASSET_IMPORT_SRC}
    ${MIKAN_ASSET_EXPORT_SRC}
    ${MIKAN_ASSET_TEST_SRC}
    ${MIKAN_CALIBRATION_SRC}
    ${MIKAN_CALIBRATION_TEST_SRC}
    ${MIKAN_COMPONENTS_SRC}
    ${MIKAN_CONFIG_SRC}
    ${MIKAN_DELEGATES_SRC}
    ${MIKAN_EVENTS_SRC}
    ${MIKAN_ECS_SRC}
    ${MIKAN_INTERPROCESS_SRC}
    ${MIKAN_LOCALIZATION_SRC}
    ${MIKAN_LOCALIZATION_TEST_SRC}
    ${MIKAN_MATH_SRC}
    ${MIKAN_NODE_EDITORS_SRC}
    ${MIKAN_OBJECT_SRC}
    ${MIKAN_OPENCV_SRC}
    ${MIKAN_PROJECT_SRC}
    ${MIKAN_PROJECT_TEST_SRC}
    ${MIKAN_RENDERER_SRC}
    ${MIKAN_SCRIPTING_SRC}
    ${MIKAN_SERVER_SRC}
    ${MIKAN_SERVER_TEST_SRC}
    ${MIKAN_SYSTEM_SRC}
    ${MIKAN_TRANSACTIONS_SRC}
    ${TINYFILEDIALOGS_SOURCE}
)

list(APPEND MIKAN_INCL_DIRS
  ${CMAKE_CURRENT_LIST_DIR}/AppCore
  ${CMAKE_CURRENT_LIST_DIR}/AppStages
  ${CMAKE_CURRENT_LIST_DIR}/Asset
  ${CMAKE_CURRENT_LIST_DIR}/Asset/Import
  ${CMAKE_CURRENT_LIST_DIR}/Asset/Export
  ${CMAKE_CURRENT_LIST_DIR}/Asset/Test
  ${CMAKE_CURRENT_LIST_DIR}/Calibration
  ${CMAKE_CURRENT_LIST_DIR}/Calibration/Test
  ${ROOT_DIR}/src/Programs/Tests/UnitTests
  ${CMAKE_CURRENT_LIST_DIR}/Config
  ${CMAKE_CURRENT_LIST_DIR}/Delegates
  ${CMAKE_CURRENT_LIST_DIR}/Events
  ${CMAKE_CURRENT_LIST_DIR}/ECS
  ${CMAKE_CURRENT_LIST_DIR}/ECS/Anchor
  ${CMAKE_CURRENT_LIST_DIR}/ECS/Camera
  ${CMAKE_CURRENT_LIST_DIR}/ECS/Collision
  ${CMAKE_CURRENT_LIST_DIR}/ECS/Compositor
  ${CMAKE_CURRENT_LIST_DIR}/ECS/Editor
  ${CMAKE_CURRENT_LIST_DIR}/ECS/Marker
  ${CMAKE_CURRENT_LIST_DIR}/ECS/Scene
  ${CMAKE_CURRENT_LIST_DIR}/ECS/Shape
  ${CMAKE_CURRENT_LIST_DIR}/ECS/Stage
  ${CMAKE_CURRENT_LIST_DIR}/ECS/Stencil
  ${CMAKE_CURRENT_LIST_DIR}/ECS/TextureSource
  ${CMAKE_CURRENT_LIST_DIR}/ECS/Script
  ${CMAKE_CURRENT_LIST_DIR}/ECS/TrackingMount
  ${CMAKE_CURRENT_LIST_DIR}/ECS/TrackingVolume
  ${CMAKE_CURRENT_LIST_DIR}/ECS/VRObject
  ${CMAKE_CURRENT_LIST_DIR}/ECS/VideoSource
  ${CMAKE_CURRENT_LIST_DIR}/ECS/Light
  ${MIKAN_LIBRARIES_DIR}/MikanDMX/Public
  ${MIKAN_LIBRARIES_DIR}/MikanOnnx/Public
  ${CMAKE_CURRENT_LIST_DIR}/Interprocess
  ${CMAKE_CURRENT_LIST_DIR}/Localization
  ${CMAKE_CURRENT_LIST_DIR}/Localization/Test
  ${CMAKE_CURRENT_LIST_DIR}/Math
  ${CMAKE_CURRENT_LIST_DIR}/NodeEditors
  ${CMAKE_CURRENT_LIST_DIR}/NodeEditors/Test
  ${CMAKE_CURRENT_LIST_DIR}/OpenCV
  ${CMAKE_CURRENT_LIST_DIR}/Project
  ${CMAKE_CURRENT_LIST_DIR}/Renderer
  ${CMAKE_CURRENT_LIST_DIR}/Server
  ${CMAKE_CURRENT_LIST_DIR}/Server/Test
  ${CMAKE_CURRENT_LIST_DIR}/Project/Test
  ${CMAKE_CURRENT_LIST_DIR}/Scripting
  ${CMAKE_CURRENT_LIST_DIR}/Transactions
  ${MIKAN_LIBRARIES_DIR}/MikanClientAPI/Public
  ${MIKAN_LIBRARIES_DIR}/MikanClientCore/Public
  ${MIKAN_LIBRARIES_DIR}/MikanCoreApp/Public
  ${MIKAN_LIBRARIES_DIR}/MikanGUI/Public
  ${MIKAN_LIBRARIES_DIR}/MikanMath/Public
  ${MIKAN_LIBRARIES_DIR}/MikanObject/Public
  ${MIKAN_LIBRARIES_DIR}/MikanWindow/Public
  ${MIKAN_LIBRARIES_DIR}/MikanRenderer/Public
  ${MIKAN_LIBRARIES_DIR}/MikanSerialization/Public
  ${MIKAN_LIBRARIES_DIR}/MikanSharedTexture/Public
  ${MIKAN_LIBRARIES_DIR}/MikanUtility/Public
  ${MIKAN_PLUGINS_DIR}/MikanGStreamer/Public
  ${RFK_GENERATED_ROOT_DIR}/MikanClientCore
  ${RFK_GENERATED_ROOT_DIR}/MikanClientAPI
  ${RFK_GENERATED_ROOT_DIR}/MikanSerialization
  ${IMGUI_DIR}
  ${IMGUI_NODE_EDITOR_DIR}
  ${IXWEBSOCKET_INCLUDE_DIR}
  ${LOCKFREEQUEUE_INCLUDE_DIR}
  ${LIBHARU_INCLUDE_DIRS}
  ${RFK_INCLUDE_DIR}
  ${ROOT_DIR}/thirdparty/glm/
  ${CONFIGURU_INCLUDE_DIR}
  ${NLOHMANN_JSON_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIR}
  ${OPENVR_INCLUDE_DIR}
  ${STB_INCLUDE_DIRS}
  ${TINYFILEDIALOGS_DIR}
  ${GLEW_INCLUDE_DIRS}
  ${FastCSV_INCLUDE_DIRS}
  ${SPOUT2_INCLUDE_DIRS}
  ${CEF_INCLUDE_PATH}
  ${LUA_INCLUDE_DIRS}
  ${LUA_BRIDGE_INCLUDE_DIRS}
  ${ROOT_DIR}/thirdparty/lrdb/include
  ${FAST_OBJ_LOADER_INCLUDE_DIRS}
  ${libharu_SOURCE_DIR}/include
  ${libharu_BINARY_DIR}/include)

list(APPEND MIKAN_REQ_LIBS
  ${OpenCV_LIBS}
  ${OPENVR_LIBRARY}
  ${MIKAN_EXTRA_LIBS}
  ${GLEW_LIBRARIES}
  ${SPOUT2_LIBRARIES}
  libcef_dll_wrapper
  # CEF ships only Debug and Release binaries. Map every non-Debug config
  # (Release, RelWithDebInfo, MinSizeRel) onto the Release libcef.lib so the
  # wrapper's C API symbols (e.g. cef_string_utf16_clear) resolve.
  "$<$<CONFIG:Debug>:${CEF_LIB_DEBUG}>"
  "$<$<NOT:$<CONFIG:Debug>>:${CEF_LIB_RELEASE}>"
  crypt32
  wintrust
  shlwapi
  ${LIBHARU_LIBRARIES}
  ${LUA_LIBRARIES}
  ${RFK_LIBRARIES}
  dylib
  easy_profiler
  fast_obj_lib
  ixwebsocket
  MikanClientAPI
  MikanClientCore
  MikanCoreApp
  MikanDMX
  MikanGUI
  MikanMath
  MikanOnnx
  MikanRenderer
  MikanSerialization
  MikanSharedTexture
  MikanUtility
  MikanWindow)

# Remove problematic files from the unity builds (header file inclusion ordering issues)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/Device/Enumerator/OpenCVCameraEnumerator.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
# Carries the editor's stb_image / stb_truetype implementations
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/ECS/Light/DMXSequenceContent.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/Interprocess/AutomationSocket.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/Interprocess/WebsocketInterprocessMessageServer.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/Interprocess/HttpInterprocessMessageServer.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/Interprocess/SharedTextureReader.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/Interprocess/SpoutLogRelay.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

# All editor code is compiled once into a shared OBJECT library that both the
# GUI executable (Mikan) and the command-line tool (MikanCmd) link.
# Usage requirements are PUBLIC so they both compile the library sources and
# propagate to each executable's entry-point translation unit.
add_library(MikanEditor OBJECT ${MIKAN_SRC})
target_include_directories(MikanEditor PUBLIC ${MIKAN_INCL_DIRS})
target_link_libraries(MikanEditor PUBLIC ${MIKAN_REQ_LIBS})
target_compile_definitions(MikanEditor PUBLIC ENABLE_SERIALIZATION_REFLECTION)
target_compile_definitions(MikanEditor PUBLIC ENABLE_MIKANCORE_REFLECTION)
target_compile_definitions(MikanEditor PUBLIC ENABLE_MIKANAPI_REFLECTION)
target_compile_definitions(MikanEditor PUBLIC ENABLE_GL_PROGRAM_CONFIG)
target_compile_definitions(MikanEditor PUBLIC JSON_DISABLE_ENUM_SERIALIZATION=1)
set_target_properties(MikanEditor PROPERTIES FOLDER MikanLibraries)

# GUI application (WinMain entry point)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
set(APP_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/winapp_resources.rc")
add_executable(Mikan WIN32 "${CMAKE_CURRENT_LIST_DIR}/AppCore/EntryPoint.cpp" ${APP_RESOURCE_WINDOWS})
# Embed the application manifest so Windows reports the real OS version to all
# CEF subprocess instances (which reuse Mikan.exe as the helper process).
# Without this, base::win::OSInfo returns build 9200 (Win 8.1 compat shim)
# instead of the real build, causing a DCHECK crash in the CEF media stack.
target_link_options(Mikan PRIVATE
    "/MANIFEST:EMBED"
    "/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/Mikan.exe.manifest"
)
ELSE() #Linux/Darwin
add_executable(Mikan "${CMAKE_CURRENT_LIST_DIR}/AppCore/EntryPoint.cpp")
ENDIF()
target_link_libraries(Mikan PRIVATE MikanEditor)

# Command-line tool (console subsystem, main() entry point).
# Links the same editor object code but uses a stripped-down CmdApp that only
# inits the logger + runs commands
add_executable(MikanCmd "${CMAKE_CURRENT_LIST_DIR}/AppCore/CmdEntryPoint.cpp")
target_link_libraries(MikanCmd PRIVATE MikanEditor)
# Mikan and MikanCmd share an output directory, so only Mikan copies the runtime
# DLLs (see copy_mikan_runtime_deps below). MikanCmd depends on Mikan so those
# copies are present when MikanCmd is built, and so the two targets' post-build
# steps never run concurrently and race to copy the same files.
add_dependencies(MikanCmd Mikan)

# Runtime dependencies
set(SDL2_DLL ${ROOT_DIR}/deps/SDL2-2.30.10/lib/x64/SDL2.dll)
set(SDL2_TTF_DLL_FOLDER ${ROOT_DIR}/deps/SDL2_ttf-2.24.0/lib/x64)
set(EASY_PROFILER_FOLDER ${ROOT_DIR}/deps/easy_profiler/bin)
set(OPENVR_DLL ${ROOT_DIR}/thirdparty/openvr/bin/win64/openvr_api.dll)

# Post build - copy runtime dependencies to binary build folder (for debugging).
# Wrapped in a function so both Mikan and MikanCmd (which share an output dir and
# link the same runtime DLLs) each receive the dependencies and stay independently
# runnable.
function(copy_mikan_runtime_deps mikan_target)
  set_property(TARGET ${mikan_target} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "../../..")

  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:MikanClientAPI>/MikanClientAPI.dll $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:MikanClientCore>/MikanClientCore.dll $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:MikanCoreApp>/MikanCoreApp.dll $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:MikanGUI>/MikanGUI.dll $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:MikanMath>/MikanMath.dll $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:MikanRenderer>/MikanRenderer.dll $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:MikanWindow>/MikanWindow.dll $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:MikanSharedTexture>/MikanSharedTexture.dll $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:MikanSerialization>/MikanSerialization.dll $<TARGET_FILE_DIR:${mikan_target}>)
  set( MIKANSERIALIZATION_PDB_COPY_CMD
    ${CMAKE_COMMAND} -E
    copy
    "$<TARGET_FILE_DIR:MikanSerialization>/$<TARGET_FILE_BASE_NAME:MikanSerialization>.pdb"
    "$<TARGET_FILE_DIR:${mikan_target}>")
  add_custom_command( TARGET ${mikan_target} POST_BUILD
    COMMAND "$<$<CONFIG:Debug,RelWithDebInfo>:${MIKANSERIALIZATION_PDB_COPY_CMD}>"
    COMMAND_EXPAND_LISTS)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:MikanUtility>/MikanUtility.dll $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:MikanWindow>/MikanWindow.dll $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${OpenCV_DIR}/x64/vc16/bin/opencv_world4100d.dll $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${OpenCV_DIR}/x64/vc16/bin/opencv_world4100.dll $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${OpenCV_DIR}/x64/vc16/bin/opencv_videoio_ffmpeg4100_64.dll $<TARGET_FILE_DIR:${mikan_target}>)
  if(MIKAN_WITH_GSTREAMER)
    add_custom_command(
      TARGET ${mikan_target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:MikanGStreamerVideo>/MikanGStreamerVideo.dll $<TARGET_FILE_DIR:${mikan_target}>)
    add_custom_command(
      TARGET ${mikan_target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:MikanARKitVideo>/MikanARKitVideo.dll $<TARGET_FILE_DIR:${mikan_target}>)
  endif()
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:MikanSteamVR>/MikanSteamVR.dll $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${OPENVR_DLL} $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:MikanWMFVideo>/MikanWMFVideo.dll $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${SDL2_DLL} $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${SDL2_TTF_DLL_FOLDER}/SDL2_ttf.dll"
    $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${GLEW_SHARED_LIBRARIES} $<TARGET_FILE_DIR:${mikan_target}>)
  # ONNX Runtime + DirectML (scene lighting estimator)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${ONNXRUNTIME_SHARED_LIBRARIES}
    $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${SPOUT2_SHARED_LIBRARIES}"
    $<TARGET_FILE_DIR:${mikan_target}>)
  # Copy CEF binary and resource files to build output
  if(DEFINED CEF_ROOT)
    # CEF_BINARY_DIR resolves to ${CEF_ROOT}/$<CONFIG>, but CEF only ships Debug/
    # and Release/ binaries. Map non-Debug configs (incl. RelWithDebInfo) to Release.
    COPY_FILES(${mikan_target} "${CEF_BINARY_FILES}" "$<IF:$<CONFIG:Debug>,${CEF_BINARY_DIR_DEBUG},${CEF_BINARY_DIR_RELEASE}>" "$<TARGET_FILE_DIR:${mikan_target}>")
    COPY_FILES(${mikan_target} "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "$<TARGET_FILE_DIR:${mikan_target}>")
  endif()
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${EASY_PROFILER_FOLDER}/easy_profiler.dll"
    $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
    TARGET ${mikan_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${LUA_SHARED_LIBRARIES}"
    $<TARGET_FILE_DIR:${mikan_target}>)
  add_custom_command(
	TARGET ${mikan_target} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	"${RFK_SHARED_LIBRARIES}"
	$<TARGET_FILE_DIR:${mikan_target}>)
endfunction()

IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  # Mikan and MikanCmd share an output directory, so a single copier is enough;
  # MikanCmd depends on Mikan (above) to guarantee the deps are present and to
  # avoid a parallel-build race copying the same files into the same folder.
  copy_mikan_runtime_deps(Mikan)
ELSE() #Linux/Darwin
ENDIF()

# Install - copy runtime dependencies to dist folder (for release)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows") 
  install(TARGETS Mikan
      RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH}
      LIBRARY DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/lib
      ARCHIVE DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/lib)

  install(TARGETS MikanCmd
      RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH}
      LIBRARY DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/lib
      ARCHIVE DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/lib)

  install(FILES
      "${OpenCV_DIR}/x64/vc16/bin/opencv_world4100d.dll"
      DESTINATION ${MIKAN_ARCH_INSTALL_PATH}
      CONFIGURATIONS Debug)
  install(FILES
      "${OpenCV_DIR}/x64/vc16/bin/opencv_world4100.dll"
      DESTINATION ${MIKAN_ARCH_INSTALL_PATH}
      CONFIGURATIONS Release RelWithDebInfo MinSizeRel)
  install(FILES "${OpenCV_DIR}/x64/vc16/bin/opencv_videoio_ffmpeg4100_64.dll"
      DESTINATION ${MIKAN_ARCH_INSTALL_PATH})

  install(FILES "${SDL2_DLL}"
      DESTINATION ${MIKAN_ARCH_INSTALL_PATH})

  install(FILES "${SPOUT2_SHARED_LIBRARIES}"
      DESTINATION ${MIKAN_ARCH_INSTALL_PATH})
      
  install(FILES "${LUA_SHARED_LIBRARIES}"
      DESTINATION ${MIKAN_ARCH_INSTALL_PATH})
	  
  install(FILES "${RFK_SHARED_LIBRARIES}"
      DESTINATION ${MIKAN_ARCH_INSTALL_PATH})

  install(FILES
      "${SDL2_TTF_DLL_FOLDER}/SDL2_ttf.dll"
      DESTINATION ${MIKAN_ARCH_INSTALL_PATH})

  install(FILES
      "${GLEW_SHARED_LIBRARIES}"
      DESTINATION ${MIKAN_ARCH_INSTALL_PATH})

  install(FILES "${OPENVR_DLL}"
      DESTINATION ${MIKAN_ARCH_INSTALL_PATH})

  install(FILES
      "${EASY_PROFILER_FOLDER}/easy_profiler.dll"
      DESTINATION ${MIKAN_ARCH_INSTALL_PATH})

  install(DIRECTORY ${ROOT_DIR}/resources
    DESTINATION ${MIKAN_ARCH_INSTALL_PATH}
    # *.txt carries the font license files, which must ship alongside the fonts
    # The graph, material, shader, and model sources ship too: a project reads
    # the bundled ones through the asset overlay rather than owning copies
    FILES_MATCHING PATTERN "*.jpg" PATTERN "*.png" PATTERN "*.ttf" PATTERN "*.json" PATTERN "*.txt" PATTERN "*.lua" PATTERN "*.onnx"
      PATTERN "*.compgraph" PATTERN "*.shapegraph" PATTERN "*.matgraph" PATTERN "*.compmat" PATTERN "*.shapemat"
      PATTERN "*.vert" PATTERN "*.frag" PATTERN "*.obj" PATTERN "*.mtl")

  # Install CEF binary and resource files
  if(DEFINED CEF_ROOT)
    foreach(_cef_file ${CEF_BINARY_FILES})
      install(FILES "${CEF_BINARY_DIR_RELEASE}/${_cef_file}"
          DESTINATION ${MIKAN_ARCH_INSTALL_PATH})
    endforeach()
    foreach(_cef_file ${CEF_RESOURCE_FILES})
      if(IS_DIRECTORY "${CEF_RESOURCE_DIR}/${_cef_file}")
        install(DIRECTORY "${CEF_RESOURCE_DIR}/${_cef_file}"
            DESTINATION ${MIKAN_ARCH_INSTALL_PATH})
      else()
        install(FILES "${CEF_RESOURCE_DIR}/${_cef_file}"
            DESTINATION ${MIKAN_ARCH_INSTALL_PATH})
      endif()
    endforeach()
  endif()

ELSE() #Linux/Darwin
ENDIF()
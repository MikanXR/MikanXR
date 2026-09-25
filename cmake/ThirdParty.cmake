include(FindPackageHandleStandardArgs)
include(FetchContent)

if (POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

if (POLICY CMP0122)
	cmake_policy(SET CMP0122 NEW)
endif()

if (POLICY CMP0146)
	cmake_policy(SET CMP0146 OLD)
endif()

# When not using MSVC, we recommend using system-wide libraries
# (installed via homebrew on Mac or apt-get in Linux/Ubuntu)
# In MSVC, the InitialSetup batch script downloads pre-build binary packages for OpenCV and SDL

# Required core libraries on various platforms
set(MIKAN_EXTRA_LIBS "")
if (WIN32) 
  list(APPEND MIKAN_EXTRA_LIBS opengl32 mfplat mf mfuuid shlwapi winmm wsock32 ws2_32)
elseif (APPLE)
  find_library(cocoa_library Cocoa)
  find_library(opengl_library OpenGL)
  find_library(corevideo_library CoreVideo)
  find_library(iokit_library IOKit)
  list(APPEND MIKAN_EXTRA_LIBS ${cocoa_library} ${opengl_library} ${corevideo_library} ${iokit_library})
  list(APPEND NNGUI_EXTRA_SOURCE darwin.mm)
elseif(CMAKE_SYSTEM MATCHES "Linux")
  list(APPEND MIKAN_EXTRA_LIBS GL Xxf86vm Xrandr Xinerama Xcursor Xi X11 pthread dl rt)
endif()

# Configuru
set (CONFIGURU_INCLUDE_DIR ${ROOT_DIR}/thirdparty/Configuru)

FetchContent_Declare(
    dylib
    GIT_REPOSITORY "https://github.com/martin-olivier/dylib"
    GIT_TAG        "v2.2.1"
)
FetchContent_MakeAvailable(dylib)

# OpenCV
# Override by adding "-DOpenCV_DIR=C:\path\to\opencv\build" to your cmake command
find_package(OpenCV REQUIRED)

# OpenVR
# Override by adding -DOPENVR_ROOT_DIR=... -DOPENVR_HEADERS_ROOT_DIR=...
find_package(OpenVR REQUIRED)

# SDL
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2TTF REQUIRED)

# GL Extension Wrangler (GLEW)
if (WIN32) 
  set (GLEW_INCLUDE_DIRS ${ROOT_DIR}/deps/glew-2.2.0/include)
  set (GLEW_LIBRARIES ${ROOT_DIR}/deps/glew-2.2.0/lib/Release/x64/glew32.lib)
  set (GLEW_SHARED_LIBRARIES ${ROOT_DIR}/deps/glew-2.2.0/bin/Release/x64/glew32.dll)
else()
  find_package(GLEW REQUIRED)
endif()

# libharu (PDF writer)
set (LIBHARU_INCLUDE_DIRS ${ROOT_DIR}/deps/libharu-2.4.5-static/include)
set (LIBHARU_LIBRARIES
  ${ROOT_DIR}/deps/libharu-2.4.5-static/lib/hpdf.lib
  ${ROOT_DIR}/deps/libharu-2.4.5-static/lib/zlibstatic.lib
  ${ROOT_DIR}/deps/libharu-2.4.5-static/lib/libpng16_static.lib
)

# Refureku
# Built from source out of the submodule rather than consumed as a prebuilt package. The
# submodule's own Refureku/CMakeLists.txt is deliberately skipped and its two children are
# added directly: that file hardcodes the archive/library/runtime output directories to
# ${CMAKE_BINARY_DIR}/Bin and /Lib, which would put the subtree's output on top of the
# flattened build/bin CI configures. Adding Generator/ and Library/ directly leaves the
# directories below in control. It also skips the submodule root's include(CTest) and its
# Dist target, which exists to assemble the prebuilt package this replaced.
set (RFK_DIR ${ROOT_DIR}/thirdparty/Refureku/Refureku)
set (RFK_INCLUDE_DIR ${RFK_DIR}/Library/Include/Public)
set (RFK_GENERATED_ROOT_DIR ${ROOT_DIR}/build/RfkGenerated)

if (NOT EXISTS "${RFK_DIR}/Library/CMakeLists.txt")
	message(FATAL_ERROR
		"Refureku sources are missing. Run: git submodule update --init --recursive thirdparty/Refureku")
endif()

# This file is include()d, not add_subdirectory()d, so it shares the root's variable scope
# and anything set here reaches every Mikan target. Save what the Refureku subtree needs
# overridden and put it back afterwards.
set (RFK_SAVED_BUILD_TESTING "${BUILD_TESTING}")
set (RFK_SAVED_UNITY_BUILD "${CMAKE_UNITY_BUILD}")
set (RFK_SAVED_CXX_STANDARD "${CMAKE_CXX_STANDARD}")
set (RFK_SAVED_ARCHIVE_OUT "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
set (RFK_SAVED_LIBRARY_OUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
set (RFK_SAVED_RUNTIME_OUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# Kodgen declares cxx_std_17 as a floor, not a pin, so cmake/Environment.cmake's
# CMAKE_CXX_STANDARD 20 would otherwise reach it, and its vendored toml11 uses
# std::result_of, which C++20 removed. 17 is also what the prebuilt package this replaced
# was compiled as, so the Refureku objects Mikan links against are unchanged.
set (CMAKE_CXX_STANDARD 17)

# Both local generation and CI configure with CMAKE_UNITY_BUILD=ON, which as a cache entry
# would otherwise reach the submodule's targets too. Kodgen's vendored toml11 does not
# compile when its translation units are concatenated.
set (CMAKE_UNITY_BUILD OFF)

# Kodgen's CMakeLists includes CTest and adds its Examples and Tests subdirectories
# unconditionally. EXCLUDE_FROM_ALL below keeps those targets out of the build; this keeps
# Refureku's own test tree from being configured at all.
set (BUILD_TESTING OFF)

# Where the generator and the reflection runtime land. Kodgen's own CMakeLists overrides
# these again in its scope, which the staging target below compensates for.
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/rfk/lib)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/rfk/bin)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/rfk/bin)

# EXCLUDE_FROM_ALL: only Refureku (linked in) and RefurekuGenerator (an add_dependencies
# target of every *Reflection step) are wanted. LibraryGenerator regenerates Refureku's own
# reflection and the Kodgen examples and tests are internal to the submodule, so none of
# them should cost a build here.
add_subdirectory(${RFK_DIR}/Generator ${CMAKE_BINARY_DIR}/rfk/Generator EXCLUDE_FROM_ALL)
add_subdirectory(${RFK_DIR}/Library ${CMAKE_BINARY_DIR}/rfk/Library EXCLUDE_FROM_ALL)

# Kodgen's CMakeLists sets its own output directories and copies libclang.dll and
# vswhere.exe beside them, in a different folder from RefurekuGenerator.exe.
# The generator loads libclang at startup and shells out to vswhere to locate the MSVC toolchain,
# and without vswhere it fails with "ParsingSettings::compilerExeName must be set to parse
# files" even though the toml sets it.
# Create a custom target to stage both beside RefurekuGenerator instead. The staging is its
# own target because a POST_BUILD command can only be attached to a target declared in the
# same directory, and RefurekuGenerator comes from the subdirectory added above.
# The target is declared on every platform so the *Reflection steps can depend on it
# unconditionally. Only Windows has anything to stage: elsewhere libclang is a system
# library the loader finds on its own, and vswhere is specific to Visual Studio.
add_custom_target(RefurekuGeneratorRuntime
	COMMENT "Staging the generator's runtime dependencies")
add_dependencies(RefurekuGeneratorRuntime RefurekuGenerator)

if (WIN32)
	set (RFK_KODGEN_THIRDPARTY ${RFK_DIR}/Generator/ThirdParty/Kodgen/Kodgen/ThirdParty)
	add_custom_command(TARGET RefurekuGeneratorRuntime POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${RFK_KODGEN_THIRDPARTY}/x64/Shared/libclang.dll" "$<TARGET_FILE_DIR:RefurekuGenerator>"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${RFK_KODGEN_THIRDPARTY}/x64/Bin/vswhere.exe" "$<TARGET_FILE_DIR:RefurekuGenerator>")
endif()

set (BUILD_TESTING "${RFK_SAVED_BUILD_TESTING}")
set (CMAKE_UNITY_BUILD "${RFK_SAVED_UNITY_BUILD}")
set (CMAKE_CXX_STANDARD "${RFK_SAVED_CXX_STANDARD}")
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${RFK_SAVED_ARCHIVE_OUT}")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${RFK_SAVED_LIBRARY_OUT}")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${RFK_SAVED_RUNTIME_OUT}")

set (RFK_LIBRARIES Refureku)
set (RFK_GENERATOR_EXE $<TARGET_FILE:RefurekuGenerator>)
set (RFK_SHARED_LIBRARIES $<TARGET_FILE:Refureku>)

# Keep the submodule's targets out of the solution's top level
foreach (rfk_target Refureku RefurekuGenerator RefurekuGeneratorRuntime Kodgen LibraryGenerator
				 CppPropertiesDemoProject CppPropertiesDemoProjectGenerator
				 RunCppPropertiesGenerator ThreadingTests)
	if (TARGET ${rfk_target})
		set_target_properties(${rfk_target} PROPERTIES FOLDER ThirdParty/Refureku)
	endif()
endforeach()

# Lua
if (WIN32) 
  set (LUA_INCLUDE_DIRS ${ROOT_DIR}/thirdparty/lua/include)
  set (LUA_LIBRARIES ${ROOT_DIR}/thirdparty/lua/lua54.lib)
  set (LUA_SHARED_LIBRARIES ${ROOT_DIR}/thirdparty/lua/lua54.dll)
else()
  find_package(LUA REQUIRED)
endif()

# LuaBridge3
set (LUA_BRIDGE_INCLUDE_DIRS ${ROOT_DIR}/thirdparty/LuaBridge3/Source)

# fast-cpp-csv-parser
set (FastCSV_INCLUDE_DIRS ${ROOT_DIR}/thirdparty/fast-cpp-csv-parser)

# ImGUI
set(IMGUI_DIR ${ROOT_DIR}/thirdparty/imgui)
set(IMGUI_SOURCE "")
list(APPEND IMGUI_SOURCE
     ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
     ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
     ${IMGUI_DIR}/imgui.cpp
     ${IMGUI_DIR}/imgui_draw.cpp
     ${IMGUI_DIR}/imgui_tables.cpp
     ${IMGUI_DIR}/imgui_widgets.cpp
)

# ImGui Node Editor (zoom-capable node canvas; submodule tracking the
# mikanxr-patches branch of the MikanXR fork of pthom/imgui-node-editor)
set(IMGUI_NODE_EDITOR_DIR ${ROOT_DIR}/thirdparty/imgui-node-editor)
set(IMGUI_NODE_EDITOR_SOURCE "")
list(APPEND IMGUI_NODE_EDITOR_SOURCE
     ${IMGUI_NODE_EDITOR_DIR}/crude_json.cpp
     ${IMGUI_NODE_EDITOR_DIR}/imgui_canvas.cpp
     ${IMGUI_NODE_EDITOR_DIR}/imgui_node_editor.cpp
     ${IMGUI_NODE_EDITOR_DIR}/imgui_node_editor_api.cpp
)

# IXWebSocket
option(USE_ZLIB "Add ZLib support" FALSE)
option(IXWEBSOCKET_INSTALL "Install IXWebSocket" FALSE)
set (IXWEBSOCKET_DIR ${ROOT_DIR}/thirdparty/IXWebSocket/)
set (IXWEBSOCKET_INCLUDE_DIR ${IXWEBSOCKET_DIR})

# GStreamer (optional — disable for test-only / CI builds with -DMIKAN_WITH_GSTREAMER=OFF)
option(MIKAN_WITH_GSTREAMER "Build the GStreamer video plugin" ON)
if(MIKAN_WITH_GSTREAMER)
  find_package(GStreamer REQUIRED COMPONENTS base)
  find_package(GStreamerPluginsBase COMPONENTS app)
  find_package(GStreamerPluginsBase COMPONENTS video)
  find_package(GStreamerPluginsBase COMPONENTS rtp)
  find_package(GStreamerPluginsBase COMPONENTS cuda)

  # gst/cuda/*.h transitively include the real NVIDIA CUDA Toolkit's cuda.h/
  # cudaGL.h/cudaD3D11.h for type definitions (CUdeviceptr, CUresult, ...) - the
  # actual driver library is loaded dynamically at runtime by gstcuda-1.0.dll
  # itself (gst_cuda_load_library()), so only the Toolkit's headers are needed
  # here, not its import libs.
  if(WIN32 AND DEFINED ENV{CUDA_PATH})
    set(CUDA_TOOLKIT_INCLUDE_DIR "$ENV{CUDA_PATH}/include")

    # Track D (JBU depth upsample, ticket D1+) links the real CUDA Driver API
    # directly (cuda.lib, next to nvcuda.dll) rather than going through
    # gstcuda's dynamically-loaded Cu*-prefixed wrappers - unlike Track C, this
    # module is deliberately GStreamer-agnostic (testable standalone, and the
    # plan calls for Track D to be developable in parallel with Track C). Both
    # paths ultimately resolve to the one system nvcuda.dll per process, so a
    # CUDA context established via GStreamer's wrappers is just as usable here
    # as one created directly - see JBUKernel.h's class comment.
    find_library(CUDA_DRIVER_LIBRARY NAMES cuda HINTS "$ENV{CUDA_PATH}/lib/x64" "$ENV{CUDA_PATH}/lib")

    # nvcc compiles JBUKernel.cu to PTX at build time (see
    # MikanARKitVideo/CMakeLists.txt) - loaded via cuModuleLoadData at runtime,
    # not linked into any host translation unit.
    find_program(CUDA_NVCC_EXECUTABLE NAMES nvcc HINTS "$ENV{CUDA_PATH}/bin")

    # cuCtxCreate grew a fourth argument in CUDA 13.0 and MikanARKitVideo calls that form,
    # so an older Toolkit is treated as absent rather than left to fail mid-compile.
    if(EXISTS "${CUDA_TOOLKIT_INCLUDE_DIR}/cuda.h")
      file(STRINGS "${CUDA_TOOLKIT_INCLUDE_DIR}/cuda.h" CUDA_VERSION_LINE REGEX "^#define CUDA_VERSION ")
      string(REGEX MATCH "[0-9]+" CUDA_TOOLKIT_VERSION "${CUDA_VERSION_LINE}")
    endif()
  endif()

  # MikanARKitVideo needs the CUDA Toolkit on top of GStreamer. The Toolkit is a separate
  # system install, so a machine can have GStreamer and not have it: the plugin and the CUDA
  # half of the unit test suite then drop out and the rest of a GStreamer build is unaffected.
  set(MIKAN_WITH_ARKIT_VIDEO OFF)
  if(CUDA_TOOLKIT_VERSION AND CUDA_TOOLKIT_VERSION GREATER_EQUAL 13000 AND CUDA_DRIVER_LIBRARY)
    set(MIKAN_WITH_ARKIT_VIDEO ON)
    MESSAGE(STATUS "CUDA Toolkit ${CUDA_TOOLKIT_VERSION} found at $ENV{CUDA_PATH} - building MikanARKitVideo")
  elseif(CUDA_TOOLKIT_VERSION)
    MESSAGE(STATUS "CUDA Toolkit ${CUDA_TOOLKIT_VERSION} at $ENV{CUDA_PATH} is older than the 13000 MikanARKitVideo needs - skipping it")
  else()
    MESSAGE(STATUS "No CUDA Toolkit found (set CUDA_PATH) - skipping MikanARKitVideo")
  endif()
  find_package(GLIB2 REQUIRED)
  find_package(GObject REQUIRED)
  set(GSTREAMER_BIN_DIR ${GSTREAMER_ROOT}/bin)
else()
  set(MIKAN_WITH_ARKIT_VIDEO OFF)
endif()

# Nlohmann JSON
set (NLOHMANN_JSON_INCLUDE_DIR ${ROOT_DIR}/thirdparty/nlohmann_json/include)

# readerwriterqueue
set (LOCKFREEQUEUE_INCLUDE_DIR ${ROOT_DIR}/thirdparty/readerwriterqueue)

# stb
set (STB_INCLUDE_DIRS ${ROOT_DIR}/thirdparty/stb)

# Vulkan-Headers and volk. volk loads vulkan-1.dll at runtime, so a consumer compiles volk.c into
# itself with VK_NO_PROTOTYPES and gains no link-time Vulkan dependency.
set (VULKAN_HEADERS_INCLUDE_DIR ${ROOT_DIR}/thirdparty/Vulkan-Headers/include)
set (VOLK_DIR ${ROOT_DIR}/thirdparty/volk)
set (VOLK_SOURCE "${VOLK_DIR}/volk.c")

# tinyfiledialogs
set(TINYFILEDIALOGS_DIR ${ROOT_DIR}/thirdparty/tinyfiledialogs)
set(TINYFILEDIALOGS_SOURCE "${TINYFILEDIALOGS_DIR}/tinyfiledialogs.c")

# fast_obj loader
set (FAST_OBJ_LOADER_INCLUDE_DIRS ${ROOT_DIR}/thirdparty/fast_obj)

# easy_profiler
find_package(easy_profiler REQUIRED)

# Spout2
if (WIN32)
  set (SPOUT2_SDK_DIR ${ROOT_DIR}/deps/Spout2-2.007h/SPOUTSDK/SpoutLibrary/Binaries/x64)
  list (APPEND SPOUT2_INCLUDE_DIRS
    ${ROOT_DIR}/deps/Spout2-2.007h/SPOUTSDK
    ${ROOT_DIR}/deps/Spout2-2.007h/SPOUTSDK/SpoutLibrary/Binaries/x64)
  set (SPOUT2_LIBRARIES ${SPOUT2_SDK_DIR}/SpoutLibrary.lib)
  set (SPOUT2_SHARED_LIBRARIES ${SPOUT2_SDK_DIR}/SpoutLibrary.dll)
endif()

# ONNX Runtime w/ DirectML (from nuget packages, fetched by InitialSetup_x64.bat).
# Used by the scene lighting estimator to run the Marigold models in-process.
if (WIN32)
  set (ONNXRUNTIME_DIR ${ROOT_DIR}/deps/onnxruntime)
  set (ONNXRUNTIME_INCLUDE_DIRS ${ONNXRUNTIME_DIR}/build/native/include)
  set (ONNXRUNTIME_LIBRARIES ${ONNXRUNTIME_DIR}/runtimes/win-x64/native/onnxruntime.lib)
  list (APPEND ONNXRUNTIME_SHARED_LIBRARIES
    ${ONNXRUNTIME_DIR}/runtimes/win-x64/native/onnxruntime.dll
    ${ROOT_DIR}/deps/directml/bin/x64-win/DirectML.dll
  )
endif()

# CEF (Chromium Embedded Framework)
# Note: CEF_ROOT can be overridden via command line -DCEF_ROOT=...
if (WIN32 AND NOT DEFINED CEF_ROOT)
  set (CEF_ROOT ${ROOT_DIR}/deps/cef/cef_binary_145.0.27+g4ddda2e+chromium-145.0.7632.117_windows64)
endif()
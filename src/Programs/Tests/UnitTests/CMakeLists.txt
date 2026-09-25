# Mikan Client Test App
file(GLOB UNIT_TEST_SRC
    "${CMAKE_CURRENT_LIST_DIR}/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

# ARKitVideoDeviceManagerLoader doesn't have its own CMake target yet (that lands
# with the plugin scaffolding), so pull it in directly.
list(APPEND UNIT_TEST_SRC
  ${MIKAN_EDITOR_DIR}/ECS/VideoSource/ARKitVideoDeviceManagerLoader.h
  ${MIKAN_EDITOR_DIR}/ECS/VideoSource/ARKitVideoDeviceManagerLoader.cpp
)

# NOTE: ARKitRTPHeaderExtension.h/.cpp (ticket C3) is deliberately NOT pulled in
# here directly. It registers named GObject/GstMeta types
# (G_DEFINE_TYPE, gst_meta_api_type_register) - GLib's type registry is process-
# global and shared across DLL boundaries, so compiling this file into both this
# executable AND MikanARKitVideo.dll (which also globs it - see that plugin's
# CMakeLists.txt) causes a duplicate-name type registration failure as soon as
# both copies' registration code runs in the same process (confirmed empirically:
# "cannot register existing type 'ARKitRTPHeaderExtension'" once the DLL is
# loaded by arkit_video_source_system's tests). It's only exercised indirectly,
# through the already-loaded plugin DLL, same as MikanARKitVideoDevice.cpp itself.

# CudaGLInterop.h/.cpp (ticket D4; NV12 planes as of "Phase 6") is plain CUDA
# Driver API C++ - no GObject/GstMeta registration, so (unlike
# ARKitRTPHeaderExtension.cpp above) it's safe to dual-compile into both this
# executable and MikanARKitVideo.dll. Needs the CUDA Toolkit headers/driver lib,
# so it is gated on MIKAN_WITH_ARKIT_VIDEO, which is GStreamer plus a CUDA
# Toolkit new enough for the plugin (see ThirdParty.cmake).
# Additionally needs MikanRenderer (IMkTexture) and GLEW (GLuint/GL_TEXTURE_2D
# types, required before cudaGL.h) - compiling it here proves it builds cleanly
# against the real headers/libs on every run, even though its actual CUDA-GL
# registration/mapping behavior needs a live GL context this headless console
# test executable doesn't have (see arkit_cuda_gl_interop_unit_tests.cpp's own
# notes on what is/isn't covered here). MIKAN_ARKIT_CUDA_GL_INTEROP_AVAILABLE
# gates that test file's actual test body (vs. a skip stub) on this block having
# run - there's no PTX/kernel module to gate on anymore since "Phase 6" moved
# NV12->RGBA conversion out of CUDA entirely.
if(MIKAN_WITH_ARKIT_VIDEO)
  list(APPEND UNIT_TEST_SRC
    ${MIKAN_PLUGINS_DIR}/MikanARKitVideo/Private/Cuda/CudaGLInterop.h
    ${MIKAN_PLUGINS_DIR}/MikanARKitVideo/Private/Cuda/CudaGLInterop.cpp
  )

  list(APPEND UNIT_TEST_INCL_DIRS
    ${CUDA_TOOLKIT_INCLUDE_DIR}
    ${MIKAN_PLUGINS_DIR}/MikanARKitVideo/Private/Cuda
    ${MIKAN_LIBRARIES_DIR}/MikanRenderer/Public
    ${GLEW_INCLUDE_DIRS}
  )

  list(APPEND UNIT_TEST_LIBS
    ${CUDA_DRIVER_LIBRARY}
    ${GLEW_LIBRARIES}
    MikanRenderer
  )
endif()

list(APPEND UNIT_TEST_INCL_DIRS
  ${CMAKE_CURRENT_LIST_DIR}
  ${RFK_INCLUDE_DIR}
  ${NLOHMANN_JSON_INCLUDE_DIR}
  ${RFK_GENERATED_ROOT_DIR}/UnitTests
  ${RFK_GENERATED_ROOT_DIR}/MikanSerialization
  ${RFK_GENERATED_ROOT_DIR}/MikanClientCore
  ${RFK_GENERATED_ROOT_DIR}/MikanClientAPI
  ${MIKAN_LIBRARIES_DIR}/MikanClientCore/Public
  ${MIKAN_LIBRARIES_DIR}/MikanClientAPI/Public
  ${MIKAN_LIBRARIES_DIR}/MikanCoreApp/Public
  ${MIKAN_LIBRARIES_DIR}/MikanMath/Public
  ${MIKAN_LIBRARIES_DIR}/MikanOnnx/Public
  ${MIKAN_LIBRARIES_DIR}/MikanSerialization/Public
  ${MIKAN_LIBRARIES_DIR}/MikanUtility/Public
  ${MIKAN_PLUGINS_DIR}/MikanARKitVideo/Private
  ${MIKAN_EDITOR_DIR}/ECS/VideoSource
  ${ROOT_DIR}/thirdparty/glm/
)

list(APPEND UNIT_TEST_LIBS 
  ${RFK_LIBRARIES}
  MikanClientCore
  MikanClientAPI
  MikanCoreApp
  MikanMath
  MikanOnnx
  MikanSerialization
  MikanUtility
)

add_executable(unit_test_suite_cpp ${UNIT_TEST_SRC})
target_include_directories(unit_test_suite_cpp PUBLIC ${UNIT_TEST_INCL_DIRS})
target_link_libraries(unit_test_suite_cpp ${UNIT_TEST_LIBS})
target_compile_definitions(unit_test_suite_cpp PRIVATE ENABLE_SERIALIZATION_REFLECTION)
target_compile_definitions(unit_test_suite_cpp PRIVATE JSON_DISABLE_ENUM_SERIALIZATION=1)
target_compile_definitions(unit_test_suite_cpp PRIVATE ENABLE_MIKANCORE_REFLECTION)
target_compile_definitions(unit_test_suite_cpp PRIVATE ENABLE_MIKANAPI_REFLECTION)
if(MIKAN_WITH_ARKIT_VIDEO)
  target_compile_definitions(unit_test_suite_cpp PRIVATE MIKAN_ARKIT_CUDA_GL_INTEROP_AVAILABLE)
endif()
SET_TARGET_PROPERTIES(unit_test_suite_cpp PROPERTIES FOLDER Test)

# Create the command to run RefurekuGenerator
add_custom_target(unit_test_suite_reflection
					WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
					COMMAND "${RFK_GENERATOR_EXE}" "${CMAKE_CURRENT_LIST_DIR}/RefurekuSettings.toml") 
set_target_properties(unit_test_suite_reflection PROPERTIES FOLDER Test)
add_dependencies(unit_test_suite_reflection RefurekuGeneratorRuntime)

# Run the RefurekuGenerator BEFORE building the project to refresh generated files
add_dependencies(unit_test_suite_cpp unit_test_suite_reflection)
add_dependencies(unit_test_suite_cpp MikanClientCore)
add_dependencies(unit_test_suite_cpp MikanClientAPI)
add_dependencies(unit_test_suite_cpp MikanCoreApp)
add_dependencies(unit_test_suite_cpp MikanMath)
add_dependencies(unit_test_suite_cpp MikanSerialization)
add_dependencies(unit_test_suite_cpp MikanSharedTexture)
add_dependencies(unit_test_suite_cpp MikanUtility)
if(MIKAN_WITH_ARKIT_VIDEO)
  add_dependencies(unit_test_suite_cpp MikanARKitVideo)
endif()

# Post build - copy runtime dependencies to binary build folder (for debugging)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows") 
  add_custom_command(
    TARGET unit_test_suite_cpp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy 
	$<TARGET_FILE_DIR:MikanClientAPI>/MikanClientAPI.dll 
	$<TARGET_FILE_DIR:unit_test_suite_cpp>)
  add_custom_command(
    TARGET unit_test_suite_cpp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy 
	$<TARGET_FILE_DIR:MikanClientCore>/MikanClientCore.dll 
	$<TARGET_FILE_DIR:unit_test_suite_cpp>)
  add_custom_command(
	TARGET unit_test_suite_cpp POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	$<TARGET_FILE_DIR:MikanCoreApp>/MikanCoreApp.dll
	$<TARGET_FILE_DIR:unit_test_suite_cpp>)
  add_custom_command(
	TARGET unit_test_suite_cpp POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	$<TARGET_FILE_DIR:MikanSharedTexture>/MikanSharedTexture.dll
	$<TARGET_FILE_DIR:unit_test_suite_cpp>)
  add_custom_command(
    TARGET unit_test_suite_cpp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${GLEW_SHARED_LIBRARIES} $<TARGET_FILE_DIR:unit_test_suite_cpp>)
  # ONNX Runtime + DirectML (needed by the onnx unit test module)
  add_custom_command(
    TARGET unit_test_suite_cpp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${ONNXRUNTIME_SHARED_LIBRARIES}
    $<TARGET_FILE_DIR:unit_test_suite_cpp>)
  add_custom_command(
	TARGET unit_test_suite_cpp POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	$<TARGET_FILE_DIR:MikanMath>/MikanMath.dll
	$<TARGET_FILE_DIR:unit_test_suite_cpp>)
  add_custom_command(
	TARGET unit_test_suite_cpp POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	$<TARGET_FILE_DIR:MikanSerialization>/MikanSerialization.dll
	$<TARGET_FILE_DIR:unit_test_suite_cpp>)
  add_custom_command(
	TARGET unit_test_suite_cpp POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	$<TARGET_FILE_DIR:MikanUtility>/MikanUtility.dll
	$<TARGET_FILE_DIR:unit_test_suite_cpp>)
  add_custom_command(
    TARGET unit_test_suite_cpp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy 
    "${SPOUT2_SHARED_LIBRARIES}" 
    $<TARGET_FILE_DIR:unit_test_suite_cpp>)
  add_custom_command(
	TARGET unit_test_suite_cpp POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	"${RFK_SHARED_LIBRARIES}"
	$<TARGET_FILE_DIR:unit_test_suite_cpp>)
  if(MIKAN_WITH_ARKIT_VIDEO)
    add_custom_command(
	  TARGET unit_test_suite_cpp POST_BUILD
	  COMMAND ${CMAKE_COMMAND} -E copy
	  $<TARGET_FILE_DIR:MikanARKitVideo>/MikanARKitVideo.dll
	  $<TARGET_FILE_DIR:unit_test_suite_cpp>)
    # MikanARKitVideo.dll now links MikanRenderer (ticket D4, CudaGLInterop's
    # IMkTexture usage) - needed alongside it for the DLL load to resolve, even
    # though unit_test_suite_cpp itself never links MikanRenderer directly.
    add_custom_command(
	  TARGET unit_test_suite_cpp POST_BUILD
	  COMMAND ${CMAKE_COMMAND} -E copy
	  $<TARGET_FILE_DIR:MikanRenderer>/MikanRenderer.dll
	  $<TARGET_FILE_DIR:unit_test_suite_cpp>)
  endif()
ELSE() #Linux/Darwin
ENDIF()

# Install
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  install(TARGETS unit_test_suite_cpp
      RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH}
      LIBRARY DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/lib
      ARCHIVE DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/lib)
ELSE() #Linux/Darwin
ENDIF()
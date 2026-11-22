# Mikan Client Test App
file(GLOB MIKAN_CLIENT_TEST_APP_SRC
    "${CMAKE_CURRENT_LIST_DIR}/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

# Shared Source
file(GLOB MIKAN_CLIENT_TEST_SHARED_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Shared/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/Shared/*.cpp"
)
source_group("Shared" FILES ${MIKAN_CLIENT_TEST_SHARED_SRC})

# DirectX Source
file(GLOB MIKAN_CLIENT_TEST_DX_SRC
    "${CMAKE_CURRENT_LIST_DIR}/DirectX/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/DirectX/*.cpp"
)
source_group("DirectX" FILES ${MIKAN_CLIENT_TEST_DX_SRC})

# OpenGL Source
file(GLOB MIKAN_CLIENT_TEST_GL_SRC
    "${CMAKE_CURRENT_LIST_DIR}/OpenGL/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/OpenGL/*.cpp"
)
source_group("OpenGL" FILES ${MIKAN_CLIENT_TEST_GL_SRC})

# Vulkan Source
file(GLOB MIKAN_CLIENT_TEST_VK_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Vulkan/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/Vulkan/*.cpp"
)
source_group("Vulkan" FILES ${MIKAN_CLIENT_TEST_VK_SRC})

# Vulkan shaders: HLSL compiled to SPIR-V by the Windows SDK's dxc, which carries the -spirv backend.
# The .hlsl files are listed for the IDE only; the FXC build step must not run on them.
file(GLOB MIKAN_CLIENT_TEST_VK_SHADER_SRC "${CMAKE_CURRENT_LIST_DIR}/Vulkan/Shaders/*.hlsl")
set_source_files_properties(${MIKAN_CLIENT_TEST_VK_SHADER_SRC} PROPERTIES HEADER_FILE_ONLY ON)
source_group("Vulkan\\Shaders" FILES ${MIKAN_CLIENT_TEST_VK_SHADER_SRC})

# volk is compiled into the executable so it carries no link-time Vulkan dependency
set_source_files_properties(${VOLK_SOURCE} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
source_group("ThirdParty" FILES ${VOLK_SOURCE})

# The Spout receiver of the frame dump: the same in-tree spoutDX sources the shared texture DLL
# compiles, so the harness reads its own sender back the way the editor does
set(MIKAN_CLIENT_TEST_SPOUT_DX_SRC
    "${MIKAN_LIBRARIES_DIR}/MikanSharedTexture/Private/SpoutDX/SpoutDX.cpp"
    "${MIKAN_LIBRARIES_DIR}/MikanSharedTexture/Private/SpoutDX/SpoutUtils.cpp"
)
set_source_files_properties(${MIKAN_CLIENT_TEST_SPOUT_DX_SRC} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
source_group("ThirdParty\\SpoutDX" FILES ${MIKAN_CLIENT_TEST_SPOUT_DX_SRC})

set(MIKAN_CLIENT_TEST_SRC
    ${MIKAN_CLIENT_TEST_APP_SRC}
    ${MIKAN_CLIENT_TEST_SHARED_SRC}
    ${MIKAN_CLIENT_TEST_DX_SRC}
    ${MIKAN_CLIENT_TEST_GL_SRC}
    ${MIKAN_CLIENT_TEST_VK_SRC}
    ${MIKAN_CLIENT_TEST_VK_SHADER_SRC}
    ${VOLK_SOURCE}
    ${MIKAN_CLIENT_TEST_SPOUT_DX_SRC}
)

list(APPEND MIKAN_CLIENT_TEST_INCL_DIRS
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/DirectX
  ${CMAKE_CURRENT_LIST_DIR}/OpenGL
  ${CMAKE_CURRENT_LIST_DIR}/Vulkan
  ${CMAKE_CURRENT_LIST_DIR}/Shared
  ${VULKAN_HEADERS_INCLUDE_DIR}
  ${VOLK_DIR}
  ${MIKAN_LIBRARIES_DIR}/MikanClientAPI/Public
  ${MIKAN_LIBRARIES_DIR}/MikanClientCore/Public
  ${MIKAN_LIBRARIES_DIR}/MikanCoreApp/Public
  ${MIKAN_LIBRARIES_DIR}/MikanRenderer/Public
  ${MIKAN_LIBRARIES_DIR}/MikanSerialization/Public
  ${MIKAN_LIBRARIES_DIR}/MikanSharedTexture/Public
  ${MIKAN_LIBRARIES_DIR}/MikanSharedTexture/Private/SpoutDX
  ${MIKAN_LIBRARIES_DIR}/MikanUtility/Public
  ${SPOUT2_INCLUDE_DIRS}
  ${ROOT_DIR}/thirdparty/glm/
  ${GLEW_INCLUDE_DIRS}
  ${NLOHMANN_JSON_INCLUDE_DIR}
  ${SDL2_INCLUDE_DIR}
  ${STB_INCLUDE_DIRS}
  ${RFK_INCLUDE_DIR})

# SDL2main is needed for Windows to provide the main() entry point
set(SDL2MAIN_LIBRARY ${ROOT_DIR}/deps/SDL2-2.30.10/lib/x64/SDL2main.lib)

list(APPEND MIKAN_CLIENT_TEST_REQ_LIBS
  d3d11
  d3dcompiler
  dxguid
  comctl32
  ${SDL2MAIN_LIBRARY}
  ${SDL2_LIBRARY}
  ${MIKAN_EXTRA_LIBS}
  ${GLEW_LIBRARIES}
  ${SPOUT2_LIBRARIES}
  MikanClientCore
  MikanClientAPI
  MikanCoreApp
  MikanRenderer
  MikanSerialization
  MikanSharedTexture
  MikanUtility)

add_executable(MikanClientTestCPP ${MIKAN_CLIENT_TEST_SRC})
target_include_directories(MikanClientTestCPP PUBLIC ${MIKAN_CLIENT_TEST_INCL_DIRS})
target_link_libraries(MikanClientTestCPP ${MIKAN_CLIENT_TEST_REQ_LIBS})
target_compile_definitions(MikanClientTestCPP PRIVATE VK_NO_PROTOTYPES)
SET_TARGET_PROPERTIES(MikanClientTestCPP PROPERTIES FOLDER Test WIN32_EXECUTABLE FALSE)

# Locate a SPIR-V capable dxc. The Windows SDK's dxc.exe is built without SPIR-V codegen, so only the
# DirectXShaderCompiler release under deps/ (fetched by InitialSetup_x64.bat) or an explicit
# -DDXC_EXECUTABLE=<path> qualifies.
find_program(DXC_EXECUTABLE NAMES dxc HINTS "${ROOT_DIR}/deps/dxc/bin/x64" NO_DEFAULT_PATH)

if (DXC_EXECUTABLE)
  set(MIKAN_CLIENT_TEST_SPV_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
  file(MAKE_DIRECTORY ${MIKAN_CLIENT_TEST_SPV_DIR})

  function(mikan_add_spirv_shader OUT_LIST SOURCE STAGE ENTRY OUTPUT_NAME)
    set(SPV_FILE ${MIKAN_CLIENT_TEST_SPV_DIR}/${OUTPUT_NAME}.spv)
    add_custom_command(
      OUTPUT ${SPV_FILE}
      COMMAND ${DXC_EXECUTABLE} -spirv -fspv-target-env=vulkan1.3 -T ${STAGE} -E ${ENTRY} -Fo ${SPV_FILE} ${SOURCE}
      DEPENDS ${SOURCE}
      COMMENT "dxc -spirv ${OUTPUT_NAME}"
      VERBATIM)
    set(${OUT_LIST} ${${OUT_LIST}} ${SPV_FILE} PARENT_SCOPE)
  endfunction()

  set(MIKAN_CLIENT_TEST_SPV_FILES)
  mikan_add_spirv_shader(MIKAN_CLIENT_TEST_SPV_FILES ${CMAKE_CURRENT_LIST_DIR}/Vulkan/Shaders/cube.hlsl vs_6_0 vs_main cube_vs)
  mikan_add_spirv_shader(MIKAN_CLIENT_TEST_SPV_FILES ${CMAKE_CURRENT_LIST_DIR}/Vulkan/Shaders/cube.hlsl ps_6_0 ps_main cube_ps)

  add_custom_target(MikanClientTestCPPShaders DEPENDS ${MIKAN_CLIENT_TEST_SPV_FILES})
  set_target_properties(MikanClientTestCPPShaders PROPERTIES FOLDER Test)
  add_dependencies(MikanClientTestCPP MikanClientTestCPPShaders)
  add_custom_command(
    TARGET MikanClientTestCPP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${MIKAN_CLIENT_TEST_SPV_DIR} $<TARGET_FILE_DIR:MikanClientTestCPP>/shaders)
else()
  message(WARNING "dxc.exe not found: the MikanClientTestCPP -vk path will have no shaders")
endif()

# Runtime dependencies
set(SDL2_DLL ${ROOT_DIR}/deps/SDL2-2.30.10/lib/x64/SDL2.dll)
set(EASY_PROFILER_FOLDER ${ROOT_DIR}/deps/easy_profiler/bin)

# Post build - copy runtime dependencies to binary build folder (for debugging)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows") 
  set_property(TARGET MikanClientTestCPP PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:MikanClientTestCPP>")

  add_custom_command(
    TARGET MikanClientTestCPP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${SPOUT2_SHARED_LIBRARIES}" $<TARGET_FILE_DIR:MikanClientTestCPP>)
  add_custom_command(
    TARGET MikanClientTestCPP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${SDL2_DLL} $<TARGET_FILE_DIR:MikanClientTestCPP>)
  add_custom_command(
    TARGET MikanClientTestCPP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${GLEW_SHARED_LIBRARIES} $<TARGET_FILE_DIR:MikanClientTestCPP>)
  add_custom_command(
    TARGET MikanClientTestCPP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanClientCore>/MikanClientCore.dll
          $<TARGET_FILE_DIR:MikanClientTestCPP>)
  add_custom_command(
    TARGET MikanClientTestCPP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanClientAPI>/MikanClientAPI.dll
          $<TARGET_FILE_DIR:MikanClientTestCPP>)
  add_custom_command(
    TARGET MikanClientTestCPP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanCoreApp>/MikanCoreApp.dll
          $<TARGET_FILE_DIR:MikanClientTestCPP>)
  add_custom_command(
    TARGET MikanClientTestCPP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanRenderer>/MikanRenderer.dll
          $<TARGET_FILE_DIR:MikanClientTestCPP>)
  add_custom_command(
    TARGET MikanClientTestCPP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanSerialization>/MikanSerialization.dll
          $<TARGET_FILE_DIR:MikanClientTestCPP>)
  add_custom_command(
    TARGET MikanClientTestCPP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanSharedTexture>/MikanSharedTexture.dll
          $<TARGET_FILE_DIR:MikanClientTestCPP>)
  add_custom_command(
    TARGET MikanClientTestCPP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanUtility>/MikanUtility.dll
          $<TARGET_FILE_DIR:MikanClientTestCPP>)
  add_custom_command(
    TARGET MikanClientTestCPP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy 
    "${EASY_PROFILER_FOLDER}/easy_profiler.dll"
    $<TARGET_FILE_DIR:MikanClientTestCPP>)
  add_custom_command(
	TARGET MikanClientTestCPP POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	"${RFK_SHARED_LIBRARIES}"
	$<TARGET_FILE_DIR:MikanClientTestCPP>)
ELSE() #Linux/Darwin
ENDIF()

# No install rule: the client test harness runs from the build tree and is not part of the shipped app.
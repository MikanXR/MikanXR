## Mikan Interprocess DLL
## =======================

MESSAGE(STATUS "Generating Mikan Shared Texture DLL Project")

file(GLOB MIKAN_SHARED_TEXTURE_PRIVATE_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Private/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/Private/*.cpp"
)

file(GLOB MIKAN_SHARED_TEXTURE_SPOUT2_DX_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Private/SpoutDX/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Private/SpoutDX/*.h"
)

file(GLOB MIKAN_SHARED_TEXTURE_SPOUT2_GL_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Private/SpoutGL/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Private/SpoutGL/*.h"
)

file(GLOB MIKAN_SHARED_TEXTURE_PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/Public/*.h"
)

file(GLOB MIKAN_SHARED_TEXTURE_LIBRARY_SRC
    ${MIKAN_SHARED_TEXTURE_PUBLIC}
    ${MIKAN_SHARED_TEXTURE_PRIVATE_SRC}
    ${MIKAN_SHARED_TEXTURE_SPOUT2_DX_SRC}
    ${MIKAN_SHARED_TEXTURE_SPOUT2_GL_SRC}
    ${VOLK_SOURCE}
)

# Required core libraries on various platforms
set(GL_PLATFORM_LIBRARIES "")
if (WIN32) 
  list(APPEND GL_PLATFORM_LIBRARIES opengl32 )
elseif (APPLE)
  find_library(opengl_library OpenGL)
  list(APPEND GL_PLATFORM_LIBRARIES ${opengl_library})
elseif(CMAKE_SYSTEM MATCHES "Linux")
  list(APPEND GL_PLATFORM_LIBRARIES GL)
endif()

# One writer backend per graphics API. Each pulls in its own API's headers, so they are kept out
# of the unity build that would otherwise merge OpenGL, Direct3D and Vulkan into one translation
# unit and put their header ordering back in conflict.
set(MIKAN_SHARED_TEXTURE_WRITER_BACKEND_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Private/SpoutOpenGLTextureWriter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Private/SpoutDX11TextureWriter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Private/SpoutDX12TextureWriter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Private/SpoutVulkanTextureWriter.cpp"
)

# Remove problematic files from the unity builds (header file inclusion ordering issues)
set_source_files_properties(${MIKAN_SHARED_TEXTURE_SPOUT2_DX_SRC} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
set_source_files_properties(${MIKAN_SHARED_TEXTURE_SPOUT2_GL_SRC} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
set_source_files_properties(${MIKAN_SHARED_TEXTURE_WRITER_BACKEND_SRC} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
set_source_files_properties(${VOLK_SOURCE} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
source_group("Public" FILES ${MIKAN_SHARED_TEXTURE_PUBLIC})
source_group("Private" FILES ${MIKAN_SHARED_TEXTURE_PRIVATE_SRC})
source_group("Private\\SpoutDX" FILES ${MIKAN_SHARED_TEXTURE_SPOUT2_DX_SRC})
source_group("Private\\SpoutGL" FILES ${MIKAN_SHARED_TEXTURE_SPOUT2_GL_SRC})

list(APPEND MIKAN_SHARED_TEXTURE_INCL_DIRS
  ${CMAKE_CURRENT_LIST_DIR}/Public
  ${CMAKE_CURRENT_LIST_DIR}/Private
  ${CMAKE_CURRENT_LIST_DIR}/Private/SpoutDX
  ${CMAKE_CURRENT_LIST_DIR}/Private/SpoutGL
  ${GLEW_INCLUDE_DIRS}
  ${SPOUT2_INCLUDE_DIRS}
  ${VULKAN_HEADERS_INCLUDE_DIR}
  ${VOLK_DIR})

list(APPEND MIKAN_INTERPROCESS_REQ_LIBS
  ${GL_PLATFORM_LIBRARIES}
  ${GLEW_LIBRARIES}
  ${SPOUT2_LIBRARIES})

add_library(MikanSharedTexture SHARED ${MIKAN_SHARED_TEXTURE_LIBRARY_SRC})
target_include_directories(MikanSharedTexture PRIVATE ${MIKAN_SHARED_TEXTURE_INCL_DIRS})
target_link_libraries(MikanSharedTexture PRIVATE ${MIKAN_INTERPROCESS_REQ_LIBS})
set_target_properties(MikanSharedTexture PROPERTIES FOLDER MikanLibraries)
set_target_properties(MikanSharedTexture PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(MikanSharedTexture PROPERTIES C_VISIBILITY_PRESET hidden)
target_compile_definitions(MikanSharedTexture PRIVATE MIKAN_SHAREDTEXTURE_EXPORTS VK_NO_PROTOTYPES VK_USE_PLATFORM_WIN32_KHR)

# Install - copy runtime dependencies to dist folder (for release)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  install(TARGETS MikanSharedTexture
      RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH})
ELSE() #Linux/Darwin
ENDIF()
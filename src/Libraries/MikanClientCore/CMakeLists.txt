## Mikan Core DLL
## =======================

MESSAGE(STATUS "Generating Mikan Core DLL Project")

file(GLOB MIKAN_CLIENT_CORE_PRIVATE_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Private/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/Private/*.cpp"
)

file(GLOB MIKAN_CLIENT_CORE_INTERPROCESS_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Private/Interprocess/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Private/Interprocess/*.h"
)

file(GLOB MIKAN_CLIENT_CORE_PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/Public/*.h"
)

file(GLOB MIKAN_CLIENT_CORE_LIBRARY_SRC
    ${MIKAN_CLIENT_CORE_PUBLIC}
    ${MIKAN_CLIENT_CORE_PRIVATE_SRC}
    ${MIKAN_CLIENT_CORE_INTERPROCESS_SRC}
)

# Remove problematic files from the unity builds (header file inclusion ordering issues)
set_source_files_properties(${MIKAN_CLIENT_CORE_SPOUT2_DX_SRC} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/Private/MikanClient.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/Private/Interprocess/WebsocketInterprocessMessageClient.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

source_group("Public" FILES ${MIKAN_CLIENT_CORE_PUBLIC})
source_group("Private" FILES ${MIKAN_CLIENT_CORE_PRIVATE_SRC})
source_group("Private\\Interprocess" FILES ${MIKAN_CLIENT_CORE_INTERPROCESS_SRC})

list(APPEND MIKAN_CLIENT_CORE_INCL_DIRS
  ${CMAKE_CURRENT_LIST_DIR}/Public
  ${CMAKE_CURRENT_LIST_DIR}/Private
  ${CMAKE_CURRENT_LIST_DIR}/Private/Interprocess
  ${MIKAN_LIBRARIES_DIR}/MikanSerialization/Public
  ${MIKAN_LIBRARIES_DIR}/MikanSharedTexture/Public
  ${MIKAN_LIBRARIES_DIR}/MikanUtility/Public
  ${RFK_GENERATED_ROOT_DIR}/MikanClientCore
  ${RFK_GENERATED_ROOT_DIR}/MikanSerialization
  ${NLOHMANN_JSON_INCLUDE_DIR}
  ${RFK_INCLUDE_DIR}
  ${SPOUT2_INCLUDE_DIRS}
  ${IXWEBSOCKET_INCLUDE_DIR}
  ${LOCKFREEQUEUE_INCLUDE_DIR}
  ${VULKAN_HEADERS_INCLUDE_DIR})

list(APPEND MIKAN_CLIENT_REQ_LIBS
  ${MIKAN_EXTRA_LIBS}
  ${RFK_LIBRARIES}
  ixwebsocket
  MikanSerialization
  MikanSharedTexture
  MikanUtility)

add_library(MikanClientCore SHARED ${MIKAN_CLIENT_CORE_LIBRARY_SRC})
target_include_directories(MikanClientCore PRIVATE ${MIKAN_CLIENT_CORE_INCL_DIRS})
target_link_libraries(MikanClientCore PRIVATE ${MIKAN_CLIENT_REQ_LIBS})
set_target_properties(MikanClientCore PROPERTIES FOLDER ClientAPI)
set_target_properties(MikanClientCore PROPERTIES PUBLIC_HEADER "${MIKAN_CLIENT_CORE_PUBLIC}")
set_target_properties(MikanClientCore PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(MikanClientCore PROPERTIES C_VISIBILITY_PRESET hidden)
target_compile_definitions(MikanClientCore PRIVATE MIKAN_CORE_EXPORTS)
target_compile_definitions(MikanClientCore PRIVATE ENABLE_MIKANCORE_REFLECTION)
target_compile_definitions(MikanClientCore PRIVATE ENABLE_SERIALIZATION_REFLECTION)
target_compile_definitions(MikanClientCore PRIVATE ENABLE_SPOUT_DX)
target_compile_definitions(MikanClientCore PRIVATE JSON_DISABLE_ENUM_SERIALIZATION=1)

# Add dependencies
add_dependencies(MikanClientCore MikanSerialization)
add_dependencies(MikanClientCore MikanSharedTexture)
add_dependencies(MikanClientCore MikanUtility)

# Create the command to run RefurekuGenerator
add_custom_target(MikanClientCoreReflection
					WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
					COMMAND "${RFK_GENERATOR_EXE}" "${CMAKE_CURRENT_LIST_DIR}/RefurekuSettings.toml") 
set_target_properties(MikanClientCoreReflection PROPERTIES FOLDER ClientAPI)
add_dependencies(MikanClientCoreReflection RefurekuGeneratorRuntime)

# Run the RefurekuGenerator BEFORE building the project to refresh generated files
add_dependencies(MikanClientCore MikanClientCoreReflection)

# Install - copy runtime dependencies to dist folder (for release)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows") 
  install(TARGETS MikanClientCore
      RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH}
      LIBRARY DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/lib
      ARCHIVE DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/lib
      PUBLIC_HEADER DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/include)
ELSE() #Linux/Darwin
ENDIF()
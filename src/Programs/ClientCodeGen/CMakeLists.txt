# Mikan Client Code Generator
file(GLOB CLIENT_CODEGEN_SRC
    "${CMAKE_CURRENT_LIST_DIR}/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

list(APPEND CLIENT_CODEGEN_INCL_DIRS
  ${CMAKE_CURRENT_LIST_DIR}
  ${MIKAN_LIBRARIES_DIR}/MikanClientAPI/Public
  ${MIKAN_LIBRARIES_DIR}/MikanClientCore/Public
  ${MIKAN_LIBRARIES_DIR}/MikanCoreApp/Public
  ${MIKAN_LIBRARIES_DIR}/MikanSerialization/Public
  ${MIKAN_LIBRARIES_DIR}/MikanUtility/Public
  ${RFK_GENERATED_ROOT_DIR}/MikanClientAPI
  ${RFK_GENERATED_ROOT_DIR}/MikanClientCore
  ${RFK_GENERATED_ROOT_DIR}/MikanSerialization
  ${NLOHMANN_JSON_INCLUDE_DIR}
  ${RFK_INCLUDE_DIR})

list(APPEND CLIENT_CODEGEN_REQ_LIBS
  ${RFK_LIBRARIES}
  MikanClientCore
  MikanClientAPI
  MikanCoreApp
  MikanSerialization
  MikanUtility)

add_executable(MikanClientCodeGen ${CLIENT_CODEGEN_SRC})
target_include_directories(MikanClientCodeGen PUBLIC ${CLIENT_CODEGEN_INCL_DIRS})
target_link_libraries(MikanClientCodeGen ${CLIENT_CODEGEN_REQ_LIBS})
target_compile_definitions(MikanClientCodeGen PRIVATE ENABLE_SERIALIZATION_REFLECTION)
target_compile_definitions(MikanClientCodeGen PRIVATE ENABLE_MIKANAPI_REFLECTION)
target_compile_definitions(MikanClientCodeGen PRIVATE ENABLE_MIKANCORE_REFLECTION)
set_target_properties(MikanClientCodeGen PROPERTIES FOLDER Programs)

# Post build - copy runtime dependencies to binary build folder (for debugging)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows") 
  set_property(TARGET MikanClientCodeGen PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:MikanClientCodeGen>")

  add_custom_command(
    TARGET MikanClientCodeGen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanClientCore>/MikanClientCore.dll
          $<TARGET_FILE_DIR:MikanClientCodeGen>)
  add_custom_command(
    TARGET MikanClientCodeGen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanClientAPI>/MikanClientAPI.dll
          $<TARGET_FILE_DIR:MikanClientCodeGen>)
  add_custom_command(
    TARGET MikanClientCodeGen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanCoreApp>/MikanCoreApp.dll
          $<TARGET_FILE_DIR:MikanClientCodeGen>)
  add_custom_command(
    TARGET MikanClientCodeGen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanSerialization>/MikanSerialization.dll
          $<TARGET_FILE_DIR:MikanClientCodeGen>)
  add_custom_command(
    TARGET MikanClientCodeGen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanSharedTexture>/MikanSharedTexture.dll
          $<TARGET_FILE_DIR:MikanClientCodeGen>)
  add_custom_command(
    TARGET MikanClientCodeGen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanUtility>/MikanUtility.dll
          $<TARGET_FILE_DIR:MikanClientCodeGen>)
  add_custom_command(
    TARGET MikanClientCodeGen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${SPOUT2_SHARED_LIBRARIES}" $<TARGET_FILE_DIR:MikanClientCodeGen>)
  add_custom_command(
    TARGET MikanClientCodeGen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${GLEW_SHARED_LIBRARIES}" $<TARGET_FILE_DIR:MikanClientCodeGen>)
  add_custom_command(
	TARGET MikanClientCodeGen POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	"${RFK_SHARED_LIBRARIES}"
	$<TARGET_FILE_DIR:MikanClientCodeGen>)
  # Install VC Runtime dependencies
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
  include(InstallRequiredSystemLibraries)
  if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
    add_custom_command(
      TARGET MikanClientCodeGen POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
      $<TARGET_FILE_DIR:MikanClientCodeGen>)
  endif()
ELSE() #Linux/Darwin
ENDIF()

# No install rule: the bindings generator is a build-time tool and is not part of the shipped app.
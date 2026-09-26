## Mikan API DLL
## =======================

MESSAGE(STATUS "Generating Mikan API DLL Project")

file(GLOB MIKAN_CLIENT_API_HEADER
	"${CMAKE_CURRENT_LIST_DIR}/Public/*.h"
)

file(GLOB MIKAN_CLIENT_API_PRIVATE_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Private/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/Private/*.cpp"
)

file(GLOB MIKAN_CLIENT_API_SRC
	${MIKAN_CLIENT_API_HEADER}
	${MIKAN_CLIENT_API_PRIVATE_SRC}
)

source_group("Public" FILES ${MIKAN_CLIENT_API_HEADER})
source_group("Private\\API" FILES ${MIKAN_CLIENT_API_PRIVATE_SRC})

list(APPEND MIKAN_CLIENT_API_INCL_DIRS
  ${CMAKE_CURRENT_LIST_DIR}/Public
  ${CMAKE_CURRENT_LIST_DIR}/Private
  ${MIKAN_LIBRARIES_DIR}/MikanClientCore/Public
  ${MIKAN_LIBRARIES_DIR}/MikanSerialization/Public
  ${MIKAN_LIBRARIES_DIR}/MikanCoreApp/Public
  ${MIKAN_LIBRARIES_DIR}/MikanUtility/Public
  ${RFK_GENERATED_ROOT_DIR}/MikanClientCore
  ${RFK_GENERATED_ROOT_DIR}/MikanClientAPI
  ${RFK_GENERATED_ROOT_DIR}/MikanSerialization
  ${RFK_INCLUDE_DIR}
  ${NLOHMANN_JSON_INCLUDE_DIR}
)

list(APPEND MIKAN_CLIENT_API_REQ_LIBS
  ${RFK_LIBRARIES}
  MikanClientCore
  MikanCoreApp
  MikanSerialization
  MikanUtility)

add_library(MikanClientAPI SHARED ${MIKAN_CLIENT_API_SRC})
target_include_directories(MikanClientAPI PRIVATE ${MIKAN_CLIENT_API_INCL_DIRS})
target_link_libraries(MikanClientAPI PRIVATE ${MIKAN_CLIENT_API_REQ_LIBS})
set_target_properties(MikanClientAPI PROPERTIES FOLDER ClientAPI)
set_target_properties(MikanClientAPI PROPERTIES PUBLIC_HEADER "${MIKAN_CLIENT_API_HEADER}")
set_target_properties(MikanClientAPI PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(MikanClientAPI PRIVATE MIKAN_API_EXPORTS)
target_compile_definitions(MikanClientAPI PRIVATE ENABLE_MIKANAPI_REFLECTION)
target_compile_definitions(MikanClientAPI PRIVATE ENABLE_MIKANCORE_REFLECTION)
target_compile_definitions(MikanClientAPI PRIVATE ENABLE_SERIALIZATION_REFLECTION)
target_compile_definitions(MikanClientAPI PRIVATE JSON_DISABLE_ENUM_SERIALIZATION=1)

if(MSVC)
  target_compile_options(MikanClientAPI PRIVATE /bigobj)
endif()

# Add dependencies
add_dependencies(MikanClientAPI MikanClientCore)
add_dependencies(MikanClientAPI MikanCoreApp)
add_dependencies(MikanClientAPI MikanSerialization)
add_dependencies(MikanClientAPI MikanUtility)

# Create the command to run RefurekuGenerator
add_custom_target(MikanClientAPIReflection
					WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
					COMMAND "${RFK_GENERATOR_EXE}" "${CMAKE_CURRENT_LIST_DIR}/RefurekuSettings.toml") 
set_target_properties(MikanClientAPIReflection PROPERTIES FOLDER ClientAPI)
add_dependencies(MikanClientAPIReflection RefurekuGeneratorRuntime)

# Run the RefurekuGenerator BEFORE building the project to refresh generated files
add_dependencies(MikanClientAPI MikanClientAPIReflection)

# Install - copy runtime dependencies to dist folder (for release)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows") 
    	  
  install(TARGETS MikanClientAPI
      RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH}
      LIBRARY DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/lib
      ARCHIVE DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/lib
      PUBLIC_HEADER DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/include)	  

ELSE() #Linux/Darwin
ENDIF()
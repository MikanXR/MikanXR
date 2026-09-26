## Mikan Serialization DLL
## =======================

MESSAGE(STATUS "Generating Mikan Serialization DLL Project")

file(GLOB MIKAN_SERIALIZATION_PRIVATE_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Private/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/Private/*.cpp"
)

file(GLOB MIKAN_SERIALIZATION_PUBLIC_SRC
	"${CMAKE_CURRENT_LIST_DIR}/Public/*.h"
)

list(APPEND MIKAN_SERIALIZATION_INCL_DIRS
  ${CMAKE_CURRENT_LIST_DIR}/Private
  ${CMAKE_CURRENT_LIST_DIR}/Public
  ${RFK_GENERATED_ROOT_DIR}/MikanSerialization
  ${RFK_GENERATED_ROOT_DIR}/MikanClientAPI
  ${NLOHMANN_JSON_INCLUDE_DIR}
  ${RFK_INCLUDE_DIR})

list(APPEND MIKAN_SERIALIZATION_REQ_LIBS
  ${RFK_LIBRARIES})

add_library(MikanSerialization SHARED ${MIKAN_SERIALIZATION_PRIVATE_SRC})
target_include_directories(MikanSerialization PRIVATE ${MIKAN_SERIALIZATION_INCL_DIRS})
target_link_libraries(MikanSerialization PRIVATE ${MIKAN_SERIALIZATION_REQ_LIBS})
set_target_properties(MikanSerialization PROPERTIES FOLDER MikanLibraries)
set_target_properties(MikanSerialization PROPERTIES PUBLIC_HEADER "${MIKAN_SERIALIZATION_PUBLIC_SRC}")
set_target_properties(MikanSerialization PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(MikanSerialization PROPERTIES C_VISIBILITY_PRESET hidden)
target_compile_definitions(MikanSerialization PRIVATE ENABLE_SERIALIZATION_REFLECTION)
target_compile_definitions(MikanSerialization PRIVATE JSON_DISABLE_ENUM_SERIALIZATION=1)
target_compile_definitions(MikanSerialization PRIVATE SERIALIZATION_EXPORTS)
target_sources(MikanSerialization PRIVATE ${MIKAN_SERIALIZATION_PRIVATE_SRC})
target_sources(MikanSerialization PUBLIC ${MIKAN_SERIALIZATION_PUBLIC_SRC})
source_group("Private" FILES ${MIKAN_SERIALIZATION_PRIVATE_SRC})
source_group("Public" FILES ${MIKAN_SERIALIZATION_PUBLIC_SRC})

# Create the command to run RefurekuGenerator
add_custom_target(MikanSerializationReflection
					WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
					COMMAND "${RFK_GENERATOR_EXE}" "${CMAKE_CURRENT_LIST_DIR}/RefurekuSettings.toml") 
set_target_properties(MikanSerializationReflection PROPERTIES FOLDER MikanLibraries)
add_dependencies(MikanSerializationReflection RefurekuGeneratorRuntime)

# Run the RefurekuGenerator BEFORE building the project to refresh generated files
add_dependencies(MikanSerialization MikanSerializationReflection)

# Install - copy runtime dependencies to dist folder (for release)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows") 
  install(TARGETS MikanSerialization
      RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH}
      LIBRARY DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/lib
      ARCHIVE DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/lib
      PUBLIC_HEADER DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/include)	  
ELSE() #Linux/Darwin
ENDIF()
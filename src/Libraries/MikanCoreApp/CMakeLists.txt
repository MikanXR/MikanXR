## Mikan Core App DLL
## ==================

MESSAGE(STATUS "Generating Mikan Core App DLL Project")

file(GLOB MIKAN_CORE_APP_HEADER
	"${CMAKE_CURRENT_LIST_DIR}/Public/*.h"
)

file(GLOB MIKAN_CORE_APP_PRIVATE_SRC
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.h"
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.cpp"
)

file(GLOB MIKAN_CORE_APP_SRC
	${MIKAN_CORE_APP_HEADER}
	${MIKAN_CORE_APP_PRIVATE_SRC}
)

source_group("Public" FILES ${MIKAN_CORE_APP_HEADER})
source_group("Private" FILES ${MIKAN_CORE_APP_PRIVATE_SRC})

list(APPEND MIKAN_CORE_APP_INCL_DIRS
	${CMAKE_CURRENT_LIST_DIR}/Public
	${CMAKE_CURRENT_LIST_DIR}/Private
	${MIKAN_LIBRARIES_DIR}/MikanUtility/Public
)

list(APPEND MIKAN_CORE_APP_REQ_LIBS
	MikanUtility
	dylib
)

# The crash handler writes minidumps through dbghelp
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	list(APPEND MIKAN_CORE_APP_REQ_LIBS dbghelp)
ENDIF()

add_library(MikanCoreApp SHARED ${MIKAN_CORE_APP_SRC})
target_include_directories(MikanCoreApp PRIVATE ${MIKAN_CORE_APP_INCL_DIRS})
target_compile_definitions(MikanCoreApp PRIVATE MIKAN_COREAPP_EXPORTS)
target_link_libraries(MikanCoreApp PRIVATE ${MIKAN_CORE_APP_REQ_LIBS})
set_target_properties(MikanCoreApp PROPERTIES FOLDER MikanLibraries)
set_target_properties(MikanCoreApp PROPERTIES PUBLIC_HEADER "${MIKAN_CORE_APP_HEADER}")

# Add dependencies
add_dependencies(MikanCoreApp MikanUtility)

# Install - copy runtime dependencies to dist folder (for release)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  install(TARGETS MikanCoreApp
      RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH}
      LIBRARY DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/lib
      ARCHIVE DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/lib
      PUBLIC_HEADER DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/include)
ELSE() #Linux/Darwin
ENDIF()
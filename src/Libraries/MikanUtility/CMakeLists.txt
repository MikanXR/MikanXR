## Mikan Utility DLL
## =================

MESSAGE(STATUS "Generating Mikan Utility DLL Project")

file(GLOB MIKAN_UTILITY_HEADER
	"${CMAKE_CURRENT_LIST_DIR}/Public/*.h"
)

file(GLOB MIKAN_UTILITY_PRIVATE_SRC
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.h"
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.cpp"
)

file(GLOB MIKAN_UTILITY_SRC
	${MIKAN_UTILITY_HEADER}
	${MIKAN_UTILITY_PRIVATE_SRC}
)

source_group("Public" FILES ${MIKAN_UTILITY_HEADER})
source_group("Private" FILES ${MIKAN_UTILITY_PRIVATE_SRC})

list(APPEND MIKAN_UTILITY_INCL_DIRS
	${CMAKE_CURRENT_LIST_DIR}/Public
	${CMAKE_CURRENT_LIST_DIR}/Private
)

add_library(MikanUtility SHARED ${MIKAN_UTILITY_SRC})
target_include_directories(MikanUtility PRIVATE ${MIKAN_UTILITY_INCL_DIRS})
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  # winhttp backs HttpDownloader, bcrypt backs HashUtils::sha256File.
  target_link_libraries(MikanUtility PRIVATE winhttp bcrypt)
ENDIF()
target_compile_definitions(MikanUtility PRIVATE MIKAN_UTILITY_EXPORTS)
set_target_properties(MikanUtility PROPERTIES FOLDER MikanLibraries)
set_target_properties(MikanUtility PROPERTIES PUBLIC_HEADER "${MIKAN_UTILITY_HEADER}")

# Install - copy runtime dependencies to dist folder (for release)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  install(TARGETS MikanUtility
      RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH}
      LIBRARY DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/lib
      ARCHIVE DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/lib
      PUBLIC_HEADER DESTINATION ${MIKAN_ARCH_INSTALL_PATH}/include)
ELSE() #Linux/Darwin
ENDIF()
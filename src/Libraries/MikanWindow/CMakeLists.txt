## Mikan Window DLL
## ==============

MESSAGE(STATUS "Generating Mikan Window DLL Project")

file(GLOB MIKAN_WINDOW_HEADER
	"${CMAKE_CURRENT_LIST_DIR}/Public/*.h"
)

file(GLOB MIKAN_WINDOW_PRIVATE_SRC
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.h"
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.cpp"
)

file(GLOB MIKAN_WINDOW_SRC
	${MIKAN_WINDOW_HEADER}
	${MIKAN_WINDOW_PRIVATE_SRC}
)

source_group("Public" FILES ${MIKAN_WINDOW_HEADER})
source_group("Private" FILES ${MIKAN_WINDOW_PRIVATE_SRC})

list(APPEND MIKAN_WINDOW_INCL_DIRS
	${CMAKE_CURRENT_LIST_DIR}/Public
	${CMAKE_CURRENT_LIST_DIR}/Private
	${MIKAN_LIBRARIES_DIR}/MikanCoreApp/Public
	${MIKAN_LIBRARIES_DIR}/MikanRenderer/Public
	${MIKAN_LIBRARIES_DIR}/MikanUtility/Public
	${ROOT_DIR}/thirdparty/glm/
	${GLEW_INCLUDE_DIRS}
	${SDL2_INCLUDE_DIR}
	${SDL2_IMAGE_INCLUDE_DIR}
	${SDL2TTF_INCLUDE_DIR}
	${EASY_PROFILER_INCLUDE_DIR}
)

list(APPEND MIKAN_WINDOW_REQ_LIBS
	${SDL2_LIBRARY}
	${SDL2_IMAGE_LIBRARY}
	${SDL2TTF_LIBRARY}
	MikanCoreApp
	MikanRenderer
	MikanUtility)

add_library(MikanWindow SHARED ${MIKAN_WINDOW_SRC})
target_link_libraries(MikanWindow ${MIKAN_WINDOW_REQ_LIBS})
target_include_directories(MikanWindow PRIVATE ${MIKAN_WINDOW_INCL_DIRS})
target_compile_definitions(MikanWindow PRIVATE MIKAN_WINDOW_EXPORTS)
set_target_properties(MikanWindow PROPERTIES FOLDER MikanLibraries)

# Install - copy runtime dependencies to dist folder (for release)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  install(TARGETS MikanWindow
      RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH})
ELSE() #Linux/Darwin
ENDIF()

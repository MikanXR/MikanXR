## Mikan GStreamer Video Plugin DLL
## ================================

MESSAGE(STATUS "Generating Mikan GStreamer Video DLL Project")

file(GLOB MIKAN_GSTREAMER_VIDEO_HEADER
	"${CMAKE_CURRENT_LIST_DIR}/Public/*.h"
)

file(GLOB MIKAN_GSTREAMER_VIDEO_PRIVATE_SRC
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.h"
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.cpp"
)

file(GLOB MIKAN_GSTREAMER_VIDEO_SRC
	${MIKAN_GSTREAMER_VIDEO_HEADER}
	${MIKAN_GSTREAMER_VIDEO_PRIVATE_SRC}
)

source_group("Public" FILES ${MIKAN_GSTREAMER_VIDEO_HEADER})
source_group("Private" FILES ${MIKAN_GSTREAMER_VIDEO_PRIVATE_SRC})

list(APPEND MIKAN_GSTREAMER_VIDEO_INCL_DIRS
	${CMAKE_CURRENT_LIST_DIR}/Public
	${CMAKE_CURRENT_LIST_DIR}/Private
	${MIKAN_LIBRARIES_DIR}/MikanCoreApp/Public
	${MIKAN_LIBRARIES_DIR}/MikanUtility/Public
	${GSTREAMER_INCLUDE_DIR}
	${GLIB2_INCLUDE_DIR}
)

list(APPEND MIKAN_GSTREAMER_VIDEO_REQ_LIBS
	${GLIB2_LIBRARIES}
	${GOBJECT_LIBRARIES}
	${GSTREAMER_LIBRARY}
	${GSTREAMER_APP_LIBRARY}
	${GSTREAMER_BASE_LIBRARY}
	${GSTREAMER_VIDEO_LIBRARY}
	MikanCoreApp
	MikanUtility)

add_library(MikanGStreamerVideo SHARED ${MIKAN_GSTREAMER_VIDEO_SRC})
target_include_directories(MikanGStreamerVideo PRIVATE ${MIKAN_GSTREAMER_VIDEO_INCL_DIRS})
target_link_libraries(MikanGStreamerVideo PRIVATE ${MIKAN_GSTREAMER_VIDEO_REQ_LIBS})
set_target_properties(MikanGStreamerVideo PROPERTIES FOLDER Plugins)
set_target_properties(MikanGStreamerVideo PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(MikanGStreamerVideo PRIVATE MIKAN_GSTREAMER_VIDEO_EXPORTS)

# Install - copy runtime dependencies to dist folder (for release)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	install(TARGETS MikanGStreamerVideo
		RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH})
ELSE() #Linux/Darwin
ENDIF()
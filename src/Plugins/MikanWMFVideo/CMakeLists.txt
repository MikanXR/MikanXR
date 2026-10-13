## Mikan WMF Video Plugin DLL
## ==========================

MESSAGE(STATUS "Generating Mikan WMF Video DLL Project")

file(GLOB MIKAN_WMF_VIDEO_HEADER
	"${CMAKE_CURRENT_LIST_DIR}/Public/*.h"
)

file(GLOB MIKAN_WMF_VIDEO_PRIVATE_SRC
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.h"
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.cpp"
)

file(GLOB MIKAN_WMF_VIDEO_SRC
	${MIKAN_WMF_VIDEO_HEADER}
	${MIKAN_WMF_VIDEO_PRIVATE_SRC}
)

source_group("Public" FILES ${MIKAN_WMF_VIDEO_HEADER})
source_group("Private" FILES ${MIKAN_WMF_VIDEO_PRIVATE_SRC})

list(APPEND MIKAN_WMF_VIDEO_INCL_DIRS
	${CMAKE_CURRENT_LIST_DIR}/Public
	${CMAKE_CURRENT_LIST_DIR}/Private
	${MIKAN_LIBRARIES_DIR}/MikanCoreApp/Public
	${MIKAN_LIBRARIES_DIR}/MikanUtility/Public
)

list(APPEND WINDOWS_MEDIA_FOUNDATION_LIBS mfplat mf mfuuid mfreadwrite wmcodecdspuuid winmm shlwapi)

list(APPEND MIKAN_WMF_VIDEO_REQ_LIBS
	${WINDOWS_MEDIA_FOUNDATION_LIBS}
	MikanCoreApp
	MikanUtility)

add_library(MikanWMFVideo SHARED ${MIKAN_WMF_VIDEO_SRC})
target_include_directories(MikanWMFVideo PRIVATE ${MIKAN_WMF_VIDEO_INCL_DIRS})
target_link_libraries(MikanWMFVideo PRIVATE ${MIKAN_WMF_VIDEO_REQ_LIBS})
set_target_properties(MikanWMFVideo PROPERTIES FOLDER Plugins)
set_target_properties(MikanWMFVideo PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(MikanWMFVideo PRIVATE MIKAN_WMF_VIDEO_EXPORTS)

# Install - copy runtime dependencies to dist folder (for release)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	install(TARGETS MikanWMFVideo
		RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH})
ELSE() #Linux/Darwin
ENDIF()
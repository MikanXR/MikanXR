## Mikan SteamVR Plugin DLL
## =======================

MESSAGE(STATUS "Generating Mikan SteamVR DLL Project")

file(GLOB MIKAN_STEAMVR_HEADER
	"${CMAKE_CURRENT_LIST_DIR}/Public/*.h"
)

file(GLOB MIKAN_STEAMVR_PRIVATE_SRC
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.h"
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.cpp"
)

file(GLOB MIKAN_STEAMVR_SRC
	${MIKAN_STEAMVR_HEADER}
	${MIKAN_STEAMVR_PRIVATE_SRC}
)

source_group("Public" FILES ${MIKAN_STEAMVR_HEADER})
source_group("Private" FILES ${MIKAN_STEAMVR_PRIVATE_SRC})

list(APPEND MIKAN_STEAMVR_INCL_DIRS
	${CMAKE_CURRENT_LIST_DIR}/Public
	${CMAKE_CURRENT_LIST_DIR}/Private
	${MIKAN_LIBRARIES_DIR}/MikanCoreApp/Public
	${MIKAN_LIBRARIES_DIR}/MikanWindow/Public
	${MIKAN_LIBRARIES_DIR}/MikanRenderer/Public
	${MIKAN_LIBRARIES_DIR}/MikanUtility/Public
	${OPENVR_INCLUDE_DIR}
	${ROOT_DIR}/thirdparty/glm/
)

list(APPEND MIKAN_GSTREAMER_REQ_LIBS
	${OPENVR_LIBRARY}
	MikanCoreApp
	MikanRenderer
	MikanWindow
	MikanUtility)

add_library(MikanSteamVR SHARED ${MIKAN_STEAMVR_SRC})
target_include_directories(MikanSteamVR PRIVATE ${MIKAN_STEAMVR_INCL_DIRS})
target_link_libraries(MikanSteamVR PRIVATE ${MIKAN_GSTREAMER_REQ_LIBS})
set_target_properties(MikanSteamVR PROPERTIES FOLDER Plugins)
set_target_properties(MikanSteamVR PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(MikanSteamVR PRIVATE MIKAN_STEAMVR_EXPORTS)


# Install - copy runtime dependencies to dist folder (for release)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	install(TARGETS MikanSteamVR
		RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH})
ELSE() #Linux/Darwin
ENDIF()
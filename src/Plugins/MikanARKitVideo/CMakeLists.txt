## Mikan ARKit Video Plugin DLL
## ============================

MESSAGE(STATUS "Generating Mikan ARKit Video DLL Project")

file(GLOB MIKAN_ARKIT_VIDEO_HEADER
	"${CMAKE_CURRENT_LIST_DIR}/Public/*.h"
)

file(GLOB MIKAN_ARKIT_VIDEO_PRIVATE_SRC
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.h"
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.cpp"
)

# Non-recursive GLOB above doesn't reach Private/Cuda/ - globbed separately.
# Plain CUDA Driver API C++ (CudaGLInterop, CudaErrorHandling) - no .cu/PTX build
# step needed since "Phase 6" replaced the NV12->RGBA CUDA kernel with a GLSL
# shader pass on the Editor side; this plugin now only does CUDA-GL texture
# registration and device-to-array plane copies.
file(GLOB MIKAN_ARKIT_VIDEO_CUDA_SRC
	"${CMAKE_CURRENT_LIST_DIR}/Private/Cuda/*.h"
	"${CMAKE_CURRENT_LIST_DIR}/Private/Cuda/*.cpp"
)

file(GLOB MIKAN_ARKIT_VIDEO_SRC
	${MIKAN_ARKIT_VIDEO_HEADER}
	${MIKAN_ARKIT_VIDEO_PRIVATE_SRC}
	${MIKAN_ARKIT_VIDEO_CUDA_SRC}
)

source_group("Public" FILES ${MIKAN_ARKIT_VIDEO_HEADER})
source_group("Private" FILES ${MIKAN_ARKIT_VIDEO_PRIVATE_SRC})
source_group("Private\\Cuda" FILES ${MIKAN_ARKIT_VIDEO_CUDA_SRC})

list(APPEND MIKAN_ARKIT_VIDEO_INCL_DIRS
	${CMAKE_CURRENT_LIST_DIR}/Public
	${CMAKE_CURRENT_LIST_DIR}/Private
	${MIKAN_LIBRARIES_DIR}/MikanCoreApp/Public
	${MIKAN_LIBRARIES_DIR}/MikanRenderer/Public
	${MIKAN_LIBRARIES_DIR}/MikanUtility/Public
	${GSTREAMER_INCLUDE_DIR}
	${GSTREAMER_RTP_INCLUDE_DIR}
	${GSTREAMER_CUDA_INCLUDE_DIR}
	${CUDA_TOOLKIT_INCLUDE_DIR}
	${GLIB2_INCLUDE_DIR}
	${GLEW_INCLUDE_DIRS}
)

list(APPEND MIKAN_ARKIT_VIDEO_REQ_LIBS
	${GLIB2_LIBRARIES}
	${GOBJECT_LIBRARIES}
	${GSTREAMER_LIBRARY}
	${GSTREAMER_APP_LIBRARY}
	${GSTREAMER_BASE_LIBRARY}
	${GSTREAMER_VIDEO_LIBRARY}
	${GSTREAMER_RTP_LIBRARY}
	${GSTREAMER_CUDA_LIBRARY}
	${CUDA_DRIVER_LIBRARY}
	${GLEW_LIBRARIES}
	MikanCoreApp
	MikanRenderer
	MikanUtility)

add_library(MikanARKitVideo SHARED ${MIKAN_ARKIT_VIDEO_SRC})
target_include_directories(MikanARKitVideo PRIVATE ${MIKAN_ARKIT_VIDEO_INCL_DIRS})
target_link_libraries(MikanARKitVideo PRIVATE ${MIKAN_ARKIT_VIDEO_REQ_LIBS})
set_target_properties(MikanARKitVideo PROPERTIES FOLDER Plugins)
set_target_properties(MikanARKitVideo PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(MikanARKitVideo PRIVATE MIKAN_ARKIT_VIDEO_EXPORTS)

# Install - copy runtime dependencies to dist folder (for release)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	install(TARGETS MikanARKitVideo
		RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH})
ELSE() #Linux/Darwin
ENDIF()

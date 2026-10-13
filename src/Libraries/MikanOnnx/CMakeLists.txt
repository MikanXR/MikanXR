## MikanOnnx Static Library
## ========================
## Thin ONNX Runtime wrapper with DirectML-first execution provider selection.
## No Editor/ECS dependencies - only the logger from MikanCoreApp.

MESSAGE(STATUS "Generating MikanOnnx Static Library")

file(GLOB MIKAN_ONNX_HEADER
	"${CMAKE_CURRENT_LIST_DIR}/Public/*.h"
)

file(GLOB MIKAN_ONNX_PRIVATE_SRC
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.h"
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.cpp"
)

file(GLOB MIKAN_ONNX_SRC
	${MIKAN_ONNX_HEADER}
	${MIKAN_ONNX_PRIVATE_SRC}
)

source_group("Public"  FILES ${MIKAN_ONNX_HEADER})
source_group("Private" FILES ${MIKAN_ONNX_PRIVATE_SRC})

add_library(MikanOnnx STATIC ${MIKAN_ONNX_SRC})

target_include_directories(MikanOnnx
	PUBLIC  ${CMAKE_CURRENT_LIST_DIR}/Public
			${ONNXRUNTIME_INCLUDE_DIRS}
	PRIVATE ${CMAKE_CURRENT_LIST_DIR}/Private
			${MIKAN_LIBRARIES_DIR}/MikanCoreApp/Public
)

target_link_libraries(MikanOnnx
	PUBLIC  ${ONNXRUNTIME_LIBRARIES}
	PRIVATE MikanCoreApp
)

set_target_properties(MikanOnnx PROPERTIES FOLDER MikanLibraries)

## MikanDMX Static Library
## =======================
## Standalone E1.31 (sACN) DMX sender. No Editor/ECS dependencies.

MESSAGE(STATUS "Generating MikanDMX Static Library")

file(GLOB MIKAN_DMX_HEADER
	"${CMAKE_CURRENT_LIST_DIR}/Public/*.h"
)

file(GLOB MIKAN_DMX_PRIVATE_SRC
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.h"
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.cpp"
)

file(GLOB MIKAN_DMX_SRC
	${MIKAN_DMX_HEADER}
	${MIKAN_DMX_PRIVATE_SRC}
)

source_group("Public"  FILES ${MIKAN_DMX_HEADER})
source_group("Private" FILES ${MIKAN_DMX_PRIVATE_SRC})

add_library(MikanDMX STATIC ${MIKAN_DMX_SRC})

target_include_directories(MikanDMX
	PUBLIC  ${CMAKE_CURRENT_LIST_DIR}/Public
	PRIVATE ${CMAKE_CURRENT_LIST_DIR}/Private
)

if(WIN32)
	target_link_libraries(MikanDMX PRIVATE Ws2_32)
endif()

set_target_properties(MikanDMX PROPERTIES FOLDER MikanLibraries)

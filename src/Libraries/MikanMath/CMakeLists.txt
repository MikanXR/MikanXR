## Mikan Math DLL
## ==============

MESSAGE(STATUS "Generating Mikan Math DLL Project")

file(GLOB MIKAN_MATH_HEADER
	"${CMAKE_CURRENT_LIST_DIR}/Public/*.h"
)

file(GLOB MIKAN_MATH_PRIVATE_SRC
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.h"
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.cpp"
)

file(GLOB MIKAN_MATH_SRC
	${MIKAN_MATH_HEADER}
	${MIKAN_MATH_PRIVATE_SRC}
)

source_group("Public" FILES ${MIKAN_MATH_HEADER})
source_group("Private" FILES ${MIKAN_MATH_PRIVATE_SRC})

list(APPEND MIKAN_MATH_INCL_DIRS
	${CMAKE_CURRENT_LIST_DIR}/Public
	${CMAKE_CURRENT_LIST_DIR}/Private
	${MIKAN_LIBRARIES_DIR}/MikanClientAPI/Public
	${MIKAN_LIBRARIES_DIR}/MikanSerialization/Public
	${ROOT_DIR}/thirdparty/glm/
)

list(APPEND MIKAN_MATH_REQ_LIBS
  MikanClientAPI
  MikanSerialization)

add_library(MikanMath SHARED ${MIKAN_MATH_SRC})
target_include_directories(MikanMath PRIVATE ${MIKAN_MATH_INCL_DIRS})
target_link_libraries(MikanMath PRIVATE ${MIKAN_MATH_REQ_LIBS})
target_compile_definitions(MikanMath PRIVATE MIKAN_MATH_EXPORTS)
set_target_properties(MikanMath PROPERTIES FOLDER MikanLibraries)

# Install - copy runtime dependencies to dist folder (for release)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  install(TARGETS MikanMath
      RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH})
ELSE() #Linux/Darwin
ENDIF()
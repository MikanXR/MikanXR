## Mikan Renderer DLL
## ==============

MESSAGE(STATUS "Generating Mikan Renderer DLL Project")

# OpenGL Libraries
set(OPENGL_LIBS "")
if (WIN32) 
  list(APPEND OPENGL_LIBS opengl32)
elseif (APPLE)
  find_library(opengl_library OpenGL)
  list(APPEND OPENGL_LIBS ${opengl_library})
elseif(CMAKE_SYSTEM MATCHES "Linux")
  list(APPEND OPENGL_LIBS GL)
endif()
list(APPEND OPENGL_LIBS ${GLEW_LIBRARIES})

file(GLOB MIKAN_RENDERER_HEADER
	"${CMAKE_CURRENT_LIST_DIR}/Public/*.h"
)

file(GLOB MIKAN_RENDERER_PRIVATE_SRC
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.h"
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.cpp"
)

file(GLOB MIKAN_RENDERER_SRC
	${MIKAN_RENDERER_HEADER}
	${MIKAN_RENDERER_PRIVATE_SRC}
)

source_group("Public" FILES ${MIKAN_RENDERER_HEADER})
source_group("Private" FILES ${MIKAN_RENDERER_PRIVATE_SRC})

list(APPEND MIKAN_RENDERER_INCL_DIRS
	${CMAKE_CURRENT_LIST_DIR}/Public
	${CMAKE_CURRENT_LIST_DIR}/Private
	${MIKAN_LIBRARIES_DIR}/MikanCoreApp/Public
	${MIKAN_LIBRARIES_DIR}/MikanUtility/Public
	${ROOT_DIR}/thirdparty/glm/
	${GLEW_INCLUDE_DIRS}
	${STB_INCLUDE_DIRS}
)

list(APPEND MIKAN_RENDERER_REQ_LIBS 
  ${OPENGL_LIBS}
  MikanCoreApp
  MikanUtility)

 # Remove problematic files from the unity builds (header file inclusion ordering issues)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/Private/GlGraphicsContext.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

add_library(MikanRenderer SHARED ${MIKAN_RENDERER_SRC})
target_link_libraries(MikanRenderer ${MIKAN_RENDERER_REQ_LIBS})
target_include_directories(MikanRenderer PRIVATE ${MIKAN_RENDERER_INCL_DIRS})
target_compile_definitions(MikanRenderer PRIVATE MIKAN_RENDERER_EXPORTS)
set_target_properties(MikanRenderer PROPERTIES FOLDER MikanLibraries)

# Install - copy runtime dependencies to dist folder (for release)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  install(TARGETS MikanRenderer
      RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH})
ELSE() #Linux/Darwin
ENDIF()
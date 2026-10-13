## Mikan GUI DLL
## =================

MESSAGE(STATUS "Generating Mikan GUI DLL Project")

file(GLOB MIKAN_GUI_HEADER
	"${CMAKE_CURRENT_LIST_DIR}/Public/*.h"
)

file(GLOB MIKAN_GUI_PRIVATE_SRC
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.h"
	"${CMAKE_CURRENT_LIST_DIR}/Private/*.cpp"
)

file(GLOB MIKAN_GUI_SRC
	${IMGUI_SOURCE}
	${IMGUI_NODE_EDITOR_SOURCE}
	${MIKAN_GUI_HEADER}
	${MIKAN_GUI_PRIVATE_SRC}
)

# ImGUI's source files are included in directly, rather than linking against a lib
source_group("ImGui" FILES ${IMGUI_SOURCE})

# imgui-node-editor's source files are included in directly, rather than linking against a lib
source_group("ImGuiNodeEditor" FILES ${IMGUI_NODE_EDITOR_SOURCE})

source_group("Public" FILES ${MIKAN_GUI_HEADER})
source_group("Private" FILES ${MIKAN_GUI_PRIVATE_SRC})

list(APPEND MIKAN_GUI_INCL_DIRS
	${CMAKE_CURRENT_LIST_DIR}/Public
	${CMAKE_CURRENT_LIST_DIR}/Private
	${MIKAN_LIBRARIES_DIR}/MikanCoreApp/Public
	${MIKAN_LIBRARIES_DIR}/MikanWindow/Public
	${MIKAN_LIBRARIES_DIR}/MikanRenderer/Public
	${MIKAN_LIBRARIES_DIR}/MikanUtility/Public
	${ROOT_DIR}/thirdparty/glm/
	${GLEW_INCLUDE_DIRS}
	${IMGUI_DIR}
	${IMGUI_NODE_EDITOR_DIR}
	${NLOHMANN_JSON_INCLUDE_DIR}
	${SDL2_INCLUDE_DIR}
	${SDL2_IMAGE_INCLUDE_DIR}
	${SDL2TTF_INCLUDE_DIR}
)

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

list(APPEND MIKAN_GUI_REQ_LIBS
	${OPENGL_LIBS}
	${SDL2_LIBRARY}
	${SDL2_IMAGE_LIBRARY}
	${SDL2TTF_LIBRARY}
	MikanCoreApp
	MikanRenderer
	MikanUtility
	MikanWindow)

# Remove problematic files from the unity builds (header file inclusion ordering issues)
set_source_files_properties(${IMGUI_SOURCE} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
set_source_files_properties(${IMGUI_NODE_EDITOR_SOURCE} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

add_library(MikanGUI SHARED ${MIKAN_GUI_SRC})
target_include_directories(MikanGUI PRIVATE ${MIKAN_GUI_INCL_DIRS})
target_link_libraries(MikanGUI ${MIKAN_GUI_REQ_LIBS})
target_compile_definitions(MikanGUI PRIVATE MIKAN_GUI_EXPORTS)
# imgui_internal.h requires the math operators to be defined before imgui.h in
# every TU, so define them target-wide rather than per-file (fragile under
# unity builds)
target_compile_definitions(MikanGUI PRIVATE IMGUI_DEFINE_MATH_OPERATORS)
IF(WIN32)
  target_compile_definitions(MikanGUI
    PRIVATE   "IMGUI_API=__declspec(dllexport)" "IMGUI_NODE_EDITOR_API=__declspec(dllexport)"
    INTERFACE "IMGUI_API=__declspec(dllimport)" "IMGUI_NODE_EDITOR_API=__declspec(dllimport)")
ENDIF()
set_target_properties(MikanGUI PROPERTIES FOLDER MikanLibraries)

# Install - copy runtime dependencies to dist folder (for release)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  install(TARGETS MikanGUI
      RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH})
ELSE() #Linux/Darwin
ENDIF()
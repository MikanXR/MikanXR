cmake_minimum_required(VERSION 3.15)

set(ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

# Version.cmake parses src/Editor/AppCore/Version.h and needs nothing but ROOT_DIR, so it
# runs first and MIKAN_VERSION_STRING is available to everything that names a release.
include(cmake/Version.cmake)

set(MIKAN_PROJECT_NAME "Mikan_${MIKAN_VERSION_STRING}")
set(MIKAN_ROOT_INSTALL_PATH "${ROOT_DIR}/dist")
file(MAKE_DIRECTORY "${MIKAN_ROOT_INSTALL_PATH}")
file(WRITE "${MIKAN_ROOT_INSTALL_PATH}/version.txt" "${MIKAN_PROJECT_NAME}")

# Any third party dependencies will install here
set(CMAKE_INSTALL_PREFIX ${MIKAN_ROOT_INSTALL_PATH}/deps)

project(Mikan LANGUAGES C CXX VERSION 1.0.0)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(cmake/Environment.cmake)
include(cmake/ThirdParty.cmake)

# Mikan will install here
# ARCH_LABEL set in Environment.cmake
set(MIKAN_ARCH_INSTALL_PATH "${MIKAN_ROOT_INSTALL_PATH}/${ARCH_LABEL}")

include(cmake/Installer.cmake)
include(cmake/ClangFormat.cmake)
include(cmake/Localization.cmake)

# Step into the subdirectories
add_subdirectory(src)
add_subdirectory(bindings)
add_subdirectory(thirdparty)

# Walks the targets defined above, so it has to come after them
include(cmake/Symbols.cmake)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CheckCXXCompilerFlag)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Shared architecture label used for install folder locations
if (${CMAKE_C_SIZEOF_DATA_PTR} EQUAL 8)
    set(BITNESS "64")
    if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        set(ARCH_LABEL "Win64")
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(ARCH_LABEL "OSX64")
    else()
        set(ARCH_LABEL "Linux64")
    endif()
else()
    set(BITNESS "32")
    if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        set(ARCH_LABEL "Win32")
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(ARCH_LABEL "OSX32")
    else()
        set(ARCH_LABEL "Linux32")
    endif()
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()
string(TOUPPER "${CMAKE_BUILD_TYPE}" U_CMAKE_BUILD_TYPE)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Quench annoying deprecation warnings when compiling GLFW on OSX
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-declarations")
endif()

if (MSVC)
  # Disable annoying MSVC warnings (all targets)
  add_definitions(/D "_CRT_SECURE_NO_WARNINGS")
  add_definitions(/D "_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING")

  # Prevent Windows.h from defining min/max macros that conflict with std::min/std::max
  add_definitions(/D "NOMINMAX")

  # Parallel build on MSVC (all targets)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

  if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:SSE2")
  endif()

  # Release keeps full symbols so crash minidumps symbolicate. /Z7 rather than /Zi
  # because sccache and unity builds need the debug info embedded in the object files.
  # /OPT:REF and /OPT:ICF keep the optimized code identical to a plain Release link,
  # which /DEBUG would otherwise turn off.
  #
  # These are directory-scoped options rather than additions to CMAKE_CXX_FLAGS_RELEASE
  # because cmake/cef_variables.cmake clears CMAKE_CXX_FLAGS_RELEASE under Ninja in the
  # scope where find_package(CEF) runs.
  # The COMPILE_LANGUAGE guard keeps /Z7 off the C# projects, whose compiler rejects it.
  add_compile_options($<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/Z7>)
  add_link_options($<$<CONFIG:Release>:/DEBUG:FULL>)
  add_link_options($<$<CONFIG:Release>:/OPT:REF>)
  add_link_options($<$<CONFIG:Release>:/OPT:ICF>)
endif()

# Compile with compiler warnings turned on
if(MSVC)
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244 /wd4100 /wd4101 /wd4018 /wd4201 /wd4189 /wd4457 /wd4456 /wd4245 /wd4838 /wd4389 /wd4702")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  CHECK_CXX_COMPILER_FLAG("-std=c++14" HAS_CPP14_FLAG)
  CHECK_CXX_COMPILER_FLAG("-std=c++11" HAS_CPP11_FLAG)

  if (HAS_CPP14_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
  elseif (HAS_CPP11_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  else()
    message(FATAL_ERROR "Unsupported compiler -- requires C++11 support!")
  endif()
endif()
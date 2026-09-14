# Step into the subdirectories
MESSAGE(STATUS "Stepping into fast_obj")
add_subdirectory(fast_obj)
set_target_properties(fast_obj_lib PROPERTIES FOLDER ThirdParty)

MESSAGE(STATUS "Stepping into IxWebSocket")
add_subdirectory(IxWebSocket)
set_target_properties(ixwebsocket PROPERTIES FOLDER ThirdParty)

# CEF libcef_dll_wrapper
if(DEFINED CEF_ROOT)
    MESSAGE(STATUS "Stepping into CEF libcef_dll_wrapper")
    # Normalize CEF_ROOT path
    file(TO_CMAKE_PATH "${CEF_ROOT}" CEF_ROOT)
    # Set CMAKE_MODULE_PATH for CEF
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
    # Override CEF's default /MT runtime with /MD to match Mikan's dynamic CRT.
    # CEF_RUNTIME_LIBRARY_FLAG is a CACHE string consumed by cef_variables.cmake before
    # libcef_dll_wrapper is configured, so setting it here (with FORCE) propagates correctly.
    set(CEF_RUNTIME_LIBRARY_FLAG "/MD" CACHE STRING "Use dynamic CRT to match Mikan" FORCE)
    # CEF defaults to /Zi, which writes a shared PDB the compiler serializes on. Mikan
    # builds everything else with /Z7 (Environment.cmake) so sccache can cache the object
    # files and unity builds stay correct, so override CEF's flag the same way.
    set(CEF_DEBUG_INFO_FLAG "/Z7" CACHE STRING "Embed debug info so sccache and unity builds work" FORCE)
    # Find CEF package
    find_package(CEF REQUIRED)
    # Add libcef_dll_wrapper
    add_subdirectory(${CEF_ROOT}/libcef_dll ${CMAKE_CURRENT_BINARY_DIR}/libcef_dll_wrapper)
    set_target_properties(libcef_dll_wrapper PROPERTIES FOLDER ThirdParty)
endif()
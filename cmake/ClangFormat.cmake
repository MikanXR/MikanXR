# ClangFormat.cmake
#
# Adds two convenience targets that delegate to cmake/RunClangFormat.cmake
# (single source of truth, shared with the CI format-check job):
#
#   cmake --build build --target FormatFix     # reformat all sources in place
#   cmake --build build --target FormatCheck   # verify formatting (fails on violations)
#
# The targets are ALWAYS created so they show up in the IDE. Locating
# clang-format is left to RunClangFormat.cmake when the targets are built, so
# a normal build that never invokes them is unaffected. Setting
# CLANG_FORMAT_EXE at configure time forwards that path instead.

if(CLANG_FORMAT_EXE)
	set(_cf_arg "-DCLANG_FORMAT_EXE=${CLANG_FORMAT_EXE}")
else()
	set(_cf_arg "")
endif()

set(_run_script "${CMAKE_CURRENT_LIST_DIR}/RunClangFormat.cmake")

add_custom_target(FormatFix
	COMMAND ${CMAKE_COMMAND} ${_cf_arg}
		-P "${_run_script}" -- --fix
	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
	COMMENT "Reformatting sources with clang-format")

add_custom_target(FormatCheck
	COMMAND ${CMAKE_COMMAND} ${_cf_arg}
		-P "${_run_script}" -- --check
	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
	COMMENT "Checking source formatting with clang-format")

# Group FormatFix / FormatCheck alongside ALL_BUILD / ZERO_CHECK / INSTALL in the IDE
# (Visual Studio, Xcode). USE_FOLDERS is enabled globally in Environment.cmake.
# CMAKE_PREDEFINED_TARGETS_FOLDER defaults to "CMakePredefinedTargets" when unset.
# Harmless no-op for non-IDE generators like Ninja.
if(CMAKE_PREDEFINED_TARGETS_FOLDER)
	set(_predef_folder "${CMAKE_PREDEFINED_TARGETS_FOLDER}")
else()
	set(_predef_folder "CMakePredefinedTargets")
endif()
set_target_properties(FormatFix FormatCheck PROPERTIES FOLDER "${_predef_folder}")

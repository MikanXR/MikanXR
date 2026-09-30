# Localization.cmake
#
# Adds two convenience targets that delegate to tools/localization.py
# (single source of truth, shared with the CI localization-check job):
#
#   cmake --build build --target LocalizationSync    # regenerate the JSON tables from the catalogs
#   cmake --build build --target LocalizationCheck   # verify they are current (fails on drift)
#
# Adding a UI string means editing resources/localization/en.json and running
# LocalizationSync, which folds the new key into every gettext catalog and
# rewrites the generated tables. LocalizationCheck is what CI runs.
#
# The targets are ALWAYS created so they show up in the IDE. If Python or polib
# is missing at build time the script says so; a normal build that never
# invokes them is unaffected.

# The repo-local virtual environment tools/SetupDevEnvironment.bat creates carries
# polib, so it wins over whatever Python is on PATH.
if(WIN32)
	set(_loc_venv_python "${CMAKE_SOURCE_DIR}/.venv/Scripts/python.exe")
else()
	set(_loc_venv_python "${CMAKE_SOURCE_DIR}/.venv/bin/python")
endif()

if(EXISTS "${_loc_venv_python}")
	set(_loc_python "${_loc_venv_python}")
	message(STATUS "Python found: ${_loc_python} (repo .venv; 'LocalizationSync'/'LocalizationCheck' targets enabled)")
else()
	find_package(Python3 COMPONENTS Interpreter QUIET)
endif()

if(_loc_python)
	# Set from the venv above
elseif(Python3_Interpreter_FOUND)
	set(_loc_python "${Python3_EXECUTABLE}")
	message(STATUS "Python found: ${_loc_python} ('LocalizationSync'/'LocalizationCheck' targets enabled)")
else()
	set(_loc_python "python")
	message(STATUS "Python not found at configure time; 'LocalizationSync'/'LocalizationCheck' targets will look for it on PATH when built.")
endif()

set(_loc_script "${CMAKE_SOURCE_DIR}/tools/localization.py")

add_custom_target(LocalizationSync
	COMMAND "${_loc_python}" "${_loc_script}" sync
	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
	COMMENT "Regenerating the localization tables from the gettext catalogs")

add_custom_target(LocalizationCheck
	COMMAND "${_loc_python}" "${_loc_script}" check
	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
	COMMENT "Checking the localization tables against the gettext catalogs")

# Group these alongside ALL_BUILD / ZERO_CHECK / INSTALL in the IDE the same way
# FormatFix / FormatCheck are (see ClangFormat.cmake).
if(CMAKE_PREDEFINED_TARGETS_FOLDER)
	set(_predef_folder "${CMAKE_PREDEFINED_TARGETS_FOLDER}")
else()
	set(_predef_folder "CMakePredefinedTargets")
endif()
set_target_properties(LocalizationSync LocalizationCheck PROPERTIES FOLDER "${_predef_folder}")

# RunClangFormat.cmake
#
# Standalone clang-format driver, runnable WITHOUT a configured build tree:
#
#   cmake -P cmake/RunClangFormat.cmake -- --check   # verify (nonzero exit on violations)
#   cmake -P cmake/RunClangFormat.cmake -- --fix     # reformat in place
#
# This same script backs the `format` / `format-check` CMake targets (see
# cmake/ClangFormat.cmake) and the CI format-check job, so local and CI always
# run identical logic. Sources are globbed at run time (under src/ only), so
# newly added files are always covered and thirdparty/ is never touched.
#
# Override the tool with -DCLANG_FORMAT_EXE=/path/to/clang-format before -P.

cmake_minimum_required(VERSION 3.15)

# This script lives in <repo>/cmake, so the repo root is one level up.
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# -- Parse mode (default: check) --------------------------------------------
set(MODE "check")
math(EXPR _argc_minus_1 "${CMAKE_ARGC} - 1")
foreach(i RANGE 0 ${_argc_minus_1})
	if("${CMAKE_ARGV${i}}" STREQUAL "--fix")
		set(MODE "fix")
	elseif("${CMAKE_ARGV${i}}" STREQUAL "--check")
		set(MODE "check")
	endif()
endforeach()

# -- Locate clang-format ----------------------------------------------------
# CI formats with clang-format 19.1.x and other major versions format
# differently, so the first 19.x among the candidates wins. In order:
# the copy InitialSetup_x64.bat puts in deps/, PATH, the copies Visual Studio
# bundles (VC/Tools/Llvm/bin in 2022, VC/Tools/Llvm/x64/bin in 2026), and a
# standalone LLVM install. An explicit -DCLANG_FORMAT_EXE skips the search.
set(REQUIRED_CLANG_FORMAT_MAJOR 19)
if(NOT CLANG_FORMAT_EXE)
	set(_cf_candidates "")
	foreach(_cf_path
			"${REPO_ROOT}/deps/clang-format/clang-format.exe"
			"${REPO_ROOT}/deps/clang-format/clang-format")
		if(EXISTS "${_cf_path}")
			list(APPEND _cf_candidates "${_cf_path}")
		endif()
	endforeach()

	find_program(_cf_on_path NAMES clang-format NO_CACHE)
	if(_cf_on_path)
		list(APPEND _cf_candidates "${_cf_on_path}")
	endif()

	# One glob per location, since a single glob returns its matches sorted
	# and would put a standalone LLVM ahead of Visual Studio.
	foreach(_cf_pattern
			"$ENV{ProgramFiles}/Microsoft Visual Studio/*/*/VC/Tools/Llvm/bin/clang-format.exe"
			"$ENV{ProgramFiles}/Microsoft Visual Studio/*/*/VC/Tools/Llvm/x64/bin/clang-format.exe"
			"$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/*/*/VC/Tools/Llvm/bin/clang-format.exe"
			"$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/*/*/VC/Tools/Llvm/x64/bin/clang-format.exe"
			"$ENV{ProgramFiles}/LLVM/bin/clang-format.exe")
		file(GLOB _cf_installed "${_cf_pattern}")
		list(APPEND _cf_candidates ${_cf_installed})
	endforeach()
	list(REMOVE_DUPLICATES _cf_candidates)

	foreach(_cf_candidate IN LISTS _cf_candidates)
		execute_process(COMMAND "${_cf_candidate}" --version
			OUTPUT_VARIABLE _cf_candidate_version OUTPUT_STRIP_TRAILING_WHITESPACE
			RESULT_VARIABLE _cf_rv)
		if(_cf_rv EQUAL 0 AND _cf_candidate_version MATCHES "version ${REQUIRED_CLANG_FORMAT_MAJOR}\\.")
			set(CLANG_FORMAT_EXE "${_cf_candidate}")
			break()
		endif()
	endforeach()

	if(NOT CLANG_FORMAT_EXE AND _cf_candidates)
		list(GET _cf_candidates 0 CLANG_FORMAT_EXE)
		message(WARNING
			"No clang-format ${REQUIRED_CLANG_FORMAT_MAJOR}.x found, using ${CLANG_FORMAT_EXE}. "
			"CI checks with clang-format 19.1.x, so its results can differ. "
			"Rerun InitialSetup_x64.bat to get deps/clang-format.")
	endif()
endif()
if(NOT CLANG_FORMAT_EXE)
	message(FATAL_ERROR
		"clang-format not found. Rerun InitialSetup_x64.bat to get deps/clang-format, "
		"install clang-format 19.1.5 on PATH (e.g. `pip install clang-format==19.1.5`), "
		"or pass -DCLANG_FORMAT_EXE=/path/to/clang-format.")
endif()

execute_process(COMMAND "${CLANG_FORMAT_EXE}" --version
	OUTPUT_VARIABLE _cf_version OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "Using ${_cf_version} (${CLANG_FORMAT_EXE})")

# -- Collect sources under src/ ---------------------------------------------
file(GLOB_RECURSE SOURCE_FILES
	"${REPO_ROOT}/src/*.cpp"
	"${REPO_ROOT}/src/*.c"
	"${REPO_ROOT}/src/*.h"
	"${REPO_ROOT}/src/*.hpp"
	"${REPO_ROOT}/src/*.inl")

list(LENGTH SOURCE_FILES _file_count)
if(_file_count EQUAL 0)
	message(FATAL_ERROR "No source files found under ${REPO_ROOT}/src")
endif()
message(STATUS "clang-format ${MODE}: ${_file_count} files under src/")

# -- Run in batches (avoids Windows command-line length limits) -------------
set(BATCH_SIZE 100)
set(_had_violations FALSE)
set(_batch "")
set(_in_batch 0)

function(_run_batch files)
	if("${files}" STREQUAL "")
		return()
	endif()
	if(MODE STREQUAL "fix")
		execute_process(
			COMMAND "${CLANG_FORMAT_EXE}" --style=file -i ${files}
			WORKING_DIRECTORY "${REPO_ROOT}"
			RESULT_VARIABLE _rv)
		if(NOT _rv EQUAL 0)
			message(FATAL_ERROR "clang-format failed while fixing (exit ${_rv})")
		endif()
	else()
		# --dry-run --Werror: exit nonzero and print diagnostics if any file
		# would be reformatted, without modifying anything.
		execute_process(
			COMMAND "${CLANG_FORMAT_EXE}" --style=file --dry-run --Werror ${files}
			WORKING_DIRECTORY "${REPO_ROOT}"
			RESULT_VARIABLE _rv)
		if(NOT _rv EQUAL 0)
			set(_had_violations TRUE PARENT_SCOPE)
		endif()
	endif()
endfunction()

foreach(_file IN LISTS SOURCE_FILES)
	list(APPEND _batch "${_file}")
	math(EXPR _in_batch "${_in_batch} + 1")
	if(_in_batch GREATER_EQUAL ${BATCH_SIZE})
		_run_batch("${_batch}")
		if(_had_violations)
			set(_any_violations TRUE)
		endif()
		set(_batch "")
		set(_in_batch 0)
	endif()
endforeach()
_run_batch("${_batch}")
if(_had_violations)
	set(_any_violations TRUE)
endif()

# -- Report -----------------------------------------------------------------
if(MODE STREQUAL "fix")
	message(STATUS "clang-format: reformatting complete.")
elseif(_any_violations)
	message(FATAL_ERROR
		"clang-format check failed: some files are not formatted. "
		"Run `cmake --build build --target format` (or "
		"`cmake -P cmake/RunClangFormat.cmake -- --fix`) to fix them.")
else()
	message(STATUS "clang-format check passed: all files conform.")
endif()

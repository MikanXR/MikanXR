# Symbols.cmake
#
# Release builds emit a PDB next to every EXE and DLL (the /Z7 and /DEBUG:FULL options in
# Environment.cmake). These rules put those PDBs in the install tree and package them, so a
# minidump from a shipped build can be symbolicated against the exact binaries that produced it.
#
#   cmake --build build --target PACKAGE_APP       # zip the installed app
#   cmake --build build --target PACKAGE_SYMBOLS   # zip the installed PDBs
#
# Both packaging targets are plain custom targets. They zip whatever the INSTALL target last
# wrote and neither build nor install anything themselves, so INSTALL has to run first.

IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(MIKAN_SYMBOLS_INSTALL_PATH "${MIKAN_ROOT_INSTALL_PATH}/symbols/${ARCH_LABEL}")

	# True for a target the C# compiler builds. Those are executables and libraries as far as
	# the TYPE property is concerned, but they have no native linker and so no PDB.
	function(mikan_is_managed_target target out_var)
		set(${out_var} FALSE PARENT_SCOPE)
		get_target_property(target_sources ${target} SOURCES)
		foreach(source IN LISTS target_sources)
			if(source MATCHES "\\.cs$")
				set(${out_var} TRUE PARENT_SCOPE)
				return()
			endif()
		endforeach()
	endfunction()

	# Recursively collect the targets defined under src/ and add a PDB install rule for each
	# one the linker produces a PDB for. The walk also finds the MikanEditor OBJECT library,
	# static libraries, the MikanClientTestCSharp assembly, and the Refureku and codegen
	# UTILITY targets, and $<TARGET_PDB_FILE> is a generate-time error for all of those, so
	# the filtering is required rather than cosmetic. Only src/ is walked: bindings/ holds
	# more C# targets and thirdparty/ is not ours to ship.
	function(mikan_install_target_pdbs directory)
		get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
		foreach(target IN LISTS targets)
			get_target_property(target_type ${target} TYPE)
			if(NOT target_type STREQUAL "EXECUTABLE"
					AND NOT target_type STREQUAL "SHARED_LIBRARY"
					AND NOT target_type STREQUAL "MODULE_LIBRARY")
				continue()
			endif()

			mikan_is_managed_target(${target} target_is_managed)
			if(NOT target_is_managed)
				install(FILES $<TARGET_PDB_FILE:${target}>
					DESTINATION ${MIKAN_SYMBOLS_INSTALL_PATH}
					OPTIONAL)
			endif()
		endforeach()

		get_property(subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
		foreach(subdirectory IN LISTS subdirectories)
			mikan_install_target_pdbs("${subdirectory}")
		endforeach()
	endfunction()

	mikan_install_target_pdbs("${ROOT_DIR}/src")

	# Both zip commands run from inside the folder they package so the archive is flat: the
	# PDBs sit at the root of the symbols zip, and Mikan.exe at the root of the app zip.
	# The folders are created here so the targets fail on an empty archive rather than on a
	# missing working directory when INSTALL has not run yet.
	file(MAKE_DIRECTORY "${MIKAN_SYMBOLS_INSTALL_PATH}")
	file(MAKE_DIRECTORY "${MIKAN_ARCH_INSTALL_PATH}")

	add_custom_target(PACKAGE_APP
		COMMAND ${CMAKE_COMMAND} -E tar cf
			"${MIKAN_ROOT_INSTALL_PATH}/Mikan_${MIKAN_VERSION_STRING}_${ARCH_LABEL}.zip"
			--format=zip -- .
		WORKING_DIRECTORY "${MIKAN_ARCH_INSTALL_PATH}"
		COMMENT "Packaging the installed application")

	add_custom_target(PACKAGE_SYMBOLS
		COMMAND ${CMAKE_COMMAND} -E tar cf
			"${MIKAN_ROOT_INSTALL_PATH}/Mikan_${MIKAN_VERSION_STRING}_${ARCH_LABEL}_symbols.zip"
			--format=zip -- .
		WORKING_DIRECTORY "${MIKAN_SYMBOLS_INSTALL_PATH}"
		COMMENT "Packaging the installed debug symbols")

	set_target_properties(PACKAGE_APP PACKAGE_SYMBOLS PROPERTIES FOLDER "Packaging")
ENDIF()

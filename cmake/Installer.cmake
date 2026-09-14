#------------------------
# Platform Specific Installers
#------------------------

IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	find_package( InnoSetup )
	IF( INNOSETUP_FOUND )
		# Create a copy of the installer script with the version and paths filled in.
		# The payload is the installed application tree, and the setup exe lands one level
		# above it so the installer is not writing into the folder it is packaging.
		# Inno Setup wants backslashes, so every path is converted on the way in.
		set(APP_VERSION "${MIKAN_VERSION_STRING}")
		string(REPLACE "/" "\\" APP_PAYLOAD_DIR "${MIKAN_ARCH_INSTALL_PATH}")
		string(REPLACE "/" "\\" APP_OUTPUT_DIR "${MIKAN_ROOT_INSTALL_PATH}")
		string(REPLACE "/" "\\" APP_LICENSE_FILE "${ROOT_DIR}/LICENSE")
		configure_file(${ROOT_DIR}/templates/installer_win64.iss.in ${CMAKE_CURRENT_BINARY_DIR}/installer_win64.iss)

		# Create a simple cmake script that invokes the Inno Install script compiler
		FILE(
			WRITE ${CMAKE_CURRENT_BINARY_DIR}/CreateInstaller.cmake
			"EXECUTE_PROCESS(
				COMMAND \"${INNOSETUP_COMPILER}\" \"${CMAKE_CURRENT_BINARY_DIR}/installer_win64.iss\"
				RESULT_VARIABLE ISCC_RESULT
			)
			IF(NOT ISCC_RESULT EQUAL 0)
				MESSAGE(FATAL_ERROR \"Inno Setup compiler failed with exit code \${ISCC_RESULT}\")
			ENDIF()"
		)

		# Create a project build target that invokes the CreateInstaller cmake script.
		# Like the packaging targets it assumes INSTALL has already populated the payload.
		ADD_CUSTOM_TARGET(
			CREATE_INSTALLER
			COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/CreateInstaller.cmake
			COMMENT "create installer"
		)
		set_target_properties(CREATE_INSTALLER PROPERTIES FOLDER "Packaging")
	ELSE()
		message("Inno Setup Compiler not found. Skipping creation of CreateInstaller project.")
	ENDIF( INNOSETUP_FOUND )
ENDIF()

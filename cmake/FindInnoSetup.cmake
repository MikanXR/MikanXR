IF( NOT INNOSETUP_FOUND )
	SET(INNOSETUP_FOUND 0)

    # Can't use "$ENV{ProgramFiles(x86)}" to avoid violating CMP0053.  See
    # http://public.kitware.com/pipermail/cmake-developers/2014-October/023190.html
    set (ProgramFiles_x86 "ProgramFiles(x86)")
	# ISCC.exe is the command line compiler. Compil32.exe is the IDE front end, which the
	# GitHub Windows runner image does not ship. Version 6 is searched first so a machine
	# with both installed builds the installer with the newer compiler.
	FIND_PATH(
	   INNOSETUP_DIR
	   NAMES
	   ISCC.exe
	   PATHS
	   "$ENV{${ProgramFiles_x86}}/Inno Setup 6"
	   "$ENV{ProgramFiles}/Inno Setup 6"
	   "$ENV{${ProgramFiles_x86}}/Inno Setup 5"
	   "$ENV{ProgramFiles}/Inno Setup 5"
	)

	IF( INNOSETUP_DIR )
		FIND_FILE( INNOSETUP_COMPILER ISCC.exe PATHS ${INNOSETUP_DIR} )

		SET(INNOSETUP_FOUND 1)

		MARK_AS_ADVANCED(
			INNOSETUP_DIR
			INNOSETUP_FOUND
			INNOSETUP_COMPILER
		)
	ENDIF( INNOSETUP_DIR )
ENDIF( NOT INNOSETUP_FOUND )

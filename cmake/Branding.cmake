#------------------------
# Branding
#------------------------

# The folder the app icon and the splash image are taken from. The repository carries a
# generic set. An official build supplies the MikanXR set, which is not part of this
# repository, either by path or as a checkout at branding/official.
set(MIKAN_BRANDING_DIR "" CACHE PATH "Folder holding icon.ico and splash.png. Empty selects branding/official/mikanxr when present, else branding/generic")

IF(MIKAN_BRANDING_DIR)
	set(MIKAN_BRANDING_PATH "${MIKAN_BRANDING_DIR}")
ELSEIF(EXISTS "${ROOT_DIR}/branding/official/mikanxr/icon.ico")
	set(MIKAN_BRANDING_PATH "${ROOT_DIR}/branding/official/mikanxr")
ELSE()
	set(MIKAN_BRANDING_PATH "${ROOT_DIR}/branding/generic")
ENDIF()
file(TO_CMAKE_PATH "${MIKAN_BRANDING_PATH}" MIKAN_BRANDING_PATH)

set(MIKAN_BRANDING_ICON "${MIKAN_BRANDING_PATH}/icon.ico")
set(MIKAN_BRANDING_SPLASH "${MIKAN_BRANDING_PATH}/splash.png")

foreach(BRANDING_FILE "${MIKAN_BRANDING_ICON}" "${MIKAN_BRANDING_SPLASH}")
	IF(NOT EXISTS "${BRANDING_FILE}")
		message(FATAL_ERROR "Branding file not found: ${BRANDING_FILE}")
	ENDIF()
endforeach()

message(STATUS "Branding: ${MIKAN_BRANDING_PATH}")

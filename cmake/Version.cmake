#------------------------
# Project Version
#------------------------

# The version lives in the header as string-literal parts (calendar style, zero
# padded, so integers would not do). This reads the same parts back so every
# name CMake produces (dist/version.txt, the zips, the installer) matches the
# string the binaries carry.
set(MIKAN_VERSION_HEADER_FILE "${ROOT_DIR}/src/Editor/AppCore/Version.h")

function(mikan_read_version_part part out_var)
  file(STRINGS ${MIKAN_VERSION_HEADER_FILE} matched_lines
    REGEX "^#define[ \t]+MIKAN_RELEASE_VERSION_${part}[ \t]+\"[^\"]*\"")
  list(LENGTH matched_lines match_count)
  if(NOT match_count EQUAL 1)
    message(FATAL_ERROR "Unable to retrieve MIKAN_RELEASE_VERSION_${part} from ${MIKAN_VERSION_HEADER_FILE}")
  endif()
  string(REGEX REPLACE "^#define[ \t]+MIKAN_RELEASE_VERSION_${part}[ \t]+\"([^\"]*)\".*$" "\\1"
    value "${matched_lines}")
  set(${out_var} "${value}" PARENT_SCOPE)
endfunction()

mikan_read_version_part(YEAR MIKAN_VERSION_YEAR)
mikan_read_version_part(MONTH MIKAN_VERSION_MONTH)
mikan_read_version_part(DAY MIKAN_VERSION_DAY)
mikan_read_version_part(REVISION MIKAN_VERSION_REVISION)

if(NOT MIKAN_VERSION_YEAR OR NOT MIKAN_VERSION_MONTH OR NOT MIKAN_VERSION_DAY)
  message(FATAL_ERROR "The release date in ${MIKAN_VERSION_HEADER_FILE} is incomplete")
endif()

# Matches MIKAN_RELEASE_VERSION_STRING in the header and the vYYYY.MM.DD git tag convention.
# The revision is already "" or ".N", so it appends as is.
set(MIKAN_VERSION_STRING "${MIKAN_VERSION_YEAR}.${MIKAN_VERSION_MONTH}.${MIKAN_VERSION_DAY}${MIKAN_VERSION_REVISION}")

message(STATUS "Project version: ${MIKAN_VERSION_STRING}")

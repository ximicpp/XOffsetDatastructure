# XOFFSET_VERSION is the sole complete release identifier (not named VERSION:
# on case-insensitive file systems that file shadows the standard <version> header).
# It is installed as share/XOffsetDatastructure/VERSION. A release is MAJOR.MINOR.PATCH,
# a prerelease adds -(alpha|beta|rc).NUMBER. Numeric CMake versions cannot express
# prerelease identity; do not infer API/ABI compatibility from this part.
file(READ "${CMAKE_CURRENT_LIST_DIR}/../XOFFSET_VERSION" XOFFSET_VERSION_FULL)
string(STRIP "${XOFFSET_VERSION_FULL}" XOFFSET_VERSION_FULL)
if(NOT XOFFSET_VERSION_FULL MATCHES "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(-(alpha|beta|rc)\\.(0|[1-9][0-9]*))?$")
    message(FATAL_ERROR "XOFFSET_VERSION must be MAJOR.MINOR.PATCH with at most one prerelease identifier -(alpha|beta|rc).NUMBER, without leading zeros")
endif()
set(XOFFSET_VERSION_NUMERIC "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
if(CMAKE_MATCH_4)
    set(XOFFSET_VERSION_PRERELEASE "${CMAKE_MATCH_5}.${CMAKE_MATCH_6}")
    set(XOFFSET_IS_PRERELEASE TRUE)
else()
    set(XOFFSET_VERSION_PRERELEASE "")
    set(XOFFSET_IS_PRERELEASE FALSE)
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../XOFFSET_VERSION")

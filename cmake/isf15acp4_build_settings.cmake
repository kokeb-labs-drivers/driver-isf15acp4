#===============================================================================
# ISF15ACP4 driver: build settings
#===============================================================================
# Read by the root CMakeLists.txt in both of its modes (plain CMake and the
# ESP-IDF component): target name, version, include directories, sources and
# ESP-IDF requirements. This file is the single source
# of the version; tests/CMakeLists.txt checks every other copy against it.
#===============================================================================

include_guard(GLOBAL)

set(ISF15ACP4_TARGET_NAME "isf15acp4")

#===============================================================================
# Version (single source of truth)
#===============================================================================
set(ISF15ACP4_VERSION_MAJOR 0)
set(ISF15ACP4_VERSION_MINOR 1)
set(ISF15ACP4_VERSION_PATCH 0)
set(ISF15ACP4_VERSION "${ISF15ACP4_VERSION_MAJOR}.${ISF15ACP4_VERSION_MINOR}.${ISF15ACP4_VERSION_PATCH}")

#===============================================================================
# Generated version header (build tree, never the source tree)
#===============================================================================
set(ISF15ACP4_VERSION_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/../inc/isf15acp4_version.h.in")
set(ISF15ACP4_VERSION_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/isf15acp4_generated")
set(ISF15ACP4_VERSION_HEADER "${ISF15ACP4_VERSION_HEADER_DIR}/isf15acp4_version.h")

# The directory must exist before ESP-IDF's idf_component_register() checks the include paths.
file(MAKE_DIRECTORY "${ISF15ACP4_VERSION_HEADER_DIR}")
configure_file("${ISF15ACP4_VERSION_TEMPLATE}" "${ISF15ACP4_VERSION_HEADER}" @ONLY)

#===============================================================================
# Public include directories, sources, ESP-IDF requirements
#===============================================================================
set(ISF15ACP4_PUBLIC_INCLUDE_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../inc"
    "${ISF15ACP4_VERSION_HEADER_DIR}"
)

# Header-only: the template implementation is src/isf15acp4.ipp, included by inc/isf15acp4.hpp.
set(ISF15ACP4_SOURCE_FILES)

set(ISF15ACP4_IDF_REQUIRES)

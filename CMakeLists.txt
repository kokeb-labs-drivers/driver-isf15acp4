#===============================================================================
# ISF15ACP4 driver: root CMake
#===============================================================================
# One file, two build systems:
#
# - ESP-IDF (ESP_PLATFORM): this folder is an ESP-IDF component. Point
#   EXTRA_COMPONENT_DIRS at it, check it out under components/, or add it as a
#   component-manager git dependency. menuconfig reads _config/Kconfig.
# - Any other CMake build (host, STM32, RP2040, nRF, ...): one INTERFACE target,
#   isf15acp4, consumed as isf15acp4::isf15acp4 both through add_subdirectory() and
#   through find_package(isf15acp4) after install.
#
#   cmake -S . -B build -DISF15ACP4_BUILD_TESTS=ON
#   cmake --build build && ctest --test-dir build --output-on-failure
#===============================================================================
cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/isf15acp4_build_settings.cmake)

#===============================================================================
# ESP-IDF component
#===============================================================================
if(ESP_PLATFORM)
    idf_component_register(
        SRCS ${ISF15ACP4_SOURCE_FILES}
        INCLUDE_DIRS ${ISF15ACP4_PUBLIC_INCLUDE_DIRS}
        REQUIRES ${ISF15ACP4_IDF_REQUIRES}
        KCONFIG ${CMAKE_CURRENT_LIST_DIR}/_config/Kconfig
    )
    if(ISF15ACP4_SOURCE_FILES)
        target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
    else()
        target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_20)
    endif()
    return()
endif()

project(isf15acp4_driver
    VERSION ${ISF15ACP4_VERSION}
    DESCRIPTION "C++20 driver for the NKK ISF15ACP4 frameless OLED pushbutton: a 96RGB x 64 SSD1331-class OLED on write-only SPI and a debounced SPST switch."
    LANGUAGES CXX)

include(GNUInstallDirs)

# ISO C++20 for every target this project builds (tests, fuzzers, examples), never the GNU dialect
# that CMake picks by default (handbook KB-CPP-001, KB-CPP20-001). CXX_EXTENSIONS does not travel
# through an INTERFACE target, so each folder that builds a target sets it as well.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The installed layout keeps the source tree's inc/ + src/ pair, because isf15acp4.hpp includes
# "../src/isf15acp4.ipp" relative to itself:
#   <prefix>/include/isf15acp4/inc/*.hpp, isf15acp4_version.h
#   <prefix>/include/isf15acp4/src/isf15acp4.ipp
set(ISF15ACP4_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/isf15acp4/inc")

#===============================================================================
# Target
#===============================================================================
add_library(${ISF15ACP4_TARGET_NAME} INTERFACE)
target_include_directories(${ISF15ACP4_TARGET_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/inc>
        $<BUILD_INTERFACE:${ISF15ACP4_VERSION_HEADER_DIR}>
        $<INSTALL_INTERFACE:${ISF15ACP4_INSTALL_INCLUDEDIR}>
)
target_compile_features(${ISF15ACP4_TARGET_NAME} INTERFACE cxx_std_20)

# isf15acp4::isf15acp4 everywhere: the alias for add_subdirectory(), EXPORT_NAME for the installed package.
add_library(isf15acp4::isf15acp4 ALIAS ${ISF15ACP4_TARGET_NAME})
set_target_properties(${ISF15ACP4_TARGET_NAME} PROPERTIES EXPORT_NAME isf15acp4)

option(ISF15ACP4_ENABLE_WARNINGS "Add -Wall -Wextra -Wpedantic to every target that links isf15acp4::isf15acp4" OFF)
if(ISF15ACP4_ENABLE_WARNINGS)
    target_compile_options(${ISF15ACP4_TARGET_NAME} INTERFACE -Wall -Wextra -Wpedantic)
endif()

#===============================================================================
# Host tests, documentation snippets, install check (tests/)
#===============================================================================
option(ISF15ACP4_BUILD_TESTS "Build the host tests in tests/" OFF)
option(ISF15ACP4_BUILD_FUZZERS "Also build the libFuzzer targets in tests/fuzz/ (Clang only)" OFF)
if(ISF15ACP4_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

#===============================================================================
# Install and package export
#===============================================================================
include(CMakePackageConfigHelpers)

install(TARGETS ${ISF15ACP4_TARGET_NAME} EXPORT isf15acp4Targets
    INCLUDES DESTINATION ${ISF15ACP4_INSTALL_INCLUDEDIR})

install(DIRECTORY inc/ DESTINATION ${ISF15ACP4_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
install(FILES "${ISF15ACP4_VERSION_HEADER}" DESTINATION ${ISF15ACP4_INSTALL_INCLUDEDIR})
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/isf15acp4/src
    FILES_MATCHING PATTERN "*.ipp")

install(EXPORT isf15acp4Targets
    FILE isf15acp4Targets.cmake
    NAMESPACE isf15acp4::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/isf15acp4)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/isf15acp4Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/isf15acp4Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/isf15acp4)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/isf15acp4ConfigVersion.cmake"
    VERSION ${ISF15ACP4_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/isf15acp4Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/isf15acp4ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/isf15acp4)

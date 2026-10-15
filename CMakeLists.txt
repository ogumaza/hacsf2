# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.20)
# hacsf2 --version reports this version. The file properties of the Windows program and the Info.plist of the macOS
# app show it too. A release changes it here and adds its notes in docs/release-notes/.
project(hacsf2 VERSION 1.0.2 LANGUAGES CXX)

# seqlib writes the MIDI and SoundFont files. ntdlib reads Nintendo's sound archives and converts their sequences.
# xsflib holds the code that any PSF-family program could use, and the CMake functions that make the version resource
# and the app. All three are submodules. xsflib's XsflibProject.cmake sets the C++ standard, a release build by default,
# the static C runtime and the compiler options that keep the output the same on every platform. ntdlib builds on
# xsflib and seqlib. Both come before it.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/externals/xsflib/CMakeLists.txt")
  message(FATAL_ERROR "externals/xsflib is missing. Run: git submodule update --init")
endif()
include(externals/xsflib/cmake/XsflibProject.cmake)
xsflib_project(WARNINGS_ON_EVERY_TARGET)
set(NTDLIB_SEQUENCE_EXPORT ON)
xsflib_add_libraries(seqlib xsflib ntdlib)

# The libraries that hacsf2, its archive check and its private tools link: seqlib, and ntdlib's sar and sequence_export.
# sequence_export's consoles.h gives the Switch's settings.
add_library(hacsf2_core INTERFACE)
target_link_libraries(hacsf2_core INTERFACE seqlib::seqlib ntdlib::sar ntdlib::sequence_export)

add_executable(hacsf2 src/main.cpp)
target_link_libraries(hacsf2 PRIVATE hacsf2_core)
# The version that hacsf2 --version prints. On Windows, a version resource gives it to Explorer, which shows it in the
# file's properties with the description.
xsflib_add_version(hacsf2 VERSION ${PROJECT_VERSION} DEFINE HACSF2_VERSION
                   DESCRIPTION "Nintendo Switch sound archive converter" PRODUCT hacsf2
                   COPYRIGHT "Copyright (c) 2026 the hacsf2 authors. MIT licence.")

# Finder cannot drop files onto a command-line executable. Package it in a macOS droplet app, with the version in its
# Info.plist.
xsflib_add_droplet_tool(hacsf2 APP_NAME Hacsf2 VERSION ${PROJECT_VERSION}
                        DESCRIPTION "Drop Nintendo Switch sound archives (.bfsar) here to convert them to MIDI, SoundFont and SFZ."
                        PROMPT "Choose Switch sound archives (.bfsar) to convert:" TRUNCATED GROUPS)

include(CTest)
if(BUILD_TESTING)
  # The program reports its version, and it rejects --var 32=1. -DHACSF2_TEST_ARCHIVE=path/to/archive.bfsar also
  # converts and checks every sequence of an archive with tests/hacsf2_test.cpp.
  xsflib_add_converter_tests(hacsf2 EXTENSION .bfsar VERSION ${PROJECT_VERSION}
                             LIBRARIES hacsf2_core ntdlib::testing)

  # Load local_tests.cmake from each immediate subdirectory to register tests that use locally supplied files.
  xsflib_include_local_tests()
endif()

# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.20)
# hacsf2 --version reports this version. The file properties of the Windows program and the Info.plist of the macOS
# app show it too. A release changes it here and adds its notes in docs/release-notes/.
project(hacsf2 VERSION 1.1.1 LANGUAGES CXX)

# The submodules: seqlib writes the MIDI, SoundFont and SFZ files. ntdlib holds the Nintendo code that hacsf2 shares
# with the other converters and the players: the archive reader, the performer, the conversion and the command-line
# frame (ntdlib::sequence_export). xsflib holds the common code that ntdlib builds on, and the CMake helpers. xsflib's
# XsflibProject.cmake sets the C++ standard, a release build by default, the static C runtime and the compiler options
# that keep the output the same on every platform. ntdlib builds on seqlib and xsflib. xsflib_add_libraries adds ntdlib
# last. XsflibProject.cmake defines that macro. The macro stops if a submodule is missing. The check below
# covers xsflib itself.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/externals/xsflib/CMakeLists.txt")
  message(FATAL_ERROR "externals/xsflib is missing. Run: git submodule update --init")
endif()
include(externals/xsflib/cmake/XsflibProject.cmake)
xsflib_project(WARNINGS_ON_EVERY_TARGET)
set(NTDLIB_SEQUENCE_EXPORT ON)
xsflib_add_libraries(seqlib xsflib ntdlib)

# clang-cl's -Wmissing-designated-field-initializers asks for every field of a designated initializer. Filled-in fields
# would copy the struct's defaults into each initializer, where they could drift from the struct. The warning is off
# for hacsf2's targets. The libraries above keep it.
if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(-Wno-missing-designated-field-initializers)
endif()

# The libraries that hacsf2 and its tests link. ntdlib's sequence_export/consoles.h holds the Switch's settings.
add_library(hacsf2_core INTERFACE)
target_link_libraries(hacsf2_core INTERFACE seqlib::seqlib ntdlib::bezel ntdlib::sar ntdlib::sequence_export
                      ntdlib::nx_bex)

# The conversions in each state that a game's code gives a sound (src/profiles.h, ntdlib/sar/game_rules.h), the values
# that the game gives the pieces that wait for them (src/rhythm_pieces.h), and the start of a sound that sounds
# (src/sounding_start.h).
add_library(hacsf2_profiles STATIC src/profiles.cpp src/profiles.h src/rhythm_pieces.cpp src/rhythm_pieces.h
                                   src/sounding_start.cpp src/sounding_start.h)
target_link_libraries(hacsf2_profiles PUBLIC hacsf2_core)
target_include_directories(hacsf2_profiles PUBLIC src)

# The handlers of two games' user procedures, and the table of the games' sound archives (src/user_procedures.h).
add_library(hacsf2_procedures STATIC src/procedure_archives.cpp src/user_procedures.cpp src/user_procedures.h)
target_link_libraries(hacsf2_procedures PUBLIC hacsf2_core)

add_executable(hacsf2 src/main.cpp src/synced_pieces.cpp src/synced_pieces.h)
target_link_libraries(hacsf2 PRIVATE hacsf2_profiles hacsf2_procedures)
xsflib_add_version(hacsf2 VERSION ${PROJECT_VERSION} DEFINE HACSF2_VERSION
                   DESCRIPTION "Nintendo Switch sound archive converter" PRODUCT hacsf2
                   COPYRIGHT "Copyright (c) 2026 the hacsf2 authors. MIT licence.")

# Windows: UTF-8 as the code page. An archive or a folder whose name the system's code page can't hold then opens.
xsflib_add_utf8_manifest(hacsf2)

# Finder cannot drop files onto a command-line executable. Package it in a macOS droplet app, with the version in its
# Info.plist.
xsflib_add_droplet_tool(hacsf2 APP_NAME Hacsf2 VERSION ${PROJECT_VERSION}
                        DESCRIPTION "Drop Nintendo Switch sound archives (.bfsar) here to convert them to MIDI, SoundFont and SFZ. Output goes in a folder beside each archive."
                        PROMPT "Choose Switch sound archives (.bfsar) to convert:" TRUNCATED GROUPS)

include(CTest)
if(BUILD_TESTING)
  # The program reports its version, and it rejects --var 32=1. -DHACSF2_TEST_ARCHIVE=path/to/archive.bfsar also
  # converts and checks every sequence of an archive with tests/hacsf2_test.cpp.
  xsflib_add_converter_tests(hacsf2 EXTENSION .bfsar VERSION ${PROJECT_VERSION}
                             LIBRARIES hacsf2_core ntdlib::testing)

  # The states of the sounds under a game's rules (tests/profiles_test.cpp).
  add_executable(hacsf2_profiles_test tests/profiles_test.cpp)
  target_link_libraries(hacsf2_profiles_test PRIVATE hacsf2_profiles xsflib::testing)
  add_test(NAME hacsf2_profiles COMMAND hacsf2_profiles_test)

  # The start of a sound that sounds (tests/sounding_start_test.cpp).
  add_executable(hacsf2_sounding_start_test tests/sounding_start_test.cpp)
  target_link_libraries(hacsf2_sounding_start_test PRIVATE hacsf2_profiles xsflib::testing)
  add_test(NAME hacsf2_sounding_start COMMAND hacsf2_sounding_start_test)

  # The table of the sound archives whose games' handlers of user procedures hacsf2 runs, and the models.
  add_executable(hacsf2_procedures_test tests/procedures_test.cpp)
  target_include_directories(hacsf2_procedures_test PRIVATE src)
  target_link_libraries(hacsf2_procedures_test PRIVATE hacsf2_procedures xsflib::testing)
  add_test(NAME hacsf2_procedures COMMAND hacsf2_procedures_test)

  # Load local_tests.cmake from each immediate subdirectory to register tests that use locally supplied files.
  xsflib_include_local_tests()
endif()

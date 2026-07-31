cmake_minimum_required(VERSION 3.16)

project(hunspell_vendor CXX)

# Vendored hunspell 1.7.2 (https://github.com/hunspell/hunspell, autotools-only
# upstream so it is built here with a small CMake target instead).
# License: MPL 1.1 / GPL 2.0+ / LGPL 2.1+ tri-license (see COPYING* files).

set(HUNSPELL_SOURCES
    src/hunspell/affentry.cxx
    src/hunspell/affixmgr.cxx
    src/hunspell/csutil.cxx
    src/hunspell/filemgr.cxx
    src/hunspell/hashmgr.cxx
    src/hunspell/hunspell.cxx
    src/hunspell/hunzip.cxx
    src/hunspell/phonet.cxx
    src/hunspell/replist.cxx
    src/hunspell/suggestmgr.cxx
)

add_library(hunspell STATIC ${HUNSPELL_SOURCES})

target_include_directories(hunspell PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hunspell
)

target_compile_definitions(hunspell PRIVATE HUNSPELL_STATIC)

set_target_properties(hunspell PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
)

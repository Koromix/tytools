# See here: http://www.cmake.org/Wiki/CmakeMingw

set(CMAKE_SYSTEM_NAME Darwin)

find_file(CMAKE_C_COMPILER
    NAMES x86_64-apple-darwin14-clang
          x86_64-apple-darwin13-clang
          x86_64-apple-darwin12-clang
          x86_64-apple-darwin11-clang)

if(CMAKE_C_COMPILER)
    string(REGEX REPLACE "-clang$" "" DARWIN_TOOLCHAIN "${CMAKE_C_COMPILER}")

    set(CMAKE_CXX_COMPILER "${DARWIN_TOOLCHAIN}-clang++")
    set(PKG_CONFIG_EXECUTABLE "${DARWIN_TOOLCHAIN}-pkg-config")

    string(REGEX REPLACE "^.*-darwin([0-9]+)-.*$" "\\1" DARWIN_MAJOR_VERSION "${CMAKE_C_COMPILER}")

    # CMAKE_SYSTEM_VERSION is needed to enable the OSX RPATH support, without it
    # Platform/Darwin.cmake does not set CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG
    set(CMAKE_SYSTEM_VERSION "${DARWIN_MAJOR_VERSION}.0")

    # Kind of a cheat, this path cannot exist (unless CMAKE_C_COMPILER is a directory)
    # but ABSOLUTE does not care about it (REALPATH would)
    get_filename_component(DARWIN_SDK "${CMAKE_C_COMPILER}/../../SDK" ABSOLUTE)

    find_file(CMAKE_FIND_ROOT_PATH
        NAMES MacOSX10.10.sdk
              MacOSX10.9.sdk
              MacOSX10.8.sdk
              MacOSX10.7.sdk
        PATHS "${DARWIN_SDK}"
        NO_DEFAULT_PATH)

    if(NOT CMAKE_FIND_ROOT_PATH)
        message(FATAL_ERROR "Cannot find Apple SDK")
    endif()
else()
    message(FATAL_ERROR "Cannot find Apple Clang compiler")
endif()

# Adjust the default behaviour of the FIND_XXX() commands:
#  - search headers and libraries in the target environment
#  - search programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)

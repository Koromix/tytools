# The MIT License (MIT)
#
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

cmake_minimum_required(VERSION 2.8.12)
project(libhs C)

if(CMAKE_C_COMPILER_ID STREQUAL Clang)
    set(CMAKE_COMPILER_IS_CLANG 1)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(LINUX 1)
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -fvisibility=hidden -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-missing-field-initializers -Wshadow -Wconversion -Wformat=2")
    foreach(mode RELEASE RELWITHDEBINFO MINSIZEREL)
        set(CMAKE_${lang}_FLAGS_${mode} "${CMAKE_${lang}_FLAGS_${mode}} -D_FORTIFY_SOURCE=2")
    endforeach()

    if(MINGW)
        add_definitions(-D__USE_MINGW_ANSI_STDIO=1)
        set(CMAKE_C_STANDARD_LIBRARIES "${CMAKE_C_STANDARD_LIBRARIES} -static-libgcc")
    endif()
elseif(MSVC)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS)
endif()

if(WIN32)
    add_definitions(-DWINVER=0x0602 -D_WIN32_WINNT=0x0602)
    list(APPEND HS_LINK_LIBRARIES setupapi hid)
elseif(APPLE)
    find_library(COREFOUNDATION_LIBRARIES CoreFoundation)
    find_library(IOKIT_LIBRARIES IOKit)
    list(APPEND HS_LINK_LIBRARIES ${COREFOUNDATION_LIBRARIES} ${IOKIT_LIBRARIES})

    if(POLICY CMP0042)
        cmake_policy(SET CMP0042 NEW)
    endif()

    set(CMAKE_INSTALL_NAME_DIR "@rpath")
    set(CMAKE_INSTALL_RPATH "@executable_path;@executable_path/../lib;@executable_path/../Frameworks")

    # Makes it easier for cross-compilation and testing. In my case, compile on Linux
    # and test on OSX using sshfs
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
elseif(LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUDEV REQUIRED libudev)

    include_directories(${LIBUDEV_INCLUDE_DIRS})
    list(APPEND HS_LINK_LIBRARIES ${LIBUDEV_LIBRARIES})
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(include)

add_subdirectory(src)

set(HS_BUILD_EXAMPLES ON CACHE BOOL "Build examples")
if(HS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

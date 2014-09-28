# ty, command-line program to manage Teensy devices
# http://github.com/Koromix/ty
# Copyright (C) 2014 Niels Martignène
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 2.8)
project(ty C)

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUDEV REQUIRED libudev)

    include_directories(${LIBUDEV_INCLUDE_DIRS})
    list(APPEND LIBRARIES ${LIBUDEV_LIBRARIES})
endif()

if(WIN32)
    list(APPEND LIBRARIES setupapi hid)
endif()

if("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_COMPILER_IS_CLANG 1)
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "-std=gnu99 -fms-extensions -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-missing-field-initializers -Wshadow -Wconversion -Wformat=2")

    if(CMAKE_COMPILER_IS_GNUCC)
        set(CMAKE_C_FLAGS_DEBUG "-Og -ggdb -D_FORTIFY_SOURCE=2")
    else()
        set(CMAKE_C_FLAGS_DEBUG "-O1 -g -D_FORTIFY_SOURCE=2")
    endif()
    set(CMAKE_C_FLAGS_RELEASE "-O2 -D_FORTIFY_SOURCE=2")
endif()

if(WIN32)
    add_definitions(-DWINVER=0x0602 -D_WIN32_WINNT=0x0602)
endif()

set(EXPERIMENTAL OFF CACHE BOOL "Build experimental and unsafe features")
if(EXPERIMENTAL)
    add_definitions(-DTY_EXPERIMENTAL=1)
endif()

include(CheckSymbolExists)
check_symbol_exists(stpcpy string.h HAVE_STPCPY)
check_symbol_exists(strndup string.h HAVE_STRNDUP)
check_symbol_exists(asprintf stdio.h HAVE_ASPRINTF)
check_symbol_exists(getdelim stdio.h HAVE_GETDELIM)
check_symbol_exists(getline stdio.h HAVE_GETLINE)

configure_file(src/config.h.in src/config.h)

include_directories(include "${CMAKE_CURRENT_BINARY_DIR}/src")

set(TY_SOURCES include/ty.h
               include/ty/board.h
               include/ty/common.h
               include/ty/device.h
               include/ty/firmware.h
               include/ty/ini.h
               include/ty/system.h

               src/board.c
               src/common.c
               src/compat.c
               src/device.c
               src/device_priv.h
               src/firmware.c
               src/firmware_ihex.c
               src/htable.c
               src/ini.c
               src/list.h
               src/system.c)
if(WIN32)
    list(APPEND TY_SOURCES src/system_win32.c
                           src/device_win32.c)
else()
    list(APPEND TY_SOURCES src/system_unix.c
                           src/system_linux.c
                           src/device_linux.c
                           src/device_unix.c)
endif()

set(TYC_SOURCES cli/list.c
                cli/main.c
                cli/monitor.c
                cli/reset.c
                cli/upload.c)

add_library(ty SHARED ${TY_SOURCES})
target_link_libraries(ty ${LIBRARIES})
add_library(ty_static STATIC ${TY_SOURCES})
target_link_libraries(ty_static ${LIBRARIES})

add_executable(tyc ${TYC_SOURCES})
target_link_libraries(tyc ty_static)

install(TARGETS ty DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)

install(TARGETS tyc RUNTIME DESTINATION bin)

install(FILES contrib/udev/teensy.rules DESTINATION /lib/udev/rules.d RENAME 49-teensy.rules)

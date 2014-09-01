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

if(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS "-std=gnu99 -D_GNU_SOURCE -Wall -Wextra -Wno-missing-field-initializers -Wshadow -Wconversion -Wformat=2")

    set(CMAKE_C_FLAGS_DEBUG "-Og -fno-omit-frame-pointer -ggdb -D_FORTIFY_SOURCE=2")
    set(CMAKE_C_FLAG_RELEASE "-O2 -fno-omit-frame-pointer -s -D_FORTIFY_SOURCE=2")
endif()

if(WIN32)
    add_definitions(-DWINVER=0x0600 -D_WIN32_WINNT=0x0600)
endif()

set(EXPERIMENTAL OFF CACHE BOOL "Build experimental and unsafe features")
if(EXPERIMENTAL)
    add_definitions(-DTY_EXPERIMENTAL=1)
endif()

include(CheckSymbolExists)
check_symbol_exists(asprintf stdio.h HAVE_ASPRINTF)

configure_file(lib/config.h.in lib/config.h)

include_directories(lib "${CMAKE_CURRENT_BINARY_DIR}/lib")

set(SOURCES lib/board.c
            lib/common.c
            lib/compat.c
            lib/device.c
            lib/firmware.c
            lib/firmware_ihex.c
            lib/system.c

            cli/list.c
            cli/main.c
            cli/monitor.c
            cli/reset.c
            cli/upload.c)
if(WIN32)
    list(APPEND SOURCES lib/system_win32.c
                        lib/device_win32.c)
else()
    list(APPEND SOURCES lib/system_unix.c
                        lib/device_linux.c
                        lib/device_unix.c)
endif()

add_executable(ty ${SOURCES})
target_link_libraries(ty ${LIBRARIES})

install(TARGETS ty RUNTIME DESTINATION bin)
install(FILES data/udev/teensy.rules DESTINATION /lib/udev/rules.d RENAME 49-teensy.rules)

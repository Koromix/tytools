# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 2.8.9)
project(ty C)

find_package(Threads)
list(APPEND LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

if(CMAKE_C_COMPILER_ID STREQUAL Clang)
    set(CMAKE_COMPILER_IS_CLANG 1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(LINUX 1)
endif()

if(LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUDEV REQUIRED libudev)

    include_directories(${LIBUDEV_INCLUDE_DIRS})
    list(APPEND LIBRARIES ${LIBUDEV_LIBRARIES})
endif()

if(APPLE)
    if(POLICY CMP0042)
        cmake_policy(SET CMP0042 NEW)
    endif()
endif()

if(WIN32)
    list(APPEND LIBRARIES setupapi hid)

    if(MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc")
    endif()
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "-std=gnu99 -fvisibility=hidden -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-missing-field-initializers -Wshadow -Wconversion -Wformat=2")

    if(CMAKE_COMPILER_IS_GNUCC)
        set(CMAKE_C_FLAGS_DEBUG "-Og -ggdb -D_FORTIFY_SOURCE=2")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-microsoft")
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
check_symbol_exists(memrchr string.h HAVE_MEMRCHR)
check_symbol_exists(asprintf stdio.h HAVE_ASPRINTF)
check_symbol_exists(getdelim stdio.h HAVE_GETDELIM)
check_symbol_exists(getline stdio.h HAVE_GETLINE)
check_symbol_exists(fstatat sys/stat.h HAVE_FSTATAT)
check_symbol_exists(pipe2 unistd.h HAVE_PIPE2)

include(CheckStructHasMember)
check_struct_has_member("struct stat" st_mtim sys/stat.h HAVE_STAT_MTIM)
if(NOT HAVE_STAT_MTIM)
  check_struct_has_member("struct stat" st_mtimespec sys/stat.h HAVE_STAT_MTIMESPEC)
endif()

configure_file(src/config.h.in src/config.h)

include_directories(include "${CMAKE_CURRENT_BINARY_DIR}/src")

set(TY_SOURCES include/ty.h
               include/ty/board.h
               include/ty/common.h
               include/ty/device.h
               include/ty/firmware.h
               include/ty/ini.h
               include/ty/process.h
               include/ty/system.h
               include/ty/timer.h

               src/board.c
               src/board_teensy.c
               src/board_priv.h
               src/common.c
               src/compat.c
               src/device.c
               src/device_priv.h
               src/firmware.c
               src/firmware_elf.c
               src/firmware_ihex.c
               src/htable.c
               src/ini.c
               src/list.h
               src/system.c)
if(WIN32)
    list(APPEND TY_SOURCES src/process_win32.c
                           src/system_win32.c
                           src/device_win32.c
                           src/timer_win32.c)
else()
    list(APPEND TY_SOURCES src/device_posix.c
                           src/device_posix_priv.h
                           src/process_posix.c
                           src/system_posix.c)

    if(LINUX)
        list(APPEND TY_SOURCES src/device_linux.c
                               src/timer_linux.c)
    elseif(APPLE)
        list(APPEND TY_SOURCES src/device_darwin.c
                               src/timer_kqueue.c)
    else()
        message(FATAL_ERROR "Unsupported platform")
    endif()
endif()

set(TYC_SOURCES cli/list.c
                cli/main.c
                cli/monitor.c
                cli/reset.c
                cli/upload.c)

add_library(ty OBJECT ${TY_SOURCES})
set_target_properties(ty PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(ty_shared SHARED $<TARGET_OBJECTS:ty>)
set_target_properties(ty_shared PROPERTIES OUTPUT_NAME ty CLEAN_DIRECT_OUTPUT ON)
target_link_libraries(ty_shared ${LIBRARIES})
add_library(ty_static STATIC $<TARGET_OBJECTS:ty>)
set_target_properties(ty_static PROPERTIES OUTPUT_NAME ty CLEAN_DIRECT_OUTPUT ON)
target_link_libraries(ty_static ${LIBRARIES})

add_executable(tyc ${TYC_SOURCES})
set(STATIC_BINARIES OFF CACHE BOOL "Build statically linked binaries")
if(STATIC_BINARIES)
    target_link_libraries(tyc ty_static)
else()
    target_link_libraries(tyc ty_shared)
endif()

install(TARGETS ty_shared DESTINATION lib)
install(TARGETS ty_static DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)

install(TARGETS tyc RUNTIME DESTINATION bin)

install(FILES contrib/udev/teensy.rules DESTINATION /usr/lib/udev/rules.d RENAME 49-teensy.rules)

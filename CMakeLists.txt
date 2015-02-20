# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 2.8.12)
project(ty C CXX)

set(TY_VERSION 0.5.0)

find_package(Threads)
list(APPEND TY_LINK_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

if(CMAKE_C_COMPILER_ID STREQUAL Clang)
    set(CMAKE_COMPILER_IS_CLANG 1)
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "-std=gnu99 -fvisibility=hidden -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-missing-field-initializers -Wshadow -Wconversion -Wformat=2")
    set(CMAKE_CXX_FLAGS "-std=gnu++1y -fvisibility=hidden -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-missing-field-initializers -Wold-style-cast -Wformat=2")

    if(CMAKE_COMPILER_IS_GNUCC)
        foreach(lang C CXX)
            set(CMAKE_${lang}_FLAGS_DEBUG "-O1 -ggdb -D_FORTIFY_SOURCE=2")
            set(CMAKE_${lang}_FLAGS_RELWITHDEBINFO "-O2 -ggdb -D_FORTIFY_SOURCE=2")
        endforeach()
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-microsoft")
        foreach(lang C CXX)
            set(CMAKE_${lang}_FLAGS_DEBUG "-O1 -g -D_FORTIFY_SOURCE=2")
            set(CMAKE_${lang}_FLAGS_RELWITHDEBINFO "-O2 -g -D_FORTIFY_SOURCE=2")
        endforeach()
    endif()
    foreach(lang C CXX)
        set(CMAKE_${lang}_FLAGS_RELEASE "-O2 -D_FORTIFY_SOURCE=2")
    endforeach()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(LINUX 1)
endif()

if(LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUDEV REQUIRED libudev)

    include_directories(${LIBUDEV_INCLUDE_DIRS})
    list(APPEND TY_LINK_LIBRARIES ${LIBUDEV_LIBRARIES})
endif()

if(APPLE)
    find_library(COREFOUNDATION_LIBRARIES CoreFoundation)
    find_library(IOKIT_LIBRARIES IOKit)
    list(APPEND TY_LINK_LIBRARIES ${COREFOUNDATION_LIBRARIES} ${IOKIT_LIBRARIES})

    if(POLICY CMP0042)
        cmake_policy(SET CMP0042 NEW)
    endif()

    set(CMAKE_INSTALL_NAME_DIR "@rpath")
    set(CMAKE_INSTALL_RPATH "@executable_path;@executable_path/../lib")

    # Makes it easier for cross-compilation and testing. In my case, compile on Linux
    # and test on OSX using sshfs
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

if(WIN32)
    add_definitions(-DWINVER=0x0602 -D_WIN32_WINNT=0x0602)
    list(APPEND TY_LINK_LIBRARIES setupapi hid)

    if(MINGW)
        set(CMAKE_C_STANDARD_LIBRARIES "${CMAKE_C_STANDARD_LIBRARIES} -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic -static-libgcc -static-libstdc++")
        set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic -static-libgcc -static-libstdc++")
    endif()
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
check_symbol_exists(pthread_cond_timedwait_relative_np pthread.h HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP)

include(CheckStructHasMember)
check_struct_has_member("struct stat" st_mtim sys/stat.h HAVE_STAT_MTIM)
if(NOT HAVE_STAT_MTIM)
    check_struct_has_member("struct stat" st_mtimespec sys/stat.h HAVE_STAT_MTIMESPEC)
endif()

find_package(Git)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags
        OUTPUT_VARIABLE TY_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    # Strip the 'v' prefix
    string(SUBSTRING ${TY_VERSION} 1 -1 TY_VERSION)
endif()

configure_file(src/config.h.in config.h)
configure_file(include/ty/version.h.in ty/version.h)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(include)

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

set(TYQ_SOURCES qt/about_dialog.cc
                qt/main.cc
                qt/main_window.cc
                qt/board_proxy.cc
                qt/board_widget.cc
                qt/descriptor_set_notifier.cc)
set(TYQ_FORMS qt/about_dialog.ui
              qt/main_window.ui
              qt/board_widget.ui)
set(TYQ_RESOURCES resources/tyqt.qrc)

add_library(ty OBJECT ${TY_SOURCES})
set_target_properties(ty PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(ty_shared SHARED $<TARGET_OBJECTS:ty>)
set_target_properties(ty_shared PROPERTIES OUTPUT_NAME ty CLEAN_DIRECT_OUTPUT ON)
target_link_libraries(ty_shared ${TY_LINK_LIBRARIES})
add_library(ty_static STATIC $<TARGET_OBJECTS:ty>)
set_target_properties(ty_static PROPERTIES OUTPUT_NAME ty CLEAN_DIRECT_OUTPUT ON)
target_link_libraries(ty_static ${TY_LINK_LIBRARIES})

set(STATIC_BINARIES OFF CACHE BOOL "Build statically linked binaries")
if(STATIC_BINARIES)
    set(TY_LIBRARIES ty_static)
else()
    set(TY_LIBRARIES ty_shared)
endif()

add_executable(tyc ${TYC_SOURCES})
target_link_libraries(tyc ${TY_LIBRARIES})

find_package(Qt5 REQUIRED
    COMPONENTS Widgets Network PrintSupport
    HINTS "${CMAKE_BINARY_DIR}/qt5"
    NO_CMAKE_FIND_ROOT_PATH)

get_target_property(QT5_LIBRARY_DIRECTORY Qt5::Core LOCATION)
get_filename_component(QT5_LIBRARY_DIRECTORY "${QT5_LIBRARY_DIRECTORY}" DIRECTORY)

qt5_wrap_ui(TYQ_FORMS_HEADERS ${TYQ_FORMS})
qt5_add_resources(TYQ_RESOURCES_RCC ${TYQ_RESOURCES})

add_executable(tyqt WIN32 ${TYQ_SOURCES} ${TYQ_FORMS_HEADERS} ${TYQ_RESOURCES_RCC})
set_target_properties(tyqt PROPERTIES AUTOMOC ON)

file(GLOB QT_SHARED_LIBRARIES "${QT5_LIBRARY_DIRECTORY}/*${CMAKE_SHARED_LIBRARY_SUFFIX}")
if(NOT QT_SHARED_LIBRARIES)
    set_property(TARGET tyqt APPEND PROPERTY COMPILE_DEFINITIONS QT_STATIC)
endif()

# Qt5 sets INTERFACE_INCLUDE_DIRECTORIES, so no need to add the correct include directories
target_link_libraries(tyqt ${TY_LIBRARIES} Qt5::Widgets Qt5::Network)
if(WIN32)
    # Fix undefined reference to _imp__WSAAsyncSelect@16
    set_property(TARGET Qt5::Network APPEND PROPERTY INTERFACE_LINK_LIBRARIES ws2_32)

    # Why is there no config package for this?
    find_library(Qt5PlatformSupport_LIBRARIES_DEBUG Qt5PlatformSupportd
        HINTS "${QT5_LIBRARY_DIRECTORY}" "${QT5_LIBRARY_DIRECTORY}/../lib"
        NO_CMAKE_FIND_ROOT_PATH)
    find_library(Qt5PlatformSupport_LIBRARIES_OPTIMIZED Qt5PlatformSupport
        HINTS "${QT5_LIBRARY_DIRECTORY}" "${QT5_LIBRARY_DIRECTORY}/../lib"
        NO_CMAKE_FIND_ROOT_PATH)

    target_link_libraries(tyqt
        Qt5::QWindowsIntegrationPlugin imm32 winmm
        debug ${Qt5PlatformSupport_LIBRARIES_DEBUG}
        optimized ${Qt5PlatformSupport_LIBRARIES_OPTIMIZED})
elseif(APPLE)
    find_library(COCOA_LIBRARIES Cocoa)
    find_library(CARBON_LIBRARIES Carbon)
    find_package(ZLIB REQUIRED)
    find_package(Cups REQUIRED)

    find_library(Qt5PlatformSupport_LIBRARIES_DEBUG Qt5PlatformSupport_debug
        HINTS "${QT5_LIBRARY_DIRECTORY}" "${QT5_LIBRARY_DIRECTORY}/../lib"
        NO_CMAKE_FIND_ROOT_PATH)
    find_library(Qt5PlatformSupport_LIBRARIES_OPTIMIZED Qt5PlatformSupport
        HINTS "${QT5_LIBRARY_DIRECTORY}" "${QT5_LIBRARY_DIRECTORY}/../lib"
        NO_CMAKE_FIND_ROOT_PATH)

    target_link_libraries(tyqt
        Qt5::QCocoaIntegrationPlugin Qt5::PrintSupport
        ${COCOA_LIBRARIES} ${CARBON_LIBRARIES} ${ZLIB_LIBRARIES} ${CUPS_LIBRARIES}
        debug ${Qt5PlatformSupport_LIBRARIES_DEBUG}
        optimized ${Qt5PlatformSupport_LIBRARIES_OPTIMIZED})
endif()

install(TARGETS ty_shared DESTINATION lib)
install(TARGETS ty_static DESTINATION lib)
install(FILES include/ty.h DESTINATION include)
install(FILES include/ty/*.h DESTINATION include/ty)
install(FILES "${CMAKE_BINARY_DIR}/ty/version.h" DESTINATION include/ty)

install(TARGETS tyc RUNTIME DESTINATION bin)

install(FILES contrib/udev/teensy.rules DESTINATION /usr/lib/udev/rules.d RENAME 49-teensy.rules)

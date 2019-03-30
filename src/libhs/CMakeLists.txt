# libhs - public domain
# Niels Martignène <niels.martignene@protonmail.com>
# https://koromix.dev/libhs

# This software is in the public domain. Where that dedication is not
# recognized, you are granted a perpetual, irrevocable license to copy,
# distribute, and modify this file as you see fit.

# See the LICENSE file for more details.

if(WIN32)
    list(APPEND LIBHS_LINK_LIBRARIES hid setupapi)
elseif(APPLE)
    find_library(COREFOUNDATION_LIBRARIES CoreFoundation)
    find_library(IOKIT_LIBRARIES IOKit)
    list(APPEND LIBHS_LINK_LIBRARIES ${COREFOUNDATION_LIBRARIES} ${IOKIT_LIBRARIES})
elseif(LINUX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GNU_SOURCE")

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUDEV REQUIRED libudev)

    include_directories(${LIBUDEV_INCLUDE_DIRS})
    list(APPEND LIBHS_LINK_LIBRARIES ${LIBUDEV_LIBRARIES})
endif()

include(CheckSymbolExists)
check_symbol_exists(stpcpy string.h _HS_HAVE_STPCPY)
check_symbol_exists(asprintf stdio.h _HS_HAVE_ASPRINTF)
configure_file(config.h.in config.h)

find_package(Threads)
list(APPEND LIBHS_LINK_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

set(LIBHS_SOURCES array.c
                  array.h
                  common.c
                  common.h
                  common_priv.h
                  compat.c
                  compat_priv.h
                  device.c
                  device.h
                  device_priv.h
                  hid.h
                  htable.c
                  match.c
                  match.h
                  match_priv.h
                  monitor.h
                  monitor_common.c
                  monitor_priv.h
                  platform.c
                  platform.h
                  serial.h)
if(WIN32)
    list(APPEND LIBHS_SOURCES device_win32.c
                              hid_win32.c
                              monitor_win32.c
                              platform_win32.c
                              serial_win32.c)
else()
    list(APPEND LIBHS_SOURCES device_posix.c
                              serial_posix.c)

    if(LINUX)
        list(APPEND LIBHS_SOURCES hid_linux.c
                                  monitor_linux.c
                                  platform_posix.c)
    elseif(APPLE)
        list(APPEND LIBHS_SOURCES hid_darwin.c
                                  monitor_darwin.c
                                  platform_darwin.c)
    else()
        message(FATAL_ERROR "Unsupported platform")
    endif()
endif()

add_library(libhs STATIC ${LIBHS_SOURCES})
set_target_properties(libhs PROPERTIES OUTPUT_NAME hs)
target_link_libraries(libhs ${LIBHS_LINK_LIBRARIES})
# We need that for auto-generated file config.h
target_include_directories(libhs PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(libhs PUBLIC _HS_HAVE_CONFIG_H)
enable_unity_build(libhs)

add_amalgamated_file(libhs "${CMAKE_BINARY_DIR}/libhs.h" libhs.h)

if(BUILD_EXAMPLES)
    add_subdirectory(examples/enumerate_devices)
    add_subdirectory(examples/monitor_devices)
    add_subdirectory(examples/serial_dumper)
endif()

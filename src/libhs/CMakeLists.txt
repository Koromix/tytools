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
check_symbol_exists(asprintf stdio.h HAVE_ASPRINTF)

find_package(Threads)
list(APPEND LIBHS_LINK_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

set(LIBHS_SOURCES array.c
                  array.h
                  common.c
                  common.h
                  common_priv.h
                  device.c
                  device_posix.c
                  device_win32.c
                  device.h
                  device_priv.h
                  hid_darwin.c
                  hid_linux.c
                  hid_win32.c
                  hid.h
                  htable.c
                  match.c
                  match.h
                  match_priv.h
                  monitor_common.c
                  monitor_darwin.c
                  monitor_linux.c
                  monitor_win32.c
                  monitor.h
                  monitor_priv.h
                  platform.c
                  platform_darwin.c
                  platform_linux.c
                  platform_win32.c
                  platform.h
                  serial_posix.c
                  serial_win32.c
                  serial.h)

add_library(libhs STATIC ${LIBHS_SOURCES})
set_target_properties(libhs PROPERTIES OUTPUT_NAME hs)
target_link_libraries(libhs ${LIBHS_LINK_LIBRARIES})
if(HAVE_ASPRINTF)
    target_compile_definitions(libhs PRIVATE HAVE_ASPRINTF)
endif()
enable_unity_build(libhs)

add_amalgamated_file(libhs "${CMAKE_BINARY_DIR}/libhs.h" libhs.h)

if(BUILD_EXAMPLES)
    add_subdirectory(examples/enumerate_devices)
    add_subdirectory(examples/monitor_devices)
    add_subdirectory(examples/serial_dumper)
endif()

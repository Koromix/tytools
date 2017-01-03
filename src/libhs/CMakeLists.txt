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

if(WIN32)
    if(MSVC AND CMAKE_VS_PLATFORM_TOOLSET MATCHES _xp)
        # Some libraries/headers (such as hid.lib) are not available when an XP compatible
        # toolset is requested (e.g. "cmake -T v140_xp"). I don't understand much about the
        # SDK/WDK/DDK/toolset mess, so just ask WindowsSDK for everything ("COMPONENTS tools"),
        # hopefully it is okay.
        find_package(WindowsSDK COMPONENTS tools)
        if(WINDOWSSDK_FOUND)
            foreach(_dir ${WINDOWSSDK_DIRS})
                get_windowssdk_include_dirs(${_dir} _include_dirs)
                if(_include_dirs)
                    list(APPEND _winsdk_include_dirs ${_include_dirs})
                endif()
                get_windowssdk_library_dirs(${_dir} _library_dirs)
                if(_library_dirs)
                    list(APPEND _winsdk_library_dirs ${_library_dirs})
                endif()
            endforeach()
        endif()

        find_path(HID_INCLUDE_DIR hidsdi.h usbuser.h HINTS ${_winsdk_include_dirs})
        find_path(USBUSER_INCLUDE_DIR usbuser.h HINTS ${_winsdk_include_dirs})
        include_directories(${HID_INCLUDE_DIR} ${USBUSER_INCLUDE_DIR})

        find_library(HID_LIBRARY hid HINTS ${_winsdk_library_dirs})
        find_library(SETUPAPI_LIBRARY setupapi HINTS ${_winsdk_library_dirs})
        list(APPEND LIBHS_LINK_LIBRARIES ${HID_LIBRARY} ${SETUPAPI_LIBRARY})
    else()
        list(APPEND LIBHS_LINK_LIBRARIES hid setupapi)
    endif()
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
check_symbol_exists(stpcpy string.h HAVE_STPCPY)
check_symbol_exists(asprintf stdio.h HAVE_ASPRINTF)
configure_file(config.h.in config.h)
add_definitions(-DHAVE_CONFIG_H)

find_package(Threads)
list(APPEND LIBHS_LINK_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

set(LIBHS_SOURCES common.c
                  common.h
                  common_priv.h
                  compat.c
                  device.c
                  device.h
                  device_priv.h
                  filter.c
                  hid.h
                  htable.c
                  libhs.h
                  list.h
                  match.h
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

if(MSVC)
    add_library(libhs_shared SHARED ${LIBHS_SOURCES})
    add_library(libhs_static STATIC ${LIBHS_SOURCES})
    target_compile_definitions(libhs_static PUBLIC HS_STATIC)
    # We need that for auto-generated file config.h
    target_include_directories(libhs_static PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_include_directories(libhs_shared PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    enable_unity_build(libhs_shared)
    enable_unity_build(libhs_static)
else()
    add_library(libhs OBJECT ${LIBHS_SOURCES})
    set_target_properties(libhs PROPERTIES POSITION_INDEPENDENT_CODE ON)
    # Same as before, need that for config.h
    target_include_directories(libhs PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    enable_unity_build(libhs)
    add_library(libhs_shared SHARED $<TARGET_OBJECTS:libhs>)
    add_library(libhs_static STATIC $<TARGET_OBJECTS:libhs>)
    set_target_properties(libhs_static PROPERTIES OUTPUT_NAME hs)
endif()
set_target_properties(libhs_shared PROPERTIES OUTPUT_NAME hs
                                              CLEAN_DIRECT_OUTPUT ON)
target_link_libraries(libhs_shared ${LIBHS_LINK_LIBRARIES})
set_target_properties(libhs_static PROPERTIES OUTPUT_NAME hs_static
                                              CLEAN_DIRECT_OUTPUT ON)
target_link_libraries(libhs_static ${LIBHS_LINK_LIBRARIES})

if(TY_BUILD_EXAMPLES)
    add_subdirectory(examples/enumerate_devices)
    add_subdirectory(examples/monitor_devices)
    add_subdirectory(examples/serial_dumper)
endif()

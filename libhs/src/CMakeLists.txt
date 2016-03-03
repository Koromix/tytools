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

include(CheckSymbolExists)
check_symbol_exists(stpcpy string.h HAVE_STPCPY)
check_symbol_exists(asprintf stdio.h HAVE_ASPRINTF)
add_definitions(-DHAVE_CONFIG_H)
configure_file(config.h.in config.h)

find_package(Threads)
list(APPEND HS_LINK_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

set(HS_SOURCES ../include/hs.h
               ../include/hs/common.h
               ../include/hs/device.h
               ../include/hs/hid.h
               ../include/hs/match.h
               ../include/hs/monitor.h
               ../include/hs/platform.h
               ../include/hs/serial.h

               common.c
               compat.c
               device.c
               device_priv.h
               filter.c
               htable.c
               list.h
               monitor.c
               monitor_priv.h
               platform.c
               util.h)
if(WIN32)
    list(APPEND HS_SOURCES device_win32.c
                           device_win32_priv.h
                           hid_win32.c
                           monitor_win32.c
                           platform_win32.c
                           serial_win32.c)
else()
    list(APPEND HS_SOURCES device_posix.c
                           device_posix_priv.h
                           serial_posix.c)

    if(LINUX)
        list(APPEND HS_SOURCES hid_linux.c
                               monitor_linux.c
                               platform_posix.c)
    elseif(APPLE)
        list(APPEND HS_SOURCES hid_darwin.c
                               monitor_darwin.c
                               platform_darwin.c)
    else()
        message(FATAL_ERROR "Unsupported platform")
    endif()
endif()

if(MSVC)
    add_library(hs_shared SHARED ${HS_SOURCES})
    add_library(hs_static STATIC ${HS_SOURCES})
    target_compile_definitions(hs_static PUBLIC HS_STATIC)
else()
    add_library(hs OBJECT ${HS_SOURCES})
    set_target_properties(hs PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(hs_shared SHARED $<TARGET_OBJECTS:hs>)
    add_library(hs_static STATIC $<TARGET_OBJECTS:hs>)
    set_target_properties(hs_static PROPERTIES OUTPUT_NAME hs)
endif()
set_target_properties(hs_shared PROPERTIES OUTPUT_NAME hs CLEAN_DIRECT_OUTPUT ON)
target_link_libraries(hs_shared ${HS_LINK_LIBRARIES})
set_target_properties(hs_static PROPERTIES CLEAN_DIRECT_OUTPUT ON)
target_link_libraries(hs_static ${HS_LINK_LIBRARIES})

# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

set(TY_SOURCES include/ty.h
               include/ty/board.h
               include/ty/common.h
               include/ty/firmware.h
               include/ty/monitor.h
               include/ty/system.h
               include/ty/task.h
               include/ty/thread.h
               include/ty/timer.h

               board.c
               board_teensy.c
               board_priv.h
               common.c
               compat.c
               firmware.c
               firmware_elf.c
               firmware_ihex.c
               firmware_priv.h
               htable.c
               list.h
               monitor.c
               system.c
               task.c
               task_priv.h
               util.h)
if(LINUX)
    list(APPEND TY_SOURCES system_posix.c
                           thread_pthread.c
                           timer_linux.c)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUDEV REQUIRED libudev)

    include_directories(${LIBUDEV_INCLUDE_DIRS})
    list(APPEND TY_LINK_LIBRARIES ${LIBUDEV_LIBRARIES})
elseif(WIN32)
    list(APPEND TY_SOURCES system_win32.c
                           thread_win32.c
                           timer_win32.c)

    list(APPEND TY_LINK_LIBRARIES setupapi hid)
elseif(APPLE)
    list(APPEND TY_SOURCES system_posix.c
                           thread_pthread.c
                           timer_kqueue.c)

    find_library(COREFOUNDATION_LIBRARIES CoreFoundation)
    find_library(IOKIT_LIBRARIES IOKit)
    list(APPEND TY_LINK_LIBRARIES ${COREFOUNDATION_LIBRARIES} ${IOKIT_LIBRARIES})
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

include(CheckSymbolExists)
check_symbol_exists(asprintf stdio.h HAVE_ASPRINTF)
if(NOT WIN32)
    check_symbol_exists(pthread_cond_timedwait_relative_np pthread.h HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP)
endif()
configure_file(config.h.in config.h)
add_definitions(-DHAVE_CONFIG_H)

list(APPEND TY_LINK_LIBRARIES ${HS_LIBRARIES})
find_package(Threads)
list(APPEND TY_LINK_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

add_library(ty OBJECT ${TY_SOURCES})
set_target_properties(ty PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(ty_shared SHARED $<TARGET_OBJECTS:ty>)
set_target_properties(ty_shared PROPERTIES OUTPUT_NAME ty CLEAN_DIRECT_OUTPUT ON)
target_link_libraries(ty_shared ${TY_LINK_LIBRARIES})
add_library(ty_static STATIC $<TARGET_OBJECTS:ty>)
set_target_properties(ty_static PROPERTIES OUTPUT_NAME ty CLEAN_DIRECT_OUTPUT ON)
target_link_libraries(ty_static ${TY_LINK_LIBRARIES})

if(WIN32)
    if(NOT STATIC_BINARIES)
        install(TARGETS ty_shared RUNTIME DESTINATION .)
    endif()
elseif(APPLE)
    if(NOT STATIC_BINARIES)
        # Kind of a hack, but it works (the Bundle generator wants to put everything in Resources)
        install(TARGETS ty_shared DESTINATION ../Frameworks)
    endif()
else()
    install(TARGETS ty_shared DESTINATION lib)
    install(TARGETS ty_static DESTINATION lib)
    install(DIRECTORY ../libty/include/ DESTINATION include)
    install(FILES "${CMAKE_BINARY_DIR}/ty/version.h" DESTINATION include/ty)
endif()

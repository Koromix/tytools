# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

set(LIBTY_SOURCES ../../include/ty/board.h
                  ../../include/ty/common.h
                  ../../include/ty/firmware.h
                  ../../include/ty/model.h
                  ../../include/ty/monitor.h
                  ../../include/ty/optline.h
                  ../../include/ty/system.h
                  ../../include/ty/task.h
                  ../../include/ty/thread.h
                  ../../include/ty/timer.h

                  board.c
                  board_priv.h
                  class_teensy.c
                  common.c
                  compat.c
                  firmware.c
                  firmware_elf.c
                  firmware_ihex.c
                  htable.c
                  list.h
                  model.c
                  model_priv.h
                  monitor.c
                  optline.c
                  system.c
                  task.c
                  task_priv.h
                  util.h)
if(LINUX)
    list(APPEND LIBTY_SOURCES system_posix.c
                              thread_pthread.c
                              timer_linux.c)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUDEV REQUIRED libudev)

    include_directories(${LIBUDEV_INCLUDE_DIRS})
    list(APPEND LIBTY_LINK_LIBRARIES ${LIBUDEV_LIBRARIES})
elseif(WIN32)
    list(APPEND LIBTY_SOURCES system_win32.c
                              thread_win32.c
                              timer_win32.c)
elseif(APPLE)
    list(APPEND LIBTY_SOURCES system_posix.c
                              thread_pthread.c
                              timer_kqueue.c)

    find_library(COREFOUNDATION_LIBRARIES CoreFoundation)
    find_library(IOKIT_LIBRARIES IOKit)
    list(APPEND LIBTY_LINK_LIBRARIES ${COREFOUNDATION_LIBRARIES} ${IOKIT_LIBRARIES})
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

configure_file(version.h.in version.h)

include(CheckSymbolExists)
check_symbol_exists(asprintf stdio.h HAVE_ASPRINTF)
if(NOT WIN32)
    check_symbol_exists(pthread_cond_timedwait_relative_np pthread.h HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP)
endif()
configure_file(config.h.in config.h)
add_definitions(-DHAVE_CONFIG_H)

list(APPEND LIBTY_LINK_LIBRARIES ${LIBHS_LIBRARIES})
find_package(Threads)
list(APPEND LIBTY_LINK_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

add_library(libty STATIC ${LIBTY_SOURCES})
set_target_properties(libty PROPERTIES OUTPUT_NAME ty)
target_link_libraries(libty PRIVATE ${LIBTY_LINK_LIBRARIES})
target_include_directories(libty PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
                                         ${CMAKE_CURRENT_BINARY_DIR}
                                 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
    target_compile_definitions(libty PUBLIC TY_STATIC)
endif()

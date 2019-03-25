# TyTools - public domain
# Niels Martignène <niels.martignene@protonmail.com>
# https://koromix.dev/tytools

# This software is in the public domain. Where that dedication is not
# recognized, you are granted a perpetual, irrevocable license to copy,
# distribute, and modify this file as you see fit.

# See the LICENSE file for more details.

set(LIBTY_SOURCES board.c
                  board.h
                  board_priv.h
                  class.c
                  class.h
                  class_priv.h
                  class_generic.c
                  class_teensy.c
                  common.c
                  common.h
                  common_priv.h
                  compat.c
                  compat_priv.h
                  firmware.c
                  firmware.h
                  firmware_elf.c
                  firmware_ihex.c
                  ini.c
                  ini.h
                  monitor.c
                  monitor.h
                  optline.c
                  optline.h
                  system.c
                  system.h
                  task.c
                  task.h
                  thread.h
                  timer.h)
if(LINUX)
    list(APPEND LIBTY_SOURCES system_posix.c
                              thread_pthread.c
                              timer_linux.c)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GNU_SOURCE")

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
check_symbol_exists(asprintf stdio.h _TY_HAVE_ASPRINTF)
if(NOT WIN32)
    check_symbol_exists(pthread_cond_timedwait_relative_np pthread.h _TY_HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP)
endif()
configure_file(config.h.in config.h)

list(APPEND LIBTY_LINK_LIBRARIES libhs)
find_package(Threads)
list(APPEND LIBTY_LINK_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

add_library(libty STATIC ${LIBTY_SOURCES})
set_target_properties(libty PROPERTIES OUTPUT_NAME ty)
target_link_libraries(libty PRIVATE ${LIBTY_LINK_LIBRARIES})
# We need that for config.h
target_include_directories(libty PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(libty PUBLIC _TY_HAVE_CONFIG_H)
enable_unity_build(libty)

configure_file(libty.h.in libty.h.in)
add_amalgamated_file(libty "${CMAKE_BINARY_DIR}/libty.h" "${CMAKE_CURRENT_BINARY_DIR}/libty.h.in"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

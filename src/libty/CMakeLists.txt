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
                  class_default.c
                  class_generic.c
                  class_teensy.c
                  common.c
                  common.h
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
                  system_posix.c
                  system_win32.c
                  system.h
                  task.c
                  task.h
                  thread_pthread.c
                  thread_win32.c
                  thread.h
                  timer_kqueue.c
                  timer_linux.c
                  timer_win32.c
                  timer.h)
if(LINUX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GNU_SOURCE")

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUDEV REQUIRED libudev)

    include_directories(${LIBUDEV_INCLUDE_DIRS})
    list(APPEND LIBTY_LINK_LIBRARIES ${LIBUDEV_LIBRARIES})
elseif(APPLE)
    find_library(COREFOUNDATION_LIBRARIES CoreFoundation)
    find_library(IOKIT_LIBRARIES IOKit)
    list(APPEND LIBTY_LINK_LIBRARIES ${COREFOUNDATION_LIBRARIES} ${IOKIT_LIBRARIES})
endif()

if(CONFIG_VARIANT)
    if(EXISTS "${CONFIG_VARIANT_PATH}/class_default.c")
        list(REMOVE_ITEM LIBTY_SOURCES class_default.c)
        list(APPEND LIBTY_SOURCES "${CONFIG_VARIANT_PATH}/class_default.c")
    endif()
endif()

configure_file(version.h.in version.h)
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

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(TY_SOURCES include/ty.h
               include/ty/board.h
               include/ty/common.h
               include/ty/device.h
               include/ty/firmware.h
               include/ty/system.h
               include/ty/task.h
               include/ty/thread.h
               include/ty/timer.h

               board.c
               board_teensy.c
               board_priv.h
               common.c
               compat.c
               device.c
               device_priv.h
               firmware.c
               firmware_elf.c
               firmware_ihex.c
               firmware_priv.h
               htable.c
               list.h
               system.c
               task.c
               task_priv.h)
if(LINUX)
    list(APPEND TY_SOURCES device_linux.c
                           device_posix.c
                           device_posix_priv.h
                           system_posix.c
                           thread_pthread.c
                           timer_linux.c)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUDEV REQUIRED libudev)

    include_directories(${LIBUDEV_INCLUDE_DIRS})
    list(APPEND TY_LINK_LIBRARIES ${LIBUDEV_LIBRARIES})
elseif(WIN32)
    list(APPEND TY_SOURCES device_win32.c
                           system_win32.c
                           thread_win32.c
                           timer_win32.c)

    list(APPEND TY_LINK_LIBRARIES setupapi hid)
elseif(APPLE)
    list(APPEND TY_SOURCES device_darwin.c
                           device_posix.c
                           device_posix_priv.h
                           system_posix.c
                           thread_pthread.c
                           timer_kqueue.c)

    find_library(COREFOUNDATION_LIBRARIES CoreFoundation)
    find_library(IOKIT_LIBRARIES IOKit)
    list(APPEND TY_LINK_LIBRARIES ${COREFOUNDATION_LIBRARIES} ${IOKIT_LIBRARIES})
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

include(CheckSymbolExists)
check_symbol_exists(stpcpy string.h HAVE_STPCPY)
check_symbol_exists(stpncpy string.h HAVE_STPNCPY)
check_symbol_exists(strndup string.h HAVE_STRNDUP)
check_symbol_exists(memrchr string.h HAVE_MEMRCHR)
check_symbol_exists(asprintf stdio.h HAVE_ASPRINTF)
check_symbol_exists(getdelim stdio.h HAVE_GETDELIM)
check_symbol_exists(getline stdio.h HAVE_GETLINE)
check_symbol_exists(fstatat sys/stat.h HAVE_FSTATAT)
check_symbol_exists(pipe2 unistd.h HAVE_PIPE2)
if(NOT WIN32)
    check_symbol_exists(pthread_cond_timedwait_relative_np pthread.h HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP)

    include(CheckStructHasMember)
    check_struct_has_member("struct stat" st_mtim sys/stat.h HAVE_STAT_MTIM)
    if(NOT HAVE_STAT_MTIM)
        check_struct_has_member("struct stat" st_mtimespec sys/stat.h HAVE_STAT_MTIMESPEC)
    endif()
endif()
configure_file(config.h.in config.h)
add_definitions(-DHAVE_CONFIG_H)

find_package(Threads)
list(APPEND TY_LINK_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

if(WIN32 AND NOT CMAKE_CROSSCOMPILING)
    execute_process(COMMAND "${CMAKE_SOURCE_DIR}/scripts/git-version.bat"
        OUTPUT_VARIABLE TY_VERSION_HEADER)
else()
    execute_process(COMMAND "${CMAKE_SOURCE_DIR}/scripts/git-version.sh"
        OUTPUT_VARIABLE TY_VERSION_HEADER)
endif()
file(WRITE "${CMAKE_BINARY_DIR}/ty/version.h" "${TY_VERSION_HEADER}")
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+(\\.[0-9]+)?(\\-[0-9]+-[a-zA-Z0-9]+)?" TY_VERSION "${TY_VERSION_HEADER}")

add_library(ty OBJECT ${TY_SOURCES})
set_target_properties(ty PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(ty_shared SHARED $<TARGET_OBJECTS:ty>)
set_target_properties(ty_shared PROPERTIES OUTPUT_NAME ty CLEAN_DIRECT_OUTPUT ON)
target_link_libraries(ty_shared ${TY_LINK_LIBRARIES})
add_library(ty_static STATIC $<TARGET_OBJECTS:ty>)
set_target_properties(ty_static PROPERTIES OUTPUT_NAME ty CLEAN_DIRECT_OUTPUT ON)
target_link_libraries(ty_static ${TY_LINK_LIBRARIES})

string(REGEX REPLACE "\\-.*$" "" CPACK_PACKAGE_VERSION ${TY_VERSION})
set(CPACK_PACKAGE_VERSION ${CPACK_PACKAGE_VERSION} PARENT_SCOPE)

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
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/version.h" DESTINATION include/ty)

    if(LINUX)
        install(FILES ../contrib/udev/teensy.rules DESTINATION lib/udev/rules.d RENAME 49-teensy.rules)
    endif()
endif()

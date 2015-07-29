# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 2.8.12)
project(ty C CXX)

find_package(Threads)
list(APPEND TY_LINK_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

if(CMAKE_C_COMPILER_ID STREQUAL Clang)
    set(CMAKE_COMPILER_IS_CLANG 1)
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "-std=gnu99 -fvisibility=hidden -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-missing-field-initializers -Wshadow -Wconversion -Wformat=2")
    set(CMAKE_CXX_FLAGS "-std=gnu++11 -fvisibility=hidden -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-missing-field-initializers -Wold-style-cast -Wformat=2")

    foreach(lang C CXX)
        set(CMAKE_${lang}_FLAGS_DEBUG "-O1 -g -D_FORTIFY_SOURCE=2")
        set(CMAKE_${lang}_FLAGS_RELWITHDEBINFO "-O2 -g -D_FORTIFY_SOURCE=2")
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
    set(CMAKE_INSTALL_RPATH "@executable_path;@executable_path/../lib;@executable_path/../Frameworks")

    # Makes it easier for cross-compilation and testing. In my case, compile on Linux
    # and test on OSX using sshfs
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

if(WIN32)
    add_definitions(-DWINVER=0x0602 -D_WIN32_WINNT=0x0602)
    list(APPEND TY_LINK_LIBRARIES setupapi hid)

    if(MINGW)
        set(CMAKE_C_STANDARD_LIBRARIES "${CMAKE_C_STANDARD_LIBRARIES} -static-libgcc -static-libstdc++ -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic")
        set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lwinpthread -Wl,-Bdynamic")
    endif()
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
if(NOT WIN32)
    check_symbol_exists(pthread_cond_timedwait_relative_np pthread.h HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP)

    include(CheckStructHasMember)
    check_struct_has_member("struct stat" st_mtim sys/stat.h HAVE_STAT_MTIM)
    if(NOT HAVE_STAT_MTIM)
        check_struct_has_member("struct stat" st_mtimespec sys/stat.h HAVE_STAT_MTIMESPEC)
    endif()
endif()
add_definitions(-DHAVE_CONFIG_H)

if(WIN32 AND NOT CMAKE_CROSSCOMPILING)
    execute_process(COMMAND "${CMAKE_SOURCE_DIR}/scripts/git-version.bat"
        OUTPUT_VARIABLE TY_VERSION_HEADER)
else()
    execute_process(COMMAND "${CMAKE_SOURCE_DIR}/scripts/git-version.sh"
        OUTPUT_VARIABLE TY_VERSION_HEADER)
endif()
file(WRITE "${CMAKE_BINARY_DIR}/ty/version.h" "${TY_VERSION_HEADER}")
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+(\\.[0-9]+)?(\\-[0-9]+-[a-zA-Z0-9]+)?" TY_VERSION "${TY_VERSION_HEADER}")

configure_file(libty/config.h.in config.h)
if (APPLE)
    configure_file(resources/tyqt_osx.plist.in tyqt_osx.plist)
elseif (NOT WIN32)
    configure_file(resources/tyqt_linux.desktop.in tyqt_linux.desktop)
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(libty/include)

set(TY_SOURCES libty/include/ty.h
               libty/include/ty/board.h
               libty/include/ty/common.h
               libty/include/ty/device.h
               libty/include/ty/firmware.h
               libty/include/ty/system.h
               libty/include/ty/thread.h
               libty/include/ty/timer.h

               libty/board.c
               libty/board_teensy.c
               libty/board_priv.h
               libty/common.c
               libty/compat.c
               libty/device.c
               libty/device_priv.h
               libty/firmware.c
               libty/firmware_elf.c
               libty/firmware_ihex.c
               libty/htable.c
               libty/list.h
               libty/system.c)
if(WIN32)
    list(APPEND TY_SOURCES libty/device_win32.c
                           libty/system_win32.c
                           libty/thread_win32.c
                           libty/timer_win32.c)
else()
    list(APPEND TY_SOURCES libty/device_posix.c
                           libty/device_posix_priv.h
                           libty/system_posix.c
                           libty/thread_pthread.c)

    if(LINUX)
        list(APPEND TY_SOURCES libty/device_linux.c
                               libty/timer_linux.c)
    elseif(APPLE)
        list(APPEND TY_SOURCES libty/device_darwin.c
                               libty/timer_kqueue.c)
    else()
        message(FATAL_ERROR "Unsupported platform")
    endif()
endif()

set(TYC_SOURCES tyc/list.c
                tyc/main.c
                tyc/monitor.c
                tyc/reset.c
                tyc/upload.c)

set(TYQ_SOURCES tyqt/about_dialog.cc
                tyqt/main.cc
                tyqt/main_window.cc
                tyqt/board.cc
                tyqt/board_widget.cc
                tyqt/commands.cc
                tyqt/descriptor_set_notifier.cc
                tyqt/selector_dialog.cc
                tyqt/session_channel.cc
                tyqt/tyqt.cc)
set(TYQ_FORMS tyqt/about_dialog.ui
              tyqt/main_window.ui
              tyqt/board_widget.ui
              tyqt/selector_dialog.ui)
set(TYQ_RESOURCES resources/tyqt.qrc)
if(WIN32)
    list(APPEND TYQ_SOURCES resources/tyqt_win32.rc)
endif()

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

qt5_wrap_ui(TYQ_FORMS_HEADERS ${TYQ_FORMS})
qt5_add_resources(TYQ_RESOURCES_RCC ${TYQ_RESOURCES})

add_executable(tyqt WIN32 ${TYQ_SOURCES} ${TYQ_FORMS_HEADERS} ${TYQ_RESOURCES_RCC})
target_link_libraries(tyqt ${TY_LIBRARIES} Qt5::Core Qt5::Widgets Qt5::Network)
set_target_properties(tyqt PROPERTIES AUTOMOC ON)

get_target_property(QT5_TYPE Qt5::Core TYPE)
if(QT5_TYPE STREQUAL "STATIC_LIBRARY")
    get_target_property(QT5_LIBRARY_DIRECTORY Qt5::Core LOCATION)
    get_filename_component(QT5_LIBRARY_DIRECTORY "${QT5_LIBRARY_DIRECTORY}" DIRECTORY)

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
        find_library(qtpcre_LIBRARIES_DEBUG qtpcred
            HINTS "${QT5_LIBRARY_DIRECTORY}" "${QT5_LIBRARY_DIRECTORY}/../lib"
            NO_CMAKE_FIND_ROOT_PATH)
        find_library(qtpcre_LIBRARIES_OPTIMIZED qtpcre
            HINTS "${QT5_LIBRARY_DIRECTORY}" "${QT5_LIBRARY_DIRECTORY}/../lib"
            NO_CMAKE_FIND_ROOT_PATH)

        target_link_libraries(tyqt
            Qt5::QWindowsIntegrationPlugin imm32 winmm
            debug ${Qt5PlatformSupport_LIBRARIES_DEBUG} ${qtpcre_LIBRARIES_DEBUG}
            optimized ${Qt5PlatformSupport_LIBRARIES_OPTIMIZED} ${qtpcre_LIBRARIES_OPTIMIZED})
    elseif(APPLE)
        find_library(COCOA_LIBRARIES Cocoa)
        find_library(CARBON_LIBRARIES Carbon)
        find_library(SC_LIBRARIES SystemConfiguration)
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
            ${COCOA_LIBRARIES} ${CARBON_LIBRARIES} ${ZLIB_LIBRARIES} ${CUPS_LIBRARIES} ${SC_LIBRARIES}
            debug ${Qt5PlatformSupport_LIBRARIES_DEBUG}
            optimized ${Qt5PlatformSupport_LIBRARIES_OPTIMIZED})
    endif()
endif()

if(WIN32)
    add_executable(tyqtc tyqt/tyqtc.c)
endif()

set(CPACK_PACKAGE_NAME "TyQt")
set(CPACK_PACKAGE_VENDOR "Koromix")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "GUI and command-line tools to manage Teensy devices")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_VERSION ${TY_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "TyQt")
set(CPACK_PACKAGE_EXECUTABLES tyqt "TyQt")
set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/resources/images/tyqt.png")

if(WIN32)
    if(QT5_TYPE STREQUAL "STATIC_LIBRARY")
        set(CPACK_GENERATOR ZIP WIX)

        set(CPACK_WIX_UPGRADE_GUID 72663aca-47a7-4b9b-aa53-aa067b872b8a)
        string(REGEX REPLACE "\\-.*$" "" CPACK_PACKAGE_VERSION ${CPACK_PACKAGE_VERSION})

        set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/resources/images/tyqt.ico")
        set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/contrib/win32/wix_banner.jpg")
        set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/contrib/win32/wix_dialog.jpg")

        install(TARGETS ty_shared RUNTIME DESTINATION .)
        install(TARGETS tyc tyqt RUNTIME DESTINATION .)

        install(FILES README.md DESTINATION . RENAME README.txt)
        install(FILES LICENSE.txt DESTINATION .)
        install(DIRECTORY contrib/win32/ DESTINATION . FILES_MATCHING PATTERN *.txt)

        include(CPack)
    else()
        message(WARNING "Cannot package Qt5 shared libraries, CPack disabled")
    endif()
elseif(APPLE)
    if(QT5_TYPE STREQUAL "STATIC_LIBRARY")
        set(CPACK_GENERATOR Bundle)

        set(CPACK_BUNDLE_NAME "TyQt")
        set(CPACK_BUNDLE_ICON "${CMAKE_SOURCE_DIR}/resources/images/tyqt.icns")
        set(CPACK_BUNDLE_PLIST "${CMAKE_BINARY_DIR}/tyqt_osx.plist")

        # Kind of a hack, but it works (the Bundle generator want to put everything in Resources)
        install(TARGETS ty_shared DESTINATION ../Frameworks)
        install(TARGETS tyqt tyc DESTINATION ../MacOS)

        install(FILES README.md DESTINATION . RENAME README.txt)
        install(FILES LICENSE.txt DESTINATION .)

        include(CPack)
    else()
        message(WARNING "Cannot package Qt5 shared libraries, CPack disabled")
    endif()
else()
    install(TARGETS ty_shared DESTINATION lib)
    install(TARGETS ty_static DESTINATION lib)
    install(DIRECTORY libty/include/ DESTINATION include)
    install(FILES "${CMAKE_BINARY_DIR}/ty/version.h" DESTINATION include/ty)

    install(TARGETS tyc tyqt RUNTIME DESTINATION bin)
    install(FILES "${CMAKE_BINARY_DIR}/tyqt_linux.desktop" DESTINATION share/applications RENAME tyqt.desktop)

    install(FILES contrib/udev/teensy.rules DESTINATION lib/udev/rules.d RENAME 49-teensy.rules)
endif()

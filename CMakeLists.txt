# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 2.8.12)
project(ty C CXX)

set(TY_VERSION 0.5.7)

find_package(Threads)
list(APPEND TY_LINK_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

if(CMAKE_C_COMPILER_ID STREQUAL Clang)
    set(CMAKE_COMPILER_IS_CLANG 1)
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "-std=gnu99 -fvisibility=hidden -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-missing-field-initializers -Wshadow -Wconversion -Wformat=2")
    set(CMAKE_CXX_FLAGS "-std=gnu++1y -fvisibility=hidden -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-missing-field-initializers -Wold-style-cast -Wformat=2")

    if(CMAKE_COMPILER_IS_GNUCC)
        foreach(lang C CXX)
            set(CMAKE_${lang}_FLAGS_DEBUG "-O1 -ggdb -D_FORTIFY_SOURCE=2")
            set(CMAKE_${lang}_FLAGS_RELWITHDEBINFO "-O2 -ggdb -D_FORTIFY_SOURCE=2")
        endforeach()
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-microsoft")
        foreach(lang C CXX)
            set(CMAKE_${lang}_FLAGS_DEBUG "-O1 -g -D_FORTIFY_SOURCE=2")
            set(CMAKE_${lang}_FLAGS_RELWITHDEBINFO "-O2 -g -D_FORTIFY_SOURCE=2")
        endforeach()
    endif()
    foreach(lang C CXX)
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
check_symbol_exists(pthread_cond_timedwait_relative_np pthread.h HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP)

include(CheckStructHasMember)
check_struct_has_member("struct stat" st_mtim sys/stat.h HAVE_STAT_MTIM)
if(NOT HAVE_STAT_MTIM)
    check_struct_has_member("struct stat" st_mtimespec sys/stat.h HAVE_STAT_MTIMESPEC)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
    find_package(Git REQUIRED)

    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags
        OUTPUT_VARIABLE TY_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    # Strip the 'v' prefix
    string(SUBSTRING ${TY_VERSION} 1 -1 TY_VERSION)
endif()

configure_file(src/config.h.in config.h)
configure_file(include/ty/version.h.in ty/version.h)
if (APPLE)
    configure_file(resources/tyqt_osx.plist.in tyqt_osx.plist)
elseif (NOT WIN32)
    configure_file(resources/tyqt_linux.desktop.in tyqt_linux.desktop)
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(include)

set(TY_SOURCES include/ty.h
               include/ty/board.h
               include/ty/common.h
               include/ty/device.h
               include/ty/firmware.h
               include/ty/system.h
               include/ty/timer.h

               src/board.c
               src/board_teensy.c
               src/board_priv.h
               src/common.c
               src/compat.c
               src/device.c
               src/device_priv.h
               src/firmware.c
               src/firmware_elf.c
               src/firmware_ihex.c
               src/htable.c
               src/list.h
               src/system.c)
if(WIN32)
    list(APPEND TY_SOURCES src/device_win32.c
                           src/system_win32.c
                           src/timer_win32.c)
else()
    list(APPEND TY_SOURCES src/device_posix.c
                           src/device_posix_priv.h
                           src/system_posix.c)

    if(LINUX)
        list(APPEND TY_SOURCES src/device_linux.c
                               src/timer_linux.c)
    elseif(APPLE)
        list(APPEND TY_SOURCES src/device_darwin.c
                               src/timer_kqueue.c)
    else()
        message(FATAL_ERROR "Unsupported platform")
    endif()
endif()

set(TYC_SOURCES cli/list.c
                cli/main.c
                cli/monitor.c
                cli/reset.c
                cli/upload.c)

set(TYQ_SOURCES qt/about_dialog.cc
                qt/main.cc
                qt/main_window.cc
                qt/board_proxy.cc
                qt/board_widget.cc
                qt/descriptor_set_notifier.cc
                qt/selector_dialog.cc
                qt/session_channel.cc
                qt/tyqt.cc)
set(TYQ_FORMS qt/about_dialog.ui
              qt/main_window.ui
              qt/board_widget.ui
              qt/selector_dialog.ui)
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
target_link_libraries(tyqt ${TY_LIBRARIES} Qt5::Widgets Qt5::Network)
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

        target_link_libraries(tyqt
            Qt5::QWindowsIntegrationPlugin imm32 winmm
            debug ${Qt5PlatformSupport_LIBRARIES_DEBUG}
            optimized ${Qt5PlatformSupport_LIBRARIES_OPTIMIZED})
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
    install(DIRECTORY include/ DESTINATION include FILES_MATCHING PATTERN *.h)
    install(FILES "${CMAKE_BINARY_DIR}/ty/version.h" DESTINATION include/ty)

    install(TARGETS tyc tyqt RUNTIME DESTINATION bin)
    install(FILES "${CMAKE_BINARY_DIR}/tyqt_linux.desktop" DESTINATION share/applications RENAME tyqt.desktop)

    install(FILES contrib/udev/teensy.rules DESTINATION lib/udev/rules.d RENAME 49-teensy.rules)
endif()

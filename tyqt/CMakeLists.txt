# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

find_package(Qt5 REQUIRED
    COMPONENTS Widgets Network PrintSupport
    HINTS "${CMAKE_SOURCE_DIR}/qt5/${HOST}"
    NO_CMAKE_FIND_ROOT_PATH)

set(TYQT_SOURCES about_dialog.cc
                 arduino_dialog.cc
                 arduino_install.cc
                 main.cc
                 main_window.cc
                 board.cc
                 board_widget.cc
                 commands.cc
                 database.cc
                 descriptor_notifier.cc
                 firmware.cc
                 monitor.cc
                 selector_dialog.cc
                 session_channel.cc
                 task.cc
                 tyqt.cc)
set(TYQT_FORMS about_dialog.ui
               arduino_dialog.ui
               main_window.ui
               board_widget.ui
               selector_dialog.ui)
set(TYQT_RESOURCES tyqt.qrc)
if(WIN32)
    list(APPEND TYQT_SOURCES tyqt_win32.rc)
endif()

qt5_wrap_ui(TYQT_FORMS_HEADERS ${TYQT_FORMS})
qt5_add_resources(TYQT_RESOURCES_RCC ${TYQT_RESOURCES})

add_executable(tyqt WIN32 ${TYQT_SOURCES} ${TYQT_FORMS_HEADERS} ${TYQT_RESOURCES_RCC})
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
        find_library(qtfreetype_LIBRARIES_DEBUG qtfreetyped
            HINTS "${QT5_LIBRARY_DIRECTORY}" "${QT5_LIBRARY_DIRECTORY}/../lib"
            NO_CMAKE_FIND_ROOT_PATH)
        find_library(qtfreetype_LIBRARIES_OPTIMIZED qtfreetype
            HINTS "${QT5_LIBRARY_DIRECTORY}" "${QT5_LIBRARY_DIRECTORY}/../lib"
            NO_CMAKE_FIND_ROOT_PATH)

        target_link_libraries(tyqt
            Qt5::QWindowsIntegrationPlugin imm32 winmm
            debug ${Qt5PlatformSupport_LIBRARIES_DEBUG} ${qtpcre_LIBRARIES_DEBUG} ${qtfreetype_LIBRARIES_DEBUG}
            optimized ${Qt5PlatformSupport_LIBRARIES_OPTIMIZED} ${qtpcre_LIBRARIES_OPTIMIZED} ${qtfreetype_LIBRARIES_OPTIMIZED})
    elseif(APPLE)
        find_library(COCOA_LIBRARIES Cocoa)
        find_library(CARBON_LIBRARIES Carbon)
        find_library(SECURITY_LIBRARIES Security)
        find_library(SC_LIBRARIES SystemConfiguration)
        find_package(ZLIB REQUIRED)
        find_package(Cups REQUIRED)

        find_library(Qt5PlatformSupport_LIBRARIES_DEBUG Qt5PlatformSupport_debug
            HINTS "${QT5_LIBRARY_DIRECTORY}" "${QT5_LIBRARY_DIRECTORY}/../lib"
            NO_CMAKE_FIND_ROOT_PATH)
        find_library(Qt5PlatformSupport_LIBRARIES_OPTIMIZED Qt5PlatformSupport
            HINTS "${QT5_LIBRARY_DIRECTORY}" "${QT5_LIBRARY_DIRECTORY}/../lib"
            NO_CMAKE_FIND_ROOT_PATH)
        find_library(qtpcre_LIBRARIES_DEBUG qtpcre_debug
            HINTS "${QT5_LIBRARY_DIRECTORY}" "${QT5_LIBRARY_DIRECTORY}/../lib"
            NO_CMAKE_FIND_ROOT_PATH)
        find_library(qtpcre_LIBRARIES_OPTIMIZED qtpcre
            HINTS "${QT5_LIBRARY_DIRECTORY}" "${QT5_LIBRARY_DIRECTORY}/../lib"
            NO_CMAKE_FIND_ROOT_PATH)

        target_link_libraries(tyqt
            Qt5::QCocoaIntegrationPlugin Qt5::PrintSupport
            debug ${Qt5PlatformSupport_LIBRARIES_DEBUG} ${qtpcre_LIBRARIES_DEBUG}
            optimized ${Qt5PlatformSupport_LIBRARIES_OPTIMIZED} ${qtpcre_LIBRARIES_OPTIMIZED}
            ${COCOA_LIBRARIES} ${CARBON_LIBRARIES} ${SECURITY_LIBRARIES}
            ${ZLIB_LIBRARIES} ${CUPS_LIBRARIES} ${SC_LIBRARIES})
    endif()
elseif(WIN32 OR APPLE)
    message(WARNING "Cannot package Qt5 shared libraries")
endif()

if(WIN32)
    add_executable(tyqtc tyqtc.c)
endif()

if (APPLE)
    configure_file(tyqt_osx.plist.in tyqt_osx.plist)
elseif (NOT WIN32)
    configure_file(tyqt_linux.desktop.in tyqt_linux.desktop)
endif()

if(WIN32)
    install(TARGETS tyqt tyqtc RUNTIME DESTINATION .)
elseif(APPLE)
    # Kind of a hack, but it works (the Bundle generator want to put everything in Resources)
    install(TARGETS tyqt DESTINATION ../MacOS)
else()
    install(TARGETS tyqt RUNTIME DESTINATION bin)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tyqt_linux.desktop" DESTINATION share/applications RENAME tyqt.desktop)
endif()

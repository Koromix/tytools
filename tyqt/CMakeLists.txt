# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

find_package(Qt5 REQUIRED COMPONENTS Widgets Network PrintSupport
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
                 enhanced_plain_text.cc
                 firmware.cc
                 log_dialog.cc
                 monitor.cc
                 preferences_dialog.cc
                 selector_dialog.cc
                 session_channel.cc
                 task.cc
                 tyqt.cc)
if(MSVC)
    list(APPEND TYQT_SOURCES "${CMAKE_SOURCE_DIR}/contrib/msvc/getopt.c")
endif()
set(TYQT_FORMS about_dialog.ui
               arduino_dialog.ui
               log_dialog.ui
               main_window.ui
               board_widget.ui
               preferences_dialog.ui
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

# Static libraries are painful. Be careful when you touch this, it was made
# through an extremely evolved trial and error process :]
get_target_property(QT5_TYPE Qt5::Core TYPE)
if(QT5_TYPE STREQUAL "STATIC_LIBRARY")
    get_target_property(QT5_DIRECTORY Qt5::Core LOCATION)
    get_filename_component(QT5_DIRECTORY "${QT5_DIRECTORY}" DIRECTORY)
    get_filename_component(QT5_DIRECTORY "${QT5_DIRECTORY}" DIRECTORY)
    set(QT5_LIBRARY_DIRECTORIES "${QT5_DIRECTORY}/lib" "${QT5_DIRECTORY}/plugins/platforms")

    if(WIN32)
        # Fix undefined reference to _imp__WSAAsyncSelect@16
        set_property(TARGET Qt5::Network APPEND PROPERTY INTERFACE_LINK_LIBRARIES ws2_32)

        # Why is there no config package for this?
        find_library(Qt5PlatformSupport_LIBRARIES Qt5PlatformSupport
            HINTS ${QT5_LIBRARY_DIRECTORIES}
            NO_CMAKE_FIND_ROOT_PATH)
        find_library(qtpcre_LIBRARIES qtpcre
            HINTS ${QT5_LIBRARY_DIRECTORIES}
            NO_CMAKE_FIND_ROOT_PATH)
        find_library(qtfreetype_LIBRARIES qtfreetype
            HINTS ${QT5_LIBRARY_DIRECTORIES}
            NO_CMAKE_FIND_ROOT_PATH)

        target_link_libraries(tyqt
            Qt5::QWindowsIntegrationPlugin imm32 winmm
            ${Qt5PlatformSupport_LIBRARIES} ${qtpcre_LIBRARIES} ${qtfreetype_LIBRARIES})
    elseif(APPLE)
        find_library(COCOA_LIBRARIES Cocoa)
        find_library(CARBON_LIBRARIES Carbon)
        find_library(SECURITY_LIBRARIES Security)
        find_library(SC_LIBRARIES SystemConfiguration)
        find_package(ZLIB REQUIRED)
        find_package(Cups REQUIRED)

        find_library(Qt5PlatformSupport_LIBRARIES Qt5PlatformSupport
            HINTS ${QT5_LIBRARY_DIRECTORIES}
            NO_CMAKE_FIND_ROOT_PATH)
        find_library(qtpcre_LIBRARIES qtpcre
            HINTS ${QT5_LIBRARY_DIRECTORIES}
            NO_CMAKE_FIND_ROOT_PATH)

        target_link_libraries(tyqt
            Qt5::QCocoaIntegrationPlugin Qt5::PrintSupport
            ${Qt5PlatformSupport_LIBRARIES} ${qtpcre_LIBRARIES}
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

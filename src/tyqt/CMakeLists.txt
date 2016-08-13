# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

find_package(EasyQt5)

set(TYQT_SOURCES about_dialog.cc
                 arduino_dialog.cc
                 arduino_install.cc
                 commands.cc
                 enhanced_group_box.cc
                 enhanced_plain_text.cc
                 main.cc
                 main_window.cc
                 preferences_dialog.cc
                 selector_dialog.cc
                 tyqt.cc)
set(TYQT_FORMS about_dialog.ui
               arduino_dialog.ui
               main_window.ui
               preferences_dialog.ui
               selector_dialog.ui)

set(TYQT_ICON_FILE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/images/tyqt")
set(TYQT_RESOURCES tyqt.qrc)
if(TY_CONFIG_VARIANT)
    if(EXISTS "${TY_CONFIG_VARIANT_PATH}/tyqt.png")
        set(TYQT_ICON_FILE_PREFIX "{TY_CONFIG_VARIANT_PATH}/tyqt")
    endif()
    if(EXISTS "${TY_CONFIG_VARIANT_PATH}/tyqt_resources.qrc")
        list(INSERT TYQT_RESOURCES 0 "${TY_CONFIG_VARIANT_PATH}/tyqt_resources.qrc")
    endif()
endif()

if(WIN32)
    configure_file(tyqt_win32.rc.in tyqt_win32.rc)
    list(APPEND TYQT_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/tyqt_win32.rc")
elseif(APPLE)
    list(APPEND TYQT_SOURCES "${TYQT_ICON_FILE_PREFIX}.icns")
endif()

qt5_wrap_ui(TYQT_FORMS_HEADERS ${TYQT_FORMS})
qt5_add_resources(TYQT_RESOURCES_RCC ${TYQT_RESOURCES})

add_executable(tyqt WIN32 MACOSX_BUNDLE
               ${TYQT_SOURCES} ${TYQT_FORMS_HEADERS} ${TYQT_RESOURCES_RCC})
set_target_properties(tyqt PROPERTIES AUTOMOC ON
                                      OUTPUT_NAME ${TY_CONFIG_TYQT_EXECUTABLE})
target_link_libraries(tyqt PRIVATE ${LIBHS_LIBRARIES} libty libtyqt EasyQt5)
target_include_directories(tyqt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                        ${CMAKE_CURRENT_BINARY_DIR})
if(APPLE)
    set_source_files_properties("${TYQT_ICON_FILE_PREFIX}.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    get_filename_component(_icon_file_name "${TYQT_ICON_FILE_PREFIX}.icns" NAME)
    set_target_properties(tyqt PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME ${TY_CONFIG_TYQT_NAME}
                                          MACOSX_BUNDLE_ICON_FILE ${_icon_file_name})
endif()

if(WIN32)
    add_executable(tyqtc tyqtc.c)
    set_target_properties(tyqtc PROPERTIES OUTPUT_NAME ${TY_CONFIG_TYQT_EXECUTABLE}c)
endif()

if(WIN32)
    install(TARGETS tyqt tyqtc RUNTIME DESTINATION .)
elseif(APPLE)
    install(TARGETS tyqt BUNDLE DESTINATION .)
else()
    install(TARGETS tyqt RUNTIME DESTINATION bin)
    configure_file(tyqt_linux.desktop.in tyqt_linux.desktop)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tyqt_linux.desktop" DESTINATION share/applications RENAME tyqt.desktop)
endif()

set(TYQT_ICON_FILE_PREFIX ${TYQT_ICON_FILE_PREFIX} PARENT_SCOPE)

# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

find_package(EasyQt5)

set(UPTY_SOURCES updater_window.cc
                 upty.cc)
set(UPTY_FORMS updater_window.ui)

set(UPTY_ICON_FILE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/images/upty")
set(UPTY_RESOURCES upty.qrc)
if(TY_CONFIG_VARIANT)
    if(EXISTS "${TY_CONFIG_VARIANT_PATH}/upty.png")
        set(UPTY_ICON_FILE_PREFIX "${TY_CONFIG_VARIANT_PATH}/upty")
    endif()
    if(EXISTS "${TY_CONFIG_VARIANT_PATH}/upty_resources.qrc")
        list(INSERT UPTY_RESOURCES 0 "${TY_CONFIG_VARIANT_PATH}/upty_resources.qrc")
    endif()
endif()

if(WIN32)
    configure_file(upty_win32.rc.in upty_win32.rc)
    list(APPEND UPTY_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/upty_win32.rc")
elseif(APPLE)
    list(APPEND UPTY_SOURCES "${UPTY_ICON_FILE_PREFIX}.icns")
endif()

qt5_wrap_ui(UPTY_FORMS_HEADERS ${UPTY_FORMS})
qt5_add_resources(UPTY_RESOURCES_RCC ${UPTY_RESOURCES})

add_executable(upty WIN32 MACOSX_BUNDLE
               ${UPTY_SOURCES} ${UPTY_FORMS_HEADERS} ${UPTY_RESOURCES_RCC})
set_target_properties(upty PROPERTIES AUTOMOC ON
                                      OUTPUT_NAME ${TY_CONFIG_UPTY_EXECUTABLE})
target_link_libraries(upty PRIVATE ${LIBHS_LIBRARIES} libty libtyqt EasyQt5)
target_include_directories(upty PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                        ${CMAKE_CURRENT_BINARY_DIR})
if(APPLE)
    set_source_files_properties("${UPTY_ICON_FILE_PREFIX}.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    get_filename_component(_icon_file_name "${UPTY_ICON_FILE_PREFIX}.icns" NAME)
    set_target_properties(upty PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME ${TY_CONFIG_UPTY_NAME}
                                          MACOSX_BUNDLE_ICON_FILE ${_icon_file_name})
endif()

if(WIN32)
    install(TARGETS upty RUNTIME DESTINATION .)
elseif(APPLE)
    install(TARGETS upty BUNDLE DESTINATION .)
else()
    install(TARGETS upty RUNTIME DESTINATION bin)
    configure_file(upty_linux.desktop.in upty_linux.desktop)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/upty_linux.desktop" DESTINATION share/applications RENAME upty.desktop)
endif()

set(UPTY_ICON_FILE_PREFIX ${UPTY_ICON_FILE_PREFIX} PARENT_SCOPE)

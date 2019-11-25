# TyTools - public domain
# Niels Martignène <niels.martignene@protonmail.com>
# https://koromix.dev/tytools

# This software is in the public domain. Where that dedication is not
# recognized, you are granted a perpetual, irrevocable license to copy,
# distribute, and modify this file as you see fit.

# See the LICENSE file for more details.

find_package(EasyQt5)

set(TYCOMMANDER_SOURCES about_dialog.cc
                        arduino_dialog.cc
                        arduino_install.cc
                        board.cc
                        board.hpp
                        board_widget.cc
                        board_widget.hpp
                        client_handler.cc
                        client_handler.hpp
                        database.cc
                        database.hpp
                        descriptor_notifier.cc
                        descriptor_notifier.hpp
                        enhanced_widgets.cc
                        enhanced_widgets.hpp
                        firmware.cc
                        firmware.hpp
                        log_dialog.cc
                        log_dialog.hpp
                        main.cc
                        main_window.cc
                        main_window.hpp
                        monitor.cc
                        monitor.hpp
                        preferences_dialog.cc
                        preferences_dialog.hpp
                        selector_dialog.cc
                        selector_dialog.hpp
                        session_channel.cc
                        session_channel.hpp
                        task.cc
                        task.hpp
                        tycommander.cc
                        tycommander.hpp)
set(TYCOMMANDER_FORMS about_dialog.ui
                      arduino_dialog.ui
                      board_widget.ui
                      log_dialog.ui
                      main_window.ui
                      preferences_dialog.ui
                      selector_dialog.ui)

set(TYCOMMANDER_ICON_FILE_PREFIX "${CMAKE_SOURCE_DIR}/resources/images/tycommander")
set(TYCOMMANDER_RESOURCES "${CMAKE_SOURCE_DIR}/resources/resources.qrc")
if(CONFIG_VARIANT)
    if(EXISTS "${CONFIG_VARIANT_PATH}/tycommander.png")
        set(TYCOMMANDER_ICON_FILE_PREFIX "{CONFIG_VARIANT_PATH}/tycommander")
    endif()
    if(EXISTS "${CONFIG_VARIANT_PATH}/resources.qrc")
        set(TYUPDATER_RESOURCES "${CONFIG_VARIANT_PATH}/resources.qrc")
    endif()
endif()

if(WIN32)
    configure_file(tycommander_win32.rc.in tycommander_win32.rc)
    list(APPEND TYCOMMANDER_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/tycommander_win32.rc")
elseif(APPLE)
    list(APPEND TYCOMMANDER_SOURCES "${TYCOMMANDER_ICON_FILE_PREFIX}.icns")
endif()

qt5_wrap_ui(TYCOMMANDER_FORMS_HEADERS ${TYCOMMANDER_FORMS})
qt5_add_resources(TYCOMMANDER_RESOURCES_RCC ${TYCOMMANDER_RESOURCES})

add_executable(tycommander WIN32 MACOSX_BUNDLE
               ${TYCOMMANDER_SOURCES} ${TYCOMMANDER_FORMS_HEADERS} ${TYCOMMANDER_RESOURCES_RCC})
set_target_properties(tycommander PROPERTIES AUTOMOC ON
                                             OUTPUT_NAME ${CONFIG_TYCOMMANDER_EXECUTABLE}
                                             MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/tycommander_macos.plist.in")
target_link_libraries(tycommander PRIVATE libhs libty EasyQt5)
# We need these include directories for moc-generated files
target_include_directories(tycommander PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                               ${CMAKE_CURRENT_BINARY_DIR})
if(APPLE)
    set_source_files_properties("${TYCOMMANDER_ICON_FILE_PREFIX}.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    get_filename_component(_icon_file_name "${TYCOMMANDER_ICON_FILE_PREFIX}.icns" NAME)
    set_target_properties(tycommander PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME ${CONFIG_TYCOMMANDER_NAME}
                                                 MACOSX_BUNDLE_ICON_FILE ${_icon_file_name})
endif()
enable_unity_build(tycommander)

if(WIN32)
    add_executable(tycommanderc tycommanderc.c)
    set_target_properties(tycommanderc PROPERTIES OUTPUT_NAME "${CONFIG_TYCOMMANDER_EXECUTABLE}C")
    enable_unity_build(tycommanderc)
endif()

if(WIN32)
    install(TARGETS tycommander tycommanderc RUNTIME DESTINATION .)
elseif(APPLE)
    install(TARGETS tycommander BUNDLE DESTINATION .)
else()
    install(TARGETS tycommander RUNTIME DESTINATION bin)
    configure_file(tycommander_linux.desktop.in tycommander_linux.desktop)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tycommander_linux.desktop" DESTINATION share/applications
            RENAME tycommander.desktop)
endif()

set(TYCOMMANDER_ICON_FILE_PREFIX ${TYCOMMANDER_ICON_FILE_PREFIX} PARENT_SCOPE)

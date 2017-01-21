# Teensy Tools - public domain
# Niels Martignène <niels.martignene@gmail.com>
# https://neodd.com/teensytools

# This software is in the public domain. Where that dedication is not
# recognized, you are granted a perpetual, irrevocable license to copy,
# distribute, and modify this file as you see fit.

# See the LICENSE file for more details.

find_package(EasyQt5)

set(TEENSYCOMMANDER_SOURCES about_dialog.cc
                            arduino_dialog.cc
                            arduino_install.cc
                            board.cc
                            board.hpp
                            board_widget.cc
                            board_widget.hpp
                            database.cc
                            database.hpp
                            descriptor_notifier.cc
                            descriptor_notifier.hpp
                            client_handler.cc
                            enhanced_group_box.cc
                            enhanced_plain_text.cc
                            firmware.cc
                            firmware.hpp
                            log_dialog.cc
                            log_dialog.hpp
                            main.cc
                            main_window.cc
                            monitor.cc
                            monitor.hpp
                            preferences_dialog.cc
                            selector_dialog.cc
                            session_channel.cc
                            session_channel.hpp
                            task.cc
                            task.hpp
                            teensycommander.cc)
set(TEENSYCOMMANDER_FORMS about_dialog.ui
                          arduino_dialog.ui
                          board_widget.ui
                          log_dialog.ui
                          main_window.ui
                          preferences_dialog.ui
                          selector_dialog.ui)

set(TEENSYCOMMANDER_ICON_FILE_PREFIX "${CMAKE_SOURCE_DIR}/resources/images/teensycommander")
set(TEENSYCOMMANDER_RESOURCES "${CMAKE_SOURCE_DIR}/resources/resources.qrc")
if(CONFIG_VARIANT)
    if(EXISTS "${CONFIG_VARIANT_PATH}/teensycommander.png")
        set(TEENSYCOMMANDER_ICON_FILE_PREFIX "{CONFIG_VARIANT_PATH}/teensycommander")
    endif()
    if(EXISTS "${CONFIG_VARIANT_PATH}/resources.qrc")
        list(INSERT TEENSYCOMMANDER_RESOURCES 0 "${CONFIG_VARIANT_PATH}/resources.qrc")
    endif()
endif()

if(WIN32)
    configure_file(teensycommander_win32.rc.in teensycommander_win32.rc)
    list(APPEND TEENSYCOMMANDER_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/teensycommander_win32.rc")
elseif(APPLE)
    list(APPEND TEENSYCOMMANDER_SOURCES "${TEENSYCOMMANDER_ICON_FILE_PREFIX}.icns")
endif()

qt5_wrap_ui(TEENSYCOMMANDER_FORMS_HEADERS ${TEENSYCOMMANDER_FORMS})
qt5_add_resources(TEENSYCOMMANDER_RESOURCES_RCC ${TEENSYCOMMANDER_RESOURCES})

add_executable(teensycommander WIN32 MACOSX_BUNDLE
               ${TEENSYCOMMANDER_SOURCES} ${TEENSYCOMMANDER_FORMS_HEADERS} ${TEENSYCOMMANDER_RESOURCES_RCC})
set_target_properties(teensycommander PROPERTIES AUTOMOC ON
                                                 OUTPUT_NAME ${CONFIG_TEENSYCOMMANDER_EXECUTABLE})
target_link_libraries(teensycommander PRIVATE libhs libty EasyQt5)
# We need these include directories for moc-generated files
target_include_directories(teensycommander PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                                   ${CMAKE_CURRENT_BINARY_DIR})
if(APPLE)
    set_source_files_properties("${TEENSYCOMMANDER_ICON_FILE_PREFIX}.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    get_filename_component(_icon_file_name "${TEENSYCOMMANDER_ICON_FILE_PREFIX}.icns" NAME)
    set_target_properties(teensycommander PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME ${CONFIG_TEENSYCOMMANDER_NAME}
                                                     MACOSX_BUNDLE_ICON_FILE ${_icon_file_name})
endif()
enable_unity_build(teensycommander)

if(WIN32)
    add_executable(teensycommanderc teensycommanderc.c)
    set_target_properties(teensycommanderc PROPERTIES OUTPUT_NAME "${CONFIG_TEENSYCOMMANDER_EXECUTABLE}C")
    enable_unity_build(teensycommanderc)
endif()

if(WIN32)
    install(TARGETS teensycommander teensycommanderc RUNTIME DESTINATION .)
elseif(APPLE)
    install(TARGETS teensycommander BUNDLE DESTINATION .)
else()
    install(TARGETS teensycommander RUNTIME DESTINATION bin)
    configure_file(teensycommander_linux.desktop.in teensycommander_linux.desktop)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/teensycommander_linux.desktop" DESTINATION share/applications
            RENAME teensycommander.desktop)
endif()

set(TEENSYCOMMANDER_ICON_FILE_PREFIX ${TEENSYCOMMANDER_ICON_FILE_PREFIX} PARENT_SCOPE)

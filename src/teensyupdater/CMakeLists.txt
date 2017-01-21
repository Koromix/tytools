# Teensy Tools - public domain
# Niels Martignène <niels.martignene@gmail.com>
# https://neodd.com/teensytools

# This software is in the public domain. Where that dedication is not
# recognized, you are granted a perpetual, irrevocable license to copy,
# distribute, and modify this file as you see fit.

# See the LICENSE file for more details.

find_package(EasyQt5)

set(TEENSYUPDATER_SOURCES ../tyqt/board.cc
                          ../tyqt/board.hpp
                          ../tyqt/database.cc
                          ../tyqt/database.hpp
                          ../tyqt/descriptor_notifier.cc
                          ../tyqt/descriptor_notifier.hpp
                          ../tyqt/firmware.cc
                          ../tyqt/firmware.hpp
                          ../tyqt/log_dialog.cc
                          ../tyqt/log_dialog.hpp
                          ../tyqt/monitor.cc
                          ../tyqt/monitor.hpp
                          ../tyqt/task.cc
                          ../tyqt/task.hpp
                          teensyupdater.cc
                          updater_window.cc)
set(TEENSYUPDATER_FORMS ../tyqt/log_dialog.ui
                        updater_window.ui)

set(TEENSYUPDATER_ICON_FILE_PREFIX "${CMAKE_SOURCE_DIR}/resources/images/teensyupdater")
set(TEENSYUPDATER_RESOURCES "${CMAKE_SOURCE_DIR}/resources/resources.qrc")
if(CONFIG_VARIANT)
    if(EXISTS "${CONFIG_VARIANT_PATH}/teensyupdater.png")
        set(TEENSYUPDATER_ICON_FILE_PREFIX "${CONFIG_VARIANT_PATH}/teensyupdater")
    endif()
    if(EXISTS "${CONFIG_VARIANT_PATH}/resources.qrc")
        list(INSERT TEENSYUPDATER_RESOURCES 0 "${CONFIG_VARIANT_PATH}/resources.qrc")
    endif()
endif()

if(WIN32)
    configure_file(teensyupdater_win32.rc.in teensyupdater_win32.rc)
    list(APPEND TEENSYUPDATER_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/teensyupdater_win32.rc")
elseif(APPLE)
    list(APPEND TEENSYUPDATER_SOURCES "${TEENSYUPDATER_ICON_FILE_PREFIX}.icns")
endif()

qt5_wrap_ui(TEENSYUPDATER_FORMS_HEADERS ${TEENSYUPDATER_FORMS})
qt5_add_resources(TEENSYUPDATER_RESOURCES_RCC ${TEENSYUPDATER_RESOURCES})

add_executable(teensyupdater WIN32 MACOSX_BUNDLE
               ${TEENSYUPDATER_SOURCES} ${TEENSYUPDATER_FORMS_HEADERS} ${TEENSYUPDATER_RESOURCES_RCC})
set_target_properties(teensyupdater PROPERTIES AUTOMOC ON
                                               OUTPUT_NAME ${CONFIG_TEENSYUPDATER_EXECUTABLE})
target_link_libraries(teensyupdater PRIVATE libhs libty EasyQt5)
# We need these include directories for moc-generated files
target_include_directories(teensyupdater PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                                 ${CMAKE_CURRENT_BINARY_DIR})
if(APPLE)
    set_source_files_properties("${TEENSYUPDATER_ICON_FILE_PREFIX}.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    get_filename_component(_icon_file_name "${TEENSYUPDATER_ICON_FILE_PREFIX}.icns" NAME)
    set_target_properties(teensyupdater PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME ${CONFIG_TEENSYUPDATER_NAME}
                                                   MACOSX_BUNDLE_ICON_FILE ${_icon_file_name})
endif()
enable_unity_build(teensyupdater)

if(WIN32)
    install(TARGETS teensyupdater RUNTIME DESTINATION .)
elseif(APPLE)
    install(TARGETS teensyupdater BUNDLE DESTINATION .)
else()
    install(TARGETS teensyupdater RUNTIME DESTINATION bin)
    configure_file(teensyupdater_linux.desktop.in teensyupdater_linux.desktop)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/teensyupdater_linux.desktop" DESTINATION share/applications RENAME teensyupdater.desktop)
endif()

set(TEENSYUPDATER_ICON_FILE_PREFIX ${TEENSYUPDATER_ICON_FILE_PREFIX} PARENT_SCOPE)

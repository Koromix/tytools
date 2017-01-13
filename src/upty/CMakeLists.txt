# Teensy Tools - public domain
# Niels Martignène <niels.martignene@gmail.com>
# https://neodd.com/teensytools

# This software is in the public domain. Where that dedication is not
# recognized, you are granted a perpetual, irrevocable license to copy,
# distribute, and modify this file as you see fit.

# See the LICENSE file for more details.

find_package(EasyQt5)

set(UPTY_SOURCES ../tyqt/board.cc
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
                 updater_window.cc
                 upty.cc)
set(UPTY_FORMS ../tyqt/log_dialog.ui
               updater_window.ui)

set(UPTY_ICON_FILE_PREFIX "${CMAKE_SOURCE_DIR}/resources/images/upty")
set(UPTY_RESOURCES "${CMAKE_SOURCE_DIR}/resources/resources.qrc")
if(CONFIG_VARIANT)
    if(EXISTS "${CONFIG_VARIANT_PATH}/upty.png")
        set(UPTY_ICON_FILE_PREFIX "${CONFIG_VARIANT_PATH}/upty")
    endif()
    if(EXISTS "${CONFIG_VARIANT_PATH}/resources.qrc")
        list(INSERT UPTY_RESOURCES 0 "${CONFIG_VARIANT_PATH}/resources.qrc")
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
                                      OUTPUT_NAME ${CONFIG_UPTY_EXECUTABLE})
target_link_libraries(upty PRIVATE libhs libty EasyQt5)
# We need these include directories for moc-generated files
target_include_directories(upty PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                        ${CMAKE_CURRENT_BINARY_DIR})
if(APPLE)
    set_source_files_properties("${UPTY_ICON_FILE_PREFIX}.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    get_filename_component(_icon_file_name "${UPTY_ICON_FILE_PREFIX}.icns" NAME)
    set_target_properties(upty PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME ${CONFIG_UPTY_NAME}
                                          MACOSX_BUNDLE_ICON_FILE ${_icon_file_name})
endif()
enable_unity_build(upty)

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

# TyTools - public domain
# Niels Martignène <niels.martignene@protonmail.com>
# https://koromix.dev/tytools

# This software is in the public domain. Where that dedication is not
# recognized, you are granted a perpetual, irrevocable license to copy,
# distribute, and modify this file as you see fit.

# See the LICENSE file for more details.

find_package(EasyQt5)

set(TYUPDATER_SOURCES ../tycommander/board.cc
                      ../tycommander/board.hpp
                      ../tycommander/database.cc
                      ../tycommander/database.hpp
                      ../tycommander/descriptor_notifier.cc
                      ../tycommander/descriptor_notifier.hpp
                      ../tycommander/firmware.cc
                      ../tycommander/firmware.hpp
                      ../tycommander/log_dialog.cc
                      ../tycommander/log_dialog.hpp
                      ../tycommander/monitor.cc
                      ../tycommander/monitor.hpp
                      ../tycommander/task.cc
                      ../tycommander/task.hpp
                      tyupdater.cc
                      tyupdater.hpp
                      updater_window.cc
                      updater_window.hpp)
set(TYUPDATER_FORMS ../tycommander/log_dialog.ui
                    updater_window.ui)

set(TYUPDATER_ICON_FILE_PREFIX "${CMAKE_SOURCE_DIR}/resources/images/tyupdater")
set(TYUPDATER_RESOURCES "${CMAKE_SOURCE_DIR}/resources/resources.qrc")
if(CONFIG_VARIANT)
    if(EXISTS "${CONFIG_VARIANT_PATH}/tyupdater.png")
        set(TYUPDATER_ICON_FILE_PREFIX "${CONFIG_VARIANT_PATH}/tyupdater")
    endif()
    if(EXISTS "${CONFIG_VARIANT_PATH}/resources.qrc")
        set(TYUPDATER_RESOURCES "${CONFIG_VARIANT_PATH}/resources.qrc")
    endif()
endif()

if(WIN32)
    configure_file(tyupdater_win32.rc.in tyupdater_win32.rc)
    list(APPEND TYUPDATER_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/tyupdater_win32.rc")
elseif(APPLE)
    list(APPEND TYUPDATER_SOURCES "${TYUPDATER_ICON_FILE_PREFIX}.icns")
endif()

qt5_wrap_ui(TYUPDATER_FORMS_HEADERS ${TYUPDATER_FORMS})
qt5_add_resources(TYUPDATER_RESOURCES_RCC ${TYUPDATER_RESOURCES})

add_executable(tyupdater WIN32 MACOSX_BUNDLE
               ${TYUPDATER_SOURCES} ${TYUPDATER_FORMS_HEADERS} ${TYUPDATER_RESOURCES_RCC})
set_target_properties(tyupdater PROPERTIES AUTOMOC ON
                                           OUTPUT_NAME ${CONFIG_TYUPDATER_EXECUTABLE}
                                           MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/tyupdater_macos.plist.in")
target_link_libraries(tyupdater PRIVATE libhs libty EasyQt5)
# We need these include directories for moc-generated files
target_include_directories(tyupdater PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                             ${CMAKE_CURRENT_BINARY_DIR})
if(APPLE)
    set_source_files_properties("${TYUPDATER_ICON_FILE_PREFIX}.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    get_filename_component(_icon_file_name "${TYUPDATER_ICON_FILE_PREFIX}.icns" NAME)
    set_target_properties(tyupdater PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME ${CONFIG_TYUPDATER_NAME}
                                               MACOSX_BUNDLE_ICON_FILE ${_icon_file_name})
endif()
enable_unity_build(tyupdater)

if(WIN32)
    install(TARGETS tyupdater RUNTIME DESTINATION .)
elseif(APPLE)
    install(TARGETS tyupdater BUNDLE DESTINATION .)
else()
    install(TARGETS tyupdater RUNTIME DESTINATION bin)
    configure_file(tyupdater_linux.desktop.in tyupdater_linux.desktop)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tyupdater_linux.desktop" DESTINATION share/applications RENAME tyupdater.desktop)
endif()

set(TYUPDATER_ICON_FILE_PREFIX ${TYUPDATER_ICON_FILE_PREFIX} PARENT_SCOPE)

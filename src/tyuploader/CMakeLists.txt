# TyTools - public domain
# Niels Martignène <niels.martignene@protonmail.com>
# https://koromix.dev/tytools

# This software is in the public domain. Where that dedication is not
# recognized, you are granted a perpetual, irrevocable license to copy,
# distribute, and modify this file as you see fit.

# See the LICENSE file for more details.

find_package(EasyQt5)

set(TYUPLOADER_SOURCES ../tycommander/board.cc
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
                      tyuploader.cc
                      tyuploader.hpp
                      uploader_window.cc
                      uploader_window.hpp)
set(TYUPLOADER_FORMS ../tycommander/log_dialog.ui
                    uploader_window.ui)

set(TYUPLOADER_ICON_FILE_PREFIX "${CMAKE_SOURCE_DIR}/resources/images/tyuploader")
set(TYUPLOADER_RESOURCES "${CMAKE_SOURCE_DIR}/resources/resources.qrc")
if(CONFIG_VARIANT)
    if(EXISTS "${CONFIG_VARIANT_PATH}/tyuploader.png")
        set(TYUPLOADER_ICON_FILE_PREFIX "${CONFIG_VARIANT_PATH}/tyuploader")
    endif()
    if(EXISTS "${CONFIG_VARIANT_PATH}/resources.qrc")
        set(TYUPLOADER_RESOURCES "${CONFIG_VARIANT_PATH}/resources.qrc")
    endif()
endif()

if(WIN32)
    configure_file(tyuploader_win32.rc.in tyuploader_win32.rc)
    list(APPEND TYUPLOADER_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/tyuploader_win32.rc")
elseif(APPLE)
    list(APPEND TYUPLOADER_SOURCES "${TYUPLOADER_ICON_FILE_PREFIX}.icns")
endif()

qt5_wrap_ui(TYUPLOADER_FORMS_HEADERS ${TYUPLOADER_FORMS})
qt5_add_resources(TYUPLOADER_RESOURCES_RCC ${TYUPLOADER_RESOURCES})

add_executable(tyuploader WIN32 MACOSX_BUNDLE
               ${TYUPLOADER_SOURCES} ${TYUPLOADER_FORMS_HEADERS} ${TYUPLOADER_RESOURCES_RCC})
set_target_properties(tyuploader PROPERTIES AUTOMOC ON
                                           OUTPUT_NAME ${CONFIG_TYUPLOADER_EXECUTABLE}
                                           MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/tyuploader_macos.plist.in")
target_link_libraries(tyuploader PRIVATE libhs libty EasyQt5)
# We need these include directories for moc-generated files
target_include_directories(tyuploader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                             ${CMAKE_CURRENT_BINARY_DIR})
if(APPLE)
    set_source_files_properties("${TYUPLOADER_ICON_FILE_PREFIX}.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    get_filename_component(_icon_file_name "${TYUPLOADER_ICON_FILE_PREFIX}.icns" NAME)
    set_target_properties(tyuploader PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME ${CONFIG_TYUPLOADER_NAME}
                                               MACOSX_BUNDLE_ICON_FILE ${_icon_file_name})
endif()
enable_unity_build(tyuploader)

if(WIN32)
    install(TARGETS tyuploader RUNTIME DESTINATION .)
elseif(APPLE)
    install(TARGETS tyuploader BUNDLE DESTINATION .)
else()
    install(TARGETS tyuploader RUNTIME DESTINATION bin)
    configure_file(tyuploader_linux.desktop.in tyuploader_linux.desktop)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tyuploader_linux.desktop" DESTINATION share/applications RENAME tyuploader.desktop)
endif()

set(TYUPLOADER_ICON_FILE_PREFIX ${TYUPLOADER_ICON_FILE_PREFIX} PARENT_SCOPE)

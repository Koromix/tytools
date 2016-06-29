# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

find_package(EasyQt5)

set(LIBTYQT_SOURCES ../../include/tyqt/board.hpp
                    ../../include/tyqt/board_widget.hpp
                    ../../include/tyqt/database.hpp
                    ../../include/tyqt/descriptor_notifier.hpp
                    ../../include/tyqt/firmware.hpp
                    ../../include/tyqt/log_dialog.hpp
                    ../../include/tyqt/monitor.hpp
                    ../../include/tyqt/session_channel.hpp
                    ../../include/tyqt/task.hpp

                    board.cc
                    board_widget.cc
                    database.cc
                    descriptor_notifier.cc
                    firmware.cc
                    log_dialog.cc
                    monitor.cc
                    session_channel.cc
                    task.cc)
set(LIBTYQT_FORMS board_widget.ui
                  log_dialog.ui)
set(LIBTYQT_RESOURCES libtyqt.qrc)

qt5_wrap_ui(LIBTYQT_FORMS_HEADERS ${LIBTYQT_FORMS})
qt5_add_resources(LIBTYQT_RESOURCES_RCC ${LIBTYQT_RESOURCES})

add_library(libtyqt STATIC ${LIBTYQT_SOURCES} ${LIBTYQT_FORMS_HEADERS} ${LIBTYQT_RESOURCES_RCC})
set_target_properties(libtyqt PROPERTIES AUTOMOC ON
                                         OUTPUT_NAME tyqt)
target_link_libraries(libtyqt PRIVATE ${LIBHS_LIBRARIES} libty EasyQt5)
target_include_directories(libtyqt PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
                                           ${CMAKE_CURRENT_BINARY_DIR}
                                   PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

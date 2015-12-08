# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

TEMPLATE = app
QT += widgets network

TARGET = tyqt

LIBS = -lty

FORMS += about_dialog.ui \
    board_widget.ui \
    main_window.ui \
    selector_dialog.ui

HEADERS += about_dialog.hh \
    board.hh \
    board_widget.hh \
    commands.hh \
    descriptor_set_notifier.hh \
    firmware.hh \
    main_window.hh \
    selector_dialog.hh \
    session_channel.hh \
    task.hh \
    tyqt.hh

SOURCES += about_dialog.cc \
    board.cc \
    board_widget.cc \
    commands.cc \
    descriptor_set_notifier.cc \
    firmware.cc \
    main.cc \
    main_window.cc \
    selector_dialog.cc \
    session_channel.cc \
    task.cc \
    tyqt.cc

RESOURCES += tyqt.qrc

# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

TEMPLATE = lib
CONFIG -= qt
CONFIG += dll

TARGET = ty

HEADERS += include/ty.h \
    include/ty/board.h \
    include/ty/common.h \
    include/ty/firmware.h \
    include/ty/monitor.h \
    include/ty/system.h \
    include/ty/task.h \
    include/ty/thread.h \
    include/ty/timer.h \
    board_priv.h \
    compat.h \
    config.h \
    firmware_priv.h \
    htable.h \
    list.h \
    task_priv.h

SOURCES += board.c \
    board_teensy.c \
    common.c \
    compat.c \
    firmware.c \
    firmware_elf.c \
    firmware_ihex.c \
    htable.c \
    monitor.c \
    system.c \
    task.c

LIBS += -lhs

win32 {
    LIBS += -lhid -lsetupapi

    SOURCES += system_win32.c \
        thread_win32.c \
        timer_win32.c
}

linux {
    LIBS += -ludev -lpthread

    SOURCES += system_posix.c \
        thread_pthread.c \
        timer_linux.c
}

macx {
    LIBS += -framework IOKit -framework CoreFoundation

    SOURCES += system_posix.c \
        thread_pthread.c \
        timer_kqueue.c
}

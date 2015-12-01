include(../ty_common.pri)

TEMPLATE = lib
CONFIG -= qt
CONFIG += dll

TARGET = ty

HEADERS += include/ty.h \
    include/ty/board.h \
    include/ty/common.h \
    include/ty/device.h \
    include/ty/firmware.h \
    include/ty/system.h \
    include/ty/thread.h \
    include/ty/timer.h \
    board_priv.h \
    compat.h \
    config.h \
    device_priv.h \
    device_posix_priv.h \
    firmware_priv.h \
    htable.h \
    list.h

SOURCES += board.c \
    board_teensy.c \
    common.c \
    compat.c \
    device.c \
    firmware.c \
    firmware_elf.c \
    firmware_ihex.c \
    htable.c \
    system.c

win32 {
    LIBS += -lhid -lsetupapi

    SOURCES += device_win32.c \
        system_win32.c \
        thread_win32.c \
        timer_win32.c
}

linux {
    LIBS += -ludev -lpthread

    SOURCES += device_linux.c \
        device_posix.c \
        system_posix.c \
        thread_pthread.c \
        timer_linux.c
}

macx {
    LIBS += -framework IOKit -framework CoreFoundation

    SOURCES += device_darwin.c \
        device_posix.c \
        system_posix.c \
        thread_pthread.c \
        timer_kqueue.c
}

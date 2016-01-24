TEMPLATE = lib
CONFIG -= qt
CONFIG += dll thread

TARGET = ../hs

HEADERS += ../include/hs.h \
    ../include/hs/common.h \
    ../include/hs/device.h \
    ../include/hs/hid.h \
    ../include/hs/match.h \
    ../include/hs/monitor.h \
    ../include/hs/platform.h \
    ../include/hs/serial.h \
    compat.h \
    device_priv.h \
    filter.h \
    htable.h \
    list.h \
    monitor_priv.h \
    util.h

SOURCES += common.c \
    compat.c \
    device.c \
    filter.c \
    htable.c \
    monitor.c \
    platform.c

win32 {
    LIBS += -lhid -lsetupapi

    SOURCES += device_win32.c \
        hid_win32.c \
        monitor_win32.c \
        platform_win32.c \
        serial_win32.c

    HEADERS += device_win32_priv.h
}

linux {
    LIBS += -ludev -lpthread

    SOURCES += device_posix.c \
        hid_linux.c \
        monitor_linux.c \
        platform_posix.c \
        serial_posix.c

    HEADERS += device_posix_priv.h
}

macx {
    LIBS += -framework IOKit -framework CoreFoundation

    SOURCES += device_posix.c \
        hid_darwin.c \
        monitor_darwin.c \
        platform_darwin.c \
        serial_posix.c

    HEADERS += device_posix_priv.h
}

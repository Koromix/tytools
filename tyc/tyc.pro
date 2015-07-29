include(../ty_common.pri)

TEMPLATE = app
CONFIG -= qt
CONFIG += console

TARGET = tyc

LIBS = -lty

HEADERS = main.h

SOURCES = list.c \
    main.c \
    monitor.c \
    reset.c \
    upload.c

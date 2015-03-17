include(../ty_common.pri)

TEMPLATE = app
QT += widgets network

TARGET = ../tyqt

LIBS = -lty

FORMS += about_dialog.ui \
    board_widget.ui \
    main_window.ui \
    selector_dialog.ui

HEADERS += about_dialog.hh \
    board_proxy.hh \
    board_widget.hh \
    descriptor_set_notifier.hh \
    main_window.hh \
    selector_dialog.hh \
    session_channel.hh \
    tyqt.hh

SOURCES += about_dialog.cc \
    board_proxy.cc \
    board_widget.cc \
    descriptor_set_notifier.cc \
    main.cc \
    main_window.cc \
    selector_dialog.cc \
    session_channel.cc \
    tyqt.cc

RESOURCES += ../resources/tyqt.qrc

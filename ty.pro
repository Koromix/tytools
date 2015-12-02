# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

TEMPLATE = subdirs

SUBDIRS = libty tyc tyqt
CONFIG += ordered

win32 {
    SUBDIRS += tyqtc
    tyqtc.file = tyqt/tyqtc.pro

    system($$QMAKE_MKDIR "\"$$OUT_PWD/ty\"" 2>NUL)
    system(scripts\git-version.bat $$OUT_PWD/ty/version.h)
}
unix {
    system($$QMAKE_MKDIR "\"$$OUT_PWD/ty\"")
    system(scripts/git-version.sh $$OUT_PWD/ty/version.h)
}

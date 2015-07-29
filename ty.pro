TEMPLATE = subdirs

SUBDIRS = src cli qt
CONFIG += ordered

win32 {
    SUBDIRS += tyqtc
    tyqtc.file = qt/tyqtc.pro

    system($$QMAKE_MKDIR "\"$$OUT_PWD/ty\"" 2>NUL)
    system(scripts\git-version.bat $$OUT_PWD/ty/version.h)
}
unix {
    system($$QMAKE_MKDIR "\"$$OUT_PWD/ty\"")
    system(scripts/git-version.sh $$OUT_PWD/ty/version.h)
}

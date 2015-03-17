TEMPLATE = subdirs

SUBDIRS = src cli qt
CONFIG += ordered

win32 {
    system($$QMAKE_MKDIR "\"$$OUT_PWD/ty\"" 2>NUL)
    system(scripts\git-version.bat $$OUT_PWD/ty/version.h)
}
unix {
    system($$QMAKE_MKDIR "\"$$OUT_PWD/ty\"")
    system(scripts/git-version.sh $$OUT_PWD/ty/version.h)
}

CONFIG -= warn_on debug_and_release app_bundle

INCLUDEPATH = $$PWD/include $$OUT_PWD/..

QMAKE_LIBDIR = $$OUT_PWD/..
QMAKE_RPATHDIR = $$OUT_PWD/..

QMAKE_CFLAGS += -std=gnu99 -fvisibility=hidden -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 \
    -Wall -Wextra -Wno-missing-field-initializers -Wshadow -Wconversion -Wformat=2
QMAKE_CXXFLAGS += -std=gnu++11 -fvisibility=hidden -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 \
    -Wall -Wextra -Wno-missing-field-initializers -Wold-style-cast -Wformat=2

win32 {
    DEFINES -= UNICODE
    DEFINES += WINVER=0x0602 _WIN32_WINNT=0x0602

    QMAKE_LFLAGS += -static-libgcc -static-libstdc++
    # CMake puts all this stuff by default, let's do it too. I guess some of it makes sense
    LIBS += -lkernel32 -luser32 -lgdi32 -lwinspool -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -ladvapi32
}

macx:clang {
    QMAKE_CXXFLAGS += -stdlib=libc++
    QMAKE_LFLAGS += -stdlib=libc++
}

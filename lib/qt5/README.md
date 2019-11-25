# Compilation on Windows

## MSVC 20xx 32-bit with static MSVCRT

These instructions have been tested with *Qt 5.12.2*, they will probably **not work for
Qt versions < 5.12**. Even if they work, you may not be able to link TyTools correctly.

Download qtbase source from http://download.qt.io/official_releases/qt/5.12/5.12.2/submodules/qtbase-everywhere-src-5.12.2.zip

Extract the directory inside it to "tytools/lib/qt5" and rename "qtbase-everywhere-src-5.12.2" to
"i686-win32-msvc-mt". Open the "VS20xx x86 Native Tools Command Prompt" and cd to
this directory.

```batch
cd i686-win32-msvc-mt
REM Now we are in tytools/lib/qt5/i686-win32-msvc-mt
configure -platform win32-msvc ^
    -opensource ^
    -confirm-license ^
    -static ^
    -static-runtime ^
    -release ^
    -nomake examples ^
    -nomake tests ^
    -no-opengl ^
    -no-harfbuzz ^
    -no-icu ^
    -no-cups ^
    -qt-pcre ^
    -qt-zlib ^
    -qt-freetype ^
    -qt-libpng ^
    -qt-libjpeg
nmake
```

Unfortunately Qt static builds are fragile and cannot be moved around. You will need to rebuild Qt
if you move your project.

## MSVC 20xx 64-bit with static MSVCRT

These instructions have been tested with *Qt 5.12.2*, they will probably **not work for
Qt versions < 5.12**. Even if they work, you may not be able to link TyTools correctly.

Download qtbase source from http://download.qt.io/official_releases/qt/5.12/5.12.2/submodules/qtbase-everywhere-src-5.12.2.zip

Extract the directory inside it to "tytools/lib/qt5" and rename "qtbase-everywhere-src-5.12.2" to
"x86_64-win32-msvc-mt". Open the "VS20xx x64 Native Tools Command Prompt" and cd to
this directory.

```batch
cd x86_64-win32-msvc-mt
REM Now we are in tytools/lib/qt5/x86_64-win32-msvc-mt
configure -platform win32-msvc ^
    -opensource ^
    -confirm-license ^
    -static ^
    -static-runtime ^
    -release ^
    -nomake examples ^
    -nomake tests ^
    -no-opengl ^
    -no-harfbuzz ^
    -no-icu ^
    -no-cups ^
    -qt-pcre ^
    -qt-zlib ^
    -qt-freetype ^
    -qt-libpng ^
    -qt-libjpeg
nmake
```

Unfortunately Qt static builds are fragile and cannot be moved around. You will need to rebuild Qt
if you move your project.

# Mac OS X / Clang 64-bit

These instructions have been tested with *Qt 5.9.7*, they will probably **not work for
Qt versions < or > 5.9**. Even if they work, you may not be able to link TyTools correctly.

A recent version of XCode must be installed.

Download qtbase source from http://download.qt.io/official_releases/qt/5.9/5.9.8/submodules/qtbase-opensource-src-5.9.8.tar.xz

Extract the directory inside it to "tytools/lib/qt5" and rename "qtbase-opensource-src-5.9.8" to
"x86_64-darwin-clang". Open a command prompt and go to that directory.

```sh
cd x86_64-darwin-clang
# Now we are in tytools/lib/qt5/x86_64-darwin-clang
./configure -platform macx-clang \
    -opensource \
    -confirm-license \
    -static \
    -release \
    -nomake examples \
    -nomake tests \
    -no-pkg-config \
    -no-harfbuzz \
    -no-icu \
    -no-cups \
    -no-freetype \
    -qt-pcre
make
```

Unfortunately Qt static builds are fragile and cannot be moved around. You will need to rebuild Qt
if you move your project.

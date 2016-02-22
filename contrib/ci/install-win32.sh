#!/bin/sh

sudo apt-get install -qqy cmake gcc-mingw-w64-i686 g++-mingw-w64-i686

wget https://bintray.com/artifact/download/koromix/ty/qtbase-$QT_VERSION-i686-w64-mingw32-static.txz
tar xJf qtbase-$QT_VERSION-i686-w64-mingw32-static.txz

#!/bin/sh

sudo apt-get install -qqy cmake gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64

wget https://bintray.com/artifact/download/koromix/ty/qtbase-$QT_VERSION-x86_64-w64-mingw32-static.txz
tar xJf qtbase-$QT_VERSION-x86_64-w64-mingw32-static.txz

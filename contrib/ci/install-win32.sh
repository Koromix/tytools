#!/bin/sh

sudo apt-get install -qqy cmake gcc-mingw-w64-i686 g++-mingw-w64-i686

wget https://bintray.com/artifact/download/koromix/ty/$QT_PACKAGE
tar xJf $QT_PACKAGE

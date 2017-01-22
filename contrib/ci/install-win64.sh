#!/bin/sh

sudo add-apt-repository -y ppa:george-edison55/cmake-3.x
sudo apt-get update -qq
sudo apt-get install -qqy cmake gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64

wget https://bintray.com/artifact/download/koromix/teensytools/$QT_PACKAGE
tar xJf $QT_PACKAGE

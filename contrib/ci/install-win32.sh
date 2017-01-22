#!/bin/sh

sudo apt-get update -qq
sudo apt-get install -qqy gcc-mingw-w64-i686 g++-mingw-w64-i686

mkdir deps && cd deps

wget http://www.cmake.org/files/v3.3/cmake-3.3.2-Linux-x86_64.tar.gz
mkdir cmake
tar -xz --strip-components=1 -C cmake -f cmake-3.3.2-Linux-x86_64.tar.gz
export PATH=$PWD/cmake/bin:$PATH

cd ..

wget https://bintray.com/artifact/download/koromix/teensytools/$QT_PACKAGE
tar xJf $QT_PACKAGE

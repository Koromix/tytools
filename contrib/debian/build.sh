#!/bin/sh -e

apt install -y libudev-dev qtbase5-dev pkg-config

# Fix git error about dubious repository ownership
git config --global safe.directory '*'

rm -rf /io/bin/Debian/build
mkdir /io/bin/Debian/build
cd /io/bin/Debian/build

cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr ../../..
ninja
DESTDIR=/io/bin/Debian ninja install

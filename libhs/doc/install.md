Installation {#install}
============

You can use [pre-built libraries](https://github.com/Koromix/libhs/releases) on Windows (MSVC
2015 and MinGW-w64) and OS X (Clang).

Three options are available to build libhs:
- [Qt Creator](https://www.qt.io/download/) on all supported platforms (libhs.pro).
- [Visual Studio 2015](https://www.visualstudio.com/) on Windows (libhs.sln).
- [CMake](http://www.cmake.org/) works on all platforms, and is able to do cross-compilation
from Linux.

Build on Windows {#install_windows}
================

[Visual Studio 2015](https://www.visualstudio.com/) is probably the easiest option
on Windows, but you can also use [Qt Creator](https://www.qt.io/download/).

### Using CMake

Open a console in the project directory and execute:
~~~~~~~~~~~~~~~~~~{.sh}
mkdir -p build/win32 && cd build/win32
cmake ../..
make
~~~~~~~~~~~~~~~~~~

Build on Linux {#install_linux}
==============

[Qt Creator](https://www.qt.io/download/) is also supported, use the following instructions if
you prefer to use the command line.

Dependencies
------------

To install the dependencies on _Debian or Ubuntu_ execute:
~~~~~~~~~~~~~~~~~~{.sh}
sudo apt-get install build-essential libudev-dev cmake
~~~~~~~~~~~~~~~~~~

On _Arch Linux_ you can do so (as root):
~~~~~~~~~~~~~~~~~~{.sh}
pacman -S --needed base-devel udev cmake
~~~~~~~~~~~~~~~~~~

Compilation
-----------

Open the project directory in a terminal and execute:
~~~~~~~~~~~~~~~~~~{.sh}
mkdir -p build/linux && cd build/linux
cmake ../..
make
~~~~~~~~~~~~~~~~~~

If you want to build a debug library, you have to specify the build type:
~~~~~~~~~~~~~~~~~~{.sh}
cmake -DCMAKE_BUILD_TYPE=Debug ../..
~~~~~~~~~~~~~~~~~~

Build on Mac OS X {#install_osx}
=================

Install [Xcode](https://developer.apple.com/xcode/downloads/) and the developer command-line
tools. Clang is the default compiler on this platform, although you can use GCC if you prefer.

You can use [CMake](http://www.cmake.org/) or [Qt Creator](https://www.qt.io/download/) to
build libhs. CMake can generate Xcode project files, refer to the relevant documentation.

### Using CMake

Open a console in the project directory and execute:
~~~~~~~~~~~~~~~~~~{.sh}
mkdir -p build/darwin && cd build/darwin
cmake ../..
make
~~~~~~~~~~~~~~~~~~

CMake supports universal binaries but does not build them by default. To generate universal
binaries for i386 and x86_64, use:
~~~~~~~~~~~~~~~~~~{.sh}
cmake -DCMAKE_OSX_ARCHITECTURES="i386;x86_64" ../..
~~~~~~~~~~~~~~~~~~

Cross-compile from Linux {#install_cross}
========================

### Using MinGW-w64 (for Windows)

Another option is to cross-compile the windows library from Linux. You need to install MinGW-w64
first.

On *Debian and Ubuntu*, install cmake and mingw-w64:
~~~~~~~~~~~~~~~~~~{.sh}
sudo apt-get install cmake mingw-w64
~~~~~~~~~~~~~~~~~~

If you use *Arch Linux*, execute (as root):
~~~~~~~~~~~~~~~~~~{.sh}
pacman -S --needed cmake mingw-w64-toolchain
~~~~~~~~~~~~~~~~~~

You can then use the appropriate toolchain file provided in the cmake directory. Open the project
directory in a console and execute:
~~~~~~~~~~~~~~~~~~{.sh}
mkdir -p build/win32 && cd build/win32
cmake -DCMAKE_TOOLCHAIN_FILE=../../cmake/i686-w64-mingw32.cmake ../..
make
~~~~~~~~~~~~~~~~~~

### Using osxcross (for Mac OS X)

You need to install [osxcross](https://github.com/tpoechtrager/osxcross).

Open a terminal and load the OSX environment (using osxcross-env). Go to the project directory
and execute:
~~~~~~~~~~~~~~~~~~{.sh}
mkdir -p build/darwin && cd build/darwin
cmake -DCMAKE_TOOLCHAIN_FILE=../../cmake/x86_64-darwin-clang.cmake ../..
make
~~~~~~~~~~~~~~~~~~

You can also make universal binaries under Linux. To build one with i386 and x86_64
binaries, use:
~~~~~~~~~~~~~~~~~~{.sh}
cmake -DCMAKE_TOOLCHAIN_FILE=../../cmake/x86_64-darwin-clang.cmake -DCMAKE_OSX_ARCHITECTURES="i386;x86_64" ../..
~~~~~~~~~~~~~~~~~~

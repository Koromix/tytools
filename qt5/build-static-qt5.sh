#!/bin/sh -e

print_usage() {
    printf "usage: %s [options] <version> (e.g. 5.5.1) <host>\n\n" "$(basename $0)"
    printf "This tool downloads the appropriate Qt sources and builds a static version\n"
    printf "of the Qt library.\n\n"
    printf "Options:\n"
    printf "   -d <directory>           Extract and build Qt in <directory>\n"
    printf "   -f                       Overwrite destination directory if it exists\n"
    printf "   -x                       Cross-compile Qt for platform <host>\n\n"
    printf "Supported hosts:\n"
    printf "   i686-w64-mingw32         Build for Windows (32 bits) using MinGW-w64\n"
    printf "   x86_64-w64-mingw32       Build for Windows (64 bits) using MinGW-w64\n"
    printf "   x86_64-darwin-clang      Build for Mac OS X using Clang (CROSS-COMPILATION DOES NOT WORK)\n"
}

while getopts ":d:fx" opt; do
    case $opt in
        d) DESTDIR=$OPTARG ;;
        f) OVERWRITE=true ;;
        x) CROSS=true ;;

        \?) echo "Invalid option -$OPTARG" >&2; print_usage >&2; exit 1 ;;
    esac
done
VERSION=${@:$OPTIND:1}
HOST=${@:$OPTIND+1:1}

[ -z "$VERSION" ] && (echo "Missing Qt version parameter" >&2; print_usage >&2; exit 1)
[ -z "$HOST" ] && (echo "Missing host parameter" >&2; print_usage >&2; exit 1)

case "$HOST" in
    i686-w64-mingw32)
        NATIVE_FLAGS="-platform win32-g++"
        CROSS_FLAGS="-xplatform win32-g++ -device-option CROSS_COMPILE=i686-w64-mingw32-"
        ;;
    x86_64-w64-mingw32)
        NATIVE_FLAGS="-platform win32-g++"
        CROSS_FLAGS="-xplatform win32-g++ -device-option CROSS_COMPILE=x86_64-w64-mingw32-"
        ;;
    x86_64-darwin-clang)
        NATIVE_FLAGS="-platform macx-clang"
        CROSS_FLAGS="-xplatform macx-clang -device-option CROSS_COMPILE=x86_64-darwin-clang-"
        ;;
    x86_64-darwin-gcc)
        NATIVE_FLAGS="-platform macx-g++"
        CROSS_FLAGS="-xplatform macx-g++ -device-option CROSS_COMPILE=x86_64-darwin-clang-"
        ;;

    *)
        echo "Invalid host '$HOST'" >&2
        print_usage >&2
        exit 1
        ;;
esac

DESTDIR=${DESTDIR:-${HOST}}
if [ "$CROSS" = true ]; then
    HOST_FLAGS="$CROSS_FLAGS"
else
    HOST_FLAGS="$NATIVE_FLAGS"
fi

# Delete any partial downloads using the checksum files, unfortunately I can't find
# any GPG signature for the source downloads.
[ ! -f qtbase-md5sums-$VERSION.txt ] && wget -O qtbase-md5sums-$VERSION.txt http://download.qt.io/official_releases/qt/$(echo $VERSION | cut -d. -f1,2)/$VERSION/submodules/md5sums.txt
md5sum --quiet -c qtbase-md5sums-$VERSION.txt 2>/dev/null | cut -d: -f1 | xargs rm -f
[ ! -f qtbase-opensource-src-$VERSION.tar.xz ] && wget http://download.qt.io/official_releases/qt/$(echo $VERSION | cut -d. -f1,2)/$VERSION/submodules/qtbase-opensource-src-$VERSION.tar.xz

echo Extracting qtbase-opensource-src-$VERSION.tar.xz...
if [ "$OVERWRITE" = true ]; then
    rm -rf "$DESTDIR"
fi
mkdir "$DESTDIR"
tar Jxf qtbase-opensource-src-$VERSION.tar.xz -C "$DESTDIR" --strip-components 1
cd "$DESTDIR"

./configure -static -opensource -confirm-license -nomake examples $HOST_FLAGS -no-pkg-config -no-opengl -no-harfbuzz -no-icu -no-cups -qt-pcre
make

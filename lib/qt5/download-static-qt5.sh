#!/bin/bash -e

print_usage() {
    printf "usage: %s [options] <version> (e.g. 5.12.2)\n\n" "$(basename $0)"
    printf "This tool downloads the appropriate Qt sources needed to build\n"
    printf "a static build of Qt\n\n"
    printf "Options:\n"
    printf "   -x [<directory>]         Extract Qt to <directory>\n"
    printf "   -f                       Overwrite destination directory if it exists\n"
}

while getopts ":x:f" opt; do
    case $opt in
        x) DESTDIR=$OPTARG ;;
        f) OVERWRITE=true ;;

        \?) echo "Invalid option -$OPTARG" >&2; print_usage >&2; exit 1 ;;
    esac
done
VERSION=${@:$OPTIND:1}

[ -z "$VERSION" ] && (echo "Missing Qt version parameter" >&2; print_usage >&2; exit 1)

# Delete any partial downloads using the checksum files, unfortunately I can't find
# any GPG signature for the source downloads to make this secure.
[ ! -f qtbase-md5sums-$VERSION.txt ] && wget -O qtbase-md5sums-$VERSION.txt http://download.qt.io/official_releases/qt/$(echo $VERSION | cut -d. -f1,2)/$VERSION/submodules/md5sums.txt
md5sum --quiet -c qtbase-md5sums-$VERSION.txt 2>/dev/null | cut -d: -f1 | xargs rm -f
[ ! -f qtbase-everywhere-src-$VERSION.tar.xz ] && wget http://download.qt.io/official_releases/qt/$(echo $VERSION | cut -d. -f1,2)/$VERSION/submodules/qtbase-everywhere-src-$VERSION.tar.xz
echo "All files downloaded"

if [ -n "$DESTDIR" ]; then
    echo Extracting qtbase-opensource-src-$VERSION.tar.xz...
    if [ "$OVERWRITE" = true ]; then
        rm -rf "$DESTDIR"
    fi
    mkdir "$DESTDIR"
    tar Jxf qtbase-opensource-src-$VERSION.tar.xz -C "$DESTDIR" --strip-components 1
    printf "Qt source extracted in '%s'\n" "$DESTDIR"
fi

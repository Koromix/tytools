#!/bin/sh -e

cd "$(dirname $0)/../.."

VERSION=$(git describe --tags | cut -c 2- | awk -F '-' '{print $1}')
DATE=$(git describe --tags | sed -e 's/.*-g//' | xargs -n1 git show -s --format=%ci | LANG=en_US xargs -0 -n1 date "+%a, %d %b %Y %H:%M:%S %z" -d)
PACKAGE_DIR=bin/Debian

rm -rf $PACKAGE_DIR/pkg
mkdir -p $PACKAGE_DIR $PACKAGE_DIR/pkg $PACKAGE_DIR/pkg/debian

docker build -t rygel/debian10 contrib/debian/docker
docker run -t -i --rm -v $(pwd):/io rygel/debian10 /io/contrib/debian/build.sh

install -D -m0755 bin/Debian/usr/bin/tycmd $PACKAGE_DIR/pkg/tycmd
install -D -m0755 bin/Debian/usr/bin/tycommander $PACKAGE_DIR/pkg/tycommander
install -D -m0755 bin/Debian/usr/bin/tyuploader $PACKAGE_DIR/pkg/tyuploader
install -D -m0644 bin/Debian/usr/share/applications/tycommander.desktop $PACKAGE_DIR/pkg/tycommander.desktop
install -D -m0644 bin/Debian/usr/share/applications/tyuploader.desktop $PACKAGE_DIR/pkg/tyuploader.desktop
install -D -m0644 resources/images/tycommander.png $PACKAGE_DIR/pkg/tycommander.png
install -D -m0644 resources/images/tyuploader.png $PACKAGE_DIR/pkg/tyuploader.png
install -D -m0644 contrib/debian/teensy.rules $PACKAGE_DIR/pkg/00-teensy.rules

install -D -m0755 contrib/debian/rules $PACKAGE_DIR/pkg/debian/rules
install -D -m0644 contrib/debian/compat $PACKAGE_DIR/pkg/debian/compat
install -D -m0644 contrib/debian/install $PACKAGE_DIR/pkg/debian/install
install -D -m0644 contrib/debian/format $PACKAGE_DIR/pkg/debian/source/format

echo "\
Source: tytools
Section: utils
Priority: optional
Maintainer: Niels Martignène <niels.martignene@protonmail.com>
Standards-Version: 4.5.1
Rules-Requires-Root: no

Package: tytools
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: GUI and command-line tools to manage Teensy devices
" > $PACKAGE_DIR/pkg/debian/control
echo "\
tytools ($VERSION) unstable; urgency=low

  * Current release.

 -- Niels Martignène <niels.martignene@protonmail.com>  $DATE
" > $PACKAGE_DIR/pkg/debian/changelog

(cd $PACKAGE_DIR/pkg && dpkg-buildpackage -uc -us)
cp $PACKAGE_DIR/*.deb $PACKAGE_DIR/../

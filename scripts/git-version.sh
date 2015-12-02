#!/bin/sh
# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

OUTFILE=$1
if [ -z "$OUTFILE" ]; then
    OUTFILE=/dev/stdout
fi

(

TY_VERSION=$(git describe --tags | cut -c2-)

if [ -n "$TY_VERSION" ]; then
    echo "/*"
    echo " * ty, a collection of GUI and command-line tools to manage Teensy devices"
    echo " *"
    echo " * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)"
    echo " * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>"
    echo " */"
    echo
    echo "#ifndef TY_VERSION_H"
    echo "#define TY_VERSION_H"
    echo
    echo "#define TY_VERSION \"$TY_VERSION\""
    echo
    echo "#endif"
else
    cat "$(dirname $0)/../libty/version.h"
fi

) >$OUTFILE 2>/dev/null

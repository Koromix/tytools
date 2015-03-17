#!/bin/sh

OUTFILE=$1
if [ -z "$OUTFILE" ]; then
    OUTFILE=/dev/stdout
fi

(

TY_VERSION=$(git describe --tags | cut -c2-)

if [ -n "$TY_VERSION" ]; then
    echo "/*"
    echo "* This Source Code Form is subject to the terms of the Mozilla Public"
    echo "* License, v. 2.0. If a copy of the MPL was not distributed with this"
    echo "* file, You can obtain one at http://mozilla.org/MPL/2.0/."
    echo "*/"
    echo
    echo "#ifndef TY_VERSION_H"
    echo "#define TY_VERSION_H"
    echo
    echo "#define TY_VERSION \"$TY_VERSION\""
    echo
    echo "#endif"
else
    cat "$(dirname $0)/../src/version.h"
fi

) >$OUTFILE 2>/dev/null

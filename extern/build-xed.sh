#!/bin/sh

if [ $# -ne 2 ]; then
    echo "usage: $0 <xed-src-path> <xed-build-path>"
    exit 1;
fi

SOURCE="$1"
BUILD="$2"

if ! [ -r "$BUILD/obj/libxed.a" -a -r "$BUILD/obj/wkit/include/xed/xed-interface.h"]; then
    cd "$BUILD"
    "$SOURCE/mfile.py" --jobs=8 -s
fi

#!/bin/sh

if [ $# -ne 2 ]; then
    echo "usage: $0 <xed-src-path> <xed-build-path>"
    exit 1;
fi

SOURCE="$1"
BUILD="$2"

if ! [ -r "$BUILD/lib/libxed.a" -a -r "$BUILD/include/xed/xed-interface.h" ]; then
    "$SOURCE/xed/src/mfile.py" --install-dir="$BUILD" --jobs=8 -s install > "$BUILD/xed/src/xed-stamp/build.log"
fi

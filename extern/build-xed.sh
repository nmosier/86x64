#!/bin/sh

if [ $# -ne 3 ]; then
    echo "usage: $0 <xed-src-path> <xed-build-path> <log-path>"
    exit 1;
fi

SOURCE="$1"
BUILD="$2"
LOG="$3"

if ! [ -r "$BUILD/lib/libxed.a" -a -r "$BUILD/include/xed/xed-interface.h" ]; then
    "$SOURCE/xed/src/mfile.py" --install-dir="$BUILD" --jobs=8 -s install > "$LOG"
fi

#!/bin/bash

usage() {
    cat<<EOF
usage: $0 [-hv] [-o archive64] [-l libabiconv] -s sympath [-w wrapper] [-i libinterpose] [-a archive64.dylib] archive32
EOF
}

error() {
    echo "$0: error encountered, exiting..." >&2
    exit 1
}

abort() {
    echo "$0: $1" >&2
    exit 1
}

ARCHIVE64="a.out"
SYMPATH=""
LIBABICONV="libabiconv.dylib"
WRAPPER_OBJ="wrapper.o"
LIBINTERPOSE="libinterpose.dylib"
DYLIB64=""
VERBOSE=""

while getopts "hvo:s:l:w:i:a:" OPTION; do
    case $OPTION in
        h)
            usage
            exit 0
            ;;
        o)
            ARCHIVE64="$OPTARG"
            ;;
        s)
            SYMPATH="$OPTARG"
            ;;
        l)
            LIBABICONV="$OPTARG"
            ;;
        w)
            WRAPPER_OBJ="$OPTARG"
            ;;
        i)
            LIBINTERPOSE="$OPTARG"
            ;;
        a)
            DYLIB64="$OPTARG"
            ;;
        v)
            VERBOSE="1"
            ;;
        "?")
            usage >&2
            exit 1
    esac
done

shift $((OPTIND-1))

[ "$SYMPATH" ] || abort "specify path to symbol info with '-s'"
[ "$DYLIB64" ] || DYLIB64="$ARCHIVE64.dylib"

ARCHIVE32="$1"
shift 1
[ "$ARCHIVE32" ] || abort "missing positional parameter 'archive32'"

v() {
    [ "$VERBOSE" ] && echo "$@"
    "$@"
}

# rebasify 32-bit archive
REBASE32=$(mktemp)
trap "rm $REBASE32" EXIT
v macho-tool rebasify "$ARCHIVE32" "$REBASE32" || error

# transform 32-bit rebased archive to 64-bit archive
TRANSFORM64=$(mktemp)
trap "rm $TRANSFORM64" EXIT
v macho-tool transform "$REBASE32" "$TRANSFORM64" || error

# generate abi conversion dylib
./abiconv.sh -o "$LIBABICONV" "$SYMPATH" || error

# link 64-bit archive with libabiconv.dylib
ABI64=$(mktemp)
trap "rm $ABI64" EXIT
v macho-tool modify --insert load-dylib,name="$LIBABICONV" "$TRANSFORM64" "$ABI64" || error

# gather lazily bound symbols
# SYMS=$(grep -Eo '^[[:alnum:]_]+' "$SYMPATH")
SYMS=$(macho-tool print --lazy-bind "$ABI64" | tail +2 | cut -d" " -f5)

# statically interpose lazily bound symbols to libabiconv
INTERPOSE64=$(mktemp)
trap "rm $INTERPOSE64" EXIT
v ./static-interpose.sh -l "$LIBABICONV" -p "__" -o "$INTERPOSE64" "$ABI64" $SYMS || error

# convert result to dylib
v macho-tool convert --archive DYLIB "$INTERPOSE64" "$DYLIB64" || error

# link wrapper
v ld -pagezero_size 0x1000 -lsystem -e _main_wrapper -o "$ARCHIVE64" "$DYLIB64" "$WRAPPER_OBJ" "$LIBINTERPOSE"

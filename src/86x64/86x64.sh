#!/bin/bash

export ROOTDIR="$(dirname "$(readlink "$0")")"

usage() {
    cat<<EOF
usage: 86x64 [-hv] [-o archive64] [-l libabiconv] -s sympath [-w wrapper] [-i libinterpose] [-a archive64.dylib] archive32
EOF
}

error() {
    echo "86x64: error encountered, exiting..." >&2
    exit 1
}

abort() {
    echo "86x64: $1" >&2
    usage >&2
    exit 1
}

ARCHIVE64="a.out"
SYMPATH="$ROOTDIR/symdefs.txt"
LIBABICONV="$ROOTDIR/libabiconv.dylib"
WRAPPER_OBJ="$ROOTDIR/libwrapper.a"
LIBINTERPOSE="$ROOTDIR/libinterpose.dylib"
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

# link 64-bit archive with libabiconv.dylib
ABI64=$(mktemp)
trap "rm $ABI64" EXIT
v macho-tool modify --insert load-dylib,name="$LIBABICONV" "$TRANSFORM64" "$ABI64" || error

# strip dollar signs (???)
DOLLAR64=$(mktemp)
v macho-tool modify --update strip-dollar-suffixes "$ABI64" "$DOLLAR64"

# gather lazily bound symbols
SYMS=$(macho-tool print --lazy-bind "$DOLLAR64" | tail +2 | cut -d" " -f5)

# statically interpose lazily bound symbols to libabiconv
INTERPOSE64=$(mktemp)
trap "rm -f $INTERPOSE64" EXIT
v "$ROOTDIR"/static-interpose.sh -l "$LIBABICONV" -p "__" -o "$INTERPOSE64" "$DOLLAR64" $SYMS || error

# interpose dyld_stub_binder
DYLD64=$(mktemp)
trap "rm -f $DYLD64" EXIT
DYLD_ORD=$(macho-tool translate --load-dylib "$LIBABICONV" "$INTERPOSE64")
v macho-tool modify --update bind,old_sym="dyld_stub_binder",new_sym="__dyld_stub_binder",new_dylib="$DYLD_ORD" "$INTERPOSE64" "$DYLD64"

# convert result to dylib
v macho-tool convert --archive DYLIB "$DYLD64" "$DYLIB64" || error

# link wrapper
v ld -arch x86_64 -rpath "$ROOTDIR" -pagezero_size 0x1000 -lsystem -e _main_wrapper -o "$ARCHIVE64" "$DYLIB64" "$WRAPPER_OBJ" "$LIBINTERPOSE" 2>&1

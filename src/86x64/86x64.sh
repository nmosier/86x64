#!/bin/bash

LINKDST="$(readlink "$0")"
export ROOTDIR="$(dirname "${LINKDST:-"$0"}")"

usage() {
    cat<<EOF
usage: 86x64 [-hv] [-o archive64] [-r root] [-l libabiconv] [-w wrapper] [-i libinterpose] [-a archive64.dylib] [-m macho-tool] archive32
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
LIBABICONV="$ROOTDIR/libabiconv.dylib"
WRAPPER_OBJ="$ROOTDIR/libwrapper.a"
LIBINTERPOSE="$ROOTDIR/libinterpose.dylib"
DYLIB64=""
MACHO_TOOL="macho-tool"

while getopts "hvo:l:w:i:a:m:" OPTION; do
    case $OPTION in
        h)
            usage
            exit 0
            ;;
        o)
            ARCHIVE64="$OPTARG"
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
        m)
            MACHO_TOOL="$OPTARG"
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
# REBASE32=$(mktemp)
# trap "rm $REBASE32" EXIT
REBASE32="${ARCHIVE32}_rebase"
v "$MACHO_TOOL" rebasify "$ARCHIVE32" "$REBASE32" || error

# transform 32-bit rebased archive to 64-bit archive
# TRANSFORM64=$(mktemp)
# trap "rm $TRANSFORM64" EXIT
TRANSFORM64="${ARCHIVE64}_transform"
v "$MACHO_TOOL" -- transform "$REBASE32" "$TRANSFORM64" || error

# link 64-bit archive with libabiconv.dylib
# ABI64=$(mktemp)
# trap "rm $ABI64" EXIT
ABI64="${ARCHIVE64}_abi"
v "$MACHO_TOOL" -- modify --insert load-dylib,name="$LIBABICONV" "$TRANSFORM64" "$ABI64" || error
  
# strip dollar signs (???)
# DOLLAR64=$(mktemp)
# trap "rm $DOLLAR64" EXIT
DOLLAR64="${ARCHIVE64}_dollar"
# v "$MACHO_TOOL" modify --update strip-dollar-suffixes "$ABI64" "$DOLLAR64"
v "$MACHO_TOOL" modify --update strip-bind,suffix='$UNIX2003' "$ABI64" "$DOLLAR64"
# DOLLAR64="$ABI64"

# gather lazily bound symbols
SYMS=$("$MACHO_TOOL" print --lazy-bind "$DOLLAR64" | tail +2 | cut -d" " -f5)

# statically interpose lazily bound symbols to libabiconv
INTERPOSE64="${ARCHIVE64}_interpose"
# INTERPOSE64=$(mktemp)
# trap "rm -f $INTERPOSE64" EXIT
v "$ROOTDIR"/static-interpose.sh -l "$LIBABICONV" -p "__" -o "$INTERPOSE64" "$DOLLAR64" $SYMS || error

# interpose dyld_stub_binder
DYLD64="${ARCHIVE64}_dyld"
# DYLD64=$(mktemp)
trap "rm -f $DYLD64" EXIT
DYLD_ORD=$("$MACHO_TOOL" translate --load-dylib "$LIBABICONV" "$INTERPOSE64")
v "$MACHO_TOOL" modify --update bind,old_sym="dyld_stub_binder",new_sym="__dyld_stub_binder",new_dylib="$DYLD_ORD" "$INTERPOSE64" "$DYLD64"
  
# convert result to dylib
v "$MACHO_TOOL" convert --archive DYLIB "$DYLD64" "$DYLIB64" || error

# link wrapper
# v ld -arch x86_64 -rpath "$ROOTDIR" -rpath $(dirname "$ARCHIVE32") -pagezero_size 0x1000 -lsystem -e _main_wrapper -o "$ARCHIVE64" "$DYLIB64" "$WRAPPER_OBJ" "$LIBINTERPOSE" 2>&1
v ld -arch x86_64 -rpath "$(dirname $LIBINTERPOSE)" -rpath "$(dirname $ARCHIVE64)" -pagezero_size 0x1000 -lsystem -e _main_wrapper -o "$ARCHIVE64" "$WRAPPER_OBJ" "$LIBINTERPOSE" "$DYLIB64" 2>&1

#!/bin/bash

usage() {
    cat <<EOF
usage: $0 [-h] [-o libabiconv] [-l ldlib] [-L libpath] [-d dyld_stub_binder.asm] [sympath=<stdin>]
EOF
}

error() {
    echo "$0: encountered error, exiting..."
    exit 1
}

OUTPATH="a.out"
SYMBOLS=""
LDFLAGS=()
LDLIBS=("-lsystem")
DYLD_STUB_BINDER_LIB="$ROOTDIR/libdyld_stub_binder.a"

while getopts "ho:l:L:d:" OPTION; do
    case $OPTION in
        h)
            usage
            exit 0
            ;;
        o)
            OUTPATH="$OPTARG"
            ;;
        l)
            LDLIBS=("${LDLIBS[@]}" "-l$OPTARG")
            ;;
        L)
            LDFLAGS=("${LDFLAGS[@]}" "-L$OPTARG")
            ;;
        d)
            DYLD_STUB_BINDER_LIB="$OPTARG"
            ;;
        ?)
            usage >&2
            exit 1
            ;;
    esac
done

shift $((OPTIND-1))

if [[ $# -eq 1 ]]; then
    SYMBOLS="$1"
    shift $((OPTIND-1))
elif [[ $# -gt 1 ]]; then
    echo "$0: excess positional arguments" >&2
    exit 1
fi



# generate 32-to-64 ABI converter
ABICONV_ASM=$(mktemp)
ABICONV_O=$(mktemp)
trap "rm -f $ABICONV_ASM $ABICONV_O $DYLD_STUB_BINDER_O" EXIT

if [ "$SYMBOLS" ]; then
    "$ROOTDIR/abigen" "$SYMBOLS"
else
    "$ROOTDIR/abigen"
fi > "$ABICONV_ASM" || error

cat "$ABICONV_ASM"

nasm -f macho64 -o "$ABICONV_O" "$ABICONV_ASM" || error
cc "${LDFLAGS[@]}" "${LDLIBS[@]}" -dynamiclib -o "$OUTPATH" "$ABICONV_O" "$DYLD_STUB_BINDER_LIB" || error

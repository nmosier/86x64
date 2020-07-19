#!/bin/bash

usage() {
    cat <<EOF
usage: $0 [-h] [-o libabiconv] [-l ldlib] [-L libpath] [sympath=<stdin>]
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

while getopts "ho:l:L:" OPTION; do
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
trap "rm $ABICONV_ASM $ABICONV_O" EXIT

if [ "$SYMBOLS" ]; then
    ./abigen < "$SYMBOLS"
else
    ./abigen
fi > "$ABICONV_ASM" || error

nasm -f macho64 -o "$ABICONV_O" "$ABICONV_ASM"
cc "${LDFLAGS[@]}" "${LDLIBS[@]}" -dynamiclib -o "$OUTPATH" "$ABICONV_O"

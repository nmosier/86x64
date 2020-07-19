#!/bin/bash

usage() {
    cat <<EOF
usage: $0 [-h] [-o libabiconv] [-l ldlib] [-L libpath] -s symbols 
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

while getopts "ho:s:l:L:" OPTION; do
    case $OPTION in
        h)
            usage
            exit 0
            ;;
        o)
            OUTPATH="$OPTARG"
            ;;
        s)
            SYMBOLS="$OPTARG"
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

if [[ $# -lt 1 ]]; then
    echo "$0: missing positional argument" >&2
    exit 1
elif [[ $# -gt 1 ]]; then
    echo "$0: excess positional arguments" >&2
    exit 1
fi

ARCHIVE="$1"

if ! [ "$SYMBOLS" ]; then
    echo "$0: specify symbols with '-i'" >&2
fi

# generate 32-to-64 ABI converter
ABICONV_ASM=$(mktemp)
ABICONV_O=$(mktemp)
trap "rm $ABICONV_ASM $ABICONV_O"
./abigen < "$SYMBOLS" > "$ABICONV_ASM" || error
nasm -f macho64 -o "$ABICONV_O" "$ABICONV_ASM"
ld "${LDFLAGS[@]}" "${LDLIBS[@]}" -o "$OUTPATH" "$ABICONV_O"

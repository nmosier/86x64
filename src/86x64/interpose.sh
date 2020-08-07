#!/bin/bash

usage() {
    cat <<EOF
usage: $0 -l libabiconv -o dyld64 [-m macho-tool] interpose64
EOF
}

LIBABICONV=""
OUT=""
MACHO_TOOL="macho-tool"

while getopts "hl:o:m:" OPTCHAR; do
    case $OPTCHAR in
        h)
            usage
            exit 0
            ;;
        l)
            LIBABICONV="$OPTARG"
            ;;
        o)
            OUT="$OPTARG"
            ;;
        m)
            MACHO_TOOL="$OPTARG"
            ;;
        "?")
            usage >&2
            exit 1
            ;;
    esac
done

if [[ -z "$LIBABICONV" ]]; then
    echo "$0: missing option '-l'"
    exit 1
fi

shift $((OPTIND-1))

if [[ $# -ne 1 ]]; then
    echo "$0: invalid number of positional arguments"
    exit 1
fi

IN="$1"

DYLD_ORD=$("$MACHO_TOOL" translate --load-dylib "$LIBABICONV" "$IN")
"$MACHO_TOOL" modify --update bind,old_sym="dyld_stub_binder",new_sym="__dyld_stub_binder",new_dylib="$DYLD_ORD" "$IN" "$OUT"

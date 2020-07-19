#!/bin/bash

# usage: $0 inpath outpath dylib prefix sym...
usage() {
    cat <<EOF
usage: $0 [-h] -l interpose_dylib [-p prefix] [-o outpath] inpath [sym...]
EOF
}

OUTPATH="a.out"
DYLIB=""
PREFIX="__"

while getopts "ho:l:p:" OPTION; do
    case $OPTION in
        h)
            usage
            exit 0
            ;;
        o)
            OUTPATH="$OPTARG"
            ;;
        l)
            DYLIB="$OPTARG"
            ;;
        p)
            PREFIX="$OPTARG"
            ;;
        "?")
            usage >&2
            exit 1
            ;;
    esac
done

shift $((OPTIND-1))

# check required options
if ! [ "$DYLIB" ]; then
    echo "$0: specify static interpose target dylib with '-l <dylib>'"; exit 1
fi

# parse required positional args
INPATH="$1"
shift 1

if ! [ "$INPATH" ]; then
    echo "$0: missing positional argument input archive"; exit 1
fi

ARGS=""

while [[ $# > 0 ]]; do
    SYM="$1"
    ORD=$(macho-tool translate --load-dylib "$DYLIB" "$INPATH")
    if [[ $? != 0 ]]; then
        exit 2
    fi
    ARGS=$(echo "$ARGS" --update bind,lazy,old_sym="$SYM",new_sym="$PREFIX$SYM",new_dylib="$ORD")
    shift 1
done

CMD=$(echo macho-tool modify "$ARGS" "$INPATH" "$OUTPATH")
echo "$CMD"
$CMD

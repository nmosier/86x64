#!/bin/bash

# usage: $0 inpath outpath dylib prefix sym...
usage() {
    cat <<EOF
usage: $0 inpath outpath dylib prefix sym...
EOF
}

if [[ $# -lt 4 ]]; then
    usage
    exit 1
fi

INPATH="$1"
shift 1
OUTPATH="$1"
shift 1
DYLIB="$1"
shift 1
PREFIX="$1"
shift 1

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




#!/bin/bash

usage() {
    cat<<EOF
usage: $0 path...
EOF
}

while getopts "h" OPTCHAR; do
    case $OPTCHAR in
        h)
            usage
            exit 0
            ;;
        "?")
            usage 2>&1
            exit 1
            ;;
    esac
done

shift $((OPTIND-1))

for LIBPATH in "$@"; do
    objdump -exports-trie "$LIBPATH" | tr -s " " | cut -f 2 -d " " | grep "^_"
done

#!/bin/bash

usage() {
    cat <<EOF
usage: $0 [-h] sym...
EOF
}

# INCLUDE="/usr/local/include"
INCLUDE=()

while getopts "hI:" OPTION
do
    case $OPTION in
        h)
            usage
            exit 0
            ;;
        I)
            INCLUDE=("${INCLUDE[@]}" "$OPTARG")
            ;;
        ?)
            usage >&2
            exit 1
            ;;
    esac
done

shift $((OPTIND-1))

SYMS=("$@")

if [[ "${#INCLUDE[@]}" == 0 ]]; then
    echo "$0: requires at least one include directory '-I'" >&2
    exit 1
fi

for SYM in "${SYMS[@]}"; do
    grep -orE "$SYM\(.*\);" "${INCLUDE[@]}"
done

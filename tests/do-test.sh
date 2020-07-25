#!/bin/bash

usage() {
    echo <<EOF
usage: $0 exec32 exec64
EOF
}

if [[ $# -ne 2 ]]; then
    usage >&2
    exit 1
fi

if ! [ -x "$1" ] || ! [ -x "$2" ]; then
    echo "file not found" >&2
    exit 1
fi

tmp1=$(mktemp)
tmp2=$(mktemp)
trap "rm -rf $tmp1 $tmp2" EXIT

if ! "$1" > "$tmp1" || ! "$2" > "$tmp2"; then
    exit 1
fi

diff "$tmp1" "$tmp2"

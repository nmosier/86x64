#!/bin/bash

usage() {
    echo <<EOF
usage: $0 exec
EOF
}

if [[ $# -ne 1 ]]; then
    usage >&2
    exit 1
fi

if ! [ -x "${1}32" ] || ! [ -x "${1}64" ]; then
    echo "file not found" >&2
    exit 1
fi

tmp1=$(mktemp)
tmp2=$(mktemp)
trap "rm -rf $tmp1 $tmp2" EXIT

if ! "${1}32" > "$tmp1" || ! sudo chroot / sh -c "cd \"$(dirname "${1}64")\"; \"${1}64\"" > "$tmp2"; then
    exit 1
fi

if [ -e "$1.sh" ]; then
    diff <("$1.sh" "$1") "$tmp2"
else
    diff "$tmp1" "$tmp2"
fi

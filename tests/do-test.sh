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

[ -f "$1" ] || (echo "file not found" >&2; exit 1)
[ -f "$2" ] || (echo "file not found" >&2; exit 1)

diff <("$1") <(sudo chroot / "$2")

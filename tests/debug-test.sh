#!/bin/bash

usage() {
    cat <<EOF
usage: $0 <testname>
EOF
}

if [[ $# -ne 1 ]]; then
    usage >&2
    exit 1
fi

sudo chroot / sh -c "cd \"$(dirname "${1}64")\" && lldb \"${1}64\""


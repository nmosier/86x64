#!/bin/bash

usage() {
    cat <<EOF
usage: $0 <testname> [args...] 
EOF
}

if [[ $# -lt 1 ]]; then
    usage >&2
    exit 1
fi

sudo chroot / sh -c "cd \"$PWD\" && lldb \"${1}64\""
# sudo chroot / sh -x -c "cd \"$PWD\"; lldb \"${TESTNAME}64\" -- ${@[@]}"

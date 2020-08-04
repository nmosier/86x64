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

CMD=$(echo cd "$PWD" '&&' "./${1}64")

sudo chroot / sh -c "$CMD"

# sudo chroot / sh -c "cd \"$PWD\" && \"${1}64\""
# sudo chroot / sh -x -c "cd \"$PWD\"; lldb \"${TESTNAME}64\" -- ${@[@]}"

# sudo chroot / sh --init-file <(echo cd "$DIR" ';' lldb "${TESTNAME}64" -- "${ARGS[@]}")

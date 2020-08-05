#!/bin/bash

usage() {
    cat <<EOF
usage: $0 [-hd] exec
EOF
}

DEBUG_CMD=
ROOT_DIR="$(dirname "$0")"

while getopts "hd" OPTCHAR; do
    case $OPTCHAR in
        h)
            usage
            exit 0
            ;;
        d)
            DEBUG_CMD=lldb
            ;;
        "?")
            usage >&2
            exit 1
            ;;
    esac
done

shift $((OPTIND-1))

if [[ $# -ne 1 ]]; then
    usage >&2
    exit 1
fi

TEST_NAME="$(basename "$1")"
SCRIPT_NAME="$ROOT_DIR/$TEST_NAME.sh"
ARGS_NAME="$ROOT_DIR/$TEST_NAME.args"

run_cmd() {
    DIR="$(dirname ${1})"
    TESTPATH="${1}"
    shift 1
    sudo chroot / sh <<EOF
cd "$DIR"
"$TESTPATH" $@ > "$tmp2"
EOF
}


for OPTIM in O0 O1 O2 O3; do
    if ! [ -x "${1}-${OPTIM}-32" ] || ! [ -x "${1}-${OPTIM}-64" ]; then
        echo "file not found for ${OPTIM}" >&2
        exit 1
    fi
    
    tmp1=$(mktemp)
    tmp2=$(mktemp)
    trap "rm -rf $tmp1 $tmp2" EXIT
    

    ([ -e "$ARGS_NAME" ] && cat "$ARGS_NAME" || echo) | while read ARGS; do
        if ! "${1}-${OPTIM}-32" $ARGS > "$tmp1" || ! run_cmd "${1}-${OPTIM}-64" $ARGS; then
            exit 1
        fi
        
        if [ -e "$SCRIPT_NAME" ]; then
            diff <("$SCRIPT_NAME" "${1}-${OPTIM}-64") "$tmp2"
        else
            diff "$tmp1" "$tmp2"
        fi || exit 1
    done

done

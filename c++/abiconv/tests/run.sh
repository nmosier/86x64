#!/bin/bash

usage() {
cat <<EOF
usage: $0 [-h]
EOF
}

while getopts "h" OPTION; do
    case $OPTION in
        "h")
            usage
            exit 0
            ;;
        "?")
            usage >&2
            exit 1
            ;;
    esac
done


run_test() {
    EXEC32="$1"
    shift 1
    EXEC="${EXEC32%32}"
    EXEC64="${EXEC}64"
    ../86x64.sh -o "$EXEC64" -s "${EXEC}.syms" "$EXEC32" || return 1
    diff "${EXEC64}.out" <("./${EXEC64}" "$@")
    return $?
}

EXEC32S="$(find . -name '*32')"
TESTS_FAILED=0
for EXEC32 in $(ls *32); do
    EXEC64="${EXEC32%32}64"
    echo "$EXEC64"
    printf "%s" "testing ${EXEC32}..."
    if run_test "$EXEC32"; then
        echo "success"
    else
        echo "failed"
        (( ++TESTS_FAILED ))
    fi
done

echo "--- Results ---"
echo "Tests failed: $TESTS_FAILED"


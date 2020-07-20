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
    BASE="$1"
    shift 1
    SYMS="${BASE}.syms"
    EXEC64="${BASE}64"
    make -s "$EXEC64" || return 1
    diff "${BASE}.out" <("./${EXEC64}" "$@")
    return $?
}

TESTS_FAILED=0
for SRC in $(ls *.c); do
    BASE="${SRC%.c}"
    printf "%s" "testing ${BASE}..."
    if run_test "$BASE"; then
        echo "success"
    else
        echo "failed"
        (( ++TESTS_FAILED ))
    fi
done

echo "--- Results ---"
echo "Tests failed: $TESTS_FAILED"


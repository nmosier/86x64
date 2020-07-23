#!/bin/bash

USAGE="usage: $0 [-l lab] [-d dump] [-n lines] symbol"

LAB=crt.inc
DUMP=ce.asm
LINES=30

while getopts "l:d:n:h" opt
do
    case $opt in
        l)
            LAB="$OPTARG"
            ;;
        d)
            DUMP="$OPTARG"
            ;;
        n)
            LINES=$(("$OPTARG"))
            ;;
        h)
            echo $USAGE
            exit 0
            ;;
        ?)
            echo $USAGE
            exit 1
            ;;
    esac
done

shift $((OPTIND-1))
if [[ $# -lt 1 ]]
then
    echo $USAGE
    exit 1
fi

SYM="$1"
shift 1

# find symbol in symbol table
ADDR=$((0x`grep -m1 "\<$SYM\>" "$LAB" | grep -o -E "[[:xdigit:]]{7}"`))

if [[ 0x$ADDR -eq 0 ]]
then
    echo "$0: \"$SYM\" not found"
    exit 1
fi

echo \"$SYM\" @ $(printf 0x%06x $ADDR)

# find address in dump
REAL_ADDR=$(grep -m1 "^"`printf %06x $ADDR`":" "$DUMP" | grep -o -E "[[:xdigit:]]+$")

if [[ 0x$REAL_ADDR -eq 0 ]]
then
    echo "$0: error in looking up \"$SYM\""
    exit 1
fi

echo \"_$SYM\" @ 0x"$REAL_ADDR"

# print out dump
REAL_ADDR=$(printf %06x $((0x$REAL_ADDR)))
L=$(awk '/^'$REAL_ADDR':/ { print NR; exit 0 }' "$DUMP")
# tail -n +"$L" "$DUMP" | head -n "$LINES"
tail -n +"$L" "$DUMP" | awk '{ print $0; } /\<ret\>/ { exit 0; }'

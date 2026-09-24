#!/bin/sh
# usage: tests/run_seqtest.sh SEQUENCE.DAT oracle-events.txt [level]
set -e
here=$(cd "$(dirname "$0")" && pwd); tmp=${TMPDIR:-/tmp}
python3 "$here/pairs_from_events.py" "$2" "$tmp/pairs.bin"
gcc -O0 -g -Wall -o "$tmp/seqtest" "$here/seqtest.c" "$here/../source/seq.c" "$here/../source/dat.c"
"$tmp/seqtest" "$1" "$tmp/pairs.bin" "${3:-1}"

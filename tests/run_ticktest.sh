#!/bin/sh
# usage: tests/run_ticktest.sh SEQUENCE.DAT ram-dump.bin PRINCE.EXE tick-events.txt level.bin   (needs KID_DAT and PRINCE_DAT in the environment)
set -e
here=$(cd "$(dirname "$0")" && pwd); tmp=${TMPDIR:-/tmp}
python3 "$here/ticks_from_events.py" "$4" "$tmp/ticks.bin"
gcc -O0 -g -Wall -o "$tmp/ticktest" "$here/ticktest.c" "$here/stubs.c" "$here/../src/seq.c" "$here/../src/dat.c" "$here/../src/char.c" "$here/../src/tiles.c" "$here/../src/control.c" "$here/../src/frame.c" "$here/../src/collision.c" "$here/../src/kid.c"
"$tmp/ticktest" "$1" "$2" "$3" "$tmp/ticks.bin" "$5"

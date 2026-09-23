#!/bin/sh
# Rebuild the snapshot test and run every capture (level 1: ram1436, level 3: ramL3). usage: tests/run_all.sh [mode]
set -e
here=$(cd "$(dirname "$0")" && pwd); W=${WORK:-$HOME/pop2dec/work}; O=$HOME/pop2dec/oracle; S=$HOME/pop2dec/sources/prince2
export KID_DAT=$S/KID.DAT PRINCE_DAT=$S/PRINCE.DAT PRINCE2_DIR=$S
cd "$here/.."
gcc -O0 -g -Wall -o $W/snaptest tests/snaptest.c tests/stubs.c tests/snap.c src/*.c
mode=${1:-tick}
for s in D E F G H1 H2 H3 H4; do echo "L1 $s $($W/snaptest $mode $S/SEQUENCE.DAT $O/w/ram1436.bin $S/PRINCE.EXE $W/$mode$s.bin 2>/dev/null | tail -1)"; done
for s in L3loose7 L3loose22 L3btn10 L3btn3 L3r1 L3r2; do [ -f $W/$mode$s.bin ] && echo "L3 $s $($W/snaptest $mode $S/SEQUENCE.DAT $O/w/ramL3.bin $S/PRINCE.EXE $W/$mode$s.bin 2>/dev/null | tail -1)"; done

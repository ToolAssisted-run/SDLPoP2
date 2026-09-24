#!/bin/sh
# tiletests.sh [LEVEL...]: build tests/tiletest and run it on every ~/pop2dec/oracle/tiles/T[CDE]<level>_<n>.case
# (tools/tilecap.py captures), one summary line per case
cd "$(dirname "$0")/.." || exit 2
B=${TMPDIR:-/tmp}/tiletest.$$
if [ -n "$BUILD" ]; then B=$BUILD/tiletest; else gcc -O2 -Wall -o $B tests/tiletest.c tests/snap.c tests/testutil.c src/*.c -lm || exit 2; fi   # (BUILD: the meson build directory)
T=$HOME/pop2dec/oracle/tiles
dat() { case $1 in 1) echo ROOFTOPS.DAT;; 2) echo DESERT.DAT;; 3|4|5) echo CAVERNS.DAT;; 6|7|8|9) echo RUINS.DAT;; 14) echo FINAL.DAT;; *) echo TEMPLE.DAT;; esac; }
for l in ${@:-1 2 3 4 5 6 7 8 9 10 11 12 13 14}; do
	for p in TC TD TE; do for c in $T/$p${l}_*.case; do
		[ -e "$c" ] || continue
		printf "%s: " "$(basename $c .case)"
		PRINCE2_DIR=${PRINCE2_DIR:-$HOME/pop2dec/sources/prince2} $B $c $T/$p${l}_ram.bin $(dat $l) 2>&1 | grep -E "^table|differ" | tr '\n' ' '; echo
	done; done
done
[ -n "$BUILD" ] || rm -f $B

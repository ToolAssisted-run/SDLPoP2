#!/bin/sh
# Rebuild the snapshot test and run every capture (level 1: ram1436, level 3: ramL3). usage: tests/run_all.sh [mode]
set -e
here=$(cd "$(dirname "$0")" && pwd); W=${WORK:-$HOME/pop2dec/work}; O=$HOME/pop2dec/oracle; S=$HOME/pop2dec/sources/prince2
export KID_DAT=$S/KID.DAT PRINCE_DAT=$S/PRINCE.DAT PRINCE2_DIR=$S
cd "$here/.."
# BUILD=dir (meson test): take the programs built there instead of compiling them here
build() { n=$1; shift; if [ -n "$BUILD" ]; then cp "$BUILD/$n" "$W/$n"; else "$@"; fi; }
build snaptest gcc -O0 -g -Wall -o $W/snaptest tests/snaptest.c tests/testutil.c tests/snap.c source/*.c
mode=${1:-tick}
for s in D E F G H1 H2 H3 H4; do echo "L1 $s $($W/snaptest $mode $S/SEQUENCE.DAT $O/w/ram1436.bin $S/PRINCE.EXE $W/$mode$s.bin 2>/dev/null | tail -1)"; done
for s in L3loose7 L3loose22 L3btn10 L3btn3 L3r1 L3r2 L3skel1 L3skel2 L3skel3 L3sk11_1 L3sk11_2 L3sk17_1 L3sk20_1 L3br1 L3br2 L3br3; do [ -f $W/$mode$s.bin ] || continue; h=; [ -f $W/heap$s.bin ] && h=$W/heap$s.bin; echo "L3 $s $($W/snaptest $mode $S/SEQUENCE.DAT $O/w/ramL3.bin $S/PRINCE.EXE $W/$mode$s.bin $h 2>/dev/null | tail -1)"; done
# keyboard -> controls (capture ~/pop2dec/oracle/keyprobe.script: every movement key, shifts, ctrl, alt)
if [ -f $W/input_keyprobe.bin ]; then build inputtest gcc -O0 -g -Wall -o $W/inputtest tests/inputtest.c tests/testutil.c tests/snap.c source/*.c && echo "input $($W/inputtest $W/input_keyprobe.bin | tail -1)"; fi
# between ticks (ds_postroom -> next ds_tick: 2D3E:0FB0, the clock, 169B:0A30, 0BA6): bins built on demand
if [ "$mode" = tick ]; then
for s in D E F G H1 H2 H3 H4 L3loose7 L3loose22 L3btn10 L3btn3 L3r1 L3r2 L3skel1 L3skel2 L3skel3 L3sk11_1 L3sk11_2 L3sk17_1 L3sk20_1 L3br1 L3br2 L3br3; do
	case $s in L3*) ev=$O/$s-snap.txt; sc=$O/$s.script; ram=$O/w/ramL3.bin;; *) ev=$O/tick$s-snap.txt; sc=$O/tick$s.script; ram=$O/w/ram1436.bin;; esac
	[ -f $ev ] || continue
	[ -f $W/between$s.bin ] || python3 tests/snaps_from_events.py --skip-pokes $sc $ev ds_postroom ds_tick $W/between$s.bin >/dev/null
	echo "$s $($W/snaptest between $S/SEQUENCE.DAT $ram $S/PRINCE.EXE $W/between$s.bin 2>/dev/null | tail -1)"
done
fi
# end to end: from the oracle's snapshot after the level load, everything in C with the script's keys (LS* captures)
if [ "$mode" = tick ]; then
build e2e gcc -O2 -g -Wall -o $W/e2e tests/e2e.c tests/testutil.c tests/snap.c source/*.c
# every level-start capture in the oracle directory (LS*: level 3 from its load; E<level>_<seed>: gen_e2e.py random runs;
# T<level>_<gap>: gen_turns.py; X/F/R/G/P: planned and fleet runs), all fields compared (E2E_STRICT; a "strict:" list names any that differ),
# from the captured post-load state and from a cold start (the core's new game: zeroed memory + PRINCE.EXE)
ram_of() { case $1 in LSL3*|E4_1) echo ramL3;; E1_1) echo ram1436;; E6_1) echo ramL6;; E10_1) echo ramL10;; SQ2_raft) echo ramP2_raft;; *) echo ram$1;; esac; }
for f in $O/LSL3r1-snap.txt $O/LSL3skel2-snap.txt $(ls $O/E*_*-snap.txt $O/T*_*-snap.txt $O/X*_*-snap.txt $O/F*_*-snap.txt $O/R[0-9]*_*-snap.txt $O/G*_*-snap.txt $O/P*_*-snap.txt $O/SQ2_raft-snap.txt 2>/dev/null | sort -V); do s=$(basename $f -snap.txt); r=$(ram_of $s); [ -f $O/w/$r.bin ] || continue
  out=$(E2E_STRICT=1 $W/e2e $S/SEQUENCE.DAT $O/w/$r.bin $S/PRINCE.EXE $f $O/$s.script 2>/dev/null)
  echo "$s $(echo "$out" | tail -1 | cut -d';' -f1) $(echo "$out" | grep '^strict: .' | cut -c1-120)"
  echo "$s cold $(E2E_COLD=1 $W/e2e $S/SEQUENCE.DAT $O/w/$r.bin $S/PRINCE.EXE $f $O/$s.script 2>/dev/null | grep -E '^cold start|^e2e' | cut -d';' -f1 | tr '\n' ' ')"
done
fi
# the core API alone: random play on every level and savestate round trips
build coretest gcc -O2 -g -Wall -o $W/coretest tests/coretest.c source/*.c && $W/coretest $S 2000 | tail -1

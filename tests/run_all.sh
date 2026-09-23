#!/bin/sh
# Rebuild the snapshot test and run every capture (level 1: ram1436, level 3: ramL3). usage: tests/run_all.sh [mode]
set -e
here=$(cd "$(dirname "$0")" && pwd); W=${WORK:-$HOME/pop2dec/work}; O=$HOME/pop2dec/oracle; S=$HOME/pop2dec/sources/prince2
export KID_DAT=$S/KID.DAT PRINCE_DAT=$S/PRINCE.DAT PRINCE2_DIR=$S
cd "$here/.."
gcc -O0 -g -Wall -o $W/snaptest tests/snaptest.c tests/testutil.c tests/snap.c src/*.c
mode=${1:-tick}
for s in D E F G H1 H2 H3 H4; do echo "L1 $s $($W/snaptest $mode $S/SEQUENCE.DAT $O/w/ram1436.bin $S/PRINCE.EXE $W/$mode$s.bin 2>/dev/null | tail -1)"; done
for s in L3loose7 L3loose22 L3btn10 L3btn3 L3r1 L3r2 L3skel1 L3skel2 L3skel3 L3sk11_1 L3sk11_2 L3sk17_1 L3sk20_1 L3br1 L3br2 L3br3; do [ -f $W/$mode$s.bin ] || continue; h=; [ -f $W/heap$s.bin ] && h=$W/heap$s.bin; echo "L3 $s $($W/snaptest $mode $S/SEQUENCE.DAT $O/w/ramL3.bin $S/PRINCE.EXE $W/$mode$s.bin $h 2>/dev/null | tail -1)"; done
# keyboard -> controls (capture ~/pop2dec/oracle/keyprobe.script: every movement key, shifts, ctrl, alt)
if [ -f $W/input_keyprobe.bin ]; then gcc -O0 -g -Wall -o $W/inputtest tests/inputtest.c tests/testutil.c tests/snap.c src/*.c && echo "input $($W/inputtest $W/input_keyprobe.bin | tail -1)"; fi
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
gcc -O0 -g -Wall -o $W/e2e tests/e2e.c tests/testutil.c tests/snap.c src/*.c
for x in LSL3r1:ramL3 LSL3skel2:ramL3 E1_1:ram1436 E4_1:ramL3 E6_1:ramL6 E10_1:ramL10 E5_1:ramE5_1 E7_1:ramE7_1 E8_1:ramE8_1 E9_1:ramE9_1 E11_1:ramE11_1 E12_1:ramE12_1 E13_1:ramE13_1 E14_1:ramE14_1 E2_1:ramE2_1; do s=${x%:*}; [ -f $O/$s-snap.txt ] && echo "$s $($W/e2e $S/SEQUENCE.DAT $O/w/${x#*:}.bin $S/PRINCE.EXE $O/$s-snap.txt $O/$s.script 2>/dev/null | tail -1)"; done
# the same from a cold start: zeroed memory, the static tables from PRINCE.EXE, 169B:0006 and the level load
# (only the three runtime words and the seed come from the capture; scene leftovers show as palette-slot bytes)
for x in LSL3r1:ramL3 E1_1:ram1436 E2_1:ramE2_1 E4_1:ramL3 E5_1:ramE5_1 E6_1:ramL6 E7_1:ramE7_1 E8_1:ramE8_1 E9_1:ramE9_1 E10_1:ramL10 E11_1:ramE11_1 E12_1:ramE12_1 E13_1:ramE13_1 E14_1:ramE14_1; do s=${x%:*}; [ -f $O/$s-snap.txt ] && echo "$s cold $(E2E_COLD=1 E2E_EXE_DS=16A:1,10C2:2,366:2 $W/e2e $S/SEQUENCE.DAT $O/w/${x#*:}.bin $S/PRINCE.EXE $O/$s-snap.txt $O/$s.script 2>/dev/null | grep -E '^cold start|^e2e' | cut -d';' -f1 | tr '\n' ' ')"; done
fi

#!/bin/sh
# The other DOS releases (docs/VERSIONS.md): end-to-end runs of the initial release (IR*) and 1.0 (V10*) captured in the
# oracle with their own game files ($POP2_WORKSPACE/sources/ir, sources/v10) and brought into 1.1's data layout (the
# workspace's oracle/convcap.py: NAME-11-snap.txt, w/ramNAME-11.bin), every field compared (E2E_STRICT). The release
# follows PRINCE.EXE. usage: tests/run_versions.sh
set -e
WS=${POP2_WORKSPACE:-$HOME/pop2dec}   # the analysis workspace (README)
here=$(cd "$(dirname "$0")" && pwd); W=${WORK:-$WS/work}; O=$WS/oracle
cd "$here/.."
# BUILD=dir (meson test): take the program built there instead of compiling it here
if [ -n "$BUILD" ]; then cp "$BUILD/e2e" "$W/e2e"; else gcc -O2 -g -Wall -o $W/e2e tests/e2e.c tests/testutil.c tests/snap.c source/*.c; fi
# the static data the runs take from the capture (the cheat and copy-protection flags, the sound device, CONFIG.DAT)
export E2E_EXE_DS="10C2:2,366:2,14A0:1,16A:1,670:1,2085:1,1FB8:2,80FC:20"
for f in $(ls $O/IRE*_*-11-snap.txt $O/V10E*_*-11-snap.txt 2>/dev/null | sort -V); do
	s=$(basename $f -11-snap.txt)
	case $s in *_S) continue;; esac   # (the screenshot runs: the drawing's, not the logic's)
	case $s in IR*) S=$WS/sources/ir;; *) S=$WS/sources/v10;; esac
	[ -f $O/w/ram$s-11.bin ] && [ -f $O/$s.script ] && [ -d $S ] || continue
	out=$(KID_DAT=$S/KID.DAT PRINCE_DAT=$S/PRINCE.DAT PRINCE2_DIR=$S E2E_STRICT=1 $W/e2e $S/SEQUENCE.DAT $O/w/ram$s-11.bin $S/PRINCE.EXE $f $O/$s.script 2>/dev/null || true)
	echo "$s $(echo "$out" | tail -1 | cut -d';' -f1) $(echo "$out" | grep '^strict: .' | cut -c1-120)"
done

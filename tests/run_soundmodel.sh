#!/bin/bash
# Every e2e capture with the core's sound model answering (E2E_SOUNDMODEL: no answers from the capture, strict), one line
# each; extra environment passes through (E2E_SNDRESYNC=1 reports each random-draw disagreement and takes the capture's seed)
WS=${POP2_WORKSPACE:-$HOME/pop2dec}   # the analysis workspace (README)
W=$WS/work; O=$WS/oracle; S=$WS/sources/prince2
E2E=${BUILD:+$BUILD/e2e}; E2E=${E2E:-$W/e2e}   # (BUILD: the meson build directory)
export KID_DAT=$S/KID.DAT PRINCE_DAT=$S/PRINCE.DAT PRINCE2_DIR=$S
ram_of() { case $1 in LSL3*|E4_1) echo ramL3;; E1_1) echo ram1436;; E6_1) echo ramL6;; E10_1) echo ramL10;; *) echo ram$1;; esac; }
for f in $O/LSL3r1-snap.txt $O/LSL3skel2-snap.txt $(ls $O/E*_*-snap.txt $O/T*_*-snap.txt $O/X*_*-snap.txt $O/F*_*-snap.txt $O/R[0-9]*_*-snap.txt $O/G*_*-snap.txt 2>/dev/null | sort -V); do s=$(basename $f -snap.txt); r=$(ram_of $s); [ -f $O/w/$r.bin ] || continue
  echo "$s $(E2E_SOUNDMODEL=1 E2E_STRICT=1 $E2E $S/SEQUENCE.DAT $O/w/$r.bin $S/PRINCE.EXE $f $O/$s.script 2>/dev/null | grep -E '^e2e|^strict: .' | tr '\n' ' ' | cut -c1-400)"
done

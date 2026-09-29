#!/bin/bash
# The sound drivers against every audio capture (docs/AUDIO.md 7), one line each: AU* (SB Pro: FM + DSP), SPK8 (PC
# speaker), MT* (Roland MT-32 through MMPU401.DRV, MIDI type 0x28) and GM* (General MIDI, 0x29)
WS=${POP2_WORKSPACE:-$HOME/pop2dec}   # the analysis workspace (README)
O=$WS/oracle; S=$WS/sources/prince2
T=${BUILD:+$BUILD/audiotest}; T=${T:-build/tests/audiotest}   # (BUILD: the meson build directory)
for f in $(ls $O/AU*-snap.txt 2>/dev/null | sort -V); do [ "$(basename $f)" = AUK8-snap.txt ] && continue; $T $S $f 2>/dev/null | sed 's#.*/##'; done
[ -f $O/SPK8-snap.txt ] && AUDIO_CAPS=0 $T $S $O/SPK8-snap.txt 2>/dev/null | sed 's#.*/##'
for f in $(ls $O/MT*-snap.txt 2>/dev/null | sort -V); do AUDIO_MIDI=0x28 $T $S $f 2>/dev/null | sed 's#.*/##'; done
for f in $(ls $O/GM[0-9]*-snap.txt 2>/dev/null | sort -V); do AUDIO_MIDI=0x29 $T $S $f 2>/dev/null | sed 's#.*/##'; done

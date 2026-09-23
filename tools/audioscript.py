#!/usr/bin/env python3
"""Oracle capture scripts for the sound drivers (tests/audiotest.c): audioscript.py BASE.script OUT.script [--audio PATH] [--speaker]
Keeps BASE's key lines (e.g. an E<level>_<seed>.script random run) and end frame, adds the driver probes (and the mixer
output with --audio). --speaker: probes for the PC speaker player (run the game with CONFIG.DAT's digital and MIDI types
set to 0, bytes 6..9, on a copy of the disk image). Run: ~/pop2dec/oracle/cap.sh NAME "prince yippeeyahoo LEVELn"."""
import sys

COMMON = """probe 1611 053C a_req SS0000 10
probe 194C 840E a_reqid SS0000 10
probe 194C 83D2 a_stop SS0000 10
probe 194C 8396 a_rel SS0000 10
probe 194C 3380 a_vol SS0000 8
probe 194C 3668 a_midistart SS0000 20
probe 194C 35E5 a_digistart SS0000 20
probe 194C 370A a_spkstart SS0000 20
"""
CARD = """probe 194C 2F10 a_isr
probe 194C 3055 a_ev
probe 4BD9 08AF a_opl
probe 4B5C 02D2 a_dsp
probe 4B5C 0432 a_dsp
probe 4B5C 0526 a_irq
probe 194C 3422 a_digiend
probe 194C 34BA a_midiend
"""
SPEAKER = """probe 194C 377B a_spkisr
probe 194C 37DF a_gate
probe 194C 380A a_gate
probe 194C 35D8 a_gate
probe 194C 33C8 a_gate
probe 194C 3839 a_div
"""

def main():
    a = sys.argv[1:]
    if len(a) < 2: sys.exit(__doc__)
    base, out = a[0], a[1]
    audio = a[a.index('--audio') + 1] if '--audio' in a else None
    keys, end = [], None
    for l in open(base):
        w = l.split()
        if w and w[0] == 'key': keys.append(l.rstrip('\n'))
        if w and w[0] == 'end': end = int(w[1])
    if end is None: sys.exit('no end line in ' + base)
    with open(out, 'w') as f:
        f.write('\n'.join(keys) + '\n' + COMMON + (SPEAKER if '--speaker' in a else CARD))
        if audio: f.write('audio 0 %d %s\n' % (end, audio))
        f.write('end %d\n' % end)

main()

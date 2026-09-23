#!/usr/bin/env python3
"""Write an oracle script capturing a scene: nisoracle.py NAME FIRST LAST STEP [extra lines...]
Shots go to ~/pop2dec/oracle/w_nis/NAME/sFRAME.tga; probes log the anim frames, fades, music/sound starts and MIDI cues
with the game's 60 Hz tick (DS:24E4).  Run: ~/pop2dec/oracle/cap.sh NAME "prince yippeeyahoo NIS21" """
import sys, os
name, first, last, step = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
O = os.path.expanduser('~/pop2dec/oracle'); W = '%s/w_nis/%s' % (O, name); os.makedirs(W, exist_ok=True)
L = ['probe 0AAC 0274 play_scene DS24E4 4', 'probe 32D4 0BE4 play_anim DS24E4 4', 'probe 32D4 0852 anim_frame DS24E4 4',
     'probe 2631 037E fade DS24E4 4', 'probe 194C 840E music DS24E4 4', 'probe 194C 314B cue DS24E4 4',
     'probe 194C 83D2 stopmusic DS24E4 4', 'probe 33B9 0000 dissolve DS24E4 4', 'probe 2D7D 0472 nis_cb DS24E4 4',
     'probe 2D7D 536E title_cb DS24E4 4', 'probe 2D7D 01AF text DS24E4 4', 'probe 194C 79A3 setpal DS24E4 4',
     'probe 33B9 0300 dis_step DS24E4 4']
L += sys.argv[5:]
for f in range(first, last + 1, step): L.append('shot %d %s/s%d.tga' % (f, W, f))
L.append('end %d' % (last + 1))
open('%s/%s.script' % (O, name), 'w').write('\n'.join(L) + '\n')

#!/usr/bin/env python3
"""Build tests/nistest and run it on every scene capture (see docs/NIS.md, "Verification").  nisrun.py [window]"""
import os, subprocess, sys
WS = os.environ.get('POP2_WORKSPACE', os.path.expanduser('~/pop2dec'))   # the analysis workspace (README)
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = WS + '/sources/prince2'; O = WS + '/oracle'; win = sys.argv[1] if len(sys.argv) > 1 else '10'
exe = os.environ.get('TMPDIR', '/tmp') + '/nistest'
import glob   # (with the whole program: transitions 2 and 3 draw their game room through the shell's 0AAC:0376)
subprocess.check_call(['gcc', '-O2', '-w', '-DNIS_ENGINE', '-o', exe, root + '/tests/nistest.c'] + sorted(glob.glob(root + '/source/*.c')) + ['-lm'])
os.environ.setdefault('PRINCE2_DIR', D)
runs = [(str(n), 'N%d' % n, 'N%d' % n) for n in range(20, 29)] + [('9', 'A9', 'A9'), ('10', 'A10', 'A10'), ('11', 'B11', 'B11'),
        ('intro', 'INTRO', 'INTRO')] + [(str(n), 'C%d' % n, 'C%d' % n) for n in (1, 2, 3, 5)] + [('6', 'C6b', 'C6b')]
tot = [0, 0, 0]
for scene, ev, shots in runs:
    if not os.path.exists('%s/%s-snap.txt' % (O, ev)): continue
    out = subprocess.run([exe, D, scene, '%s/%s-snap.txt' % (O, ev), '%s/w_nis/%s' % (O, shots), win], capture_output=True, text=True).stdout
    line = [l for l in out.splitlines() if l.startswith('scene')][-1]; print(line)
    w = line.split(); tot[0] += int(w[2]); tot[1] += int(w[4]); tot[2] += int(w[7])
print('all: %d shots, %d exact synced, %d exact within +-%s' % (tot[0], tot[1], tot[2], win))

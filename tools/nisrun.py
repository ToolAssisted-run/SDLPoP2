#!/usr/bin/env python3
"""Build tests/nistest and run it on every scene capture (see docs/NIS.md, "Verification").  nisrun.py [window]"""
import os, subprocess, sys
home = os.path.expanduser('~'); root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = home + '/pop2dec/sources/prince2'; O = home + '/pop2dec/oracle'; win = sys.argv[1] if len(sys.argv) > 1 else '10'
exe = os.environ.get('TMPDIR', '/tmp') + '/nistest'
subprocess.check_call(['gcc', '-O2', '-o', exe, root + '/tests/nistest.c', root + '/src/nis.c', root + '/src/dat.c'])
runs = [(str(n), 'N%d' % n, 'N%d' % n) for n in range(20, 29)] + [('9', 'A9', 'A9'), ('10', 'A10', 'A10'), ('11', 'B11', 'B11'),
        ('intro', 'INTRO', 'INTRO')] + [(str(n), 'C%d' % n, 'C%d' % n) for n in (1, 2, 3, 5)] + [('6', 'C6b', 'C6b')]
tot = [0, 0, 0]
for scene, ev, shots in runs:
    if not os.path.exists('%s/%s-snap.txt' % (O, ev)): continue
    out = subprocess.run([exe, D, scene, '%s/%s-snap.txt' % (O, ev), '%s/w_nis/%s' % (O, shots), win], capture_output=True, text=True).stdout
    line = [l for l in out.splitlines() if l.startswith('scene')][-1]; print(line)
    w = line.split(); tot[0] += int(w[2]); tot[1] += int(w[4]); tot[2] += int(w[7])
print('all: %d shots, %d exact synced, %d exact within +-%s' % (tot[0], tot[1], tot[2], win))

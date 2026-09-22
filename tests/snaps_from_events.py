#!/usr/bin/env python3
"""Pair whole-DS snapshots from an oracle capture: for each LABEL_A event, the next LABEL_B event.
usage: snaps_from_events.py events.txt LABEL_A LABEL_B out.bin [extra_label:size ...]
records: frame(u32) size(u32) A[size] B[size] then, per extra label, its first sample between A and B (zero-filled if absent)"""
import sys, struct
ev, la, lb, outp = sys.argv[1:5]
extras = [(e.split(":")[0], int(e.split(":")[1])) for e in sys.argv[5:]]
out = open(outp, "wb"); cur = None; n = 0; ext = {}
for line in open(ev):
    f = line.split()
    if len(f) < 4: continue
    lab = f[3]
    if lab != la and lab != lb and lab not in dict(extras): continue
    frame = int(f[0][6:]); mem = bytes.fromhex(line.split("mem=")[1].strip())
    if lab == la: cur = (frame, mem); ext = {}
    elif lab == lb and cur:
        out.write(struct.pack("<II", cur[0], len(mem)) + cur[1] + mem + b"".join(ext.get(k, b"")[:sz].ljust(sz, b"\0") for k, sz in extras)); n += 1; cur = None
    elif cur and lab not in ext: ext[lab] = mem
print("pairs:", n)

#!/usr/bin/env python3
"""Pair whole-DS snapshots from an oracle capture: for each LABEL_A event, the next LABEL_B event.
usage: snaps_from_events.py events.txt LABEL_A LABEL_B out.bin  -> records: frame(u32) A[16640] B[16640]"""
import sys, struct
ev, la, lb, outp = sys.argv[1:5]
out = open(outp, "wb"); cur = None; n = 0
for line in open(ev):
    if " %s " % la not in line and " %s " % lb not in line: continue
    f = line.split(); frame = int(f[0][6:]); lab = f[3]; mem = bytes.fromhex(line.split("mem=")[1].strip())
    if lab == la: cur = (frame, mem)
    elif lab == lb and cur:
        out.write(struct.pack("<I", cur[0]) + cur[1] + mem); n += 1; cur = None
print("pairs:", n)

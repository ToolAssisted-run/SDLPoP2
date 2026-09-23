#!/usr/bin/env python3
"""Per tick pair (LABEL_A .. LABEL_B, as snaps_from_events.py pairs them), the first HEAP sample between them.
usage: heap_from_events.py events.txt LABEL_A LABEL_B HEAP out.bin   (records: size(u32) heap[size], zero size if absent)"""
import sys, struct
ev, la, lb, lh, outp = sys.argv[1:6]
out = open(outp, "wb"); cur = None; h = None; n = 0
for line in open(ev):
    f = line.split()
    if len(f) < 4 or f[3] not in (la, lb, lh): continue
    mem = bytes.fromhex(line.split("mem=")[1].strip())
    if f[3] == la: cur = 1; h = None
    elif f[3] == lb and cur: out.write(struct.pack("<I", len(h or b"")) + (h or b"")); n += 1; cur = None
    elif cur and h is None: h = mem
print("heap records:", n)

#!/usr/bin/env python3
"""Pairs each tick's key table (probe keytab, DS:1D00 x 0x70 at 169B:05E0) with the controls control() saw next
(probe kc_ctrl, DS:5CD4). usage: inputtest.py events.txt out.bin  -> records: keytab[0x70] ctrl[3] direction[1]"""
import sys
out = open(sys.argv[2], "wb"); kt = None; n = 0
for line in open(sys.argv[1]):
    f = line.split()
    if len(f) < 4: continue
    if f[3] == "keytab": kt = bytes.fromhex(line.split("mem=")[1].strip())
    elif f[3] == "kc_ctrl" and kt: ctl = bytes.fromhex(line.split("mem=")[1].strip())[:3]
    elif f[3] == "kc_char" and kt:   # Char index / direction at the same control() entry: keep the prince's
        m = bytes.fromhex(line.split("mem=")[1].strip())
        if m[0] == 0x0A: out.write(kt + ctl + m[1:2]); n += 1
        kt = None
print("records:", n)

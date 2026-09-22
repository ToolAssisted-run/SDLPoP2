#!/usr/bin/env python3
"""kc_in (Char), kc_ctrl (control_x/y/shift), kc_ctrl1 (ctrl1[5]), kc_out (Char) probe groups -> quads.bin
   record = Char_in[64] + controls[8] + ctrl1[8] + Char_out[64]"""
import re, sys
ev=[]
for line in open(sys.argv[1]):
    m=re.search(r"probe=\d (kc_\w+) .* mem=([0-9A-F]{128})",line)
    if m: ev.append((m.group(1),bytes.fromhex(m.group(2))))
out=[]; i=0
while i+3<len(ev):
    if [e[0] for e in ev[i:i+4]]==["kc_in","kc_ctrl","kc_ctrl1","kc_out"]:
        out.append(ev[i][1]+ev[i+1][1][:8]+ev[i+2][1][:8]+ev[i+3][1]); i+=4
    else: i+=1
open(sys.argv[2],"wb").write(b"".join(out)); print(len(out),"quads")

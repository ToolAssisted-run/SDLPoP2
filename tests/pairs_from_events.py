#!/usr/bin/env python3
"""Turn oracle-run events with probes 'seq_in' (entry) and 'seq_out' (retf) carrying mem= samples into a pairs.bin."""
import re, sys
pairs=[]; pending=None
for line in open(sys.argv[1]):
    m=re.search(r"probe=\d (seq_in|seq_out) .* mem=([0-9A-F]{128})",line)
    if not m: continue
    if m.group(1)=="seq_in": pending=bytes.fromhex(m.group(2))
    elif pending is not None: pairs.append(pending+bytes.fromhex(m.group(2))); pending=None
open(sys.argv[2],"wb").write(b"".join(pairs)); print(len(pairs),"pairs")

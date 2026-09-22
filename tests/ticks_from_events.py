#!/usr/bin/env python3
"""Turn a tick capture (oracle tick2.script probes) into fixed-size binary records for ticktest.
Per tick: tk_in(Kid) tk_misc tk_opp tk_ch0..4 tk_collin at 169B:0692, the first kc_ctrl/kc_ctrl1 pair after it (2FDF:048C),
then tk_out(Char) tk_coll tk_flags tk_img at 169B:07D3. Record: 13*64 + 8 + 16 + 32 + 64... see ticktest.c."""
import sys, struct
recs = {}; order = []
cur = None
for line in open(sys.argv[1]):
    if not line.startswith("frame="): continue
    f = line.split(); frame = int(f[0][6:]); label = f[3]; mem = bytes.fromhex(line.split("mem=")[1].strip()) if "mem=" in line else b""
    if label == "tk_in":
        cur = {"frame": frame, "in": mem}; order.append(cur)
    elif cur is None: continue
    elif label in ("kc_ctrl", "kc_ctrl1"):
        if "out" in cur: continue                      # the guard's control() call, after the kid's frame
        cur.setdefault(label, mem)
    else: cur.setdefault(label, mem)
want = [("in", 64), ("tk_misc", 64), ("tk_opp", 64), ("tk_ch0", 64), ("tk_ch1", 64), ("tk_ch2", 64), ("tk_ch3", 64), ("tk_ch4", 64), ("tk_collin", 64), ("kc_ctrl", 8), ("kc_ctrl1", 16), ("tk_out", 64), ("tk_coll", 64), ("tk_flags", 32), ("tk_img", 64), ("tk_flagsin", 32)]
out = open(sys.argv[2], "wb"); n = 0
for r in order:
    r.setdefault("tk_flagsin", b"")
    if any(k not in r for k, _ in want): continue
    out.write(struct.pack("<I", r["frame"]))
    for k, sz in want: out.write(r[k][:sz].ljust(sz, b"\0"))
    n += 1
print("ticks:", n)

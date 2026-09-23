#!/usr/bin/env python3
"""plan2script.py LEVEL PLAN NAME ORACLE_DIR: an oracle-run script that plays a per-tick plan (explore.c) by writing the
key table (DS:1D00, phys 3CF50) and the BIOS shift flags (0040:0017) at each tick's start (probe 169B:05E0), so the
inputs reach exactly the tick they were planned for. Starts the level with `prince yippeeyahoo LEVELn` (levels 2..14)."""
import sys, os
level, plan, name, odir = int(sys.argv[1]), sys.argv[2], sys.argv[3], sys.argv[4]
ticks = [tuple(int(v) for v in l.split()) for l in open(plan) if l.strip()]
out = ["key 300 tab 1\nkey 304 tab 0\nkey 320 tab 1\nkey 324 tab 0\nkey 340 enter 1\nkey 344 enter 0\n"] if level > 2 else []   # the copy check (past level 2 only)
out.append("probe 2FDF 048C kc_ctrl 40F24 8\nprobe 2FDF 048C kc_ctrl1 41372 10\n")
out.append("ram 700 %s/w/ram%s.bin\n" % (odir, name))
for ip, lab in (("05E0", "ds_tick"), ("064F", "ds_postroom"), ("00F5", "ls_a"), ("0135", "ls_b")):
    out.append("probe 169B %s %s 3DB50 4300\n" % (ip, lab))
# key table positions (DS:1D00): arrows 58/5A/55/5D, diagonals 54/56/5C/5E (Home PgUp End PgDn)
pos = {(-1, 0): 0x58, (1, 0): 0x5A, (0, -1): 0x55, (0, 1): 0x5D, (-1, -1): 0x54, (1, -1): 0x56, (-1, 1): 0x5C, (1, 1): 0x5E}
if os.environ.get("POKE_HP"):   # the prince's hp and max hp (Kid+0x12/0x13, phys 40D98) at the first tick, as explore's EXPLORE_HP
    hp = int(os.environ["POKE_HP"]); out.append("probepoke ds_tick 1 40D98 %02X%02X\n" % (hp, hp))
for k, (x, y, sh) in enumerate(ticks, 1):
    table = bytearray(0x70)
    if (x, y) in pos: table[pos[(x, y)]] = 1
    out.append("probepoke ds_tick %d 3CF50 %s\n" % (k, table.hex().upper()))
    out.append("probepoke ds_tick %d 417 %02X\n" % (k, 2 if sh else 0))
# after the plan: nothing held
out.append("probepoke ds_tick %d 3CF50 %s\nprobepoke ds_tick %d 417 00\n" % (len(ticks) + 1, "00" * 0x70, len(ticks) + 1))
out.append("end %d\n" % (600 + 7 * len(ticks) + 200))
open(os.path.join(odir, name + ".script"), "w").write("".join(out))
print(name, "prince yippeeyahoo LEVEL%d" % level, len(ticks), "ticks")

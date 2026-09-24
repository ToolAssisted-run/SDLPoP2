# Oracle equivalence tests

`seqtest`: replays (Char before, Char after) pairs captured around every play_seq call in the DOS
game (oracle-run probes at 0AFF:03AA and 0AFF:06D2 with a 64-byte memory sample of Char at DS:5AB6)
through the reconstructed `play_seq`, comparing the 13 fields the player writes. Capture script:
`<workspace>/oracle/seqcap.script`; run `tests/run_seqtest.sh SEQUENCE.DAT events.txt`.
Result 2026-09-22: 110/110 cases identical (level 1, running, jumping, turning, falling, guard).

`ctltest`: control() equivalence. Capture: `<workspace>/oracle/kidctl2.script` (probes kc_in/kc_ctrl/
kc_ctrl1/kc_opp/kc_kid/kc_misc(DS:5CC4)/kc_out); `tests/quads_from_events.py events quads.bin`; run
`PRINCE_DAT=PRINCE.DAT ctltest SEQUENCE.DAT level2000.bin PRINCE.EXE quads.bin`. Result 2026-09-22:
334/335 identical. Open: case 75, a charid-2 character with the sword drawn whose captured Opp record is
not the Kid; the game starts seq 0x7F (kid engage), so that call reached control() through a path where
Opp/Char differ from the play_all_chars convention. To be revisited with a per-call stack sample.

## Tick test (retired)
The first whole-tick harness (tests/ticktest.c, 1451 ticks over five captures of play_kid_frame 169B:0692) was
replaced by the snapshot tests below once the whole tick body was reconstructed; it is in the git history.

## Snapshot tests (tests/snaptest.c)
Captures probe the whole data segment DS:2B00..6C00 (16640 bytes; the tracer allows samples up to 64 KiB) at the
phases of the tick body: ds_tick 169B:05E0, ds_prechars 0616, ds_postchars 0619, ds_preleave 0642, ds_postroom 064F
(`<workspace>/oracle/gen_ticks.py` NAME --snap). tests/snap.c maps DS offsets onto the reconstructed globals.

    python3 tests/snaps_from_events.py tickD-snap.txt ds_prechars ds_postchars chars.bin
    GUARD_DAT=.../GUARD.DAT KID_DAT=... PRINCE_DAT=... snaptest chars SEQUENCE.DAT ram1436.bin PRINCE.EXE chars.bin

Status (8 captures, level 1): play_all_chars 1641/1641 ticks, room transitions 1841/1841 (11 room changes), and
the whole tick body (tick mode: ds_tick -> ds_postroom with the prince's captured controls, kc_ctrl:8 kc_ctrl1:16)
1837/1841; the 4 remaining pairs span the ESC pause of the older scripts (frames 1502, 2650), not code.

    python3 tests/snaps_from_events.py tickD-snap.txt ds_tick ds_postroom tick.bin kc_ctrl:8 kc_ctrl1:16
    snaptest tick SEQUENCE.DAT ram1436.bin PRINCE.EXE tick.bin

Level 3 (`<workspace>/oracle/gen_ticks.py` L3loose7 / L3loose22 / L3btn10 / L3btn3, run with `prince yippeeyahoo LEVEL3`,
RAM image w/ramL3.bin): loose floors, falling floors, buttons and gates match on every tick of L3loose7, L3loose22
and L3btn3; L3btn10 has 5 ticks left that need the falling rock (tile 2, segment 186A). The random explorations
L3r1/L3r2 reach skeleton rooms (charid 4 AI and SKELETON.DAT not reconstructed yet).
Snapshots of new captures cover DS:2900..6C00 (the falling-floor list starts at DS:293E); pair files record the size.

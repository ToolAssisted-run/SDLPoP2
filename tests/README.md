# Oracle equivalence tests

`seqtest`: replays (Char before, Char after) pairs captured around every play_seq call in the DOS
game (oracle-run probes at 0AFF:03AA and 0AFF:06D2 with a 64-byte memory sample of Char at DS:5AB6)
through the reconstructed `play_seq`, comparing the 13 fields the player writes. Capture script:
`~/pop2dec/oracle/seqcap.script`; run `tests/run_seqtest.sh SEQUENCE.DAT events.txt`.
Result 2026-09-22: 110/110 cases identical (level 1, running, jumping, turning, falling, guard).

`ctltest`: control() equivalence. Capture: `~/pop2dec/oracle/kidctl2.script` (probes kc_in/kc_ctrl/
kc_ctrl1/kc_opp/kc_kid/kc_misc(DS:5CC4)/kc_out); `tests/quads_from_events.py events quads.bin`; run
`PRINCE_DAT=PRINCE.DAT ctltest SEQUENCE.DAT level2000.bin PRINCE.EXE quads.bin`. Result 2026-09-22:
334/335 identical. Open: case 75, a charid-2 character with the sword drawn whose captured Opp record is
not the Kid; the game starts seq 0x7F (kid engage), so that call reached control() through a path where
Opp/Char differ from the play_all_chars convention. To be revisited with a per-call stack sample.

## Tick test (tests/ticktest.c)
Replays play_kid_frame (169B:0692) tick by tick. Capture script (oracle, see pop2dec/oracle/tick2.script):
probes at 169B:0692 for Kid (40D86), DS:5CC4 misc (40F14), Opp (40D46), chars[0..4] (40DC6..40EC6), the
collision arrays DS:2B24 (3DD74) and flags DS:6948 (41B98); at 2FDF:048C the post-input controls
(40F24, 8 bytes) and ctrl1 (41372, 16 bytes); at 169B:07D3 Char (40D06), the arrays, flags and the sprite box
vars DS:6112 (41362). A `ram 1436 file` line dumps the level as loaded. Run:

    KID_DAT=.../KID.DAT PRINCE_DAT=.../PRINCE.DAT tests/run_ticktest.sh SEQUENCE.DAT ram1436.bin PRINCE.EXE tick-events.txt level.bin

Known, accepted differences: the dead prince's counter waits for the death sound (not modelled, reported
separately), and on the level-restart tick the game leaves the last drawn character's box in the image
variables (the harness recomputes the prince's). Out-of-level ticks skip the collision comparison for the
same reason. One open case: the level's room records (character counts and init records at level+0x1867) are
runtime state that the game rewrites when characters change rooms; the harness uses the records as loaded, so a
guard scan (031BC4) after a teleport can count differently (capture E tick 182). Reconstructing the room-change
bookkeeping (OVL01 02D444 / 02DC8C) will close it. Status: 1451 ticks over five captures (window jump, running, turning, standing jumps, ledge grabs,
crouching, sword fight, falls, deaths, teleports into rooms 1, 10, 16) identical.

## Snapshot tests (tests/snaptest.c)
Captures probe the whole data segment DS:2B00..6C00 (16640 bytes; the tracer allows samples up to 64 KiB) at the
phases of the tick body: ds_tick 169B:05E0, ds_prechars 0616, ds_postchars 0619, ds_preleave 0642, ds_postroom 064F
(pop2dec/oracle/gen_ticks.py NAME --snap). tests/snap.c maps DS offsets onto the reconstructed globals.

    python3 tests/snaps_from_events.py tickD-snap.txt ds_prechars ds_postchars chars.bin
    GUARD_DAT=.../GUARD.DAT KID_DAT=... PRINCE_DAT=... snaptest chars SEQUENCE.DAT ram1436.bin PRINCE.EXE chars.bin

Status (8 captures, level 1): play_all_chars 1641/1641 ticks, room transitions 1841/1841 (11 room changes), and
the whole tick body (tick mode: ds_tick -> ds_postroom with the prince's captured controls, kc_ctrl:8 kc_ctrl1:16)
1837/1841; the 4 remaining pairs span the ESC pause of the older scripts (frames 1502, 2650), not code.

    python3 tests/snaps_from_events.py tickD-snap.txt ds_tick ds_postroom tick.bin kc_ctrl:8 kc_ctrl1:16
    snaptest tick SEQUENCE.DAT ram1436.bin PRINCE.EXE tick.bin

Level 3 (pop2dec/oracle/gen_ticks.py L3loose7 / L3loose22 / L3btn10 / L3btn3, run with `prince yippeeyahoo LEVEL3`,
RAM image w/ramL3.bin): loose floors, falling floors, buttons and gates match on every tick of L3loose7, L3loose22
and L3btn3; L3btn10 has 5 ticks left that need the falling rock (tile 2, segment 186A). The random explorations
L3r1/L3r2 reach skeleton rooms (charid 4 AI and SKELETON.DAT not reconstructed yet).
Snapshots of new captures cover DS:2900..6C00 (the falling-floor list starts at DS:293E); pair files record the size.

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

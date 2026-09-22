# Oracle equivalence tests

`seqtest`: replays (Char before, Char after) pairs captured around every play_seq call in the DOS
game (oracle-run probes at 0AFF:03AA and 0AFF:06D2 with a 64-byte memory sample of Char at DS:5AB6)
through the reconstructed `play_seq`, comparing the 13 fields the player writes. Capture script:
`~/pop2dec/oracle/seqcap.script`; run `tests/run_seqtest.sh SEQUENCE.DAT events.txt`.
Result 2026-09-22: 110/110 cases identical (level 1, running, jumping, turning, falling, guard).

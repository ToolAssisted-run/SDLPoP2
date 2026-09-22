# SEQUENCE.DAT (PoP2 animation sequences)

DAT container: dword table_offset, word table_size at 0; at table_offset: word type_count (1),
type tag "SQES" (4 bytes), word ?, word entry_count (233), then 11-byte entries
{word id, dword offset, word size, word flags (0x40), byte ?}. Each resource: 1 checksum byte, then
the sequence as 16-bit little-endian words, then (when the sequence ends with JMP) one trailing
byte = the target SEQUENCE ID of that jump.

Opcodes are PoP1's (SDLPoP `SEQ_*` values, sign-extended to a word): FFFF JMP (target = trailing
byte, a sequence id), FFFE FLIP, FFFD UP, FFFC DOWN, FFFB DX n, FFFA DY n, FFF9 ACT n,
FFF8 SET_FALL x y, FFF7 JMP_IF_FEATHER seq, FFF6 DIE, FFF5 KNOCK_UP, FFF4 KNOCK_DOWN,
FFF3 ? (PoP1 GET_ITEM slot; appears in jumps, probably a sound/effect), FFF2 ? (rare),
FFF1 SND n (footsteps in run/jump sequences), FFEE ? n (20 uses). Anything else is a frame number.

Sequence ids and frame numbers follow PoP1's numbering for the shared moves: 1 start_run
(JMP 200 = run cycle), 2 stand (f15), 3 standing_jump (f16..), 4 run_jump (f34..44), 5 turn
(f45..52), 6 run_turn (f53..65), 8 jump_up_and_grab (f67..80), 10 climb_up (f135..141),
11 release_ledge_and_land (f81..85), 12 fall (act 4, f106, JMP 233), 13 stop_run (f35..).
Full decoded listing with SDLPoP names where the id matches: SEQUENCE_DUMP.txt (tools/seqdump.py).

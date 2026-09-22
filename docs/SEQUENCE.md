# SEQUENCE.DAT (PoP2 animation sequences)

DAT container: dword table_offset, word table_size at 0; at table_offset: word type_count (1),
type tag "SQES" (4 bytes), word ?, word entry_count (233), then 11-byte entries
{word id, dword offset, word size, word flags (0x40), byte ?}. Each resource: 1 checksum byte, then
the sequence as 16-bit little-endian words, then (when the sequence ends with JMP) one trailing
byte = the target SEQUENCE ID of that jump.

Opcodes (from the player at 0AFF:03AA, jump table cs:03F4, index = opcode + 0x18; Char = DS:5AB6):
FFFF JMP seq (word; target sequence id) - FFFE FLIP (Char+1 direction; extra dx 25 when Char+6 == 10) -
FFFD UP (Char+0xA curr_row--) - FFFC DOWN (curr_row++) - FFFB DX n (Char+2 x += facing-adjusted n) -
FFFA DY n (Char+4 y += n) - FFF9 ACT n (Char+0xB action) - FFF8 SET_FALL x y (Char+0xC/0xD; 10000 = keep) -
FFF7 ADD_FALL x y (clamped to 16/32) - FFF6 JMP_IF_FEATHER seq (DS:5D36) - FFF5 arg -> DS:85F8 when
action in {0,1} - FFF4 KNOCK_DOWN (DS:613E = 1) - FFF3 KNOCK_UP (DS:613E = -1) - FFF2 n: 1/2 = sequence
control via 0AFF:1954 (restart when at position 0), 3 = call 366C:1704 - FFF1 SND n (1 = alternating
footsteps, else sound n; 0x10F special) - FFF0 level-6 counter DS:5CEC - FFEF CLEAR_CHAR (room = 0, no
frame) - FFEE Char+0x19 = 0 - FFED y = curr_row*63 + 56 (+37 for actions 7/8) - FFEC (frame = opcode,
unused?) - FFEB n -> Char+0x24 - FFEA cond a b c: JMP b if 2751:008C(a) else JMP c - FFE9 HOLD (stay on
this item) - FFE8 FLASH n [m]: screen effect on/off (0FB3:2A34 / 294C). Anything else is a frame number,
written to Char+7. Sequence position = Char+0x15 (word index), sequence id = Char+0x17.

Sequence ids and frame numbers follow PoP1's numbering for the shared moves: 1 start_run
(JMP 200 = run cycle), 2 stand (f15), 3 standing_jump (f16..), 4 run_jump (f34..44), 5 turn
(f45..52), 6 run_turn (f53..65), 8 jump_up_and_grab (f67..80), 10 climb_up (f135..141),
11 release_ledge_and_land (f81..85), 12 fall (act 4, f106, JMP 233), 13 stop_run (f35..).
Full decoded listing with SDLPoP names where the id matches: SEQUENCE_DUMP.txt (tools/seqdump.py).

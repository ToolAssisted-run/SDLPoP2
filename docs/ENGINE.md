# Engine structure (as identified so far; DOS addresses are runtime CS:IP, DS = 3B25)

- int8_handler (194C:7EE7): PIT splitter; four countdown timers at DS:24DC; 32-bit BIOS-style tick
  DS:4E24; chains to the previous vector.
- Main state machine 1286:03B6 -> game_tick_step (169B:0A30) once per 12 Hz tick:
  - tick DS:5D04++ ; tick_logic (0FB3:12F4) = objects_update (1375:1FBA), tick_029a (0993:029A),
    tick_objects_05ca, draw_room (0FB3:1308); then tick_13c2; pending callbacks (0FB3:219D).
  - play_all_chars (169B:07EC): for each character i in room_table[drawn_room].nchars:
    load_char(i); if dead (direction 0x56) / out of level / y > 254: char_fell_out;
    else load_char_and_opp(i) (Opp = saved copy), load_fram_det_col, char_control_step
    (0AFF:1258: control_by_charid 1611:0068 for charids 2..11, OVL01 kid_control 2FDF:048C for
    the others), play_seq, then if x within 0x22..0x221: fall_accel, fall_speed, load_frame_to_obj,
    load_fram_det_col, collision/level specials; save_char.
- play_seq (0AFF:03AA), fall_accel/fall_speed (0AFF:08E4/091C) match PoP1's logic.
- Character records: Char DS:5AB6, Opp DS:5AF6, Char_saved DS:5B36, chars[5] DS:5B76 (64 bytes).
- Level: DS:2BB8 (see LEVEL.md). Input: control_x/y/shift DS:5CD4/5/6 from key_states DS:1D0D.
- Resources: get_resource(id, 4CC tag) 194C:6F4C, lock_resource 194C:15B2; level files read directly.
- RTLink overlays: 15 code overlays (OVL01 = gameplay: kid control, room entry; OVL00 = intro/menus).

## Kid control (OVL01, 2FDF:048C = control)
play_kid_frame (169B:0692): Char = Kid; play_kid_control (0AFF:10E8) -> control_kid (0AFF:11F8):
load_ctrl1_saved, read_input (0823:10A0 -> read_keyb_control), update_ctrl1_edges (0AFF:13B8:
ctrl1_* at DS:6122..6126 become -1 on a new press, 0 on release, 1 once consumed), control_dispatch
(0AFF:12CA: flip x to forward/backward for a left-facing character, flip y when upside_down, call
control(), flip back), save_ctrl1. control() is PoP1's frame-range dispatch: 15 or 50..52 standing,
45..49 turning, 1..3 start_run, 67..69 jumpup, <15 running, 87..99 hanging, 109 crouched,
0xF6..0x107 with-sword, 0xD9..0xE2, dead frames, ... Sequences start through seqtbl_offset_char
(2FDF:000E, also sets Char+0x19 = seq id). After control: play_seq, fall_accel, fall_speed,
load_frame_to_obj, and the OVL01 3212 checks (collisions/press), then Kid = Char.

## Frames and tile helpers (verified through the control test)
- Frame table entries are 7 bytes {image:2, sword:2, dx:1, dy:1, flags:1 (low 5 bits = x weight)}; load_frame
  (0AFF:02AC) copies the entry to cur_frame at DS:5CC6. Kid-type charids 0/1/6 use the table at the start of
  the data resource (3891:0000, = PRINCE.EXE overlay-15 payload); charids 2/4/10/12, 7/11, 8 use the FRAM
  resource locked from DS:0CB8, indexed from frame 149 (offset -0x413), with frame shifts (+0x46 for
  0x66..0x6A on 2/4/10/12; +0x2B below 0xB7 for charid 8).
- Direction tables: DS:0CF8 {-1, +1} = front, DS:0CFA {+1, -1} = behind, indexed by direction + 1.
  0AFF:0F6C(n) = tile n in front, 0AFF:0F94 = tile BEHIND (used by "climb down": the prince climbs down
  the edge behind him, like PoP1), 0AFF:14E2 = tile above. Column x table DS:0D06 = 130 + 32*col.
- Char fields learned from control: +0F "moved" flag, +10 sword-drawn flag (1 -> sword control),
  +11 alive (<0), +12 hp, +14 hp delta, +23 (Opp side: >1 enables engagement), +39 opp_index; Kid.index
  = 10 marks the player's record inside control routines.
- Control test status: 331/335 captured cases identical; the rest need DS:5CC4 (engagement toggle) which
  the next capture samples.

## The prince's tick (169B:0692 play_kid_frame; verified by tests/ticktest.c)
loadkid; if out of the level (0AFF:0DF6): play_kid_control + char_fell_out (0AFF:0D1E: level kind 5
lets the sea rooms 0x13/0x10 catch the fall, elsewhere death at y 0x180). Otherwise: pick the opponent
(Kid+0x39, or the nearest live char, OVL01 02DCC8), 0AFF:080A loads Kid into Char and chars[n] into Opp,
load_fram_det_col, play_kid_control (returns -1 on the level-restart tick), then unless DS:5CD8 (frozen):
play_seq and, if a frame was produced, fall_accel, fall_speed, load_frame_to_obj (0993:09B6), load_fram_det_col,
set_char_collision (OVL01 032ADE), opponent bump (169B:0E94), check_collisions (03212C), check_bumped (03250C),
check_gate_push (032E78), 1375:06F2 (per action: ledge grab on jump-up frames 0x66..0x69, standing-on-air
check 0AFF:09F8 -> start_fall 0AFF:0AAA, falling -> OVL01 02FE86 land/loose-floor), 1375:0758 (spikes 5/6/0x22,
loose 0xB, chomper 0xF at or above), level-kind 2/3/4 hooks, and the loose-floor shake when a sequence set
DS:613E (knock). Finally Kid = Char.

Dead kid: the death counter Char+0x11 goes 0..7 one per tick but waits while the death sound plays
(1611 sound slot DS:087E); at 7 -> 169B:123E restarts the level and that tick produces no frame.

## Sprite box and collisions (OVL01 segment 3212, tables in DS)
- load_frame_to_obj: obj_x = char_dx_forward(dx) - 130, obj_y = y + dy, obj_id = frame image, chtab 2 for
  charids 0/1/6 (KID.DAT SHAP resource 25002 + image, or 24602 + image above 221), 3 otherwise
  (charid 4 frames 0xCE..0xD1: chtab 4, image + 0x68). A SHAP body starts with {height:2, width-1:2}.
- set_char_collision: image_height/width at DS:6112/6114, char_x_left = obj_x + 130 - width (facing right)
  or - sword extra width (facing left; FRAM entry byte +2 of the sword frame minus 2 when the sword is drawn),
  char_x_right = left + width (+ extra), char_top_y = obj_y - height + 1, Char+1B..+22 = the drawn box,
  rows/cols at DS:6135..6138 (y_to_row = (y-3)/63, -1 at y <= 3; col_from_x18 with left clamped to 0 and right
  to 9), frame flag 0x20 narrows the x range by 9 on both sides.
- wall_type (032378): 0 passable; tile 0x14/0x19 -> 4, 7 -> 2 if height < 0x13 else 4, 2 -> 3 if modifier < 5
  and DS:0174 set else 4, 4 (gate) -> 1 (5 on level kind 3, 8 in room 9 of level 8), 0xC -> 6. Tables DS:0D48
  (left edge offset) = {0,25,0,-14,2,12,6,0,10,0} and DS:0D52 (right) = {0,0,31,-14,0,0,10,0,15,0}; a tile's
  wall spans [colx + 14 + left, colx + 14 + 31 - right]; 578 (DS:0D22) / 0 mean "no wall".
- check_collisions: rows curr/below/above over columns x_to_col(char_x_left) - 2 .. x_to_col(char_x_right) + 4
  (max 11), flags 0x0F = wall's left edge < char_x_right, 0xF0 = char_x_left < wall's right edge, plus the room
  of each column. Layout DS:2B24 above_flags[10], 2B2E prev/curr collision row, 2B30 below_flags, 2B3A/2B3B
  checked cols, 2B3C/2B3D bump_col_left/right_of_wall, 2B3E above_room, 2B48 below_room, 2B52 prev_room,
  2B5C curr_room, 2B66 tile_left_xpos; flags for the current row at DS:6952, previous row's at DS:6948
  (move_coll_to_prev picks the above/below row when the character changed row by one or two).
  Skipped for action 7, seq 0x4E/0x46, and level-7 moving objects (DS:440A). The arrays are shared with the
  other characters (play_all_chars runs the same routines), so the "previous" state seen by the prince is
  whatever character ran last.
- check_bumped: bump_col_left_of_wall >= 0 -> look right (needs facing right or sword drawn),
  else bump_col_right_of_wall -> look left; the tile must block (032636: gate needs can_bump_into_gate =
  gate lower than the character's box height, tile 7 needs height >= 0x13 or modifier bits, 0xC needs
  height > 0x19, 10 never); then bump_apply (0326E2): x += distance to the wall edge, next tile beyond a wall
  tile; empty -> knocked back 8 and seq 0x2D, else bump_stand (0327DA): seq 0x2E/0x2F (0x41/0x40 with the
  sword, 0x79 in sword frames, -10 x when falling fast).
- check_gate_push (032E78): a gate at/behind/in front whose column is flagged 0xFF in both current and previous
  flags and low enough: seq 0x32 (pushed) unless frame 0x6D / modifier 0 / sword drawn, then 032F20 moves
  the character out by the smaller of the near/far distances.
- get_edge_distance (032C84): rebuilds the box, then distance to a wall in front (edge_type 1, < 0x20) or to
  the edge of the tile (edge_type 0; loose/empty/0x1E in front, or 6/0x16/2/0x22 with a -1) else 0x1C
  with edge_type 2.

## Falling and landing
- start_fall (0AFF:0AAA): row++, sequence by frame (0x51 -> 7 and y += 12; 9 -> 7; 0xD -> 0x13; 0x1A -> 0x12;
  0x2C -> 0x15; 0x51..0x55 -> 0x13; hanging 0x96..0xB3 -> 0x5F/0x51 for the prince, 0x53/0x52 otherwise;
  charid 2 in seq 100 -> 0xBA; else 7); most cases snap x away from walls/ledges (distance < 8 -> -11,
  > 0x18 -> -0x15) and y to row*63+1 when something is above; level kind 1 uses seq 0x1B, feather fall 0xE4.
- falling (02FE86): scream at fall_y > 30; above the row's floor line (row*63+56) only the ledge grab
  (0AFF:0E28: shift held, fall_y < 32, tile above-front is floor and above is empty: x snaps to the edge
  (+4-7 or +4-9), seq 15 hang, DS:6144 = 12); at/below it: wall -> pushed out (0AFF:0F10), empty -> row++,
  loose/chomper -> fall through (hp -100 unless fall_y <= 32 and the player, fall_y halves, y = DS:0D40[row]-7),
  else land (02FFE0): fall_y < 0x16 -> seq 0x11 (0x3F/0xBB with the sword, drawn), < 0x21 -> hp -1 and seq
  0x14, else death (seq 0x16, sound 0, 0301D2 nudges away from the edge); tiles 0x17/0x18 go to OVL02 34724.
- take_hp (0AFF:095C): schedules -n in Char+0x14 (never past -hp); returns 1 when the character dies.

## The tick body (169B:05E0)
1375:1A52 falling loose floors (13-byte entries DS:293E, count DS:6186) -> 1375:0006 animated tiles (4-byte entries
DS:6676, count DS:6670; 1375:1414 adds one) -> level-kind hooks -> 2D3E:0A4A guard spawns -> 169B:0FF0 guards'
line of sight (Char+0x23) -> play_kid_frame -> play_all_chars -> 2D3E:1F48 sword hits, 2D3E:19C2 hurt characters
(action 99) -> 1611:0164, 169B:0DB4, 169B:11E2 -> 0823:1008 hp deltas -> 2D3E:108A prince left the room?
-> 0823:0E72 switch the drawn room.

## Rooms and their characters (room.c)
- Each room has a character count and five 23-byte records at level+0x1867 + (room-1)*0x74 (DS:43AB + room*0x74):
  +0 tile position (row*10+col at load; 30 = outside; runtime saves write row*10), +1 x, +3 direction, +4 skill,
  +5 sequence, +7 sequence position (nonzero = resume), +9 palette, +A slot index, +B sword drawn, +C hp,
  +D -> Char+0x38, +E -> Char+0x39, +F type (charid = DS:0096[type]), +10 max hp, +11 y / state, +15 home row.
- Only the drawn room's characters are live in chars[]. 2D3E:108A detects the prince leaving (2D3E:14DE, edges
  at char_x_left_coll < 0x7E / > 0x1C1, y < -16 / >= 0xE7), moves him with 2D3E:1454 and handles the old room's
  characters (2D3E:1152): each either follows him (engaged guards near the exit edge, anyone already standing in
  his new room) or is written back to its record (2D3E:04CC; a character outside the grid is re-homed through
  2D3E:133A, which walks the room links). 0823:0E72 then sets the neighbours (0FB3:0026), pulls records within
  0xD0 of the shared edge out of the side rooms (2D3E:03DA) and rebuilds chars[] (2D3E:0064).
- chars[-1] is Kid in the data segment; code that indexes chars with -1 reads the prince.

## Guards (guard.c)
- 2D3E:1864 clears the controls and dispatches by charid: 2 (level-1 guards) -> 2D3E:192C, which picks OVL10
  366C:03E2 (sword sheathed), 043C (frames 0xBA..0xD4: turning/noticing) or 0774 (sword drawn). The routines
  "press" controls through 366C:0002..0042 (forward, backward, up, down, down+back, down+forward, shift), then
  the shared control() state machine runs, exactly like PoP1's autocontrol.
- Sword fighting uses PoP1's per-skill probability tables (DS:1BB6 strike, 1BCE restrike, 1BE6 block, 1BFE
  block after a hit, 1C16 advance; skill = room record +4) against 2751:008C, PoP1's generator
  (seed DS:2B7A, seed = seed*0x343FD + 0x269EC3, (seed >> 16) % (n+1)).
- Sword reach: 2D3E:21C6 returns near/far distances by charid and relative facing.
- Guard frames come from the guard DAT's FRAM table (GUARD.DAT 750 on level 1, frames from 149); guard sprites
  are GUARD.DAT SHAP 751 + image, or 851 + image at or above the type's threshold (DS:06BC[type], 31 for type 0).

## Fights, sight, spawns, animated tiles (fight.c, room.c, anim.c)
- 2D3E:1F48: every character of the drawn room strikes at the prince and he at each of them (2D3E:1FB0): a strike
  frame within reach either is parried (opponent in frame 0xA1/0x96, facing: opponent frame := 0xA1, seq 0x45) or,
  on the connecting frame (2D3E:23C8: 0x9A, 0xF5 for the prince), marks the opponent with action 99.
  2D3E:19C2 then applies hits (2D3E:1AAA: -1 hp, seq 0x4A/0x5E, 0x2D against charids 7/8; dying falls with seq 0x51
  at an edge or dies via 2D3E:1D74) and sets the guard's recovery timer from DS:13D0[skill].
- 169B:0FF0: guards' line of sight (Char+0x23): 3 = clear line and the nearest on its side, 2 = clear line,
  1 = a gap, loose floor or closed gate in between, 0 = wall / other row / not applicable.
- 2D3E:0A4A spawns guards from the room's spawn points (level+0x26D7 + room*0x22) when the prince is alive,
  the tick is not a multiple of 3 and no live guard is already between him and the point on that side.
- Animated tiles (PoP1's trobs): list at DS:6676 (4 bytes: tile position, room, state, tile), count DS:6670, at most
  20. 1375:0006 runs each entry's tile handler (1375:0096, most handlers in the level-kind overlay) on a copy of
  the tile's attribute (DS:5CF0) and drops entries whose state went negative. Room switches start the animations
  of the tiles on screen (0823:0B78); the list is only cleared at level start (169B:0070).
- The input reader 0823:10A0 clears next_room and the controls before reading the keyboard/joystick.
- Level-kind overlays: the 33FD overlay loaded on level 1 (kind 5) is the file saved as ovl02_33FD (the capture
  numbering does not follow the RTLink descriptor order; identify overlays by content).

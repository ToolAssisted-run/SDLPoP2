# The three DOS releases: IR, 1.0 and 1.1

Prince of Persia 2 shipped in three DOS versions. SDLPoP2 reconstructs **1.1**, the CD build: its PRINCE.EXE
(md5 8e04bed6…) is the 1.1 executable. This document lists every difference found between the three, and a plan
for letting the user choose which one SDLPoP2 runs.

The findings come from four sources:

- Ghidra projects of all three executables, with their overlays unpacked (jaffanator2 `~/pop2dec/ghidra/pop2vir`,
  `pop2v10`, `pop2v11`).
- An instruction-level alignment of each pair of builds (every changed place, resident code and all 15 overlays).
- A resource-by-resource comparison of every game file.
- Oracle runs of IR and 1.0 in the DOSBox-X oracle.

The working notes and tools are in `~/pop2dec/versions/`. `NOTES.md` has one line per finding, with the Ghidra
addresses.

## 1. The builds

| | IR (initial release) | 1.0 | 1.1 (SDLPoP2) |
|---|---|---|---|
| title string | `PRINCE OF PERSIA 2` | `PRINCE OF PERSIA 2 1.0` | `PRINCE OF PERSIA 2 v1.1` |
| original PRINCE.EXE | 290,415 B, md5 7bcdb72c… | 292,865 B, md5 117f3203… | 259,583 B, md5 8e04bed6… |
| overlays (RTLink/Plus) | stored raw | stored raw | compressed |
| DGROUP (at load segment 0823) | 3916 | 3956 | 3B25 |
| DS:0 in the EXE file | 0x44540 | 0x45050 | 0x3CE40 |
| Kid / chars[] / tick | DS:5C80 / 5CC0 / 5E60 (16-bit) | DS:5B12 / 5B52 / 5CE8 (32-bit) | DS:5B36 / 5B76 / 5D04 (32-bit) |
| cheat word (TXT4 10) | `MAKINIT` | `YIPPEEYAHOO` | `YIPPEEYAHOO` |
| extra files | FRAGSND.DAT, TANDYSND.DAT | FRAGSND.DAT, TANDYSND.DAT | none (the Tandy and fragment sound are gone) |

The zips also hold cracked PRINCE.EXEs (IR b8b55078…, 1.0 eb46006c…). Only the original executables were used.

The game data of 1.0 and 1.1 is **identical**: every .DAT file matches, and only the drivers, CONFIG.DAT and SETUP
differ. The two versions differ only in code, and a 1.1 install has all the data 1.0 needs. IR's data differs in
nine files (see §4).

1.1 was recompiled: its code differs from 1.0's almost everywhere at the byte level (register allocation,
operand order, branch layout), but very few changes alter behaviour (§2). IR is an older code base, with many real
changes (§3).

Legend: **G** = gameplay (changes the state that a TAS or an oracle capture sees); **R** = RNG timing (changes when
random() is drawn, so it is gameplay too); **S** = sound or music only; **V** = drawing or palette only; **U** =
menus, cheats or system.

## 2. 1.0 → 1.1

| | where (1.1 address, SDLPoP2) | 1.0 | 1.1 |
|---|---|---|---|
| G | find_opponent 2D3E:08E8 (room.c) | opponent "near" when \|dx\| ≤ 43 | ≤ 42 |
| G | enter_level_door 2FDF (control.c) | x = col_x_left[col−1] + 0x10 | + 0x1E |
| G | pick_up, the sword on level 8 room 9 (items.c) | facing left, dx +6 | +2 |
| G | sword mode 2FDF:1BFA (control.c, control_2fdf_1bfa) | out of range when d ≥ 0xCD | d > 0xCF |
| G | guard_armed 366C:0774 (guard.c) | `if (f23 == 1 \|\| f23 == 2) { 366C:08F0; return; }` first (steps relative to Opp) | the test is `f23 == 1 && f23 == 2`, which is never true, so 366C:08F0 is never called. SDLPoP2 lacks 08F0; transcribe it from 1.1's unused copy (OVL10 036FB0) |
| G | guard 036AFC (guard.c) | char_dx_forward(10) at the end | (8) |
| V | dead_body 0993:0A8E (render_sprites.c): frames 0xB9 and 0x6A..0x6E | does not move the body's image 0xC forward (0AFF:0390) | moves it |
| S | anim 1375:0C66 (anim.c, sound 7) | also needs DS:005C == 0 | – |
| U | NEWBUMP command-line switch (0AFF:0AAA, kid.c bump) | with NEWBUMP the position correction comes after play_seq; without it, before (as in 1.1) | the switch is gone |
| U | hotkey hp display 0823:0528 | 0FB3:25D4(10, …) | (8, …) |
| U | menu and save 0D5E:1288 / 0CF2 | takes an argument / pops 6 bytes | none / 2 |
| V | render hook 0AFF:15AE | DS:6109 = a word from 17C1:016E | 0x140 |
| – | memory housekeeping 1286:087E, 169B:0070 | 195B:50F2(100,50), 195B:1D5A/1A0E(−1,0xFF) | removed (heap layout only) |
| – | dead code with no effect | the DS:6646 countdown (never set); control_standing `si < 0xCD` | – |

Checked with the oracle: a new game starts with hp 3, max 3, in both 1.0 and 1.1. 1.0's do_startpos branch
(`DS:0998 == 0 → hp 4`) is not taken on a new game.

## 3. IR → 1.0 (1.1 behaves like 1.0 in every row unless noted)

### 3.1 The prince: movement, collisions, tiles

| | where | IR | 1.0 / 1.1 |
|---|---|---|---|
| G | x_to_col / col_from_x18, 0AFF:1010 (control.c) | truncates: at x < 14 the column is one higher | floors (`col--` when x18 < 0) |
| G | push_out_of_wall 0AFF:0F10 (kid.c) | else-branch d = 15 − d | d −= 32 |
| G | tile_is_wall_kind 0FB3:290A (tiles.c) | {0x14, 2, 7, 0x19} | also 0x2B. Tile 0x2B occurs once: level 13, room 4, row 1 col 1 (the shadow's room) |
| G | tile_passable_2f800, 2D3E:2420 (guard.c) | no wall test (walls pass) | `!tile_is_wall_kind(t) && !(gate < 0x70)` first. Used by the sword draw, sword mode and guard AI |
| G | char_on_floor 0AFF:18C0 (game.c, render_sprites.c) | in tile 6, y = 63·row + 0x39 always | only on frames with flag 0x40 |
| G | level_edge_tile 0AFF:0174 (tiles.c) | level 9 room 16: the outside is a wall (20) | empty (0) |
| G | tile_col_in_drawn_room 0AFF:1144 | left and right neighbours only | also the diagonal neighbours (DS:5CC1..5CC4) |
| G | check_collisions (collision.c) | skipped for f19 0x4E | for 0x4E or 0x46 |
| G | control_runjump OVL01 031596 (control.c) | step dx 0xB for param 4 (9 for 100); tile loop counts `!empty && test(mod, t)`; window < −0x13 | dx 9 for both; `test(t) \|\| t == 0x0B`; window < −0x14 |
| G | sword_seq 2FDF:19D4 (control.c:59) | only the tile-4 exception | also a loose floor (0x0B) counts as passable, in front and behind |
| G | bump_fall 0327A8 (collision.c) | no load_fram_det_col() at the end | load_fram_det_col() |
| G | word_6140 (the noise flag), 03294E (bump_fall and 2 more callers) | set for every character | not for charid 1 |
| G | control() end (control.c:176) | frame 44 → ctrl1_forward = 0 only; frame 26 → nothing | frame 44 or 26 → control_rest() (all four ctrl1 edges cleared) |
| G | control_standing auto-turn (control.c:424) | any opponent | not when Opp.charid == 6 |
| G | control_standing_turn 2FDF:0A5A (control.c:316): turn_count on kinds 2 and 6 | everywhere on those kinds | except rooms 1/2 of level 14 |
| G | die_at_bottom 0AFF:0DC4 (kid.c) | action = 1 only for charid 0 | for every character |
| G | landing 02FFE0, charid 1 (the spirit) | fall ≥ 0x16: the medium-landing check without hp loss | always the soft landing |
| G | landing sound 02FE86, charid 2 | – | also calls die_at_bottom when curr_row > 5 |
| G | check_gate_guard 3212:0C88 / check_gate_push 3212:0D58 (collision.c) | dead characters are pushed out of a gate too | only while alive (alive < 0) |
| G | check_guard_bumped 3212:0E8E (collision.c) | – | also requires f19 != 0x46 |
| G | tick_main 169B:05E0 (tick.c) | sword hits and hurt run even with drawn_room 0 | need drawn_room != 0 |
| G | play_all_chars 169B:07EC (kid.c) | no drawn_room == 0 guard; on level kind 6, a Jaffar leaving the room list mid-loop shifts the slots | returns on room 0; kind 6: n--, i--, no save_char |
| G | mobs: a closer button (gate_trigger 1375:11A6 case 6, mobs.c) | ignored while the gate has an animation; returns 4 only if pos != 0 | forces state 4 unless already 4; ignored on a dead frame with alive ≥ 2 |
| G | press_button 1375:15D4, link timer 0x1F (mobs.c) | triggers the links only | also stores mod \| 0x800 in the tile |
| G | close_entrance 169B:034A (level.c) | only in the start room | also anywhere on levels 6 and 7 |
| G | level_sword 1286:0D06 (level.c) | level 8 with Kid.room == 9 → 1 | level 7/8 → 2 |

### 3.2 Guards, fights, dead characters

| | where | IR | 1.0 / 1.1 |
|---|---|---|---|
| G | guard_after_seq 366C:0052 (guard.c) | alive++ without a cap. **Oracle**: a dead guard counts to 0x7F, spends one tick at 0x80 ("alive"), then char_control_step resets it and the cycle repeats every 128 ticks | stops at 0x14 (oracle: confirmed) |
| G | check_strike 2D3E:1FB0 (fight.c) | the spirit (charid 1) can strike the prince | `charid 1 && Opp.charid 0 → return` |
| G | 02EE8A (hit and knock-back) | frame-flag test reads another record's frame (DS:5B19), a bug | Char.frame |
| G | wake_in_range 366C:0E4A (skeleton.c) | – | also requires Kid.charid == 0 |
| G | charid-10 hit 366C:15CC (called from check_strike's 366C:1580) | revive timer 0xF0 | 0x1E0 (SDLPoP2 still stubs 1580) |
| G | heads 366C (heads.c:153) | seq 0x9C when `lim > d && f19 == 0x93` | `(row differs \|\| lim > d) && f19 == 0x93` |
| G | char_dies 2D3E:1D74 (fight.c): the prince's death music by the killer | killer charid 6 → 2 | 0x11 |
| G | 02FC2C (first free slot among the room's characters) | in Char.room | in drawn_room |
| – | char_control_step 0AFF:1258 | alive++ without a cap | capped at 0x14, but the code sits under alive < 6, so there is no difference |

### 3.3 The spirit (charid 1)

| | where | IR | 1.0 / 1.1 |
|---|---|---|---|
| V | seqtbl_offset_char and start_fall 0AFF:0AAA: shadow_hook_2f9a2 (spirit palette) | not called | called for charid 1 |
| V | 0823:1008 → ovl_2f9f2 (the hp colours) | whenever Kid.charid == 1 | only on level kind 6 |
| V | turn_flash 2F86:01FC (spirit.c) | tests level_kind == 14 (never true), so it also flashes in rooms 1/2 of level 14 | no flash there (level_number 14) |
| G | 02FBA4, after loading the spirit | resets Char.room and DS:6B55 from a 2F86 query, plays sound 0x106 in rooms 7/8 of kind 6, ends without clear_char/loadkid | copies the record back, clear_char(), loadkid() |
| S | play_sound 1611:01C6, dead_char_sound 1611:0068 | the spirit makes sounds | silent for charid 1 |

### 3.4 Level-specific

| | level | where | IR | 1.0 / 1.1 |
|---|---|---|---|---|
| G | 2 (the sand-tile puzzle) | standing_frame 33FD:032A (kind1.c) | {0xF, 0x6D, 7, 0xB, 0xD, 0x26, 0x2C, 0x1C, 0x9E, 0xAA, 0xAB} | {0xF, 0x6D, 7, 0x2E, 0xB..0xE, 0x26, 0x2C, 0x1A, 0x9E, 0xAA, 0xAB} |
| G | 2 | anim_gate_kind1 33FD:0538 (the raft gate) | advances only while the tile is on screen | always |
| G | 5 (the bridge) | every "bridge rooms" test: guard AI, take_hp, sword mode, play_all_chars (≈10 places) | `level 5 && curr_row == 1 && room 10\|7\|12` | `curr_row != 0 && …` |
| G | 5 | sword mode (control.c:568) | no rtlink_0dd5 (the bridge end) case | goes to sword_actions at the bridge end |
| G | 5 | level reset 33FD:0C1A | also resets the sway and count (byte_2b78, word_693c) | only on room entry |
| G | 8 (the sword) | pick_up (items.c) | take_item(−1) always; the room-9 case has no word_2bb2 test and no dx moves; seq 0xED, sound 0xFE; always loads the images and sets byte_5cba | as SDLPoP2 (dx −14, then +2 in 1.1 or +6 in 1.0 when facing left) |
| G | 8 | the sword scene 37F0:007C (ruins.c) | no 1286:03B6(5,4); no word_2bb2 (IR has no such flag); room_load(9) even when cut short | as SDLPoP2 |
| G | 8 | starting the sword scene | in control_dead | in control_dispatch |
| G | 13 | shadow13.c (the shadow climbing, frame 0x5B) | – | then set_char_collision() when facing right |
| G | 13 | tile 0x2B | not a wall | a wall |
| G | 14 (Jaffars) | kid_exit_dir 2D3E:14DE (room.c) | no ovl_342b4 case | `(drawn_room 7\|8) && kind 6 && a Jaffar in seq 0xF3` → the living one's side |
| R | 14 | enter_room_chars (room.c:325) | – | a Jaffar entering room 6: direction = random(1) − 1 (an RNG draw) |
| R | 14 | anim_tile2a 33FD:19B4 (final.c) | rearm delay random(0x28) + 0x1E | + 0x50 |
| G | 14 | guard_appears 33FD:01BA | rec->f04 = 7 | 5 |
| G | 14 | jaffar_room6 33FD:101A | much simpler: acts only when facing the same way as the prince. It has no frame-0xF condition and no Kid-state guards, never turns round, has no Kid frame 0x91 case, and no frame 0x6D → 0x31 | as SDLPoP2 |
| G | 14 | jaffar 33FD:1186 | chase also needs Opp.hp < 3 && Opp.f19 != 0xF2 | chase = Opp.f10 == 0xFF |
| G | 14 | jaffar_chase 33FD:0794, can_cast 33FD:08C0 | no Opp.f19 == 0xF1 tests | step and cast blocked while Opp.f19 == 0xF1 |
| G | 14 | to_wp2 33FD:0D72 | cols 0xC..0xD, frame 0xB → control_runjump(4) | cols ≥ 0xB, x < col_x_left[2] + 0x24, f19 != 4, frame 7\|0xB → dx +2, control_runjump(4) |
| G | 14 | jaffar_leaves_room6 33FD:1416 | does not clear rec->w15 | clears it |
| G | 14 | ovl_kind6_char 33FD:1CEA (a fireball kills a Jaffar) | no next_room = Char.room; sound first; music 0x10B | next_room = Char.room |
| S | 14 | kind6_tick 33FD:03C6, room hook 33FD:1708 (room music) | different conditions (no word_2bb4, no alive or counter tests) | as SDLPoP2 |
| S | 1, 5, 8, 9 | sea walk 33FD:0240; lever room 37F0:0000; room hook 0x22 (sword room music, no flags); room hook 0x14/0x15 (level 9, no column test) | – | as SDLPoP2 |

### 3.5 RNG, clocks and timing

| | where | IR | 1.0 / 1.1 |
|---|---|---|---|
| R | the game tick (169B:0A56) | 16-bit | 32-bit. It feeds every `tick % n` test: spawn_ok % 3 (2D3E:0CF8), the spirit flame % 9, the palette rotations % 3, the demo timing; they differ after 65,535 ticks |
| R | word_2ba4, the lateness counter (169B:05A1, game.c) | a flag (late → 1, on time → 0) | counts up to 0x14 and down. It gates torch redraws, palettes and the level-14 tile timers' random draws |
| – | the two RNG changes on level 14 (above) | | |

### 3.6 Game flow

| | where | IR | 1.0 / 1.1 |
|---|---|---|---|
| U | story_scene 0AAC:0120 (level.c) | the copy-protection scene only after level 2 | also before the first level of a game started at level 3 or later |
| U | level_kind_reset 169B:0FB4 | kind 4: DS:2BAE only; kind 6: nothing | kind 4: 2BAE/2BB0/2BB2; kind 6: 2BB4 |
| U | feather fall 0823:13C4 | plays 0x69 over the music | stops the level music, word_087e = −1 |
| U | hotkeys 0823:02BE | restoring a game allowed during the demo | refused |
| U | IR at start-up (oracle) | "Digital sounds will not be played because there is no extended or expanded memory available. Press any key" | – |
| U | cheat 'k' | charid 10's timer 0xF0 | 0x1E0 |

### 3.7 Drawing (V)

- **Description objects.** IR's CUST data puts the description objects at other indices, and the code follows:
  - the level-1 sea hand uses objects 0x38..0x3C (DS:13E8 table, render_hooks.c:89) and descriptor block 0x3E+;
    1.0 uses 0x1D..0x21 and 0x15+;
  - the hand itself is drawn through per-room object choices (room 0x13: 0x27/0x2F, room 0x10: 0x30/0x3D/0x45)
    instead of being moved by (+0xC, −0x1E).
- **Other drawing and palette changes:** the torch redraw parity (1375:08D0); the level-14 room-hook palette loads;
  0FB3:0984 / 14D4 / 1C32 and 0AFF:15AE / 18F2; a temple value `*(G_1070+0x48)` = 0xFFFF in IR (0 in 1.0), still
  to identify.
- **NIS player (OVL00).** A handful of script constants differ (a 4 s wait of 0xF0 ticks becomes 0x12C, wait cue
  0x64 becomes 0x63, the sultan's-daughter scene parameters). The scenes' content comes from each version's own NIS
  data.

### 3.8 Sound (S)

- 1.1 dropped FRAGSND.DAT (fragment sound) and TANDYSND.DAT (Tandy). IR and 1.0 load them.
- Footsteps (seq_sound 0AFF:0716) need `(DS:2077 & 3) == 0` in IR.
- The level entrance sound 0x1A (169B:0798) plays even when the prince is not standing.
- Sound-driver start-up differs: 2797:0260, and the IR memory check above.

## 4. Data (IR → 1.0; 1.0 = 1.1)

- **SEQUENCE.DAT (14 sequences):**
  - one extra hold of the last frame in 1.0: 12, 53, 117, 121, 169 (0xA9), 191–195 (the dead guards' poses), 209;
  - 22 (0x16, crushed): IR has an extra DY(−2);
  - 197 (0xC5): 1.0's loop is 21 frames against IR's 15;
  - 243 (0xF3, a Jaffar dies): IR `347 348 349 350 DX(2) 351 DX(−8) 352`; 1.0 `347 348 DX(−1) 349 350 351 352`.
- **PRINCE.DAT, levels 3–14** (resources 2002–2013) and 2027 (level 8's second part):
  - potion kinds throughout;
  - torch phases and ambient-sound groups throughout;
  - re-laid rooms: L3 room 10; L5 room 2 (loose floor → wall); L8 room 17 and rooms 1/6; L10 room 19 (gate → wall);
    L12 (8 torches → floor, rooms 18/23); L13 room 18; L14 (room 2, four potions in rooms 7/8); 2027 room 11;
  - guards moved or changed: L3, L4, L5, L7, L8, L9, L12, 2027;
  - door-link blocks (level + 0x10E0);
  - one spawn byte (L10).

  Full list: `~/pop2dec/versions/leveldiff_ir.txt`.
- **PRINCE.DAT, other resources:**
  - demos 25–27: three level-1 demos in IR; levels 1, 4 and 8 in 1.0;
  - TXT4 10, the cheat word;
  - TXT4 1011, a credits page ("Special thanks to the Brøderbund Quality Assurance Department"), is 1.0 only: IR's
    1011 is 1.0's 1012;
  - TXT4 8011 "Reloads", 1.0 only;
  - SHAP 1011 and 1246 differ by a pixel.
- **Graphics:** CAVERNS.DAT (SHAP 3805), FINAL.DAT (CUST 4150/4175, 5 shapes), ROOFTOPS.DAT (CUST 4375/4450,
  7 shapes), TRANS.DAT (PALT 4211/25010, SHAP 4210–4213).
- **NIS files:** NIS.DAT (STRL 26000); NIS3VC/NISIBM/NISMIDI (SND 31021 only in IR; 25009, 25015, 26019, 28100,
  30000, 30010, 31020 differ).
- **Sounds:** IBMSND, MIDISND and TANDYSND (SND 10267 only in IR; 10198, 10199, 10254, 10255 only in 1.0; about ten
  differ), FRAGSND (SND 30500 only in IR).

Resource-level table: `~/pop2dec/versions/datafiles_diff.txt`.

## 5. Implementation plan

### 5.1 The version, and how it is chosen

- Add a `game_version` setting with the values `ir`, `1.0` and `1.1`. Its default, `auto`, is detected from the
  game directory:
  - PRINCE.EXE's md5 identifies all three originals;
  - otherwise PRINCE.DAT's md5 separates IR (9264767b…) from 1.0/1.1 (6b304016…).
- Carry the value through `pop2_init` into the core, the SDL frontend's settings menu, the Chimera core options,
  and a JaffarPlus/quickerSDLPoP2 config key.
- A single global `game_ver` (enum V_IR, V_10, V_11) is read by the code paths.
- Because 1.0 and 1.1 share the data, allow `1.0` with a 1.1 install. The static DS tables are the same; they only
  sit at other offsets (§5.3). IR needs IR's own files; refuse IR with 1.x data, and 1.x with IR data.

### 5.2 Data

- Load the version's own files; nothing needs converting. The level format, sequence format and resource container
  are the same.
- Check what depends on resource ids:
  - the credits (TXT4 1011/1012);
  - the "Reloads" string (8011; IR has none, so hide or replace that menu line);
  - sound ids that exist in only one version (IR's 10267, 31021, 30500; 1.0's 10198, 10199, 10254, 10255). A
    missing resource must stay silent, never crash.
- The cheat word comes from TXT4 10, so it follows the data automatically. Check that SDLPoP2 reads it from there.

### 5.3 What SDLPoP2 reads from PRINCE.EXE

Today these reads use 1.1's fixed offsets:

| read | where |
|---|---|
| the DS image | core.c: file 0x3CE40, 0x27BF bytes |
| the font | text.c: file 0x3B220 |
| the EXE tables | glue.c: glue_load_exe_tables |
| about 70 static tables through ds_ptr / ds_byte / ds_word | glue_load_ds_tables, guard, mobs, caverns, heads, blades, the render hooks, settings.c DS:1BB6 / 13D0 |

The plan:

1. **A per-version location table** for the file offset of DS:0 and the static length:
   - 1.1: 0x3CE40;
   - 1.0: 0x45050;
   - IR: 0x44540.

   The data descriptor is stored raw in all three. Also add per-version entries for the font and for the EXE
   tables.
2. **The DS offsets.** SDLPoP2 keeps addressing DS by 1.1 offsets (the canonical layout, which matches all the
   existing comments and captures). A generated per-version translation table maps 1.1 offset → version offset.
   - The table is piecewise: the shifts are −0x38/−0x20/−0xC/−0xE for 1.0, and −0x3C…+0x16E for IR.
   - Build it from `~/pop2dec/versions/dsmap_*.json` (every code reference) plus the static-table search. All but
     11 of the tables SDLPoP2 uses were found byte-identical in 1.0 and IR.
   - The exceptions are near-pointer tables (DS:0670, 06BC, 06D2, 07F2, 07FC, 0806, 091E, 092C, 15EA, 15F0): their
     *pointers* need the same translation.
   - IR's DS:13E8 table has other values (the description indices of §3.7), which is correct for IR's data.
   - The simplest form is to load the version's DS image, then build a 1.1-layout image from it through the table,
     once at start-up. `ds_img` stays in the 1.1 layout, and no call site changes.
3. **state.c's DS map** (save states, oracle comparisons) gets the same translation for oracle work on IR and 1.0.
   Save states store the version and refuse a mismatch.

### 5.4 The code paths

- Each difference in §2 and §3 becomes a `game_ver` test at the site named in the tables. Group them per module:
  control.c, kid.c, collision.c, tiles.c, guard.c, fight.c, room.c, mobs.c, spirit.c, kind1.c, final.c,
  bridge5.c, ruins.c, items.c, level.c, tick.c, game.c, skeleton.c, heads.c, shadow13.c, render_*.c, sound.c.
- Suggested helpers:
  - `V_IR_ONLY(...)` / `V_1X(...)` for the one-line conditions;
  - small version tables for the constants (start delays, dx values, frame sets, wall kinds).
- New code needed:
  - 366C:08F0 (1.0's guard_armed step);
  - the IR versions of jaffar_room6 and to_wp2;
  - IR's sea-hand drawing;
  - 366C:1580 / 15CC (charid 10). SDLPoP2 stubs these in all versions today.
- State that 1.1 has and IR lacks (word_2bb0/2bb2/2bb4, the 1.0 checkpoint of DS:6646) stays at zero in IR.
- IR's 16-bit tick: keep one 32-bit counter, and mask it to 16 bits wherever IR reads it.
- The NEWBUMP switch (IR and 1.0 only): a setting, or a command-line flag in the SDL frontend. The default (off)
  behaves like 1.1.

### 5.5 Presentation

- Drawing follows each version's data once the DS tables are translated. IR's sea hand (OVL02) and the palette
  hooks need their IR paths.
- The NIS player reads each version's NIS data. The few OVL00 constants (waits, cues) become per-version values.
- The IR start-up memory message never needs to show: the reconstruction always has memory.
- Sound: the drivers are SDLPoP2's own. The music choices of §3.4 and §3.8 are per-version conditions.
  Tandy/fragment sound (IR and 1.0) can come later, as an optional device.

### 5.6 Copy protection

IR asks its question only after level 2. With a LEVELn start at level 3 or later, it never asks. 1.0 and 1.1 ask
before the first level of such a game.

SDLPoP2's publication rule already settles this: there is no way to skip the protection, and level skips must show
it from level 3 on. IR mode therefore also asks up front on a level-3+ start, as 1.x does. This is a documented
deviation from IR.

### 5.7 Tests

- **Oracle disks for IR and 1.0.** Game folder plus the oracle CONFIG.DAT, made with `fat16put.py`. This session
  already built and ran them (see NOTES.md).
- **Capture scripts** (gen_ticks.py): per-version probe addresses. Translate the 1.1 CS:IP probes through the
  function map, e.g. play_kid_frame at 169B:0692 (1.1) is 16A6:06B8 (1.0) and 1679:0682 (IR); translate the DS
  samples through §5.3. IR also needs a key press to dismiss its start-up message.
- **The existing ticktest/roomtest** gain a `--version`. Record a small capture set per version:
  - level 1 walk and fight;
  - the dead guard's 128-tick cycle (IR);
  - level 2 puzzle frames;
  - a level-5 bridge row;
  - level 14 Jaffars;
  - x < 14 at a room edge;
  - a closer button during a gate animation.
- CI runs all three.

### 5.8 JaffarPlus / quickerSDLPoP2

- Regenerate the single-header core with the version as a runtime parameter, from the script, and hash it.
- The version changes the state space, so the hash includes it and the `.jaffar` configs name it.

### 5.9 Order of work

1. **Plumbing:** the setting, detection, the DS/EXE location table, the DS translation (5.1–5.3). 1.1 behaviour must
   stay bit-identical: all existing tests pass.
2. **1.0**, about 10 conditions (§2) plus 366C:08F0. Oracle captures on the 1.0 disk.
3. **IR, gameplay and RNG rows** (§3.1–3.6), module by module. After each module, oracle captures on the IR disk.
4. **IR, presentation** (§3.7, 3.8): the sea hand, the palettes, NIS constants, sounds.
5. **quickerSDLPoP2 and JaffarPlus**, then docs (this file becomes the reference), then the CI matrix.

## 6. Still open

- Temple OVL07 3579:02FE: `*(G_1070+0x48)` = 0xFFFF in IR (0 in 1.0). Probably drawing; to identify.
- Level-14 room-hook palette loads (IR's two extra loads): drawing, to confirm against the start of 1.0's hook.
- The level-5 bridge-row test (`curr_row == 1` in IR, `!= 0` in 1.x) sits in about ten places. List each one again
  when implementing; the alignment found them function by function.
- The 1.0 do_startpos branch (`DS:0998 == 0 → hp 4`): not taken on a new game. Check whether a restored game or
  another path reaches it.

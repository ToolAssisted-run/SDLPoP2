# SDLPoP2 findings (complete log)

Everything learned while reconstructing Prince of Persia 2 (DOS 1.0, "Prince of Persia Collection Limited Edition" CD)
into C. Addresses are runtime `SEG:OFF` as the game runs under DOSBox-X (DS = SS = 3B25, physical DS:0 = 0x3B250).
Overlays (RTLink) are named by the segment they load at; the same segment holds different overlays per level kind.
`docs/ENGINE.md` is the structured overview; this file keeps every finding, including dead ends and open questions.
Kept up to date as work goes on (newest findings are also in the dated log at the end).

---------------------------------------------------------------------------------------------------------------------

## 1. Method and tools

### Oracle
- Chimera + DOSBox-X headless (`~/chimera-oracle/chimera-core-dosbox-x`, branch `pop2-tracer`), driver
  `build/meson-native/oracle-run --workdir W --rom pop.hdd --autoexec c: --autoexec 'cd prince2' --autoexec CMD
  --script S --events E`.
- Script commands: `key FRAME name 0|1`, `probe SEG OFF label [PHYS LEN]` (register file + stack + a memory sample
  up to 64 KiB at each hit), `watch PHYS LEN label` (a write to memory, with the writing CS:IP), `poke FRAME PHYS HEX`,
  `ram FRAME file` (full 640 KiB dump), `inject` (call injection), `trace`, `end FRAME`, and (added 2026-09-23)
  `probepoke LABEL HIT PHYS HEX`: write memory at the HIT-th hit of a probe, used for tick-exact input (tracer commit
  e8f6906, `tracer_probe_poke`).
- Local runner: `~/pop2dec/oracle/cap.sh NAME "prince yippeeyahoo LEVELn"` (30-60 s per run; 4 in parallel on the
  4-core box). Level 1 without cheat: `prince` + ESC at frame 1300.
- The boot is deterministic: every `LEVELn` start has the same RNG seed at level start, 0x3528860F.
- Copy protection (manual symbol) appears when starting past level 2 via the cheat: answered by TAB x2 + ENTER at
  frames 300..344.
- Probes used everywhere: `169B:05E0 ds_tick`, `169B:064F ds_postroom` (after the room switch, before tick_tail),
  `169B:00F5 ls_a` (after a level load), `169B:0135 ls_b`, `2FDF:048C kc_ctrl/kc_ctrl1` (control() inputs), each with
  a DS:2900..6C00 sample (0x4300 bytes).

### Captures
- `gen_e2e.py LEVEL SEED [KEYS]`: random key runs (E<level>_<seed>). `gen_turns.py`: alternating turns (T*).
- `tools/explore.c`: Go-Explore over the C core (cells = room,row,col; restart from rarely tried cells with random
  held inputs); `tools/plan2script.py`: turns a per-tick plan into a script that pokes the key table DS:1D00 (phys
  3CF50) and BIOS shift flags (0040:0017, phys 417) at each tick's start. The oracle then follows the plan exactly
  (X4_1: 14 rooms, 1003 ticks, all fields identical).

### Tests (`tests/run_all.sh`)
- snaptest (tick/chars/room/between modes over DS snapshots), inputtest, e2e (free-running whole runs from the
  captured post-load state and from a cold start), coretest (the API: random play + savestate round trips).
- e2e `E2E_STRICT=1` compares every mapped field (except DS:2B68 palette and DS:2B9A ambient sound).
- e2e resolves only input/platform ambiguities from the capture, never logic: same-frame key timing and answers to
  "is sound n playing" are retried (state rollback) when the first choice disagrees with the capture; ambient-sound
  random draws are caught up (<= 4) at each tick start; the lateness meter follows the capture; story scenes resync.

### Reading code
- Ghidra decompilation is unreliable in the overlays (lost branches, bad jump tables): transcription is done from r2
  disassembly (`r2 -a x86 -b 16 -m 0 -q -c 'e asm.lines=false; e asm.comments=false; e asm.flags=false; pD N'` over a
  RAM slice; `~/pop2dec/work/asm.py FILE START END`).
- Overlays in a RAM dump are the ones loaded at that moment; RTLink loads on first call (a later dump may be needed,
  e.g. OVL14 at 37F0 only after level 8 room 9 is entered). Identify an overlay by comparing its bytes with
  `~/pop2dec/image/ovlNN.bin`.
- RTLink thunks live in 2A31 (`call 0x558; ljmp SEG:OFF`); resident code calls overlay routines through them or via
  far pointers in DS tables.
- Many byte compares are signed (jge/jl on 0xFF timers): always check the jcc.

---------------------------------------------------------------------------------------------------------------------

## 2. Memory map (DS = 3B25)

### Static data (PRINCE.EXE file offset 0x3CE40 = DS:0, 0x27BF bytes; the rest of DS is zero at start)
| DS | what |
|---|---|
| 0096 / 00A2 | level type -> charid / charid -> type |
| 016A | story/timer stage (-1 until the first scene after level 3; the clock runs when >= 0) |
| 01AA.. (0x1C records) | per-background room hooks (far pointers; see 5.12) |
| 01AC | pointer to the drawn room's background description (CUST resource) |
| 02FE[35] | background id -> hook record (0x2E2 = none) |
| 0344 | current hook record |
| 0366 | copy protection answered (set by 0D5E:1288) |
| 05AC | per kind: first image of the second scenery bank |
| 0654 | kind tick far pointers (kind 0..7): 1 33FD:0170, 2 347C:0FC4, 3 33FD:0BEA, 4 none, 5 33FD:0232, 6 33FD:03C6 |
| 0672 | guard file per level type: GUARD, FLAME, SKELETON, GUARD, -, HEAD, HEAD, BIRD, HEAD, JINNEE |
| 06BC | per guard type: pointer to the first image of the second bank |
| 0764/076C/0776 | exit door / door close / door open speeds |
| 0810 | falling-object floor depth per type |
| 087E / 0880 | queued effect sound / music (play_sound 1611:01C6 / 1611:01A8) |
| 0882 / 0884 | sounds playing (death / level end checks) |
| 0996 / 0998 | level-13 flag / current level number |
| 0CF8 / 0CFA | direction tables {-1,+1} front, {+1,-1} behind |
| 0CFC..0D22 | column x: 130 + 32*col for cols -5..14 (DS:0D06 = col 0); DS:0D08 right edges; out-of-range columns read the neighbouring data (the core backs the tables with the DS image) |
| 0D26 | 32*col (tile left); 0D40 row floor y (66, 129, 192) |
| 0D5D | sound priorities (3 bytes per sound) |
| 10C2 | cheat word given |
| 13D0 | refract timers |
| 1774/177C, 176C/1784 | blade boxes and tables |
| 1908/18E6/192A | slab draw tables |
| 1B84/1B90/1B9C | head bite tables |
| 1BB4 | handle: guard file resource 755 (head attachment offsets, types 5/6) |
| 1BB6 | guard AI probability tables (12 words each: strike, restrike, block, impblock, advance) |
| 1D00 | key table by physical position (keypad 54..5E, WER/SDF/XCV 1E..3C, UIO/JKL/M,. 23..41; shifts 37/43, ctrl 2A, alt 45) |
| 24DC[4] | timer countdowns (IRQ0); 24DE = the frame timer |

### Game state (BSS)
| DS | what |
|---|---|
| 2900.. | falling objects (mobs) 13 bytes each at 293E, count 6186 |
| 2B24 | collision history (coll) |
| 2B68 | level-1 palette state (drawing only) |
| 2B6A / 2B6B | level 2: puzzle answer / column last stood on |
| 2B6C | collapsing floor pointers (near heap) |
| 2B7A | RNG seed (LCG x*0x343FD+0x269EC3; random_2751(n) = (seed>>16) % (n+1)) |
| 2B90/2B92 | redraw requests (whole screen / message only) |
| 2B96 | cleared by the OVL01 initialisers |
| 2B98 | sound on (ambient) |
| 2B9A..2BB7 | "room 0" tiles, overlapping real variables: 2B9A ambient sound, 2BA2 input device, 2BA4 lateness meter, 2BA6, 2BA8 demo, 2BAA, 2BAE/2BB0/2BB2 kind-4 flags (2BB0 level-8 sword-room music played, 2BB2 sword taken), 2BB4 level-14 room-8 sound |
| 2BB8 | level (tiles 28x30 at +0, attrs 29x30 dwords at +0x348, links at +0x17BC..., room records at +0x1867 (0x74 each: nchars + 5 x 23-byte records), spawns +0x26D7 (0x22 per room), header: kind +0x1845, type +0x1846, number +0x1847, nrooms +0x1840, start room/tile/dir +0x1860.., moving walls +0x1852 (DS:440A)) |
| 43FD / 43FF | level kind / number (inside the level header) |
| 5AB6 Char, 5AF6 Opp, 5B36 Kid, 5B76 chars[5] | 64-byte character records (see 3) |
| 5CB6 | demo/keep-level flag; 5CB8 death music; 5CBA sword type; 5CBB entrance sound countdown; 5CBE turn counter |
| 5CC6 | cur_frame (7 bytes) |
| 5CD0/5CDA/5CDC | clock message flags / message countdown and its start (0x258 after a death) |
| 5CD2 / 5CEA | minutes left (75) / ticks per minute (0x2CF) |
| 5CD4..5CD6 | control_x/y/shift |
| 5CD8 | restart requested |
| 5CDE.. | drawn room and neighbours L R A B AL AR BL BR |
| 5CE8 | cutscene frame timer |
| 5CEC | level counter (next level) |
| 5CEE / 5CCE | new-room redraw / flip redraw |
| 5CF0 | animation modifier (32-bit copy of the current tile's attribute) |
| 5D04 | tick counter |
| 5D08 | guard palette slots |
| 5D36 | feather fall countdown (0xE4) |
| 5D38 | upside-down countdown (0x438) |
| 5D3A.. (0x17 each), 60F8 count | the frame's sprite list (x, y, chtab, image, layer = charid, ...) |
| 60FC/60FE/6100/6102 | obj_x / obj_y / obj_id / obj_chtab |
| 6103..610B | clip rect |
| 6110 | sword table handle |
| 6112.. | image height/width, char_x_left/right, coll variants, char_top_y |
| 6122..6126 | ctrl1 forward/backward/up/down/shift (-1 new press, 0, 1 consumed) |
| 6128 | kid ctrl1 saved |
| 612E..6138 | current tile context: tile, modifier (word), tilepos, room, tile_col, tile_row, char cols/rows |
| 613A / 613C | room attr / tile pointers |
| 613E | knock; 6140 word_6140 |
| 6662 | cur_mob (13 bytes); 66C6 index |
| 6670 / 6676 | trob count / trobs (tilepos, room, state, tile) |
| 6672 | cur_trob |
| 68EA.. | misc guard/fight counters |
| 6936/6937/6938/693A | level 1: char grabbing (index, stage, x, action 9) |
| 693E, 6940..6947 | level 5 water (reset by 33FD:0C1A) |
| 6948 / 6952 | previous / current collision flags |
| 6B6C | starting level (menu 1, or LEVELn); 6B6D next room; 6B70 obj_xl; 6B71 start hp; 6B72 anim tile |

---------------------------------------------------------------------------------------------------------------------

## 3. Characters
- Record (64 bytes): +00 index (10 = the prince inside control), +01 direction (0 right, -1 left, 0x56 gone),
  +02 x, +04 y, +06 charid, +07 frame (16-bit), +09 col, +0A row, +0B action, +0C fall_x, +0D fall_y, +0E room,
  +0F flag (victory music pending; "moved"), +10 sword drawn, +11 alive (<0 alive, counts up dead), +12 hp,
  +13 max hp, +14 hp delta, +15 seq pos, +17 seq id, +19 last seq started, +1B..22 box, +23 sight, +24 state,
  +26/+28 drawn x/y, +2C.. previous box, +34 palette slot (word; 2 prince, 4/8 guards, 8 flash/spirit),
  +36/37 draw-only, +38 skill, +39 opponent, +3A shake.
- Charids: 0 prince, 1 shadow/spirit, 2 guard, 4 skeleton, 6 ?, 7/8 heads, 10 flame, 11 beast (charid-11 creature),
  12 prince-in-record.
- Frames: 7-byte entries {image, sword, dx, dy, flags (low 5 bits weight)}; kid table in PRINCE.EXE (file 0x3A500),
  guards FRAM 750 of the guard file, indexed from frame 149 (offset -0x413); shifts +0x46 (0x66..0x6A for 2/4/10/12),
  +0x2B below 0xB7 for charid 8. A frame >= 0x400 (an opcode stored as frame) reads past the table in DOS; the core
  reads zeros.
- Sequences (SEQUENCE.DAT, SQES): PoP1 opcodes widened to words (see SEQUENCE.md). FFEF = clear char (0AFF:1BA2),
  which ends the step WITHOUT storing a frame (0AFF:0428); FFEC stores the opcode as frame and stops.
- clear_char (0AFF:1BA2 -> 1BCE): the hp display call 0FB3:25D4 first reloads Char from chars[index] (dropping the
  tick's sequence progress), then direction 0x56, action/alive/hp 0, the prince's opponent re-picked if it was this
  one (not for index 0), room 0. Level 14 room 6 seq 0xF0 also calls 33FD:1416 (not reconstructed).
- char_control_step (0AFF:1258): alive <0 & hp 0 -> alive 0; alive <6 with +0F: count to 0x14 then clear +0F;
  alive >= 6: victory music 1611:0068 (charid 2 at alive 6: 0xD1; 7/8 if sound 0x2768 not playing: 0x93; 10 if 0x27D3
  not playing: 0xC0; 11 clears +0F; others 0x4A when the prince lives, seq 0x78, alive > 4 and none of 0x275C/0x2771/
  0x2772 plays), skipped when a live guard is in the prince's room (2FDF:1DD4) or a spawn matches (2D3E:0C66) or on
  kind 5; +0F cleared when played.

## 4. Tick, frame and input
- Tick (169B:05E0): falling floors (1375:1A52), tile animations (1375:0006), skeleton wake, guard spawns, guards see
  kid (169B:0FF0), play_kid_frame (169B:0692), play_all_chars (169B:07EC), sword hits/hurt, reload the prince's opponent
  (1611:0164: load_char(Kid.opp_index = DS:5B6F) unless 0xFF; sound 0xC when Kid or it is on frame 0xA7 and its type
  is not 7/8; Char is left holding the opponent for the rest of the tick), checkpoints (169B:0DB4),
  kind tick (169B:11E2 via DS:0654), apply hp (0823:1008), kid left room (2D3E:108A), switch room (0823:0E72);
  then tick_tail: chars fell below (2D3E:0FB0), clock (0823:0D5A), restart prompt when out of time.
- Frame (169B:0505): frame_begin (169B:0BA6: frame timer DS:24DE = 5 or 6 (Kid+0x10 == 1)), tick, level end /
  restart checks, frame_end (169B:0A30: tick++, redraws (drawing pass 0FB3:12F4 = mobs 1375:1FBA, prince 0993:07F8,
  characters 0993:08D0, room), countdowns), then 169B:05A1: DS:2BA4 lateness meter (frame timer still running ->
  on time, meter -1 and wait (2797:0134); ran out -> late, meter +1 up to 0x14), then 18C8:0008 (cheat keys).
- Ticks are ~5.86 video frames apart (a ~60 Hz timer vs 70.086 Hz frames), plus lag on room redraws.
- Input: read_input (0823:10A0) keyboard (0823:1178) or joystick (0823:110C); hotkeys 0823:02BE: any key or shift
  restarts a dead prince (alive > 6, minutes left) or a demo; the keystroke queue is the C library's (194C:9858/9A0F,
  8 deep, pumped from BIOS each frame; DOSBox repeat 500 ms then 33 ms).
- A dead prince's controls are zeroed before control(); same-frame keys cannot be read from them.

## 5. Mechanics by subsystem

### 5.1 Game start and level load
- 169B:0006 (game start): DS:2B96 = 0 (four OVL01 initialisers via 169B:018E), 5CDC/5CDA/5CE8/5CD0 = 0; unless DS:5CB6:
  75 minutes, 0x2CF ticks, start hp 3 (cheat + LEVELn: the level number, 3..12; before the load it is 0 so 3).
- DS:6B6C: 0823:0192 parses LEVELn (clamped 1..14); the menu sets 1.
- Level load 1286:01F2 (level changed or a scene played; otherwise 1286:0332 reloads): resource 0x7CF+n in PRINCE.DAT
  (+0x14 with GAMEPLAY), 1286:05E2 (tile 7 loses attr bit 7), checkpoint restore (0D5E:11A0), 1286:0D06 (sword type
  DS:5CBA: 0xFF level 6, 2 levels 7/8, else 1; a far override at DS:5AB2 unused), 1286:0454 sword images/table
  (FRAM 1000, or 1200 for type 2), 1286:03B6 kind initialiser (kind 1 33FD:0380, kind 2 347C:0F24 & others via 2A31
  thunks, kind 5 33FD:005A: 6937=0, 2B68=6936=0xFF), kind 1 also 33FD:0324 (DS:14A0 = 0xFF).
- Level begin (169B:00F5..0135): record sequences restart, collision reset, kind reset (169B:0FB4: kind 3 floors
  freed, level 5 33FD:0C1A water reset; kind 4 DS:2BAE/2BB0/2BB2 = 0; kind 6 DS:2BB4 = 0), init kid, entrance closes.
- Story scenes (0AAC:000E/0120/0274): scene 0x64 = copy protection (0D5E:1288 sets DS:0366 = 1 once, unless demo);
  after levels 1 (9), 2 (0x64), 3 (0xA), 5 (1), 8 (2), 13 (3); from level 4 on the clock stage (DS:016A) scenes
  0x14+stage. Scenes (NIS) are not reconstructed; their residue is the guard palette slots (DS:5D08).

### 5.2 Tiles and animations
- Tile classes: empty (0FB3:28D4) 0,9,0x21,0x23,0x1B,0x25; wall (290A) 0x14,2,7,0x19,0x2B; floor (2810) = neither;
  loose (283C) 0xB,0xF,0x1A,0xC,0xD,0x17,0x18.
- Tile animation dispatch 1375:0096: a fixed jump table (1375:00C2, tile-4) into resident code and 33FD/347C/37F0
  offsets, so the handler depends on the kind's overlay. Kind 1: tile 4 33FD:0538 (gate that only opens), 0x1C/0x1D
  0658/06E4 (cycle 0..7 while visible), 0x1E 07CE (puzzle tiles).
- Buttons/links: link timers compared signed (0xFF idle = -1) in 1375:15D4/16BC; a first press adds the button's
  animation.
- Room entry animations (0823:0B78): 0x1C/1D/1F/25..28/2B added; 0xA and 0x13/0x20 random start; tile 2 trap; 0x17
  kind-3 floors; 0x1E kind 1 (33FD:0A18 mode 3); rooms 6..8 kind 6 with bit 0x1000 -> 33FD:1962/1B94.

### 5.3 Items (items.c)
- Pickup (2FDF:104C, standing handler on shift): a potion (0xA) or sword (0x16) under or in front; 2FDF:10BE steps up
  and crouches, then (crouched, seq 0x32 or shift pressed) takes it: sword seq 0x5B (level 8 room 9 without DS:2BB2:
  seq 0xED instead), potion seq 0x4E. 0AFF:1548: DS:27C0 = item (-1 sword, else potion kind+1), shift consumed, tile
  becomes floor (modifier kept per kind | 0xC000).
- Effects (0AFF:1954 at the sequence's opcode): sword clears +0x10; potions 0 heal 1, 1 life (+1 max up to 12, full),
  2 feather fall (DS:5D36 = 0xE4), 3 upside down, 4 poison (or back upright), 5 sound 0xE + seq 0x6F.

### 5.4 Turn counter and spirit (spirit.c; kinds 2 and 6, OVL01 2F86)
- A standing turn (2FDF:0ED9 -> 2F86:0078) counts in DS:5CBE when the last seq was the turn (seq 5), else restarts
  at 1. From the 4th: palette flash and each turn costs 1 hp and 1 max hp (dying -> seq 0x47). The 8th with > 4 hp
  leaves the body (2F86:0264: a room record charid 0, type 0xA, seq 0x47) and the prince continues as the spirit
  (charid 1, palette 8, seq 2); otherwise death. Flash frames: 2F86:01FC/0456. The standing handler's tail
  (2FDF:0C23) and turn-run (2FDF:0E18) reset a charid-0 prince's palette slot to 0 when not turning.
- The body (charid 0, frames 0xB4..0xB6; control 2FDF:09B2) drains the spirit: each tick the prince loses 1 hp (or 1
  max hp when that would kill) and the body 1 hp. Turns only count from 2FDF:0ED9 while chained: the counter resets
  when the prince stands (frame 0xF), and not in level 14's rooms 1/2.
- The spirit crouching within 0x20 px of its body (2F86:000A; distance 040C, 999 on another row, DS:0CFB by
  direction) lies into it (seq 0x47, f0f 0); on a dead frame with |distance| <= 1 it rejoins (04CE: the body's
  character is cleared, charid 0, seq 0xE7). The spirit dying (alive > 6, 0344) turns the body into the prince
  (index 0xA, the spirit's hp) and clears the body's character. 2F86:0142 / 0192 are sounds and palettes.

### 5.5 Temple (kind 2; OVL05 33FD, OVL07 347C, OVL10 366C)
- Blades (OVL05), torches, slabs (tile 0x1A -> mob type 10; settled slabs return to the ceiling in the drawing pass,
  347C:0C22: when the settle timer passes 0x10), skeletons (OVL10).
- Sliding walls (walls.c): tile 0x19 buttons (347C:0B3E) start a type-6 falling object at the tile (w7 -1 pulled
  back, 1..8 pushing out, 0 stopped, 0x64 shut; speed 1 against a wall). 07D4 moves it; 027A/056C push, stop or crush
  the prince (crushed: hp to 0, frame 0xB9; against a wall: seq 0x47); 0368 edges; 0412 collision rows; 0A80/0A0E/0B0A
  proximity for control, traps and 1375:14FC (AL/DL = prince's room/row). Enabled by DS:440A (level+0x1852), set on
  levels 10-13.

### 5.6 Ruins (kind 4; OVL05, OVL06 347C, OVL09 366C)
- Level-6 entrance, crumbling floors, beast (charid 11, 366C:11DA; record on leaving 366C:166A: resting -> random
  sleep 0x24..0x60 in the record; walking at a gap or wall turns and steps back), heads (charids 7/8, 366C:0E0A
  family). Head sight (00C4): a floor is checked when leaving a row (down from its lower half, up from its upper).
- Biting heads (f24 1) sit on the prince's last drawn sprite (366C:0002): the sprite list entry (chtab 2, layer 0)
  plus offsets from the guard file's resource 755 by the prince's image; x = sprite x + offset + 0x82.
- Tile 7 runs (347C:12C8/1226/12FA): facing a tile 7 (modifier low bits not 3) while standing/crouching opens the
  whole horizontal run of 7s (attr bit 0x80), across rooms.

### 5.7 Caverns (kind 3; OVL04 33FD)
- Rocks, collapsing floors (heap objects), traps (186A blade trap, tile 2, type-4 objects).

### 5.7b Level 5's water (OVL12 at 37F0, water.c)
- Rooms 7 | 10 | 12 form one row (37F0:0164: column -10 in room 7, +10 in room 12); tile 0x2C is water. RTLink
  thunks: 2A31:0DE9 -> 0000 (the tick, from OVL04 33FD:0BEA on level 5 in drawn rooms 10/7/12), 0DD5 -> 023C,
  0DDF -> 0286, 0DC1 -> 0426 (room hook 0x21 and rooms 7/12 without description), 0DCB -> 0588 (tile 0x2C anim),
  0DF3 -> 0742 (its start); direct: 37F0:03D2 (guards), 0574 (leave hook), 0782/08F6 (falling object type 0xB).
  Other 37F0 overlays: OVL11 (hook id 0: graphics), OVL13 (hook 0x20: graphics), OVL14 (level 8 room 9).
- Tick 0000: the prince (0194) and the drawn room's first character (0206) swim when on tile 0x2C (050C: y = row
  floor + DS:1C87[col]; on standing frames it bobs -1/+1 with the wave nibble of room 10 row 1). A prince in room 7
  (not f10 0xFF, col <= 5) while its gate (position 13) is closed/closing holds the gate's plate (OVL04 33FD:0000).
  Waves (0454): DS:2B78 = all when both swim, else bits around the swimmer's column; splash sound 0x43 by
  random(0x28 * n) while 0x2753 is silent. The plug (001E): both swimming within 3 columns count DS:693C up (else
  down, by 2 when not both); at 0x3C with the prince in columns 4..6, the other in 5..6 and the prince not on
  f19 0x55, DS:693E runs 1..3, then bubbles (094A: type-0xB objects from room 10's row-1 positions 17..13, tile
  cleared, attr 0xC000, size random(1): 5 steps small or 10 big, DS:693F+col) until none is left or the prince is
  on f19 0x55; DS:693E = -1 and he is pushed right 10 px at col <= 4.
- 023C: (guards/prince) the water's edge ahead: facing right at column < 2, or (035E) the opponent near the left
  edge with the gate down; facing left at column >= 8. In 366C:0CAA (advance) it means move forward (guard.c had
  it as stop). 0286: the skeleton's water rules (clear_char when falling outside; seq 0x65 at the right; turn at the
  edge). 03D2: room ahead (column < 3 facing right, > 3 facing left). Tile 0x2C anim (0588): frame 0/1 by
  random(3) where DS:2B78 has the column.

### 5.7c Level 13's shadow room (OVL13 at 37F0, shadow13.c)
- Room 4 (background 0x20): the kind-2 tick (347C:0FC4 -> 37F0:0236): walking left on row 1 past x 0xDA kills the
  prince (seq 0xE6, x 0xD2, DS:310C = 0x85); dying there (frame 0xB9, f24 != 0xC) once (DS:0996 1 -> -1) raises the
  shadow (02E4: a room-4 record, charid 1, 1 hp, seq 0xE7). The shadow's control (0078) walks/jumps to the prince
  and, lying on him (f24 0xD, frame 0xB9), merges (0000: prince full hp, f24 0xD). Restart is refused while a shadow
  is in room 4 (0823:050A -> 03CA). Tile 0x2B (040A): a flame counter. Hook 0x20 (0510/06E0): graphics.

### 5.8 Level 2 (kind 1; OVL03 33FD, kind1.c)
- Room 1 puzzle: six tiles 0x1E at positions 12..17; entering starts them rising after a random delay (0A18 mode 3);
  standing on one (a standing frame) and leaving presses it; the kind tick (0170) counts in the gate's attribute while
  only the answer tile (DS:2B6A) is down and opens the gate at 0x14. The answer is drawn (random(2), random(4)+1) by
  33FD:0380 when a room with background 6 loads (a room hook, 5.12), once (DS:14A0).
- Room 1 x < 0x82 exits the level; room 3 right edge: random(0x14) sound draws while sound 0x273E is not playing.

### 5.9 Level 1 (kind 5; OVL02 33FD, kind5.c)
- The sea (rooms 0x10/0x13): kind tick 0232 = 01CE (palette cycling every third tick, no state) + 03C8 (the prince:
  in those rooms or when grabbed, DS:6936 == 0xA) + 0428 (the drawn room's characters in those rooms or grabbed).
  0068: a character below y 0xAC (not f19 0x44/0xF/0x3B, not action 2) is grabbed (0370: DS:6936 = index, DS:6937 =
  1, DS:693A = action 9, DS:6938 = x - 0x82 -+ image_width/2, sound 0x30); the step counts up only while room
  description 0x12/0x13 is loaded; at 8 the grab ends and a character on row >= 2 dies (frame 0xB9, take_hp(100),
  85F8 = 0xF). 2D3E:10FD grabs a character that left the level at the bottom (x = char_dx_forward(0x140)).
- Room 0x13: the prince running/jumping (action 2/6, frame 0x50) or on seq 0x3B at x < 0xD0 is pushed left 1 px
  per tick (0AE2/0AA0; 0ABA lets him grab there).
- DS:2B68 (0128 via 0CD6:003A in the full redraw): 1 in rooms 0x13/0x10/0xF, 0 elsewhere (palette); now compared.
- Room 15 to the right (2D3E:1746 -> 0240): past x 0x201 the prince walks into the sea: with digital sound
  (DS:2085 bit 0; 3 at runtime) the waves play first (sound 0x20) until 0x2730/0x273F play; then hp drains one per
  display step and he sinks (seq 0x47). Not yet seen in a capture (a guard in room 15 is in the way).

### 5.10 Level 14 (kind 6; OVL08 33FD, final.c)
- Layout: floating platforms; room 1 (start, row 1 cols 1..5) -> down 2, up 3; 3 -> up 4; 4 -> right 5 -> right 6;
  6 -> up 7 -> right 8 -> up 10 (a type-0 guard). Rooms 1..8 have FINAL descriptions (ids 0x17..0x1E).
- Kind tick 33FD:03C6 (DS:0668): 0570: the prince in room 2 with char_x_right <= 0x131 and not in freefall (action 4)
  is sent back: 045C = scene 5, 169B:018E (DS:2B96 = 0), the full level load 1286:01F2 (kind init included), then
  035A puts him in room 4 col 5 row 1 facing right (seq 0x37, f10 1, hp_delta = hp, next_room 4). The spirit
  (charid 1, f19 != 0x47) dies (seq 0x47) when standing (frame flag 0x40) in room 5 col <= 4 while its body (the
  drawn room's first charid-0 character, 2F86:03CC) lies in room 7 or 8 (room_of_char = 2D3E:133A). 0640: load/save
  every character, then room 3: 0126/01BA: with no guard (or one done appearing, f19 != 0xEE) one appears on the row
  the prince is not on (random(0x14) must be 0 when one is there; col 4 or 2 (+4 by random(1))): a new record
  (type -> charid DS:0096[level.type], hp random(2)+3, seq 0xEE, 2D3E:0EAC palette slot), opp_index set if none.
  Room sounds (3: 0x10D; 6: 0x10E/0x10C by room 6's count; 7/8: 0x107/0x10C, room 8 sets DS:2BB4 = 1) only with
  sound on (DS:2B98) and the music (DS:0884) not playing: a sound query.
- Tile animations (1375:0096 calls 33FD directly): 0x1F 15A2 (counter; background 0x1A: redraws 5..10, stop at 11);
  0x28 162A (bg 0x19: delay bits 3..10, showing bit 11, frame bits 0..2 = random(5)); 0x29 17AC (bg 0x1C) and 0x2A
  19B4 (bg 0x1D/0x1E, index < 6 in room 7, >= 6 in room 8): steps bits 4..10 up to 7 / 9 then a random(0x28)+0x28 /
  +0x50 delay (only when DS:2BA4 and trobs; else start at once), bit 12 kept. Starts (0823:0B78, attr bit 0x1000):
  room 6 -> 1962 (tile 0x29, index DS:2B74++), rooms 7/8 -> 1B94 (0x2A, DS:2B75++); room hook 0x1C..0x1E (1708)
  resets DS:2B74 = 0 and DS:2B75 = 0 (room 7) / 6 (room 8). DS:610E (1 during a full redraw) is 0 in tick code.
- Jaffar = charid 6 (type 3). Room 6 (101A): four stand (frame 0xF) until the prince's body comes 0x12..0x30 px in
  front (or behind: he turns), then step (seq 0x4B, f10 1); the last one (n == 1) goes (seq 0xF0) and clear_char of
  seq 0xF0 (0AFF:1BA2 -> 1416) puts him above room 7 (x 0x2C8, row -3). A hit by one (2D3E:1E7D -> 06AE) starts the
  others. Rooms 7/8 (1186): waypoints DS:1A2E (13 bytes: col, row, dir, x range, per-side catch rows/cols); record
  +0x11 = mode (1 patrol, 2 chase), aimed waypoint, reached waypoint, y. Patrol (0860) moves away from the prince's
  catch zone (13A2), chase (0794) towards it; the moves (0AF2/0C4A/0D72/0E8E) drive the prince's own control routines
  (2FDF:0EF0 forward, 1272 standing jump, 17A6 running jump, 1284 jump up/grab, 1530 climb, 0C90 down, 15D6 release,
  0F9C step). Casting: when the body is on his row within reach (08C0) he faces it and casts (seq 0xF2): frame 0x11A
  kills the prince (0FB8). Drawing a sword within 0x3E px sweeps the prince away (seq 0xF1, f10 0xFF). f19 0xF3 at
  frame 0x15F: counter_5cec++ (the level is won). A dead guard in room 3 (2FDF:0687 -> 0054) turns into a tile 0xA.
- The spirit (charid 1) with shift in rooms 7/8 (2FDF:19D4 -> 1E4A) casts (seq 0xF2) for 2 hp (needs > 2); frames
  0x110..0x119 (2FDF:048C -> 1F8A) launch a fireball at 0x119 (1FA0): falling object type 0xC (1375:1B10 -> 1ED0):
  x speed +-8 growing by 2 to 16, frames 0..3, bursts (wd bit 8, 5 more steps) at a wall (1DC0, 0FB3:290A) or on
  Jaffar (1CEA, the kind-6 character hook: take_hp(100), seq 0xF3, next_room = his room).
- Drawing a fireball (1375:205C -> 1BE6) changes state: one in the left/right room that shows is moved into the
  drawn room (x -+ 0x140), and DS:0842 = the drawn image's width (image 0x130 + frame, 0x134 + step when bursting;
  kid chtab ids 25001 + image + 1 - 400, found in FINAL.DAT through the resource chain; 17 at start); the wall test
  (1DC0) uses DS:0842. The shift key gives ctrl1_shift -1, Ctrl (BIOS flag 4) -2: the spirit's cast and the
  prince's sword are Ctrl (pop2_input.shift = 2; plan2script writes flag 4).
- Not reconstructed: the other drawing hooks (0000, 0330, 1512, 15E8, 16D8, 1898, 1ACA, 1E72).
  The climb needs the spirit (> 4 hp at the 8th turn); a LEVEL14 start has 3 hp (captures poke 8).

### 5.11 Sound and timing dependencies (platform)
- play_sound (1611:01C6) only queues by priority (DS:0D5D); the frame end starts sounds; "playing" is the driver's
  (194C:8426). Decisions depending on it: death waits (DS:0882/0884), victory music (+0F), level 2 random draws,
  gate sounds, level 14 sounds. The core answers through `sound_playing()` (default: playing) and
  `death_sound_playing()`; a duration model of the digital sounds is still to be built.
- Lateness meter DS:2BA4 (169B:05A1): gates palette effects and level 14 animation delays; `frame_on_time()`.

### 5.12 Room descriptions and hooks (roomhooks.c)
- A room has a description when any of its tiles has attribute bits 0xC000 (0CD6:027A); DS:5CE7 holds this for the
  drawn room (set at the end of set_neighbour_rooms 0FB3:0026, not for room 0).
- 0CD6:02BE(drawn_room), called by switch_room (0823:0EAF) and the flip redraw (169B:0A79): without a description,
  unload the current one (0CD6:073A: leave hook = entry 1, free, DS:01AC = 0, DS:0344 = record 0x24 -> none), and on
  level 5 rooms 7/12 call 2A31:0DC1; with one, load "CUST" (room + 159) * 25 from the kind's scenery file (DESERT,
  TEMPLE, CAVERNS, RUINS, ROOFTOPS, FINAL; byte 1 = background id), unload the previous if its id differs, set
  DS:0344 = DS:02FE[id] (id < 0x23, else 0x2E2) and call entry 0 (enter hook).
- Hook records (DS:01AA + 0x1C*k; entry 0 enter, 1 leave): id 0 37F0:0000 / 37F0:0012; 6 2A31:0D3F = 33FD:0380
  (level 2 puzzle answer); 0x14/0x15 347C:0226; 0x16 347C:01EE / 0202; 0x17 33FD:145E; 0x19..0x1B 33FD:1494;
  0x1C..0x1E 33FD:1708; 0x1F 347C:0FB2; 0x20 2A31:0DFD / 37F0:06E0; 0x21 2A31:0DC1 / 37F0:0574;
  0x22 37F0:001C / 0060 (OVL14, level 8 room 9: music; DS:2BB0 = 1 when the prince enters at column >= 9 and the
  sword (DS:2BB2) is still there; leaving only resets the palette).
- Rooms with descriptions per scenery file: DESERT 1 (6), 2 (8), 3 (7); CAVERNS 3 (0), 10 (0x21); TEMPLE 2 (0x1F),
  4 (0x20); RUINS 2 (0x14), 9 (0x22), 11..15 (1..5), 16 (0x15), 27 (0x16); ROOFTOPS 1..5, 10..12, 15, 16, 19 (0x09..0x13,
  no hooks); FINAL 1..8 (0x17..0x1E). Which level uses them is decided by the level's 0xC000 attribute bits.
- A picked-up item's tile gets 0xC000 only while a description is loaded (0AFF:1548 tests DS:01AC).
- 0x14/0x15 (347C:0226, OVL06): level 9, prince col >= 9 in room 0x10: music 0x5C; col < 5 in room 2, once
  (DS:2BAE): 0x5B. 0x16 (01EE/0202) and 0x17..0x1B palettes/sounds; 0x1C..0x1E see 5.10; 0x1F (347C:0FB2): col >= 9
  music 0x5C.
- Implemented: all but ids 0, 0x20, 0x21 (37F0 overlays), logged as missing (ROOMHOOK_IN_xx / ROOMHOOK_OUT_xx).
- 33FD:03F2 (OVL05, kinds 2/4): tile 0xC blocks while its modifier & 0x1F is 3..15.

### 5.13 Drawing pass state
- The drawing changes game state in places, modelled in game.c: mobs (slab return), the prince's sprite entry
  (for biting heads), characters' drawn position/box (draw_chars_state). The prince's pass (0993:07F8) also has hooks
  (level 5 room 3, frames 0x110..0x119/0x132..0x13E, spirit on kind 6) not yet modelled.

---------------------------------------------------------------------------------------------------------------------

### 5.13b More drawing-pass state (found by the fleet runs)
- Object drawing (1375:1FBA, table 1375:201E by type): floors 0/1/3 (2062): one in the left room reaching past its
  edge (DS:082A[type]) moves into the drawn room; traps 4 (186A:0008): kept only in the drawn room (or moved in from
  the left room's column 10), else speed -1 (removed next tick); walls 6 / bubbles 0xB draw without lasting state.
- The prince's drawing (0993:07F8 -> 0C04 body, 0C3A / 0D40 sword) leaves obj_* at his last sprite, usually the
  sword (chtab 0: PRINCE.DAT SHAP 1001 + image, 1201 with sword type 2 on levels 7/8; chtab 1: 3001 + image). A
  character drawn next whose frame has no image (0xFFFF, e.g. a collapsing skeleton at 0xB9) gets its box from it.
- KID.DAT 25065 is a 1-byte placeholder; the original reads the heap after it (h 0, w 5 as observed).

### 5.14 Drawing the sword (Ctrl)
- Ctrl (BIOS flag 4) gives ctrl1_shift -2. Standing (index 0xA = the prince) with -2 and no direction: 2FDF:19D4
  (control.c sword_seq_0317c4): seq 0x37 (sound 0x13), first stepping back so the stance fits: from a wall in front
  on the temple (1375:14FC -> 347C:0B0A, within a column), a closing gate at his tile (3212:0896), or an edge in
  front (tile not floor, loose, or blocking gate): distance_to_edge_weight + adjustment < 0x15 (facing right) / 0xF
  (left) -> x = char_dx_forward(d - 0x15 / d - 0xF); nothing behind either -> no draw (f10 0). -1 during sword frames
  and in level 14's rooms 1/2; the spirit in rooms 7/8 casts (5.10).
- A strike with Ctrl (2FDF:1EB6, prince or spirit) sets DS:68EC = 0xF (guards hold back while it counts down).
- A guard hit by the prince (2D3E:1E3A -> 366C:00FC, OVL10) falls dead at once (seq 0xB9, x back 8) on level 1's
  ship (rooms 0x10/0x13, or facing left with nothing behind), or in a room with a spawn point flagged 0x80
  (2D3E:0E54) when random(3) <= dead characters in the room or another body lies on his tile; not when facing the
  prince's way nor on/before tiles 3/8/4.

## 6. The C core (`src/core.h`)
- `pop2_init(dir)`, `pop2_new_game(level, seed)`, `pop2_frame(&input)` (one tick), `pop2_save/load/hash`,
  `pop2_missing()` (routines not reconstructed that the tick reached).
- State = the field table in `src/state.c` (DS-mapped fields + C-only state + the checkpoint copy).
- Platform hooks (weak): sound_playing, death_sound_playing, level_end_sound_playing, ambient_sound, bios_key,
  frame_on_time, platform_wait_frame, room_background_id.
- Speed: ~17k ticks/s with two hashes per tick; explorer ~500k ticks/s.

## 7. Verification status (2026-09-23)
- 130+ captures: random runs E1..E14 (seeds 1..7; seed 21 with Ctrl, gen_e2e.py CTRL=1), turn runs, planned deep runs X3..X13: all identical in every
  field (strict), warm and cold start. The last scratch-Char difference (E10_6, X6, X9, X12) was the missing 1611:0164
  reload. X2_1's first capture crashed DOS ("Corrupt MCB chain"): the copy-protection answer keys (TAB/ENTER) were
  typed on levels 1-2, where no question is asked; plan2script.py now types them only on levels > 2.
- Warm-started level-2 runs need DS:14A0 = 0xFF (the puzzle answer is chosen at level load; e2e.c resets it).
- Frozen ticks (169B:05E0 returns before 0823:0E72: prince dead/out of the level, no post-tick sample) are compared
  at the next tick's start sample (after 169B:0BA6 frame_begin, applied to a copy), without the drawing pass's
  scratch (obj_*, curr_tile, tile_col/row): 9150 such ticks over the captures, all identical.
- Old single-tick harness cases L1 D/E/F differ by a mid-tick room change the harness does not model.

## 8. Open list
- Story scenes;
  sound duration model; the prince's drawing-pass hooks; hotkeys besides restart; the stubs still logged by
  note()/note_missing() (see `grep -n 'note(' src/*.c`).

---------------------------------------------------------------------------------------------------------------------

## 9. Dated log
- 2026-09-22: oracle and tracer built; play_seq, control, tick, rooms, guards verified against captures.
- 2026-09-23 (morning): level 3 mechanics, kinds 2/4, e2e over all levels.
- 2026-09-23: signed link timers (level 13); kind 1 (level 2) puzzle; cold start; core API; savestates; glue move.
- 2026-09-23: fuzzing with fresh seeds: turn counter/spirit, head sight parity, lateness meter, pickups/potions,
  standing tail palette reset, same-frame key rule for dead prince.
- 2026-09-23: tick-exact planned runs (explore + probepoke); sliding walls; slab return; head attachment; tile-7 runs;
  sword table by type (FRAM 1200 on levels 7/8); clear_char reload; FFEF without frame; victory music and sound answers.
- 2026-09-23: room descriptions and hooks (0CD6:027A/02BE/073A, DS:01AA/02FE/0344/5CE7, CUST resources); OVL14 at
  37F0 is level 8's room-9 script; pickups add 0xC000 only with a description loaded (fixed X5_1).
- 2026-09-23: 1611:0164 opponent reload after the fight code (fixed the last strict differences); X2_1 crash was
  copy-protection keys on levels 1-2; run_all.sh now covers the X captures too. All captures strict-clean.
- 2026-09-23: e2e compares frozen ticks too (all identical); X14_1 (level 14 planned run: falls out of the level).
- 2026-09-23: level 14 (OVL08) reconstructed (final.c): kind tick, tile anims, Jaffar, fireballs; X14_2 (hp poked to 8,
  explore to room 8) identical; e2e applies DS probepokes (POKE_HP); coretest reaches no unreconstructed routine.
- 2026-09-23: spirit rejoin/death and the body's drain (2F86:000A/040C/04CE/0344, 2FDF:09B2); X14_3 (hp 12, 11 chained
  turns in room 7) identical; room hooks 0x14..0x16, 0x1F; tile 0xC on kinds 2/4.
- 2026-09-23: fireball draw state (DS:0842), Ctrl input; X14_4 (the spirit walks off and casts; the fireball bursts on
  a wall) identical.
- 2026-09-23: sword drawing 2FDF:19D4 complete; DS:68EC on Ctrl strikes (was written to a stray variable);
  366C:00FC; Ctrl random runs E1..E13_21 identical.
- 2026-09-23: level 1's sea (kind5.c); X1_1 (ship rooms 16/19, hp 12) and X1_2 (a guard in room 15) identical;
  DS:2B68 compared now; frozen compares skip curr_room too.
- 2026-09-23: level 5 water (water.c, OVL12); guard_advance's water check fixed; all room hooks done; X5_2 (explored
  to room 7 through the water, hp 12) identical.
- 2026-09-23: 1286:087E at level load: type-0 guard sprites take the free palette slots (DS:5D08/09, Char.pal_slot);
  DS:0670 = loaded type. Every cold start is now byte-exact. Deep explorations (300k iterations, hp 12, Ctrl) of
  levels 1-13 reach no unreconstructed routine.
- 2026-09-23: fleet (jaffanator2, 256 cores): 252 explorations (all levels, hp 12, Ctrl) in ~10 min, captured and
  compared in ~20 min: 232 identical; fixes: level 13 shadow (OVL13), trap/floor draw state, 3212:0582 uses the
  below-left/right neighbours (not above), crouch + forward (seq 0x4F) and DS:4406 (a stray variable before), sword
  sprites and their image bases, the placeholder image. Now 252/252 and all 137 local captures identical.

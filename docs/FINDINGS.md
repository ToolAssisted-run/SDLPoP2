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
  kid (169B:0FF0), play_kid_frame (169B:0692), play_all_chars (169B:07EC), sword hits/hurt, checkpoints (169B:0DB4),
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
- The reverse (spirit rejoining, 2F86:0344/04CE) and level 14's use are not reconstructed.

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

### 5.8 Level 2 (kind 1; OVL03 33FD, kind1.c)
- Room 1 puzzle: six tiles 0x1E at positions 12..17; entering starts them rising after a random delay (0A18 mode 3);
  standing on one (a standing frame) and leaving presses it; the kind tick (0170) counts in the gate's attribute while
  only the answer tile (DS:2B6A) is down and opens the gate at 0x14. The answer is drawn (random(2), random(4)+1) by
  33FD:0380 when a room with background 6 loads (a room hook, 5.12), once (DS:14A0).
- Room 1 x < 0x82 exits the level; room 3 right edge: random(0x14) sound draws while sound 0x273E is not playing.

### 5.9 Level 1 (kind 5; OVL02 33FD)
- Rooms 0x10/0x13 (the ship): characters grabbing (0068/0370: DS:6936..693A), the prince's fall caught in the sea
  rooms (0AFF:0D1E). Kind tick 0232 = 01CE (palette) + 03C8 + 0428. Not reconstructed yet.

### 5.10 Level 14 (kind 6; OVL08 33FD)
- Kind tick 03C6: 0570 (prince in room 2: back to the start via scene 5 and a reload; spirit-body logic), 0640 (room
  3: guards appear with random choices, 0126/01BA), room sounds (room 8 sets DS:2BB4). Tile anims 0x1F/0x28/0x29/0x2A.
  Not reconstructed. The climb needs the spirit (> 4 hp); a LEVEL14 start has 3 hp.

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
- Implemented: ids 6 and 0x22; the others are logged as missing (ROOMHOOK_IN_xx / ROOMHOOK_OUT_xx).

### 5.13 Drawing pass state
- The drawing changes game state in places, modelled in game.c: mobs (slab return), the prince's sprite entry
  (for biting heads), characters' drawn position/box (draw_chars_state). The prince's pass (0993:07F8) also has hooks
  (level 5 room 3, frames 0x110..0x119/0x132..0x13E, spirit on kind 6) not yet modelled.

---------------------------------------------------------------------------------------------------------------------

## 6. The C core (`src/core.h`)
- `pop2_init(dir)`, `pop2_new_game(level, seed)`, `pop2_frame(&input)` (one tick), `pop2_save/load/hash`,
  `pop2_missing()` (routines not reconstructed that the tick reached).
- State = the field table in `src/state.c` (DS-mapped fields + C-only state + the checkpoint copy).
- Platform hooks (weak): sound_playing, death_sound_playing, level_end_sound_playing, ambient_sound, bios_key,
  frame_on_time, platform_wait_frame, room_background_id.
- Speed: ~17k ticks/s with two hashes per tick; explorer ~500k ticks/s.

## 7. Verification status (2026-09-23)
- 100+ captures: random runs E1..E14 (seeds 1..7), turn runs, planned deep runs X3..X13: all identical in every
  field except the scratch record Char after two guards follow the prince into a room (E10_6, X6, X9, X12; a
  character loop order). X2_1's capture crashed DOS ("Corrupt MCB chain", to investigate).
- Old single-tick harness cases L1 D/E/F differ by a mid-tick room change the harness does not model.

## 8. Open list
- Room hooks other than ids 6/0x22 (5.12); level 1 kind tick; level 14 (OVL08); spirit rejoin; story scenes;
  sound duration model; the prince's drawing-pass hooks; hotkeys besides restart; X5_1; X2_1 crash; Char scratch order.

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

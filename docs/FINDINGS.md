# SDLPoP2 findings (complete log)

Everything learned while reconstructing Prince of Persia 2 (DOS 1.0, "Prince of Persia Collection Limited Edition" CD)
into C. Addresses are runtime `SEG:OFF` as the game runs under DOSBox-X (DS = SS = 3B25, physical DS:0 = 0x3B250).
Overlays (RTLink) are named by the segment they load at; the same segment holds different overlays per level kind.
`docs/ENGINE.md` is the structured overview; this file keeps every finding, including dead ends and open questions.
Kept up to date as work goes on (newest findings are also in the dated log at the end).

---------------------------------------------------------------------------------------------------------------------

## 1. Method and tools

### Oracle
- Chimera + DOSBox-X headless (`chimera-core-dosbox-x`, branch `pop2-tracer`), driver
  `build/meson-native/oracle-run --workdir W --rom pop.hdd --autoexec c: --autoexec 'cd prince2' --autoexec CMD
  --script S --events E`.
- Script commands: `key FRAME name 0|1`, `probe SEG OFF label [PHYS LEN]` (register file + stack + a memory sample
  up to 64 KiB at each hit), `watch PHYS LEN label` (a write to memory, with the writing CS:IP), `poke FRAME PHYS HEX`,
  `ram FRAME file` (full 640 KiB dump), `inject` (call injection), `trace`, `end FRAME`, and (added 2026-09-23)
  `probepoke LABEL HIT PHYS HEX`: write memory at the HIT-th hit of a probe, used for tick-exact input (tracer commit
  e8f6906, `tracer_probe_poke`).
- Local runner: `<workspace>/oracle/cap.sh NAME "prince yippeeyahoo LEVELn"` (30-60 s per run; 4 in parallel on a
  4-core machine). Level 1 without cheat: `prince` + ESC at frame 1300.
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
- `tools/framecap.py` (frame captures, 5.16) also samples DS:8000..9000 at each pass (the game's offscreen port
  DS:[5CC2] lies there: its bits, +0, a far pointer); with BUF2=PHYS it repeats the buffer probes at PHYS, for a run
  across a story scene that moves the buffer (level 8's scene 6: 0x514B2); tests/frametest.c uses the port's.

### Tests (`tests/run_all.sh`)
- snaptest (tick/chars/room/between modes over DS snapshots), inputtest, e2e (free-running whole runs from the
  captured post-load state and from a cold start), coretest (the API: random play + savestate round trips).
- e2e `E2E_STRICT=1` compares every mapped field (except DS:2B68 palette and, unless E2E_SOUNDMODEL, DS:2B98..2B9B
  ambient state).
- tests/run_soundmodel.sh: every capture with the sound model answering (see 5.11).
- e2e resolves only input/platform ambiguities from the capture, never logic: same-frame key timing and answers to
  "is sound n playing" are retried (state rollback) when the first choice disagrees with the capture; ambient-sound
  random draws are caught up (<= 4) at each tick start; the lateness meter follows the capture; story scenes resync.

### Reading code
- Ghidra decompilation is unreliable in the overlays (lost branches, bad jump tables): transcription is done from r2
  disassembly (`r2 -a x86 -b 16 -m 0 -q -c 'e asm.lines=false; e asm.comments=false; e asm.flags=false; pD N'` over a
  RAM slice; `<workspace>/work/asm.py FILE START END`).
- Overlays in a RAM dump are the ones loaded at that moment; RTLink loads on first call (a later dump may be needed,
  e.g. OVL14 at 37F0 only after level 8 room 9 is entered). Identify an overlay by comparing its bytes with
  `<workspace>/image/ovlNN.bin`.
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
| 693E, 6940..6947 | level 5 rope bridge collapse (reset by 33FD:0C1A) |
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
  freed, level 5 33FD:0C1A bridge reset; kind 4 DS:2BAE/2BB0/2BB2 = 0; kind 6 DS:2BB4 = 0), init kid, entrance closes.
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

### 5.7b Level 5's rope bridge (OVL12 at 37F0, bridge5.c)
- Rooms 7 | 10 | 12 form one row (37F0:0164: column -10 in room 7, +10 in room 12); tile 0x2C is the rope bridge over the
  chasm of room 10 (named "water" in notes before 2026-09-24: oracle shots show the bridge, planks falling). RTLink
  thunks: 2A31:0DE9 -> 0000 (the tick, from OVL04 33FD:0BEA on level 5 in drawn rooms 10/7/12), 0DD5 -> 023C,
  0DDF -> 0286, 0DC1 -> 0426 (room hook 0x21 and rooms 7/12 without description), 0DCB -> 0588 (tile 0x2C anim),
  0DF3 -> 0742 (its start); direct: 37F0:03D2 (guards), 0574 (leave hook), 0782/08F6 (falling object type 0xB).
  Other 37F0 overlays: OVL11 (hook id 0: graphics), OVL13 (hook 0x20: graphics), OVL14 (level 8 room 9).
- Tick 0000: the prince (0194) and the drawn room's first character (0206) sag with the bridge when on tile 0x2C (050C: y =
  row floor + DS:1C87[col], the bridge's curve; on standing frames it bobs -1/+1 with the sway nibble of room 10 row 1). A prince in room 7
  (not f10 0xFF, col <= 5) while its gate (position 13) is closed/closing holds the gate's plate (OVL04 33FD:0000).
  Sway (0454): DS:2B78 = all when both are on the bridge, else bits around the column of the one on it; sound 0x43
  by random(0x28 * n) while 0x2753 is silent. The collapse (001E): both on the bridge within 3 columns count DS:693C up (else
  down, by 2 when not both); at 0x3C with the prince in columns 4..6, the other in 5..6 and the prince not on
  f19 0x55, DS:693E runs 1..3, then planks fall (094A: type-0xB objects from room 10's row-1 positions 17..13, tile
  cleared, attr 0xC000, size random(1): 5 steps small or 10 big, DS:693F+col) until none is left or the prince is
  on f19 0x55; DS:693E = -1 and he is pushed right 10 px at col <= 4.
- 023C: (guards/prince) the bridge's end ahead: facing right at column < 2, or (035E) the opponent near the left
  edge with the gate down; facing left at column >= 8. In 366C:0CAA (advance) it means move forward (guard.c had
  it as stop). 0286: the skeleton's bridge rules (clear_char when falling outside; seq 0x65 at the right; turn at the
  edge). 03D2: room ahead (column < 3 facing right, > 3 facing left). Tile 0x2C anim (0588): frame 0/1 by
  random(3) where DS:2B78 has the column.

### 5.7d Level 5's room 3 (OVL11 at 37F0, lever5.c)
- Loaded with room description 0 (hook 37F0:0000 allocates a zeroed heap block at DS:2B76; flags at +0x3C, +0x3E).
  Crouching on the trap tile 0x12 (row 2, col 3; 2FDF:088F -> 05E8) starts seq 0x80 (x 0x10C, or 0x120 facing
  right); at frame 0x127 (2FDF:048C -> 04FA) the trap's tile is removed (1375:17FC) and the seq lifts the prince; above
  y 0x37 with the mouth (tile 0x1B, row 0 col 6; attribute nibble >= 3 = open) closed he is caught (y 0x14, f24 0xA,
  seq 0x81), then take_hp(100). Tile 0x1B's animation (0676): trob state 0 opens to 3, else closes to 0. Button
  links to it (1375:1396 -> 0786): 0 closed, 1 open, -1 moving (attr bit 0x800 or other values).
- The enter hook 37F0:0000 also queues music 0x21; the leave hook 0012 (0622) stops it (194C:83D2(0x2731)) and frees
  the sounds 0x21..0x24 that are not playing (1611:085C, memory only). Both were missing: with them the sound model's
  e2e of X5_3 goes from 514 differing ticks to 3 (X5_2 1454 -> 651, F5_16 1191 -> 658).
- +0x3C keeps counting: the drawing of the prince's type-0xD object (37F0:023A) adds 1 each frame once he is lifted
  (the animation's frame, 37F0:0360); game.c's drawing state does it (lever5_draw_state). The logic only tests >= 1.

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
- The kind tick 0170 = 011A (outside room 1, every third tick while DS:2BA4 is 0: palette rotations 2699:0048 of
  0xB0..0xBC, 0xC4..0xC7, 0xBF..0xC3), 020A, 002A, then sounds: outside room 1 sound 0x26 (the wind) whenever it is
  not playing and music 0x10B stopped; in room 1, while 0x10B plays, 0x26 stopped; otherwise the chime 33FD:0000
  (music 0xFB, only with DS:2085 bit 1) once the prince lives, is not in state 7 (Kid+0x24), the ambient piece
  (DS:2B9A) is over, no puzzle tile moves (1375:25D6) and the gate is shut (33FD:0618). The gate's animation stops
  sound 7 when it has opened (0x14). These were missing (the sound queue is not in DS:2900..6C00, so e2e could not
  see them); with them the queue events (probes 1611:01FE / 01BB / 194C:83D2, SQ2_raft) equal the game's tick by tick.
- Planning the puzzle: the answer is drawn when room 1 is entered, after the level's ambient-music draws, which follow
  the sound timing: the explorer's cold start (pop2_new_game) drew answer 2 where the oracle drew 1 for the same
  inputs. P2_raft was planned from a prefix (36 ticks left, 58 idle: all tiles up) with the answer set to the
  oracle's (1), the explorer's puzzle key and the gate opening as the goal; e2e (warm and cold) is identical to it.
- 33FD:0380's drawing part (33FD:03CF): with DS:14A0 0 or 1, description objects 15 and 16 of room 1 (background 6)
  show images first + 0xF + DS:14A0 and first + 0x12 + DS:14A0 (the puzzle's clue: a skull or another face) instead
  of their own (17 and 20).

### 5.9 Level 1 (kind 5; OVL02 33FD, kind5.c)
- The sea (rooms 0x10/0x13): kind tick 0232 = 01CE (palette rotations 0xE6..0xE8, 0xE9..0xEB, 0xEC..0xED every third
  tick while DS:2BA4 is 0 in drawn rooms 0x13/0x10/0xF; the renderer's, through hook_pal_rotate) + 03C8 (the prince:
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

### 5.11 Sound: queue, ambient pieces and the driver's timing (sound.c)
The game never waits for sound, but it asks the driver whether a sound still plays, and those answers steer the death
waits, the level end, several room effects and the ambient pieces' random draws (the seed). source/sound.c models it.
- Queue. play_sound (1611:01C6) sets DS:087E = n unless the queued sound has higher priority (DS:0D5C table, 3 bytes
  per sound: [0] flag, [1] priority, lower wins; skipped when prio[new] > prio[queued]), feather fall (DS:5D36) runs,
  or Char.charid == 1 (the shadow). Music: 1611:01A8 sets DS:0880 only when empty (first queued wins); 1611:0002(m)
  queues word DS:0886[m]. Fall scream 1611:0030(room, passed in al): sound 1 unless level kind 5 in rooms 0xF/0x10/0x13.
- Start at the pass's end (169B:0AF3 -> 1611:04D0): should_start(DS:0882, DS:087E) (1611:0582): nothing queued -> no;
  current effect ended -> yes; else by the current one's flag: 0 never interrupted, 1 by a different sound, 2 by any,
  and only if prio[current] >= prio[new]. Yes -> stop 0882, 0882 = 087E, start. 087E = -1. Then a queued music:
  stop 0884, 0884 = 0880, start, 1611:0826 (a MIDI resource, byte 0 bit 1, becomes the ambient piece: DS:2B9A = id,
  DS:2B99 = 0xFF), 0880 = -1. 1611:04D0 does nothing with the command-line word NOSOUNDS (194C:2CEA(DS:096E)).
- Driver (194C). Channels: DS:2088/208C (0: digital, busy + resource pointer), DS:209E/20A2 (1: MIDI), DS:20B0/20B4
  (2, unused here). playing(10000 + n) (194C:8426 -> 33CE) = the busy flag of the channel whose current resource is n
  (id 0: any channel busy); stop(0) (83D2) stops all. DIGISND.DAT and MIDISND.DAT ids are disjoint; digital sounds go
  to channel 0, MIDI to channel 1, a start replaces the channel's sound. Everything stops on restart (0823:051C), the
  restart prompt (169B:123E), pause (0823:10D8), level skip (0823:04AF) and the level end (169B:057E) except when
  going on from levels 5 and 9.
- Digital lengths: DIGISND header 01, rate word (0x2AF8 = 11000), 08, length word: len / 11000 s (measured: channel-0
  busy spans match to a frame). Packed samples (byte 3 = 0xFF, no length: 0x20 0x26 0x2F 0x31 0x36 0x258): measured
  0x31 = 97-98 frames; 0x36 (level 13's moving wall) still playing after 286 frames, a loop it seems.
- MIDI lengths: MIDISND = byte 2 + a format-0 SMF, 480 PPQ. The driver's tempo meta handler (194C:3171) reads the
  three tempo bytes as the high byte and then a little-endian word, so 0x0C3500 (75 bpm) plays as 0x0C0035 (786485
  us per quarter), 0x09A31A as 0x091AA3, 0x0852AE as 0x08AE52: pieces run 1.7% fast, 5.5% fast, 4.3% slow... A 240 Hz
  interrupt (DS:1FBA = 240, DS:1FBC = 480) adds a 16.16 increment (194C:2FB4 with the 16.16 divide 194C:7C68:
  trunc(trunc(256e6/240) * 65536 / trunc(tempo * 256 / 480)); runtime DS:1FC0 = 0x28AFF for 75 bpm) until the
  end-of-track tick. Predicted 441.25 frames for the level-3 pieces: measured 441.
- Ambient pieces (169B:0AFC -> 1611:03CC(DS:2B98), struct: [0] on = DS:2085 & 2 (DS:2085 = 3 at runtime, set by
  1611:02AC at program start), [1] variant group, word [2] = the piece): if the prince is in a room, after
  loadkid, when should_change (1611:0700) and DS:091E[kind] != 0: di = 2FDF:1DD4 (a live opponent in his room;
  cleared if DS:093A[kind] == 0xFF); special pieces (1611:0606): di and level 5 rooms 10/7/12 -> 0x40, di and
  chars[0].charid 0xC -> 0xC6, 0xA -> 0xC7, the shadow (Kid charid 1) with f12 > 2 -> 0x3C, level 13 rooms 4/0x1D ->
  0x3D (variant 0xFF); else the group = di ? DS:093A[kind] : level byte +0x2B1B + room*30 + tilepos (a per-tile
  table), piece = base[group] + random(count[group]) (tables at words DS:091E[kind] / DS:092C[kind]), the next one
  if it equals the current (wrapping to base). should_change: off, prince alive (Char.alive >= 0) or silent -> no;
  the piece ended -> yes; variant 0xFF: no unless level 8 with sound 0xFF playing; then with an opponent: DS:093A >
  variant; without: no if DS:093A > variant or 0xFF, a guard spawn point in the prince's row (2D3E:0C66), level 8
  with 0xFF playing; else yes. Silent (1611:0696): prince dead, the level-end music playing (1611:02CE; kind 1 none,
  2 0x1E, 4 0x1D, else 0x1C), a type-2 level whose chars[0] draws the sword (f19 0x58/0x66/0xD8, 366C:11F8), level 9
  room 16, level 8 room 9 from column 8.
- Waits: the death wait (0AFF:1155) holds while DS:0882 plays (except sounds 4, 7, 0x36), and at alive 7 while 0882
  or 0884 plays; the level end (169B:0541) waits for the level-end music and the same effect rule. Level 14 room
  sounds need DS:2B98 on and 0884 not playing (33FD:03CF). Level 1's waves use DS:2085 bit 0.
- Timing model: a pass of the main loop lasts DS:24DE's reload (169B:0BA6: 5, or 6 when Kid+0x10 == 1) ticks of the
  60 Hz frame timer, one more when late (lateness meter DS:2BA4, `frame_on_time()`); queries inside the tick happen
  at its start, the starts and the ambient checks at its end (5 ms later in the model).
- Limits (measured over the captures): the pass's end moves with the drawing time (tens of ms), level 1's timer runs
  ~3.5% slow (passes average 6.05 frames with the meter at 0: lost timer interrupts, it seems), and a late pass's
  length is unknown; a piece ending within ~40 ms of a check can go either way.
- Result: tests/run_soundmodel.sh (E2E_SOUNDMODEL: no answers from the capture): 74 of 154 captures identical in
  every field for the whole run; the first difference elsewhere is an ambient draw one pass early or late, or a death
  wait (levels 1, 9, 13). The ordinary e2e keeps taking the capture's answers (sound_query_hook) and stays clean.
- Audit: with probes on 1611:01FE (accepted effect, ax) and 1611:01BB (music, si), the core's queue events equal the
  capture's pass by pass (SQ1_2).

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
- Implemented: all (the 37F0 ones' drawing parts in render_ovl37f0.c / render_palette.c / render_desc.c, 5.17).
- 33FD:03F2 (OVL05, kinds 2/4): tile 0xC blocks while its modifier & 0x1F is 3..15.

### 5.13 Drawing pass state
- The drawing changes game state in places, modelled in game.c: mobs (slab return), the prince's sprite entry
  (for biting heads), characters' drawn position/box (draw_chars_state). The prince's pass (0993:07F8) also has hooks
  (level 5 room 3, frames 0x110..0x119/0x132..0x13E, spirit on kind 6) not yet modelled.

---------------------------------------------------------------------------------------------------------------------

### 5.13b More drawing-pass state (found by the fleet runs)
- Object drawing (1375:1FBA, table 1375:201E by type): floors 0/1/3 (2062): one in the left room reaching past its
  edge (DS:082A[type]) moves into the drawn room; traps 4 (186A:0008): kept only in the drawn room (or moved in from
  the left room's column 10), else speed -1 (removed next tick); walls 6 / level 5's falling planks 0xB draw without lasting state.
- The prince's drawing (0993:07F8 -> 0C04 body, 0C3A / 0D40 sword) leaves obj_* at his last sprite, usually the
  sword (chtab 0: PRINCE.DAT SHAP 1001 + image, 1201 with sword type 2 on levels 7/8; chtab 1: 3001 + image). A
  character drawn next whose frame has no image (0xFFFF, e.g. a collapsing skeleton at 0xB9) gets its box from it.
- KID.DAT 25065 is a 1-byte placeholder; the original reads the heap after it: h 0 and a width that depends on the
  heap's history (5 in F7_9, 0xC00A in R3_7_2, both level 7). The core uses 5; R3_7_2 is kept in oracle/known/.

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

### 5.15 Drawing: tables, tiles, descriptions (render*.c; tests/tiletest.c, tools/tilecap.py)
- Draw tables (seg 39E0): back (table 0, at 0, max 130, count DS:60F0), fore (table 1, at 0xA28, max 100, count
  DS:60F2), table 3 (seg [26CA]:11F8, max 30), objects (DS:5D3A, 0x17 bytes, max 30). Entry (20 bytes): image set,
  piece byte, image id, x, y (top left), col (DS:6B6F), row (DS:6B6E) - the drawing globals, not the adder's
  arguments -, rect top/left/bottom/right (the image box cut to the clip DS:60DE), mode, mirror.
- Adders 0993:0124/019C (back, piece +1 / +7, 019C skips id 0), 0220 (fore +7), 0330 (fore +0xD; set 1 for tiles
  0x0A/0x3C, set 0 for 0x16, else 4) take the piece from the piece table DS:[0x1090] (19 bytes per tile type: [0]
  the entry's piece byte, three {id, x, y}); 0993:0008 fills: y += rowY DS:0D40[row] - image height, x += DS:0D26[col].
- Image set 4 (the level kind's tiles, first 3500): ids from DS:05AC[kind] on (caverns 99, ruins 89, temple 78,
  -1 = all for desert/rooftops/final) come from resource id + 200 (0993:0DC8); a description's images are
  registered at first + object number (26BC:073C).
- Whole-room build 0FB3:0122: rows 2..0, columns 0..9 (0FB3:01CA: layers 0, 5, 0xB via 03DC, fore 1 via 0858;
  03DC starts at piece 0594), then row -1 from the room above - or, with a description, 0CD6:0142 clears the
  row-above cache (tile 0, modifier 0xC000). Drawers per kind: far table DS:[0x6188] by tile type, special DS:618A
  (caverns 34C1:0686, ruins 34A3:0986, temple 3579:075E) used by 0FB3:0984 for a chomper (tile 4) closing on a
  character; tile 0x0A (potion) is resident 0FB3:2394 for all kinds but desert/rooftops.
- 0FB3:13C2 before drawing: 1ECE (kinds 2, 4, 5) moves the back entries with piece byte 0 to the front (stable);
  1F68 (level 14 room 1) moves the fore entries of image 0x6372 to the front.
- Descriptions (CUST, 0CD6:0398 load): header 0x1C bytes ([0] count, [1] background, [2] first image resource,
  [0xE] redraw flag), objects 0x19 bytes ([0] image (0xFF none), [1] y, [3] x, [5] layer, [6] mode, [7] id = first +
  number, [9] loaded, [0xB] rect (relative to the image, becomes the screen rect cut to the image), [0x14] piece
  byte). 0FB3:0624 puts the layer's objects in (whole-room build: layers 0/5 at the first tile, 1/2 at col 9 row 0)
  plus the extra pieces 0x6370..0x6372 (0FB3:228E/22F4: level 9 room 2, level 14 room 1, level 10 room 0x16);
  layer-0xB objects are placed by the kinds' tile drawers (desert/rooftops/final 33FD, animated by the modifiers;
  0CD6:007A = an object at a tile). Rooftops' foreground objects go behind the prince (33FD:0000: action 3/4 with
  boxes meeting; 33FD:0826: during a grab).
- Verified (tiletest on tilecap captures, entry for entry): every captured room build of levels 1-9 and 14 (caverns,
  ruins, desert, rooftops, final); temple in progress.

### 5.16 Drawing: frames (render_frame.c, render_sprites.c, render_hooks.c, render_screen.c, render_palette.c; tests/frametest.c, tools/framecap.py)
- A frame (169B:0A98): the table counts DS:60F0..60F9 cleared, 0FB3:12F4 fills the tables (1375:1FBA falling
  objects, 0993:029A the characters with 0823:0F38 the hit points, 0993:05CA, 0FB3:1308 the redraw requests), 13C2
  draws them (sort 1ECE / 1F68, 0993:0684 puts back the saved screens, tables 0, 3, 1 through 0B78 / 0BFC / 0EE0 /
  11AE), 218A copies the changed rectangles (DS:27D8 count, DS:27FC, at most 40; a new one merged into the first
  one it meets after InsetRect -1) to the screen, last first (upside down while DS:5D38 counts: 194C:1346 flips the
  offscreen buffer around the copies). DS:2450 (the current port) is the offscreen one during 13C2, the screen after.
- The whole redraw (169B:0430): DS:610E = 1; 0FB3:0002 (0993:075E frees the saved screens, 0FB3:0122, 1375:1476
  clears the requests); 0CD6:003A (ruins with no room above: DS:0986 erased; rooftops: 33FD:0186 palette); 13C2;
  0CD6:0792(0) (the description's layer-0xB objects: back layers under them requested, background 0x21 only the
  last over DS:1C80); DS:610E = 0, 12F4 + 13C2 again; 2D3E:0F50 (guard palettes for types 0/5/6, type 2); the whole
  screen DS:097E copied (0823:1326); 0FB3:294C.
- Redraw requests DS:61E4..6662 (above[10], fore2[30], objects-at[30], fore_full[30], fore_part[30], back[30],
  full[30]) are set by the drawing (1375:0F5A over a box with 0F16 / 0DC6 / 0E12 / 0E56) and also at tick time by
  the tile animations (1375:01F4..0416) and the overlays; 0FB3:1308 walks them (rows 2..0 then the row above).
- Frame objects (DS:5D3A, 0x17 bytes): added by 0993:02AE (characters: type = charid for 7/8/0xA/6/1, else 2) and
  1375:22DC (falling objects); 0FB3:18DE draws those keyed to a tile plus, transitively, every one meeting their
  rect union (bubble sort 1A68 by the DS:0844 / 0854 type orders), each type by 1C32 (0993:03CC adds a table-3
  sprite, the overlays' types 0x80.. through their hooks).
- Saved screens (DS:5FEC count, 6-byte slots at DS:5FEE: bitmap, id, kind, flag): 0993:04F0 saves under every
  sprite (kind 0), under the description's layer-2 objects at a whole redraw before the first back entry with a
  piece byte (0CD6:06B4, kind 2, kept), under piece-byte-1 description entries (0FB3:0CBA, kind 1), desert's six
  copies (33FD:0770, kind 3). 0993:0684 puts back the flag-0 slots last first (kind 2 kept, kept ids < 0x64 flag
  0); 0993:059E keeps an unchanged character's slots; 0CD6:0684 (tick) re-arms a description object's slot.
- Blitters: modes 0 / 10 of row-run images (type 2: 2583:0006, mode 10 skips fill runs of 0), 4-bit (type 3:
  194C:04BA, 0 transparent, color = nibble | 16 * log2(mask); image set 0: 0x8000 -> 0xF0, set 1: 0x4000 -> 0xE0,
  guards: the sprite's mask), 8-bit (194C:6D42: modes 0 copy, 1 and, 2 or, 3 xor, 4-7 inverted, 8 black
  silhouette, 10 transparent); modes other than 0/2/3/8/10 draw nothing but still add their dirty rectangle.
  1-bit images (entry +2 set) paint their pixels in the entry's mode as a color (potion bubbles 0xEA..). Row-run
  images are converted at load (194C:122C) with the file mask (a pixel's high nibble k -> the k-th set bit; scenery
  0x3FF0, rooftops 0x7FF0, final 0xFFF0, the pieces 0x6370..0x6372 0xFFFE); big ones are decoded in chunks of
  0xE37E / ((w + 1) * 2) rows, each its own LZG/RLE stream; resources are read with one extra byte (LZG needs it).
- KID.DAT images are converted at every draw, with the shape list's mask word of that moment (corrected 2026-09-24;
  it was thought to be a first-load conversion undone by purges). 0993:0E70 keeps the raw SHAP resource (194C:6F4C,
  made purgeable by 194C:1870) in the list entry and leaves the entry's byte 0 clear, so 0993:1034 -> 26BC:0630 calls
  26BC:040A on a copy of the entry at each draw: 194C:0AFE -> 0B28 converts into a new handle with the list header's
  +4 (the mask), and 0FB3:0EE0 frees that copy after drawing it (26BC:0000: bytes 0 and 1 set). The mask is 2 (colors
  0x10..) except while 0FB3:0EE0 draws a sprite whose mask is set (0FB3:0F76 -> 2C9C writes it, 0FB3:0FB4 puts 2
  back): e.g. the prince's flashes in mask 8 (colors 0x30..) on levels 11 and 13. 0B28's rules: 4-bit images (flags
  bit 15) nibble | 16 * the mask's lowest bit (194C:0784); 8-bit images with bit 15 (KID.DAT 24882.., level 13's
  merge) a pixel's high nibble k -> the k-th bit set (194C:06E6, the same table as 122C); a purge and reload changes
  nothing. Only ids 0x83, 0x84, 0xD8..0xDA (the hit points) are converted once, into the entry itself (0993:0EF0:
  26BC:040A with flag 1 sets byte 0, not byte 1: kept), when the image sets are made (1286:066A), with mask 2. Both
  tables (06E6 and 122C) are 16 bytes on the stack of which only the first "bits set" are written: a high nibble past
  them reads stack bytes (0 in the level-13 captures, but once 0x09 for k = 1 in 06E6: the LZG unpacker's leftovers;
  the renderer uses 0).
- Image sets: 0 PRINCE 1000+n (sword type 2: 1200; type 1 with DS:4410: ids 47..57 -> 1101+i), 1 PRINCE 3000+n,
  2 KID 25001+n (-0x190 from 0xDE), 3 the guard file 750+n (ranges DS:06BC / 06D2 per type enabled by 1286:07EE ->
  851+i; charids 10/12 by their own type), 4 the scenery file 3500+n (+200 from DS:05AC[kind]).
- The palette (0FB3:2B1C: PALC count / PALS colors; while blacked out colors from 0x10 go to the saved copy
  DS:27DA; 2B1C's arguments are pushed resource, start, count, sub; a short resource is read on past its end): level
  load 1286: PALS 10 at 0 (0FB3:293A), 3000 at 0xE0 (not final), 3500 sub 0 x 0xA0 at 0x40, 750 at 0x20 (guard
  types but 4), KID 25001 sub kind-1 at 0x10, and image set 0's shape list (SHPL 1000, mask 0x8000, 26BC:0034
  installs its 16 colors, those of PALS 1000) at 0xF0: the swords' colors. The level's own load keeps what it does
  not set (no clearing). A room switch (0823:0E72 -> 0FB3:29B8): unless
  already saved (a flash, 2A34, also saves), colors 0x10..0xFF saved and black and the prince's old box (DS:5B62)
  erased on the current port cut to DS:097E; 294C restores after the redraw. Description rooms' hooks (DS:02FE[bg]
  -> far pointers from DS:01AA, entry 0 on load 0CD6:02BE, 1 on leave 073A) change palettes: ruins bg 0x16 sub 1
  (347C:01EE; leave 0202 sub 0 while alive and time left), level 8 bg 0x22 sub 2 (37F0:001C / 0060), final bgs
  0x17 / 0x19..0x1B / 0x1C..0x1E (33FD:145E / 1494 / 1708: 3500 subs 0/1/2 x 0xC0, PRINCE 1000 at 0xF0,
  25001 sub 7 at 0x20, 3000 at 0xE0 in rooms 3 / 7 / 8; 145E and 1494 room 4 also fill the offscreen DS:097E),
  level 5's OVL11 / 12 / 13 (25303 x 0x20 at 0x10 / 3500 sub 1 in room 0xA / the description's first id at 0xE0 and 2000
  at 0x30; leave: 750 at 0x20 / 3500 sub 0 / 3000 at 0xE0).
- The status line (drawn straight to the screen): hit points 0FB3:24EA (KID images 0xD8 / 0xD9 from x 2 by 8, the
  rest to x 0x62 erased below the level's start value DS:6B71), opponents 25D4 (image 0x83 from x 0x134 by -10,
  heads count two a flask, 0x84 for an odd one; nothing for the prince, riser, shadow, level 5's guards off row 0
  in rooms 0xA/7/0xC), 259C both, 0823:0F38 each frame on a change and the blinking last flask. Messages: 0FB3:2104
  erases the whole line, 2136 the message area DS:098E (the whole line while DS:5CDC is 0x258, the countdown after a
  death); the level's first room (169B:03AE) erases the whole screen (DS:1F2A) or calls 2136(1), then 24EA.
- Verified (tests/frametest.c on tools/framecap.py captures of all 14 levels, four script seeds): every frame's
  tables, dirty rectangles and offscreen buffer exact over ~32,000 passes (full mode, from the game state at
  169B:0A98 with the saved-screen list chained; the one pass that differed on level 11 (FR11_4, the prince in mask 8)
  was the per-draw conversion above); the screen (VGA dumps every
  3rd frame of one seed, ~13,000 dumps, status line included but for the message text) exact apart from dumps taken
  mid-copy or within two frames of a tick-time write, and captures made before the msgclral probe.
- The level 5 / 8 / 13 overlay parts and the tick-time palette rotations: 5.17.
- In the program (shell.c): the core calls weak hooks (game.c hook_draw at 169B:0430 / 0A98 before its state parts,
  hook_hp_bars 0FB3:259C, hook_first_room 169B:03AE's screen steps; glue.c redraw_room 0FB3:29B8, hp_bar_draw /
  hp_bar_clear 25D4; level.c hook_level_loaded; roomhooks.c hook_room_enter / leave) that the shell turns into
  renderer calls with the game state put back after each. The requests the game logic makes at tick time (the tile
  animations 1375:01F4..0416, the overlays' 0F5A / 0E8C) are not in the core: the shell has the renderer redraw
  every tile of the drawn room (and the row above) whose type or modifier changed since the last frame, whole, with
  the characters over it (render_track_tiles); the level-kind overlays' own requests (levels 1, 2, 5) come through
  weak hooks instead (5.17) and those tiles are not tracked. Checked: tests/run_shell.sh's in-game shots (MENU1, DEATH1) exact.

### 5.17 The drawing's parts of tick code; the 37F0 room overlays' drawing (render_kind_desc.c, render_ovl37f0.c)
- Tick code makes redraw requests (1375:0DC6 / 0F5A ...), changes description objects and saved-screen flags, and
  rotates the palette; the core reports these through weak hooks (no-ops in the core, shell.c routes them to the
  renderer; the state they touch is the renderer's): kind1.c hook_desert_gate / wave / tile1e / press, hook_pal_rotate
  (kinds 1 and 5), anim.c hook_roof_tick (kind 5), lever5.c hook_lever5_mouth / trap, bridge5.c hook_bridge_sway. The
  tile tracker (render_track_tiles, 5.16) skips the tile types whose requests are reported (kind 1: 4, 0x1C, 0x1D,
  0x1E; kind 5: 0x25, 0x26, 0x27; level 5: 0x1B, 0x2C).
- Level 2 (OVL03): 33FD:0538 (tile 4, the raft's gate) each step: description object 2's rect left - 2 (the raft
  uncovered), back layers under it at the tile, the one right and the two above (1375:0DC6 with the tile index, +1,
  -10, -9: the index is DS:6672's own tile position, whatever room it is in), 0CD6:0684 (its saved screen put back);
  it acts on the drawn room's description DS:[01AC]. 0658 / 06E4 (the waves 0x1C / 0x1D): back layers under object
  1's rect at the tile. 07CE (0x1E) while moving: 08BE: the rect DS:1498 at the tile (1375:0454 = 17C1:0034 column /
  00B4 row of the tile in the drawn room's grid + 17C1:016E), back layers at the tile's own index, the one right of it
  (1375:0536) and below (06A8). 0A18 mode 1 (a tile pressed): 0904 re-arms the saved screens of the clue's six copies
  (id 0x72) at x = 32 (column + 1): the first two with an edge there, or the one spanning it at y 0x72..0x7F.
- Level 1 (OVL02, rooftops), the same for its animations: 33FD:0680 (tile 0x25 step): object 0x19 at the tile
  (0CD6:0108) and again 12 lower / 30 left (0876): the tiles under both requested, unless a grab in progress
  (030E) meets it; 08C0 (tile 0x26, the ship leaving, step m, in the drawn room): object 19 at x -m and object
  0x15 + ((m - 1) & 3) at x 1 - m, the tiles under their boxes (one pixel wider) requested, object 19's saved screen
  put back (0CD6:0684, which returns its slot) and that slot's rect cut at the box's right edge (0B12); 05AC (tile
  0x27, step m): the tiles under object m + 0x1E. A saved screen is put back over its (possibly cut) rect +0x10 from
  its own bitmap (2699:0184: CopyBits with that rect as both source and destination), and that rect is the dirty
  rectangle: the renderer keeps the saved bitmap's bounds apart from the rect (saved_bg.bounds). frametest syncs the
  rects from the heap each pass: FSX1_1 (level 1, the ship scene, X1_1's plan) 916 passes exact (173 with a differing
  dirty rectangle before).
- Palette rotations 2699:0048(start, count, wait 0): colors start..start+count-1 rotated down by one, on the DAC as it
  is (GetPalette / SetPalette): only OVL02 33FD:01CE and OVL03 33FD:011A call it (a byte search of every overlay).
- Level 5 room 3 (OVL11, description 0): the caverns' drawer table DS:16B8 points straight at 37F0 for tiles 0x12
  (012A), 0x1B (06EE) and 0x2C (0610), so the loaded 37F0 overlay decides. 012A: in a whole redraw, before the lift,
  image 0x62D7 at DS:1C30 / 1C2E as a back entry (mode 0xA). The images 0x62D7 + n (0..9) are 37F0:002A's: image
  n + 0x12E of a copy of image set 2's shape list header (KID.DAT, first 25002) with the mask | 4 = CAVERNS.DAT SHAP
  25303 + n converted with mask 6 (colors 0x10 / 0x20: the palette 25303 of the enter hook); 0FB3:0D28 / 0F04 map
  ids 0x62D7..0x62E2 to them (03AC(id - 0x62D7)), and 0D3B first calls 01E6 (once, block +0x40: the screen under
  image 0 saved, id 0x65, kind 2). 06EE: object (modifier & 7) + 2 when drawn from its own column (whole-screen clip
  outside whole redraws). 044A (0993:0870: the prince on frame 0x127 in room 3 once lifted): an object of type 0xD,
  image DS:1C32[k] + 1 at DS:1C52 / 1C50[k] from his position (-20 mirrored), the fore layer under it requested.
  023A (0FB3:1D3C, drawing type 0xD): a table-3 sprite (mode 0xA), the first frame object moved down DS:1C46[k],
  caught: palette 25303 sub-palette +0x3C - 4 at 0x10 (0 <= that < 8), +0x3C + 1, and once +0x40 the slot 0x65 put
  back (0CD6:0684(1)). k = 0360: (+0x3C / 2) % 9, caught min(+0x3C + 8, 9). Tick time: 04FA's first step requests the
  tiles under image 0 (053B), the mouth's animation 0676 the tiles under DS:1C78 at the tile (0756).
- Level 5's rope bridge (OVL12, rooms 7 / 10 / 12): 0610 (tile 0x2C, layers 5 and 2): object (modifier & 0xF) + 3 at the tile
  lowered by the bridge's sag DS:1C87[col] (0654 / 0698 move it and its rect and back). 06CE (tick time, from
  0588 with the new frame v and from 0742 with the attribute & 0xF0): object v + 3's rect so lowered, at the tile's
  column and one row lower (17C1:016E), one pixel higher: back layers under it. 08D2 (object type 0x8B): a sprite
  of description object DS:1C90[id & 0xF] + 1 one pixel up (0993:03CC; in room 10 of level 5 chtab-4 sprites whose
  id is below the description's count are its objects: 0CD6:0224 / 01EC). 0782 (falling object 0xB, a plank): its
  position as the drawn room sees it and its tile key; in level 5 room 10: moved by DS:1C9A / 1CAE[step], the
  floor-depth entries of type 0xB (DS:0826 / 0840) = its object's image height / width + 1, requests, the object.
  Verified by P5_plug (explorer EXPLORE_KEY=plug EXPLORE_STOPPLUG=1, hp 12: the prince and a skeleton fight
  on the rope bridge until DS:693C passes 0x3C; five planks drop out and both fall): 1834 ticks identical (strict,
  warm and cold) and FRP5_plug 1854 passes exact, 350 of them with the planks' type-0x8B objects.
- Level 13 room 4 (OVL13, description 0x20): 0486 (the temple's tile 0x2B, layer 0xB, whole-screen clip): modifier bit
  7: object DS:1CC2[m & 0x7F] in the foreground (layer 1 for the call), else object 0xB + m. The enter hook 0510
  reloads objects 0xB..0x26 from a copy of the prince's shape list header with the description's first resource and
  mask 0x4000 (26BC:040A: colors 0xE0.., the palette the hook loads at 0xE0); render_image_set_mask keeps that per
  image. 05FC (0993:0888, frames 0x132..0x13E; itself tests room 4 and level 13): the sword table's entry (DS:[6110],
  cur_frame's sword) names description object image + 0x17, placed at its offsets from obj_x / obj_y (0AFF:0390),
  clipped to its own rect, drawn in the background layer (0 for the call). 05FC is verified: FRP13_shadow reaches
  frames 0x132..0x13E (the merge, passes 3284..3354), tables exact; its entry keeps the drawing globals DS:6B6F / 6B6E of the last tile
  loop (0x0A / 0xFF: frametest now loads them from the pass's state), and the prince's own object on these frames
  (image -1) has the empty rect DS:1F12 (0AFF:1846 / 18AE; the renderer kept the screen clip, which reordered two
  sprites).
- Level 8 room 9 (OVL14): the chomper drawer's room-9 case (34A3:0A33 -> 2A31:0E43 -> 37F0:0000): description
  object 7 drawn in layer 1 for the call (then its layer is 0), within the clip the drawer cut.
- Verified (tests/frametest.c full mode; new captures FRX5_3, FRX5_2, FRG8_sword, FRP2_raft, FRP13_shadow): FRX5_3
  (the trap, the lift, caught) 1072 passes exact (408 differing before), FRX5_2 (the bridge rooms) 1784 exact (49
  before), FRP2_raft (level 2's puzzle and the raft) 472 exact (44 before: the clue's images). FRG8_sword: the
  passes after scene 6 differed (1276 px each) only because the capture read the wrong memory: the scene's allocations reallocate the game's offscreen port (DS:5CC2 0x8546 ->
  0x85AA) and its bits move from phys 0x4CF22 to 0x514B2, so 0x4CF22 no longer changed (bar a heap block over its
  first rows). FRG8_sword2 (tools/framecap.py BUF2=514B2: frametest takes the buffer the port names) is exact in all
  554 passes (the saved-screen list after the scene synced from the game's: 0AAC:0376's whole redraw is not probed).
  FRP13_shadow: 231 differing passes -> 1 (9 px: the stack bytes of 194C:06E6 above), by the per-draw KID.DAT colors
  (5.16), the 8-bit bank mapping and the empty rect of image -1.
- The shell with the hooks (tests/shelltest.c SHELL_VRAM on a framecap capture with VRAM_STEP and a ds_tick probe;
  probepoke plans played): P2_raft (level 2: the waves, the puzzle, the raft) 1350 VGA dumps, 1300 exact, 44 within
  two frames, 6 differ (the room switch: DOS draws the new room ~10 frames later, under the blacked-out palette);
  before: 66 exact. RGB shots (SHOT_STEP=3, palette rotations included): 900, 800 exact, 79 within two frames, 21
  differ (the room switch; the DAC rotated at tick time while the pixels are copied at the pass's end; frames whose
  VGA dumps are exact). X5_3 (level 5 room 3, VGA dumps every 4 frames): 1639 dumps, 951 exact, 32 within two frames
  (157 before), the game state identical but drawing scratch (SHELL_FOLLOW). X1_1 (level 1, RGB shots every 3 frames:
  the sea's palette rotations, the ship): 1789 shots, 1219 exact, 451 within four frames, 119 differ (636 / 271 / 880
  before); the rotations are in phase in every shot; the rest is a tick of timing in the ship scene and, after tick
  550, the sound model's death wait (the state differs there: e2e takes the capture's answers, the shell does not).

### 5.18 The renderer's last differences (2026-09-24)
- FRG8_sword (level 8 after scene 6, "the prince's standing sprite not drawn"): not the game's drawing. After
  37F0:007C's scene 6 and the level's reload, the offscreen port (DS:5CC2 0x8546 -> 0x85AA) and its 64000-byte
  buffer are allocated anew, at phys 0x514B2 instead of 0x4CF22; the capture kept sampling 0x4CF22 (a stale copy,
  rows 0..2 overwritten by a heap block, the prince not in it). With the buffer the port names (FRG8_sword2) all 554
  passes are exact; nothing in the core or the renderer changed for it (DS:2BA6, 0993:07F8's skips and the purges are
  not involved).
- FRP13_shadow (colors 0x30.. instead of 0x10..) and FR11_4's one pass ("a KID.DAT image purged and reloaded"): both
  the same rule, deterministic: KID.DAT images are converted at each draw with the list's mask of that moment (5.16);
  the prince's sprite carries mask 8 in some passes (table-3 entry +0x10 = Kid.pal_slot, +0x34, through 0993:09B6),
  so those draws are in 0x30... The memory manager is not involved (a purged raw resource is reloaded unconverted).
  The 8-bit KID.DAT images of level 13's merge (flags 0xF300 / 0xF400) go through 194C:06E6's bank table too.
- The two table-3 entries in swapped order (FRP13_shadow 3342..3354): the prince's object on frames 0x132..0x13E has
  image -1, whose rect is DS:1F12 (empty; 0AFF:18AE) - the renderer kept the screen clip, so the object met every
  other object's rect and changed 0FB3:18DE's drawing order.
- Left: FRP13_shadow pass 3284, 9 px: 194C:06E6 maps KID.DAT 24882's pixels of high nibble 1 through its table's
  second byte, which mask 8 (one bit) never writes: a stack byte left by the calls 0B28 made before (the unpacker
  194C:0886 -> 77CE for these LZG images, it seems): 0x09 there, 0 for nibble 15 in the passes after. Modelling it
  would need those routines' stack frames byte for byte; the renderer uses 0.

## 6. The C core (`source/core.h`)
- `pop2_init(dir)`, `pop2_new_game(level, seed)`, `pop2_frame(&input)` (one tick), `pop2_save/load/hash`,
  `pop2_missing()` (routines not reconstructed that the tick reached).
- State = the field table in `source/state.c` (DS-mapped fields + C-only state + the checkpoint copy).
- Platform hooks (weak): bios_key, frame_on_time, platform_wait_frame, room_background_id; sound: the model in
  sound.c answers, `sound_clock_hook` / `sound_query_hook` let a platform replace its clock or its answers.
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
- Rendering: one pass of FRP13_shadow (9 px of stack bytes read by 194C:06E6, 5.16);
- Story scenes;
  sound: packed digital sample lengths (0x20 0x26 0x2F 0x258), draw-time jitter; the prince's drawing-pass hooks; hotkeys besides restart; the stubs still logged by
  note()/note_missing() (see `grep -n 'note(' source/*.c`).

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
- 2026-09-23: level 5 rope bridge (bridge5.c, OVL12; first misnamed "water"); guard_advance's bridge check fixed; all
  room hooks done; X5_2 (explored to room 7 across the bridge, hp 12) identical.
- 2026-09-23: 1286:087E at level load: type-0 guard sprites take the free palette slots (DS:5D08/09, Char.pal_slot);
  DS:0670 = loaded type. Every cold start is now byte-exact. Deep explorations (300k iterations, hp 12, Ctrl) of
  levels 1-13 reach no unreconstructed routine.
- 2026-09-23: fleet (a 256-core machine): 252 explorations (all levels, hp 12, Ctrl) in ~10 min, captured and
  compared in ~20 min: 232 identical; fixes: level 13 shadow (OVL13), trap/floor draw state, 3212:0582 uses the
  below-left/right neighbours (not above), crouch + forward (seq 0x4F) and DS:4406 (a stray variable before), sword
  sprites and their image bases, the placeholder image. Now 252/252 and all 137 local captures identical.
- 2026-09-23: fleet round 2 (fresh seeds, half at the normal 3 hp): 250/252; fixes: 3212:06BA compares the gate
  modifier as a byte (> 0x40), 366C:1166 (skeleton collapse on landing), 33FD:0754 wired (caverns floor landing).
  The pulled F*/R* captures joined tests/run_all.sh.
- 2026-09-23: fleet round 3 (1M-iteration explorations): 251/252; the one difference is the heap-dependent placeholder
  width above (a character's box for one tick).
- 2026-09-23: fleet round 4 (explorer cells keyed by the live animation count too): 248/252; fixes: 33FD:09EE (level
  2: landing from a jump presses the puzzle tiles, was an empty stub), 2FDF:232A (crushed by a caverns gate: seq
  0x76, take_hp(100)), 366C:11F8 wired. Silent stubs audited: 366C:0D5A is a bare retf in OVL09 (charids 7/8, correct
  as empty); OVL11 reconstructed later (5.7d).
- 2026-09-24: level 5 room 3 trap and mouth (OVL11, lever5.c); X5_3 (crouch on the trap, caught) identical.
- 2026-09-24: sound model (sound.c): queue/priorities/flags (1611), ambient pieces (1611:03CC and helpers), driver
  channels and lengths; the driver's tempo byte swap (194C:3171) and 240 Hz 16.16 counter (194C:2FB4) explain the
  MIDI lengths; frame timer reload 5/6 (169B:0BA6). Model-only e2e: 74/154 captures fully identical (35 before the
  tempo findings); the default suite unchanged.
- 2026-09-24: 0AFF:0DF6 (outside the level) tests the level kind (DS:43FD), not the number: on level 14 (kind 6) a
  character in room 3 or 5 at row >= 11 is out of the level (the pits; the spirit falling there dies and the body
  takes over). G14_fall (the spirit walks into room 5's gap) identical. Explorer EXPLORE_KEY=jaffar (Jaffar kills).
- 2026-09-24: G14_hit6 (a spirit's fireball hits a Jaffar in room 6, seq 0xF3) identical. Level 14's win (counter
  +1 at seq 0xF3 frame 0x15F, only in 33FD:1186, i.e. a Jaffar in rooms 7/8) not reached by 780M explorer ticks.
- 2026-09-24: level 14 room 6, verified: Jaffars standing still fall through room 6's floor gaps (seq 0x52) and
  leave the room's list when they fall out of it (G14_k3); a dead Jaffar's body stays in the list, and the last one
  starts seq 0xF0 only when the list holds him alone (nchars 1, the spirit not counted), then waits in room 8's list
  (G14_leave: the spirit kills one with the sword, the last leaves). Explorer EXPLORE_J78 / EXPLORE_STOPKEY.
- 2026-09-24: level 14 rooms 7/8 verified (G14_win: the last Jaffar in room 8, the spirit's fireball hits him, the
  prince dies there): 33FD:0AD0 walks left by cx - x_min + 2 (the core had the sign reversed); the kind tick's gate
  33FD:03D9 is DS:0884 playing (the core asked for sound 0xFFFF); the room hook 33FD:1708 queues the room music (room
  6 0x10E; room 7 0x107 once DS:2BB4; room 8 0x107 while room 8's list DS:474B is not empty and 0x107 is not playing).
  MIDISND byte 0 = 0x82 marks looping pieces (0x21 0x3C 0x3F 0x6A 0xFB 0xFD 0xFF 0x107 0x10C..0x10E). e2e now answers
  sound queries with the model by default (the capture only through the retry on a differing tick): all captures
  strict-clean. The level-14 win itself (counter +1 at Jaffar's seq 0xF3 frame 0x15F) is still unverified.
- 2026-09-24: video: VGA mode 13h (320x200, 256 colours; DOSBox VRAM chain-4: pixel A at (A & ~3) * 4 + (A & 3)); the
  game draws into an offscreen buffer in conventional memory at phys 0x4CF22 (64000 bytes, 320 per row), so RAM
  dumps hold the screen. Images: the PoP1 format (height, width, flags: bits 12..14 depth - 1, 8..11 packing 0 raw,
  1 RLE, 2 RLE by columns, 3 LZG, 4 LZG by columns; source/image.c). Resource types: SHAP images, SHPL shape-set header
  (first id, count, 16 colours of 6-bit RGB), PALS/PALC palettes, PIEC tile pieces, CUST room descriptions, FRAM,
  FONT, TXT4 texts, _SCR/_PSL/PALT/STRL story scenes (NIS.DAT, TRANS.DAT, FINAL.DAT), _SND sounds. oracle-run has
  `shot F PATH` (TGA, top-down rows, BGR) and `mem F DOMAIN PATH` (4 = video RAM) script commands now.
- 2026-09-24: renderer tables: whole-room builds of all kinds but temple match the game entry for entry (5.15);
  level 2's puzzle solved by the fleet explorer and verified (P2_99: 462 ticks identical).
- 2026-09-24: level 14's win verified (G14_end: prefix k3b + fleet explorer, hp 12; the spirit's fireball hits the
  last Jaffar in room 7, his seq 0xF3 reaches frame 0x15F, counter DS:5CEC 14 -> 15, the game ends into the ending
  scene 11): 856 ticks identical. Every level's completion is now verified in the oracle.
- 2026-09-24: audio drivers (docs/AUDIO.md: DIGI.DRV = DSB_PRO, MIDI.DRV = MSB_PRO OPL2 + PRESETS.DEF, 194C
  sequencer/PC speaker) and story scenes (docs/NIS.md) reconstructed by subagents and verified against the oracle.
- 2026-09-24: 37F0:007C (OVL14, level 8 room 9, 2A31:0E1B at seq 0xED frame 0xB9: the sword taken) reconstructed
  (ruins.c sword_scene: scene 6, the level reloaded in full with DS:2BB8..+0x2EF9 put back, seq 0xE7 in room 9,
  DS:2BB2 = 1, music 0xFF); G8_sword (explorer EXPLORE_STOPSEQ=0xE7, hp 12) 545 ticks identical through and after
  the scene. Program shell (docs/SHELL.md) committed.
- 2026-09-24: the drawing's parts of tick code through weak hooks (FINDINGS 5.17): level 2's raft, waves and puzzle
  tiles (33FD:0538 / 0658 / 06E4 / 07CE / 0904), the palette rotations (2699:0048 from OVL02 / OVL03), level 1's
  0x25 / 0x26 / 0x27 (33FD:0680 / 08C0 / 0B12 / 05AC); saved screens keep their bitmap's bounds apart from their rect.
  Level 2's puzzle clue images (33FD:03CF), its kind tick's sounds and the gate's stop (SQ2_raft queue events equal);
  level 5 room 3's music 0x21 (37F0:0000 / 0622). The 37F0 overlays' drawing: OVL11 (the trap, the mouth, the lift and
  the catch: images 0x62D7.. = CAVERNS.DAT 25303.. with mask 6), OVL12 (the bridge tile, its sway, the falling planks),
  OVL13 (tile 0x2B, objects 0xB..0x26 with mask 0x4000, 05FC), OVL14 (the teeth). New captures P2_raft (explorer with
  the oracle's puzzle answer), SQ2_raft, P13_shadow (e2e identical); frame captures FRP2_raft, FSP2_raft, FRX5_3,
  FVX5_3, FRX5_2, FRG8_sword, FRP13_shadow, FSX1_1. tests/shelltest.c plays plans and compares VGA dumps / RGB shots;
  tests/nistest.c -DNIS_ENGINE (nisrun.py) draws transitions 2 and 3's rooms through shell_nis_room: scene 2 670 / 753
  exact (664 / 747), scene 3 420 / 552 (399 / 482); the room hook's fill (level 14 room 1's sky) now drawn there.
- 2026-09-24: cheat keys K, g, k, S (0823:0682 / 0768 / 07A6 / 06F0) in the shell; CK1 / CK10 (keys pressed between two
  ticks: those letters are movement keys too) identical tick for tick. Sound model (run_soundmodel.sh): 83 of 163
  captures identical as before, differing ticks 8878 -> 6852 (level 5's room-3 music).
- 2026-09-24: the renderer's last differences (5.18): FRG8_sword's after-scene passes were a capture artefact (scene 6
  moves the offscreen buffer to 0x514B2; framecap.py BUF2 + the port probe, FRG8_sword2 554/554 exact); KID.DAT images
  are converted at every draw with the list's mask (0FB3:2C9C / 26BC:0630 / 040A / 26BC:0000), 8-bit ones through
  194C:06E6's banks: FRP13_shadow 231 -> 1 differing pass, FR11_4's "purge" pass exact; image -1 has the empty rect
  DS:1F12 (0AFF:18AE); frametest loads DS:6B6E / 6B6F. 37F0:05FC verified (FRP13_shadow frames 0x132..0x13E).
  P5_plug (explorer EXPLORE_KEY=plug / EXPLORE_STOPPLUG): level 5's bridge collapse, 1834 ticks identical; FRP5_plug
  1854 passes exact (OVL12 0782 / 08D2 drawn in 350). All frame captures exact but that one pass (9 px of stack bytes).
- 2026-09-24: level 5's "water" is the rope bridge over room 10's chasm (oracle shots of P5_plug: the prince and a
  skeleton fight on it, planks drop out, both fall): water.c renamed bridge5.c and the names/notes corrected
  (swim -> on the bridge / sag, waves -> sway, plug -> collapse, bubbles -> falling planks). Behaviour unchanged.
- 2026-09-24: SDLPoP2's own cheats (source/cheats.c: god mode, the shadow / flame at will, the sword type, looking into
  rooms, teleport, fly); the god-mode hooks sit at every place the prince is hurt or killed (fight.c's strikes, head
  bites, rocks, sinking floors, blades, wall and gate crushes, falling floors, landings, the out-of-level falls, level
  1's sea, 5's mouth trap, 13's flames, 14's casts, the spirit's drain and costs, poison). tests/cheatstest.c: a sweep
  putting the prince on every floor tile of every room (876 deaths without god mode, none with it) and savestate round
  trips through random use of every cheat. Those found two savestate faults, now fixed: state_load re-selected the
  guard frame table (guard_frame_table's fallback for a type without a guard file) from the level loaded, where play
  leaves it alone; and curr_room_tiles was saved relative to tiles0 as if tiles0 and level were one block. The DOS
  game's control keys include the letter grids W E R / S D F / X C V and U I O / J K L / M , . (keyboard_controls):
  a cheat on one of those letters also moves the prince (the DOS cheats K, S, W, I, R, T do); SDLPoP2's use free keys.
- 2026-09-24: the shadow / flame cheats on any level: the game's spirit is one character (charid 1); the flame is only
  its drawing (2F86:052E: images 0x122 + tick % 9 of the kid's set, found in FINAL.DAT, with more than 2 hp) and its
  colors are bank 0x30, PALS 2000 sub-palette 0 (the shadow, level 13's room 4) or 1 (the flame). Off the temple and
  final levels bank 0x30 holds a guard palette, so the frontend puts the spirit's colors there after each drawing
  while the cheat's spirit is out (a second guard palette in the same room shows in them meanwhile). No game state
  depends on the form (kid_sprite_state skips the spirit).

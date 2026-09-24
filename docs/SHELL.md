# The shell: the DOS program around the game's tick

`src/shell.c` (program flow, keys, demos, clock), `src/menu.c` (the screens of segment 0D5E), `src/text.c` (fonts,
text, the 194C graphics subset, the status line), `src/loader.c` (resource files, TXT4, CONFIG.DAT, the game's own
files). Tested by `tests/shelltest.c` against the oracle. Addresses are runtime `SEG:OFF` (DS = 3B25).

## 1. API (shell.h)

```c
shell_set_seed(seed);                       // optional: DOS time() at start (the oracle's DOSBox: 0xCBE2D)
shell_init(game_dir, argc, argv);           // argv = the DOS command line words: "yippeeyahoo", "LEVEL5", ...
for (;;) {                                  // once per video frame (VGA 70.086 Hz, what DOSBox shows)
    shell_input in = {0};                   // held keys by PC scan code, BIOS shift flags, keystrokes typed
    shell_input_key(&in, scan, down, ascii);    // for each key event of the frame
    if (shell_step(&in) == SHELL_EXIT) break;   // shell_exit_code(), shell_exit_message()
    present(screen_buf, render_palette);    // render.h: 320x200 indices, 6-bit DAC
}
```

- An SDL frontend keeps one `shell_input` across frames (the held keys persist) and clears `ntyped` after each step:
  on `SDL_KEYDOWN` / `SDL_KEYUP` (repeats included, they are the typematic keystrokes) it calls
  `shell_input_key(&in, shell_pc_scancode(e.key.keysym.scancode), down, ascii)` with the character the key types
  (the keysym for letters, digits and punctuation, upper case with shift; 0 for the others); it steps at the VGA's
  70.086 Hz and presents `screen_buf` through `render_palette`. The core's `pop2_frame` loop stays for playing
  a level without the program around it.
- The shell keeps the original's structure (blocking loops waiting for keys and timers) and runs as a coroutine
  (ucontext, 1 MB stack): `shell_step()` resumes it until it has waited one video frame (`sh_frame()`). The game's
  own clock, the 60 Hz timer (four countdowns DS:24DC, of which DS:24DE is the tick's `frame_delay`, and the tick
  count DS:24E4), is derived from the frames (60 x 44900 / 3146875 ticks a frame). A game tick (169B:0504's pass)
  runs when `frame_delay` has run down: 5 or 6 timer ticks, ~5.8 frames.
- Input: `in.down[scan]` feeds the keyboard interrupt's table (DS:1D0D + scan code, 0823:160B), `in.shift_flags`
  the BIOS flags (0040:0017: 1/2 shift, 4 ctrl, 8 alt), `in.typed[]` the DOS keystroke queue (0823:16EE: ASCII, or
  the scan code << 8 for extended keys and Alt+letter; 8 deep, 194C:99EC). `shell_input_key()` builds these from
  key events. The frontend supplies key repeat (DOSBox: 500 ms, then 33 ms).
- Output: the screen is `screen_buf` / `render_palette`. The game screen is the renderer's (`render_frame()`,
  `render_redraw_all()`, weak here, called after each tick / after a level's first room, with the game state saved
  and restored around them: the drawing's own state effects are the core's `draw_*_state`); the menus, the status
  line texts and the dissolves are drawn here; scenes by nis.c (`nis_step` into the same buffers).
- Scenes: `shell_mode() == SH_SCENE`, `shell_scene()` the number; nis.c draws into screen_buf / render_palette, its
  sounds go through the same hooks as below (`nis_set_sound_callback`), its room hook to `shell_nis_room` (weak).
- Sounds outside the tick's queue go to `sound_start_hook(res - 10000)` / `sound_stop_hook` (sound.c; the SDL
  frontend's `audio_request`): `shell_sound(0xFFFE)` the error beep (also 169B:0B9C's blink beep, from game.c),
  `10000 + 0x10C` the title music, `10000 + 0xFD` the hall of fame's, `9999` "unable to save". Volume changes (Alt-S,
  the options menu, PRINCE.OPT, CONFIG.DAT) call the weak `platform_sound_volume(15 | 0)` (194C:3380 -> `audio_volume`).
- Files: `file_dir` (loader.h) is where PRINCE.SAV / HOF / OPT are read and written ("" = the game directory, as
  DOS does; the tests set it elsewhere). `file_size / file_read / file_write_at / file_create` are weak.
- `SHELL_TRACE=1` prints the shell's steps with their frame; `shell_tick_hook` (tests) is called at each tick's
  start (where the oracle's 169B:05E0 probe fires).

## 2. The program's flow

| DOS | C | what |
|---|---|---|
| 2D3E:019A (OVL00, via 2A31:0CBD) | `shell_main` (start) | memory check; CONFIG.DAT (194C:2D44, else "Could not find the system configuration file"); input device (joystick only if 26B5:000C finds one); USEHIMEM; PRINCE.DAT; video check (NOVIDEODETECT); keyboard interrupt (0823:1587); 60 Hz timer (194C:7D5A); sound drivers (2D3E:007C, messages DS:0B30 / 0B86); CONFIG +6 = -1: sound off; screen port; sound files (2797:0260 by CONFIG); 1611:02AC ambient on; 0FB3:293A; the cheat word: TXT4 10 "YIPPEEYAHOO" on the command line -> DS:10C2 |
| 0823:0000 | `shell_main` | SBDIAG message (not kept); 0D5E:2166 PRINCE.OPT; 0823:0192 LEVELn (cheat only, 1..14 -> DS:6B6C); opens KID.DAT, SEQUENCE.DAT (DS:0598); `random(1)` (the seed from `time()`, 2751:00D6); level kind 0 |
| 0823:00B2 | `main_loop` | cheat + NIS/TREE words: story scenes for ever; 0823:13E6 (stop sounds, DS:0998 = 0xFF, DS:5CC0 = 0); DS:6B6C = 0 -> the title; else `game(level)`; after it: a demo -> 0; time out (DS:5CD2 = 0) -> title; level > 14 -> scene 0xB (the end), `hall_of_fame_enter(minutes left)`, title; else if the result is 0 while DS:6B6C is set: DS:6B6C = the level number (play on) |
| 0823:01CA | `title` | DS:00EC = 0 (not the first time): a key pressed during the demo (DS:2BAA), or `title_credits()`, or `hall_of_fame(0)` returning a key, starts the game; then scenes 7, 4, 8 (the intro; played as nis.c's NIS_INTRO, the music running on); a key (scene result 2) -> level 1 (DS:6B6C = 1, DS:00EC = 1, demo off), else the next demo (15DB:0316); DS:1392 (Alt-L during the demo): restore a saved game (0823:028C) |
| 169B:0006 | `game` | level.c `game_start` (clock 75 min, 0x2CF ticks, hp 3; cheat LEVEL: hp 3..12), then the level loop |
| 169B:0070 | `level_loop` | unless a restored game (DS:5CB6): story scene (0AAC:000E / 0120 = level.c `story_scene`, played by `sh_scene`); DS:1392 during a scene: restore; full load (1286:01F2) after a scene, a new level or a restore, else the reload 1286:0332 (level.c `load_level_ex`); `level_begin`; 169B:03AE; demo start; 169B:0504; after a demo: level 0 |
| 169B:03AE | `first_room` | level.c `level_first_room`, the renderer's full redraw, then 5 timer ticks (DS:24DE = 5, keys pumped) |
| 169B:0504 | `play_loop` | per pass: 0BA6 `frame_begin`, the tick (05E0), the restart / level-end checks, 0A30 `frame_end`, 18C8:0008 (cheat mode: loadkid; its debug displays are not kept), the renderer; on time (DS:24DE still running): wait for it (2797:0134), else the lateness meter DS:2BA4 counts up (`frame_on_time_hook`) |
| 169B:123E | `restart_prompt` | time out: stop sounds, scene 0x1C, DS:5CD2 = 0, DS:016A = 8 (then the title) |
| 0AAC:0274 | `sh_scene` | scenes: < 0x14 and not 6 drop the checkpoint; the offscreen port freed; not 1/100/6 (and an uninitialised word compared with 4, 0AAC:0093): palette black, screen erased; 100 = the copy protection; 1..6 transitions (2A31:0CD1), 7..0x13 story (0CC7), >= 0x14 the sultan's daughter (0CDB, n - 0x13): nis.c, with its own poll routine (2D7D:49F6: Esc or space end the scene, Alt-L ends it and asks for a restore (DS:1392), Alt-S toggles the sound, other keys are dropped): result 2 (the title starts the game); then 0FB3:293A and 1286:07CE |

Death and restart: the core (kidctl.c / game.c) counts the dead prince to "PRESS KEY TO CONTINUE" (DS:5CDC = 0x258,
0FB3:20A4, blinking with a beep below 0x78); a key or the action button then asks for the restart (0823:02BE,
DS:5CD8); 169B:120C restarts (or leaves for the title when DS:6B6C is 0); the countdown running out leaves for
the title too (169B:0B0C). A level ends when DS:5CEC != DS:0998 and the level-end sounds are done (169B:052E).

## 3. Keys (0823:02BE, 0823:0528; in `hotkeys_02be`, which replaces input.c's partial one)

Every keystroke first passes 2797:00E2: Ctrl-Q (0x11) and Alt-Q (0x1000) quit the program (2797:000A).

| key | DOS code | what |
|---|---|---|
| Esc | 0x1B | pause: DS:2B96 = 1, then 0823:10C4 shows "GAME PAUSED", stops the sounds, waits for the poll routine, which is 0823:10A0 itself: the key goes through these keys again (Esc there pauses again, nested; a key after a death restarts) |
| space | 0x20 | the time left (DS:5CD0 = 1, only once DS:016A >= 0) |
| Alt-A | 0x1E00 | restart the level (unless the level-end music plays) |
| Alt-R | 0x1300 | back to the title (DS:6B6C = 0, DS:016A = -1, restart) |
| Alt-S | 0x1F00 | sound on/off (0823:0AF0: 194C:3380(15/0), PRINCE.OPT), "Sound On/Off" |
| Alt-M | 0x3200 | ambient music on/off (1611:07D4; "Music Unavailable" without MIDI), PRINCE.OPT |
| Alt-O | 0x1800 | options menu (0D5E:0696), not in a demo |
| Alt-G | 0x2200 | save (0D5E:060A), if the menus are allowed (0D5E:0516: clock running, no falling objects) and the prince lives (0D5E:0CD4) |
| Alt-L | 0x2600 | restore (0D5E:0544); during a demo: DS:1392 = 1 (the title restores) |
| Alt-H | 0x2300 | hall of fame (0D5E:1BEA(1)) |
| Alt-J / Alt-K | 0x2400 / 0x2500 | joystick (0823:0B1A: "Joystick Mode / Not Found / Unavailable") / "Keyboard Mode" |
| Alt-V | 0x2F00 | "PRINCE OF PERSIA 2 v1.1" |
| Alt-N | 0x3100 | next level: only up to level 3 without the cheat word (which also caps the clock at 15 minutes); with it, level 14 goes to 1 |
| any key / button | | after a death (Kid alive > 6, time left) or during a demo (then DS:2BAA = 1): restart / leave the demo |

Messages go to the status line (0FB3:204C) with DS:5CDC = DS:5CDA = 0x18. With the cheat word (0823:0528):
`+` / `-` minutes, `I` upside down, `R` "Room n", `T` one more hit point (0823:0F16, music 0x65), `W` feather fall
(0823:13C4), `r` revive a dead prince, F3 the demo player on/off ("PLAYER ON/OFF"), `K` one hit point less (hp delta
-1, sound 0x1F, 0823:1008 / 0F38; seq 0x47 at 0), `g` the opponent one more hit point and maximum, `k` every
character of the drawn room dies (skeletons collapse 366C:1166, charid 10 revive timer 0x1E0 and seq 0x6B, heads
face away with hp 0 and seq 0x9A, the others seq 0x55), `S` on kinds 2 / 6 (the turn counter at 7 and a turn counted,
2F86:0078: the spirit leaves with more than 4 hp, else death; palette 2000 at 0x30). Verified: CK1 (level 1: g, K, k,
T, K) every tick identical, CK10 (level 10: S) identical but a tick's drawing scratch (the letters are movement keys
too: the captures press them between two ticks). Not kept: `B` (blank / redraw toggle, DS:2B92), F1/F2/F5..F8 and
Alt-D (the debug displays' flags DS:10CC..10DA, read only by 18C8, and dumps), SAYCHEESE/2BA2 nudges, WATCHMEM.

## 4. Demos (15DB)

PRINCE.DAT untyped resources 25, 26, 27 (levels 1, 4, 8), in turn (DS:2AD8). Byte 0 the level, word +1 the last
tick, word +3 the length, then records at +5: a word tick, a byte count, then count x {byte character (0..4, 5 =
the prince), word input}. 15DB:0316 arms a demo from the title (mode 4, DS:2BA8 = 1, DS:6B6C = its level);
15DB:0278 starts it at the level's start (mode 3, the tick counter DS:5D04 = 0, the seed = 1 (2751:00D6(1)), one
`random(0x11)`, the checkpoint dropped). 15DB:0064 (`ovl_15db_64`, called by the core in the prince's and every
character's control) replays: the record at the read position is passed once the tick is beyond it; at its tick
the entry for this character (in order) sets its slot; the slot always sets ctrl1_forward/backward/up/down = the
2-bit fields - 1 and ctrl1_shift = bits 8..12 - 2. 15DB:000C (`demo_timing_check`, in kidctl.c) ends it after the
last tick (-1: the level loop ends). A key during a demo restarts it and sets DS:2BAA (the title then starts level 1).

## 5. The screens (0D5E, menu.c)

Common: 04B2 enters (sounds stopped, the game's palette 16..255 put aside and black (0FB3:29B8), poll routine =
0D5E:0390 (only Esc and space count; Ctrl-Q/Alt-Q quit), status line erased, a new full-screen offscreen port);
03DC draws the frame (SHPL 8000's images 1 at 0,0, 2 at v=192, 3 at v=8 and mirrored at h=312; inside filled
with colour 0xC); 022E shows it: DAC 0..31 black, copy to the screen, fade banks 0-1 in (2631:037E, 64 steps, one per
retrace); 02B4 fades out; 02D4 leaves (the game's offscreen port again, colours 0..15 from PALS 10 (0FB3:293A),
the game's palette back (0FB3:294C), optionally chars[0..4].f26 = 0 (0993:118C), switch_room, DS:5CCE = 1).
Text: 0D5E:00B4 = a string three times (font n in colour 0 one pixel right, font n in 0xE, font n+1 in 0xF; the
"alt" variant 2 / 0xD / 4) justified in a rectangle by 194C:64FE (see 7).

- Saved games (0544 restore, 060A save): the list screen (0850: title TXT4 8004 / 8000 in font 12, "TAB to
  select" (8001) 18 pixels above the footer, "ENTER to load/save" (8013/8003) left, "ESCAPE to cancel" (8002)
  right), ten slots in two columns (0742: left 0x13 / 0xA3, width 0x89, top 0x25 + 22 i, height 0x14; 0816: fill
  0xB frame 6, highlighted fill 0xE frame 3), 09AE chooses: TAB next, Enter (an empty slot beeps), Esc cancels,
  Home/End/arrows move (restore only; saving, the arrows edit the name in a 25BF line editor of 24 characters).
  Restoring (0CF2) sets DS:6B6C / DS:5CEC to its level and DS:5CD8 (restart), then asks the copy protection.
- PRINCE.SAV: 0xFC-byte header (a word, then 10 names of 25 bytes at +2) and 10 slots of 0x2597 bytes at
  0xFC + 0x2597 i: the checkpoint block DS:5AB2 (+0 index (0: no checkpoint), +8 level, +9 max hp, +A direction,
  +C sword type, +F upside-down count, +17 tiles 0x3C0, +3D7 attributes 0xF00, +12D7 room records 0xE80, +2157
  guard spawns 0x440) with +4 minutes, +6 clock ticks, +B DS:5CB9, +11 DS:016A. Without a checkpoint the block is
  zero but +8 level, +9 start hp, +A the level's start direction (DS:441A). Loading copies it to the checkpoint (or
  drops it for index 0), sets the clock, level (DS:0998), hp (DS:6B71, Kid +12/+13), DS:5CB9, 5CBA, 5D38, 016A and
  DS:5CB6 = 1 (a restored game: the level loop skips the scene and keeps the clock; the first clock tick clears it).
- Options (0696 / 1E88 / 1D7A / 21CE): TXT4 8016 title, six items (1D2C: two columns at 0x26 / 0xC6, rows 0x28 + 22 i):
  Sound On/Off (DS:2086), Ambient Music On/Off (DS:2B98), Keyboard/Joystick Mode, Save Game, Resume Game (=
  restore), Quit Game; a pointer (SHPL 1000 image 5, mirrored, in palette bank 1) left of the item. TAB / arrows /
  Home / End move, Enter toggles (a beep when nothing changes; Save only when allowed), Esc resumes. PRINCE.OPT: two
  words, sound on (DS:2086 == 15) and ambient music on (0D5E:20F4 writes after every options menu, Alt-S, Alt-M;
  2166 reads at start).
- Hall of fame (1BEA show, 1684 enter): PRINCE.HOF = a word count (<= 5) and 29-byte entries {name 27, word minutes
  left}, best first; 19F4 inserts before the first entry with fewer or equal minutes (a full list always takes the
  new entry, into the last place); the new name is typed (18FC, 24 characters, not empty); music 0xFD; shown 0x384
  ticks or until a key. From the title the list shows only when not empty.
- Copy protection (1288, the story "scene" 0x64 asked before level 3+ when DS:0366 is 0, not in a demo; DS:0366 = 1
  once asked): PRINCE.DAT untyped 8000 = a count and {page, symbol} word pairs; one drawn by `random(count - 1)`
  (redrawn once if equal to the last), "Select the symbol that appears / on page %d of the manual" (TXT4 8005 /
  8006), the ten symbols (SHPL 8000 images 4..13 at v = 0x45 + 42 row, h = 0x3E + 42 column), TAB / arrows /
  Home / End, Enter; three tries, then the program quits with "Copy protection failure."
- Title credits (225C / 23C8 / 2402): five pages of TXT4 lists (1000..1006, 1007..1008, 1009..1010, 1011, 1012) from
  pen (37, 16), each list's first string left in (pen-19 .. pen, 16..304), the others right-justified one line (21
  pixels) down; one-string lists centred (1011, 1012 and 1020..1026 in the whole inside); music 0x10C; the first
  page fades in, the others dissolve (33B9:0000 over 90 ticks); each shows 0x1E0 ticks; a key (Esc/space) starts
  the game. (With BLINK on the command line the first page's DS:03B0 line blinks instead.)
- Line editor (25BF): TextEdit-like: printable characters inserted if they fit the width and the length limit,
  Backspace, Del, Left/Right, Home/Up/PgUp start, End/Down/PgDn end; a beep otherwise; the caret blinks every 30
  ticks (DS:24DA / 2); the fields draw with 0D5E:000C (the shadowed text, "alt" when the port's colour is not 0xC).

## 6. The status line (0FB3:204C / 20A4 / 2104 / 2136, text.c)

Rows 193..201 of the screen (not the offscreen buffer). A message: upper-cased, the message area (DS:098E = 193, 98,
202, 235) filled with colour 0, then centred horizontally, at the bottom (194C:5334 vjust 1, hjust 0) in the screen
port's default font. "PRESS KEY TO CONTINUE" ("PRESS BUTTON" with a joystick) goes in the whole line (0..320) unless a
restart is pending; 2136 clears the area (the whole line for the death message) and with its argument resets
DS:5CDC / 5CDA. The time: "N MINUTES LEFT", "N SECONDS LEFT" ((ticks + 1) / 12 in the last minute), "1 SECOND LEFT",
"TIME HAS EXPIRED!". game.c / kidctl.c call `shell_status(op)` where the game shows these (weak no-op without the
shell). The hit points (0FB3:24EA / 25D4) are the renderer's.

## 7. Text, fonts, resources (text.c, loader.c)

- FONT resources (PRINCE.DAT 10..13, 100): [0] first character, [1] last, words ascent, descent, leading, extra
  advance, then per character a word offset to {height, width, word, rows of (width + 7) / 8 bytes, high bit first}.
  A glyph is drawn at (pen v - ascent, pen h) in the port's colour where its bits are set; the pen moves by width +
  extra. Font 0 (the ports' default, the status line's) is not a resource: PRINCE.EXE's data at 3891:0D20 (file
  0x3B220), characters 0x1B..0x7E, 7 + 2 pixels, extra 1.
- 194C:64FE, text in a rectangle: lines broken by 537A (after '\r', after '-', at spaces; right-justified text
  breaks before a space), a left-justified line after the first drops one leading space (two after a '.'); total
  height = lines x (ascent + descent + leading) - leading; vjust < 0 top, 0 centre (rounding up the half height and
  down the text's), > 0 bottom; hjust likewise per line with the line's width.
- TXT4 (2751:0050 -> 194C:85BA / 8652): a word length, then a bit stream (high bit first) from bit 2 of the next
  byte, whose top two bits + 5 are the escape width: 4-bit codes index the table DS:2500 (" aetonisrdlhugfcwyp...");
  code 0 makes the next code escape-width bits (8: a raw byte). Lists are a count byte then NUL-terminated strings
  (194C:8580). TXT4 10 is the cheat word, 1000..1026 the credits and the ending's texts, 8000..8016 the menus'.
- Shape sets (SHPL: first SHAP id, count, a mask word the loader fills, a byte 16, then 16 colours from +7) install
  their colours in the palette bank of the mask (26BC:0034 / 194C:082A); images are SHAP first + index - 1.
- Resource files: 2797:01D4 opens (a list, searched newest first by `res_get`), 2797:01B6 closes. Note: `dat.c`'s
  `dat_find` reports each resource one byte short (an index entry is a checksum byte and `size` data bytes);
  `res_get` adds it back (the last byte of TXT4 streams and of images matters: see 10).
- CONFIG.DAT (SETUP's 16 words, DS:[1FB8]): +2 input device, +6 digital card (-1 none: sound off), +8 MIDI type
  (0x21 here; 0x20 or 0x29 choose other music files and make the joystick "unavailable" with digital sound).

## 7b. What a level load loads (1286:01F2; the renderer's and the sound's side)

1286:0D7C(3) purges the level's resources; a new level drops the checkpoint (not for a restored game) and sets
DS:0998; DS:5CEC = DS:0998; 1286:0050 the game's offscreen port (DS:097E, 320 x 192); 1286:02EE the level (PRINCE.DAT
untyped 0x7CF + n, + 0x14 with GAMEPLAY on the command line, 0x2EF9 bytes to DS:2BB8), 1286:05E2, the checkpoint
(0D5E:11A0); 0AAC:00AE (a level-name decision: the status line or the whole screen is erased); 1286:017C the
prince's second set (PRINCE.DAT SHPL 3000, slot 1, mask 0x4000; colours 0xE0.. from it but on level kind 6);
1286:0006 the scenery file for the kind (DS:059C[kind]: DESERT, TEMPLE, CAVERNS, RUINS, ROOFTOPS, FINAL.DAT; the
previous one closed); 1286:00A2 the tiles (SHPL 3500 of that file, slot 4, mask 0x3FF0 / kind 5 0x7FF0 / kind 6
0xFFF0; colours 0x40..0xDF from it unless a room description is kept), 1286:0122 the kind's extra sets and
sounds, DS:0638 / 0646 per kind; 1286:066A the prince (KID.DAT SHPL 25001, slot 2, with images 0x83..0x84 and
0xD8..0xDA marked; kind 6 opens DS:2EE8's file), 1286:06F0 the kind's extra prince images; 1286:087E the guards
(DS:0672[level type]: GUARD, FLAME, SKELETON, HEAD, BIRD, JINNEE.DAT); 1286:0D06 the sword type; 1286:0454 the sword
images (FRAM 1000 / 1200); 1611:0350(0x1A) sounds; 1286:043A the tile pieces (PIEC 3500); a new kind: 1286:0364
(the kind's sounds, DS:08D6 / 08E4) and 1286:07CE (colours 16..31 = KID.DAT PALS 25001 entry kind - 1); 1286:03B6 the
kind's initialiser (overlay); kind 1: 33FD:0324; 1286:0592 the kind's tile drawers (DS:045E / 046C -> DS:6188..).
A reload (1286:0332: a restart) only does 02EE, 0D06, 0454, 0FB3:29B8 and erases the screen.

The resource "slots" (1286:0632, DS:6166..: 26BC:012A shape lists with per-image load masks) and 194C's purgeable
handles decide what stays in memory; the only effect on the game found so far is KID.DAT 25065's placeholder image,
whose width comes from the heap (FINDINGS 5.13b). SETUP.DAT and SETUP.CFG are SETUP.EXE's; PRINCE.EXE reads only
CONFIG.DAT (and PRESETS.DEF, DIGI.DRV, MIDI.DRV for the drivers).

## 8. What is platform only (not transcribed)

194C (graphics primitives beyond text.c's subset, the resource manager's memory handling, the event queue, sound
drivers: see audio.c), 2812 (the C library: files, printf, malloc), 2B15 (RTLink decompression), 2A31 (overlay
thunks), 26B5 / 0823:164B (joystick), 0823:1587..1636 (interrupt handlers), 2699 (offscreen ports), 2631 (palette
fades: `sh_fade` keeps their timing: 64 steps, one per retrace, interruptible), 2768 (dialog boxes; `sh_key` is
2768:02CA), 2583 / 04BA / 25A1 (image blitters: `gfx_draw_shape`), 18C8 (debug displays), 2D3E:0000 / 007C (SBDIAG
and driver error messages), memory and video checks.

## 9. Verification (the oracle)

`tests/shelltest.c GAME_DIR SCRIPT WORDS...` runs the shell on an oracle script (the same key / shot lines);
`SHELL_CMP=capture` compares the whole game state (every `snap_fields` field but the drawing scratch obj_*,
curr_tile/room, tile_col/row, as e2e's frozen ticks) at every tick start with the capture's ds_tick probes,
`SHELL_SYNC=1` maps each key to the same distance after the same tick as in the capture, `SHELL_SEED=cbe2d` (the
oracle's clock), `SHELL_FILES` (scratch directory). Scripts in ~/pop2dec/oracle/shell (the local oracle for screen
shots; oracle/shell/rcap.sh runs probe-only captures on jaffanator2, whose oracle-run predates `shot` and DS-relative
probes).

| capture | what | result |
|---|---|---|
| ATTRACT, ATTRACT2c | `prince`, no key: the attract loop | scenes 7, 4, 8, demo, credits, hall of fame (empty: skipped), scenes: same sequence (frames: scene 7 6713 / DOSBox 6745, the demo 3841 / 3993, the credits 3352 / 3397: disk and CPU time); the first demo (level 1, 584 ticks, the prince and the guards on recorded input) identical tick for tick but 3 ticks (two lateness-meter ticks of the slower emulated CPU, one ambient-music draw a tick early) |
| SHOTS1 | the five credit pages | all five pixel-identical |
| MENU1 | `yippeeyahoo LEVEL1`: options (TAB x2, Esc), space, Esc pause + key (the prince had died: the restart), save "ab", restore (cancel), hall of fame | 158 ticks identical but one tick at the end (the Alt-H key reached the game one tick later: the key race e2e also meets); options, save, restore and hall-of-fame screens pixel-identical (but the blinking caret); "GAME PAUSED" and "GAME SAVED" identical |
| CP1 | `yippeeyahoo LEVEL3`: the copy protection, TAB x2, Enter | the question (same page drawn: the RNG state matches) and the highlight pixel-identical; the level then identical (tick 0's drawing scratch aside, see 10) |
| SAVELOAD1 | LEVEL3: save "x", walk, restore it (level reload from the saved state), walk | 139 ticks identical (tick 0 of both level starts: drawing scratch) |
| ATTRACT3 | `prince`, 62000 frames: the whole attract cycle, three demos (levels 1, 4, 8: 584 + 747 + 380 ticks; demo 2's copy-protection "scene" skipped in a demo) | following the platform's timing as e2e does (the lateness meter, the ambient music's draws within 4 of the capture's), every tick identical but drawing scratch (char_x_left/right(_coll), char_col_*, char_top_y/row: 36 ticks) |
| DEATH1 | `yippeeyahoo LEVEL1`, no key: the prince dies, "PRESS KEY TO CONTINUE", 600 ticks of countdown with the blinking message and its beeps (194C:840E(0xFFFE) every 12 ticks) | 769 ticks identical; the status line identical |
| TIMEOUT2 | `yippeeyahoo LEVEL4`: copy protection, Alt-N (scene 0x14, level 5, the clock starts: DS:016A = 0), 80 `-` presses (cheat) to 1 minute, the clock runs out: the time messages, 169B:123E, scene 0x1C, the title | 754 ticks identical but the `-` presses reaching the game a tick earlier or later (the key race: minutes_left / DS:5CDA for 30 ticks) and the first tick's drawing scratch; scene 0x1C then the title's scene 7 in both |

| ENDING1 / ENDING2 | `yippeeyahoo LEVEL14`, DS:5CEC poked to 15 (the level won): scene 0xB, the hall of fame entry, "zy" typed, Enter | the same flow (the ending scene 4701 / 4778 frames); the entry screen before and after typing pixel-identical (the caret aside); PRINCE.HOF = 1 entry "zy", 75 minutes; the title 4 frames apart |

In-game shots (MENU1 m985 / m1400, DEATH1 d3700..) compare the picture the renderer (render*.c, not the shell's) draws
under the status line: their differences are in rows 0..191, the status line itself matches.

Plans (tools/plan2script.py `probepoke ds_tick N` lines: the key table and the BIOS shift flags at the N-th tick)
are played by shelltest too, and `SHELL_VRAM=CAPTURE.frames` compares the screen with a tools/framecap.py capture's
VGA dumps (VRAM_STEP) or RGB shots (SHOT_STEP, the palette included) at the same distance after the same tick, and
+-2 frames around (SHELL_CMP=CAPTURE.frames takes the capture's ds_tick records: framecap.py with an extra line
`probe 169B 05E0 ds_tick 3DB50 4300`; SHELL_CMP_LABEL=pre_ds for captures without them, timing only;
SHELL_VRAM_OUT=dir writes the differing screens). P2_raft (level 2 with the puzzle solved and the raft, FINDINGS
5.17): 1300 of 1350 VGA dumps exact, 44 within two frames, 6 differ (the room switch's timing).

`tests/run_shell.sh` reruns all of these (the captures under ~/pop2dec/oracle and ~/pop2dec/oracle/shell).

The core's e2e captures stay identical with the core changes of 10 (tests/run_all.sh's e2e set).

## 10. Changes in the core's files (for the shell) and findings about them

- input.c: `hotkeys_02be` -> `hotkeys_02be_core` + a weak `hotkeys_02be` (shell.c has the whole 0823:02BE).
- glue.c: `ovl_15db_64` and `restart_prompt` weak (shell.c: the demo player, 169B:123E); `frame_on_time_hook`;
  weak `demo_timing_check` and `shell_status`.
- kidctl.c: 15DB:000C (`demo_timing_check`) where the prince's control ends; `shell_status(3)` at 0FB3:20A4.
- game.c: `shell_status` at the time message (0823:0E58) and the blinking press-key message, 169B:0B9C's beep
  (`sound_res_start(0xFFFE)`); FIX: 169B:0A30 at DS:5CDA == 1 calls 0FB3:2136(1), which resets DS:5CDC and 5CDA
  (the core left the countdown at 1, so a message's countdown never ended; no capture reached it); FIX: the flip
  redraw (DS:5CCE) ends with 0FB3:259C, whose state part loads the prince's opponent into Opp (0AFF:080A) and Char
  (0FB3:25D4's load_char) (`hp_display_state`; seen after the save menu).
- level.c: `load_level_ex(n, full)` (the level loop's full / reload choice; `load_level` unchanged in effect);
  1286:0213 keeps the checkpoint for a restored game (DS:5CB6); FIX: 1286:0D06 takes the sword type from the
  checkpoint block when there is one (DS:5AB2 is the checkpoint's far pointer, not an unused override);
  `checkpoint_get_block` / `checkpoint_put_block` (the DS:5AB2 layout, for saved games).
- core.c: `pop2_reset_state()` (the program's start state, used by `pop2_new_game_loaded` and the shell).
- roomhooks.c: `room_unload_pub` (0CD6:073A, the options menu).
- Found, not changed: DS:2BA2 (`input_device`) and DS:2BAA (`word_2baa`) are separate globals in the core although
  they lie inside tiles0 (DS:2B9A..2BB7, compared as a whole); the shell's DS:2BA6 uses tiles0 + 0xC. `dat_find` returns sizes one byte short (see 7; images decoded through it may lose their last
  byte: the menu frame's side strips lost their last rows until `res_get` added it back); the renderer's
  `render_frame` changes game state (obj_* and the characters' drawn positions) - the shell saves and restores the
  state around it; at a level's first tick the prince's drawing pass (0993:07F8 -> 0A8E ...) leaves image_height /
  width, char_x_left/right(_coll), char_top_y, char_col/row values the core does not compute (scratch the next tick
  recomputes; FINDINGS 5.13).

## 10b. For the core: 37F0:007C (OVL14, level 8 room 9, reached through 2A31:0E1B where kidctl.c notes it)

The sword scene: the level (0x2EF9 bytes from DS:2BB8) copied aside; `sh_scene(6)` (the kid given to nis.c by
0AAC:0442: DS:5B37 / 5B38 / 5B3A, DS:6116); a key-cut scene (2) sets drawn_room 0; 169B:018E; 1286:03B6(5, 4) (the
kind initialiser); loadkid; Char.direction 0; seq 0xE7; Char.room = next_room = 9; char_y_to_floor; Char.hp_delta =
Char hp; DS:0998 = 8, the full load 1286:01F2(8), then the copied level put back; DS:2BB2 = 1 (the sword taken);
unless cut short: room_load(9) (0CD6:02BE); 1375:0F5A(0xA, DS:5B51, 1375:0DC6); 1375:0E8C(0x19); play_seq; Kid =
Char; 0AFF:1376; ctrl1_shift = 0; music 0xFF unless it plays (then 1611:0826(0xFF, DS:2B98)). Not written here: it is
tick code; `sh_scene` is public for it.

## 11. Missing

- The dissolve (33B9) takes one step a frame here; the original takes as many as the CPU allows (nis.c models the
  CPU time for the scenes); the pattern is the same.
- The level name / 0AAC:00AE drawing decisions, 1286:07CE (the level kind's graphics after a menu or scene) and the
  hit points are the renderer's (`render_after_menu` weak).
- Disk and drawing time: the oracle loads levels and draws more slowly (e.g. level 3 starts ~77 frames later),
  so frame-exact timelines differ; tick-relative behaviour matches.
- The joystick (never found here), the debug keys and displays (18C8), SBDIAG, memory/video errors.
- The scenes' room hook (0AAC:0376, transitions 2 and 3) is the weak `shell_nis_room`: it loads the level, switches
  to the room (its description hook drawing too), calls the renderer's full redraw and puts the game state back;
  compared with the oracle's C2 / C3 shots (docs/NIS.md): the room exact, the game's load time not modelled.

# Story scenes (NIS)

Reconstruction: `src/nis.c` (API in `src/nis.h`), test `tests/nistest.c`, tools `tools/nis*.py`.
The DOS game plays its story scenes ("NIS", non-interactive sequences) from overlay **OVL00**, loaded at segments
2D3E (start-up), **2D7D** (the scenes), **32D4** (the animation player) and **33B9** (dissolves). Addresses are runtime
`SEG:OFF` (as in the oracle probes and `~/pop2dec/work/ovl00_2D3E.asm`, the listing made from the relocated overlay
image `~/pop2dec/image/ovl00_*.bin`). The resident library they call is segment 194C (graphics, resources, sound,
timer), with 25A1 / 2583 / 26BC (images), 2631 (palette fades), 2797 (waits, resource files), 2751 (rect helper).

## Who plays what

`play_scene(n)` = **0AAC:0274** (resident). It blacks the palette and erases the screen first unless n is 1, 100 or 6
(0AAC:0080, which also reads an uninitialised local), plays the scene, erases the screen after unless n is 1, 100, 2, 3
or 6 (0AAC:0050), then restores game state (0FB3:293A, 1286:07CE). Return: 2 when a key stopped the scene.

| n | code | what | callers |
|---|------|------|---------|
| 1..6 | 2D7D:0708 (thunk 2A31:0CD1) | transitions from TRANS.DAT (`_SCR` 25001..25012) | 0AAC:0120 by level (see below); 4 in the intro; 6 from OVL14 (level 8 room 9, 37F0:037F7C) |
| 7 | 2D7D:259C | story 1: "In an ancient Persia..." (NIS.DAT 27xxx pictures, texts STRL 27000, speech NISDIGI 27001..) | intro |
| 8 | 2D7D:2E27 | story 2: the Prince returns, Jaffar's spell | intro |
| 9 | 2D7D:1C0F | story 3 (NIS.DAT 28xxx, music 28100/28101) | 0AAC:0120 at level 1 |
| 10 | 2D7D:4A95 | story 4 (29xxx, anims 29001/29002): "Meanwhile..." | 0AAC:0120 at level 3 |
| 11 | 2D7D:41B1 | the ending (30xxx, anims 30001/30002; STRL 30000/31000) | 0823 main loop after level 14 (`> 14`) |
| 20..28 | 2D7D:04F9 (thunk 2A31:0CDB) with n-19 | the sultan's daughter (NIS.DAT `_SCR` 26001..26009) | 0AAC:0120 by time left; 28 from 169B:123E |
| 100 | 0D5E:1288 | the copy protection question (not a scene) | 0AAC:0120 |

- The intro (0823:01CA): `play_scene(7)`, then 4 (the title), then 8, each only if the one before was not stopped by a
  key. The music of scene 7's end (25011) keeps playing through 4 and 8 and drives their timing (cue points).
- Between levels (0AAC:0120, param 2 = the new level): level 1 -> 9, 2 -> 100, 3 -> 10, 5 -> 1, 9 -> 2, 13 -> 3
  (jump table 0AAC:0168); from level 4 on, a daughter scene 20 + k when `k = (1 - DS:5CD2) / 9 + 7` (DS:5CD2: the
  minutes left, negative) exceeds the last one shown (DS:016A); DS:016A = 0xFF forces 20.
- Cheats (with the `yippeeyahoo` argument): `NISn` plays scene n forever, `TREEn` plays 19+n (0823 main loop).

## Resources

DAT files: the resource files open form a chain searched from the most recently opened (194C:6F4C; 2797:01D4 opens,
01B6 closes). Each body follows a checksum byte (the sum of checksum and body is 0xFF) and is **the whole size field**
long (src/dat.c's `dat_find` reports one byte less).

The scenes open NIS.DAT (20..28, 7..10), TRANS.DAT (1..6), CAVERNS.DAT / RUINS.DAT (transitions 1, 2); the sound files
by the configuration (2797:0260 on DS:1394: NISDIGI.DAT with the digitizer, NISMIDI.DAT for MIDI music (NIS3VC.DAT for
music device 0x20/0x29), NISIBM.DAT without both).

| type | layout |
|------|--------|
| `_SCR` | an animation script: rect (top, left, bottom, right) where it shows, then ops (below) |
| `_PSL` | sound ids to preload for the script of the same id: count word, ids |
| `SHPL` | a shape list: first image id (word), count, palette-bank mask (word, set by the loader), colour count byte, colours |
| `SHAP` | images: height, width, flags words; flags low byte 1 = the 8-bit row-run form (below) |
| `PALT` | 768 bytes, 6-bit VGA palette |
| `STRL` | strings: a count byte, then NUL-terminated strings (1-based, 194C:8580) |
| `FONT` | first char, last char, ascent, descent, leading, spacing (signed words at +2..+9), char offsets (+10); glyph = height, width, stride word (0 in the file), rows of 1-bit pixels, (width+7)/8 bytes a row, high bit first |
| `SND` | byte 0 = type: 1 digitized (rate word at +1, packed samples at +10 with their unpacked length first), 2 MIDI (a standard MIDI file follows; bit 7 = loop) |

### Images (194C:0AFE unpack, 2583:0006 / 194C:6D42 draw)

Row-run images (flags low byte != 0; all the NIS images, 0xF301): the packed data is in chunks of
`0xE37E / ((width + 1) * 2)` rows, each a length word (its unpacked size) and an LZG (method 3) or RLE (1) stream with
its own window. Unpacked: per row a word (the row's byte count) and packets: 0..7F copy n+1 bytes, 80..FF repeat the
next byte (n & 7F)+1 times. 8-bit ones (flags bit 15, depth bits 0x70) get their colours moved to the shape list's
palette banks (194C:122C: image bank k -> the k-th set bit of the mask; colour 0 stays). The blitter 2583:0006: mode 0
copies, any other mode leaves the *runs* of colour 0 transparent (literal zeros are drawn).
Packed-pixel images (1..8 bits, 194C:0B28) become one byte a pixel; 194C:6D42 draws them: mode 0 copy, 10 transparent
0, 8 paint colour 0, else and/or/xor.

## Graphics library (194C, QuickDraw-like)

Ports (0x2C bytes): +0 bits (far), +4 bounds rect, +0xC rowbytes, +0xE row offsets, +0x10 portRect, +0x18 clip rect (in
pixel coordinates, bounds-relative), +0x20 background colour, +0x22/+0x24 pen v/h, +0x26 text mode, +0x28 foreground
colour, +0x2A font. thePort = DS:2450; the screen port DS:5D0A (320x200 at A000:0000). Rects are (top, left, bottom,
right).

| addr | |
|------|-|
| 194C:3890 NewPort(rect) | offscreen port: bounds = portRect = rect, clip = its size (194C:4FC2 copies the defaults from DS:247E) |
| 194C:52E8 SetOrigin(v, h) | shifts portRect and bounds |
| 194C:4C34 / 4C9A | clip &= rect (saved on the caller's stack) / restore |
| 194C:6632 FillRect(rect, colour) | on thePort, clipped |
| 194C:4CB0 -> 6698 CopyBits(dst, src, dstRect, srcRect, mode) | clipped to dst's clip when dst is thePort |
| 194C:5266 SectRect, 647E UnionRect, 50EC OffsetRect, 4D50 EmptyRect | |
| 194C:5334 -> 64FE text box(str, vjust, hjust, rect) | lines broken by 194C:537D (after a space, before a space, after '-', at CR), centred (0) / right-bottom (>0) / left-top (<0); line height ascent+descent+leading |
| 194C:4CD2 / 6B06 | draw chars / a glyph (mode 0: set pixels painted in the foreground colour) |
| 194C:79A3 SetPalette(first, count, ptr, wait) | wait = wait for the vertical retrace (194C:7A26) first; ptr 0 = black |
| 194C:7969 GetPalette | always waits for the retrace |
| 194C:13E6 dissolve copy(level, ...) | copies the pixels whose byte of a 16-bit LCG (x*5+1, low then high byte, seed DS:1F1E) is <= level |

## Timer, waits, sound

- 194C:7EE7, the timer interrupt: +1 the tick DS:24E4 (60 Hz, PIT divisor 19886) and counts down the four timers
  DS:24DC..24E2. 2797:0158(k, n) sets timer k and waits for it; 2797:0104(k) waits. Timer 3 paces the palette fades.
- 2797:009C, the key check (DS:4872 handler): a key ends the scene (the scene fades out, 2631:064A).
- Music (194C:2DF0 start, 2F10 interrupt, 2FFA events): the MIDI sequencer runs from a 240 Hz timer (divisor 4971) that
  calls the tick every 4th time. Per interrupt: a 16.16 accumulator `+= inc`, `inc = fdiv(fdiv(1e6<<8, 240<<16),
  fdiv(tempo<<8, division<<16))` (fdiv = 16.16 division, 194C:7C68); the carry is subtracted from every track's delta.
  Meta 7 (cue point) sets DS:2087 = the text's first byte; meta 0x51 (only track 0) sets the tempo **with its low two
  bytes swapped** (194C:316B reads them as a little-endian word); end of track 0 ends the song (the end callback
  194C:34BA restarts it when the type byte has bit 7). 2D7D:0135 waits until the cue reaches a value (at once when no
  sound plays).
- 194C:840E plays a sound by id, 8426 tells whether one plays, 83D2 stops, 805C preloads. Digitized sounds play for
  their unpacked length / rate (the model: `ceil`, + 1 tick).

## Palette fades (2631)

2631:037E fade(palt, mask, delay): 03E6 reads the current colours of the banks in `mask` (one GetPalette each; 0xFFFF:
one for all), then 64 steps (04E0), each once timer 3 has run out (then set to `delay`): out = start + (target -
start) * step / 64 (16-bit product, shifted toward zero), written by 2631:0296 (0xFFFF: 256 colours, 0xFFFE: 240 from
16, 0xFFF8: 208 from 48, else bank by bank, a retrace wait every 4 banks). Target: a PALT or black. It is skipped when
start == target (all 768 bytes compared). 2631:064A = fade to black then erase the screen.
An anim can carry a fade (anim+0x4BC, set by callbacks 2D7D:184C / 1885), stepped with its redraws.

## Animation player (32D4)

`play_anim(shapes, base, n, with_bg, cb0, restore, flags, abort)` = **32D4:0BE4**: script `_SCR` base+n, images 1..count
of the shape list, sounds `_PSL` base+n preloaded. 32D4:0524 builds the anim: a port the size of the script's rect
(origin 0,0), a copy of the screen under it when with_bg (restored into the dirty rect each redraw; else filled with
the parent's colour), 32 members (0x22 bytes at +0x78). The loop: run the script (32D4:00B8) until it waits; abort
(a key or a callback returning nonzero) fades out and ends.

Member: +0 image (1-based), +2/+4 first/last image, +6 flags (bits 0-3 draw mode (2583 blit mode), 4-7 move period,
8-11 image period, 12 backwards, 13 ping-pong), +8/+9 counters, +A x, +E y, +12 dx, +16 dy, +1A ddx, +1E ddy (16.16).
Reset (32D4:0BB0): flags 2, x = y = 0x8000:0000.

A frame (32D4:0852): count it, remember the tick; redraw the dirty rect (32D4:09D0: background, members 0..31 in order,
then callback 0 with DS:6934 when that is set); copy the dirty rect to the screen; clear it; then for each member: every
(image period + 1) frames step the image (wrapping or ping-pong), every (move period + 1) frames add the velocity (the
member's old and new rects go dirty) and the acceleration to the velocity.

| op | bytes | meaning |
|----|-------|---------|
| 00 | | end, once the frame time (last frame + delay) has passed |
| 01 n d | 5 | frames: when the frame time has passed, show a frame; n frames d ticks apart (d applies from the next) |
| 02 t | 3 | delay t ticks (also moves the frame time) |
| 03 id | 3 | when the frame time has passed, play sound id |
| 04 id | 3 | stop sound id |
| 05 id | 3 | wait while sound id plays |
| 06 c | 2 | wait for MIDI cue c (none when no cue has come) |
| 07 | 1 | redraw now (the next frame skips its redraw) |
| 10+k w | 3 | callback k(anim, w); nonzero stops the anim |
| 14+k n rel | 5 | loop k: n times back to op + rel + 3 |
| 20+m and or | 5 | member m flags = flags & and | or |
| 40+m dx dy | 5 | move by (whole pixels) |
| 60+m x y | 5 | place at |
| 80+m first last | 5 | images first..last (0 = none, the member resets) |
| A0+m dx dy | 9 | velocity (two 16.16) |
| C0+m ax ay | 9 | acceleration |
| E0+m k | 2 | stamp the member: k > 0 into the screen and the background, k < 0 the background, 0 the anim |

`tools/nisscr.py FILE ID` disassembles a script.

## Dissolves (33B9)

33B9:0000 dissolve(dur, rect, src, mode): into thePort, showing each step on the screen. 33B9:01AC times two steps
once per (mode, area) (DS:6962..) to pick the first level `255 / (2*dur/elapsed + 1)` (0 steps under 6); each step
(33B9:0300) sets the level to 255 * ticks elapsed / dur and dissolve-copies with the seed reset to 0x4583 (mode 1); the
last copies all. Steps run back to back, so their number depends on the machine's speed (the model costs them ~14
cycles a pixel).

## Scene helpers (2D7D) as named in nis.c

Arguments in the order the game pushes them (Pascal):

| game | C |
|------|---|
| 26BC:0876 (list, id, x, y, mode) | `draw_shape(l, id, x, y, mode)` |
| 2583:0006 (img, x, y, mode) | `draw_img(im, x, y, mode)` |
| 25A1:00FA (list, id, 1) | `shape_get(l, id)` |
| 2D7D:000E (id, mask) | `shpl_load(id, mask)` |
| 2D7D:016B | the list 25000 (mask 1) and fonts 10, 11 |
| 2D7D:01AF (base, n) | `show_text(base, n)`: caption bar: STRL base string n (0: empty) |
| 2D7D:009C (id, n) | `play_or_time(id, n)`: play a sound (without a digitizer: a timer of n ticks) |
| 2D7D:00E4 (id) | `wait_or_time(id)` |
| 2D7D:00CA (id, n) | `say(id, n)` |
| 2D7D:4119 (base, t, off, n) | `say_text(base, t, off, n)`: caption t, then say base+off |
| 2D7D:0135 (c) | `wait_cue(c)` |
| 2D7D:3EBF (port, colour) | `pic_fill(p, c)`: fill the picture rect DS:13B0 (32, 40, 152, 280) of a port |
| 2D7D:3EEA (port, list, id, x, y, mode) | `pic_draw(p, l, id, x, y, mode)` |
| 2D7D:4147 (rect*, list, id, x, y) | `shape_rect(l, id, x, y)` |
| 2D7D:4011 (port, dur, rect) | `pic_dissolve(p, dur, &r)` |
| 2D7D:3FBD (port, delay) | `pic_fade_in(p, delay)`: black (banks 1..15), show, fade in to DS:6906 |
| 2D7D:3F52 (port) | `pic_flash(p)` |
| 2D7D:3F9E (port) | `pic_show(p)` |
| 2631:037E (palt, mask, delay) | `fade_to(palt, mask, delay)` |
| 2631:0296 (mask, pal) | `setpal_banks(pal, mask)` (not in push order) |
| 2797:0158 (k, n) | `timer_wait(k, n)` |
| 2797:0104 (k) | `wait_countdown(k)` |
| 2797:0176 (id) | `wait_sound(id)` |
| 194C:79A3 (first, count, ptr, wait) | `setpal(first, count, ptr, wait)` |
| 32D4:0BE4 (...) | `play_anim(l, base, n, with_bg, cb0, restore, flags, abort)` |

## The model (src/nis.c)

- The scene code runs as a coroutine (ucontext, 1 MB stack) that gives control back to `nis_step` wherever the game
  busy-waits. Time is counted in CPU cycles of the reference machine (the oracle: DOSBox-X, 22000 cycles/ms): video
  frames every 313900 (70.086 Hz), the PIT interrupt every 366670 (60 Hz, the tick) or, while a song plays, every 91657
  (240 Hz, the sequencer, chaining to the tick with a 16.16 ratio). Adding/removing the sequencer's timer restarts the
  PIT count (194C:7D76 / 7E37). The scene's own work costs cycles by a rough per-routine model (`cpu()`: image
  unpacking ~15/byte, blits, dissolve ~14/pixel, glyphs, digitized-sound preparation 194C:8118 ~110/byte, fade set-up
  heap work 300000), so loads and dissolves take time as in the game.
- `nis_step` output: the pixels as the frame begins, the palette as set at the retrace that begins it (palette writes
  follow retrace waits). A retrace wait returns at once only right at the frame start.
- Sounds: MIDI (timing of cue points and the end, tempo bug included), digitized (unpacked length / rate). Events are
  reported through `nis_set_sound_callback` (kind, SND id); nothing is played.
- Known not modelled: the game loses timer ticks and misses retraces while the sequencer's interrupt is busy (dense
  MIDI; fades then run ~1.1-1.25 frames a step, `NIS_ISR_MISS` has an optional model of it, off by default), disk
  time, and the machine-speed dependent dissolve step count only approximately. These shift frames by a few; the
  drawing itself matches.

Scene functions: `scene_tree` (20..28, 2D7D:04F9, callback 2D7D:0472), `scene_story1` (7, 2D7D:259C), `scene_story2`
(8, 2D7D:2E27), `scene_story3` (9, 2D7D:1C0F), `scene_story4` (10, 2D7D:4A95, callback 2D7D:5332), `scene_end` (11,
2D7D:41B1, with the prologue `scene_final_intro` 2D7D:466B from FINAL.DAT, callbacks 2D7D:474C / 18E4), `scene_trans`
(1..6, 2D7D:0708; the title 4 with callback 2D7D:536E; 5 = 2D7D:117A, 6 = 02EAAD). `play_scene` = 0AAC:0274's
black-out/erase wrapper; `NIS_INTRO` plays 7, 4, 8 with the music running on.

Engine hooks (nis.h): `nis_set_room_hook` for 0AAC:0376 (transitions 2 and 3 draw a game room: level 10 room 22,
level 14 room 1), `nis_set_kid` for the kid of transition 6 (0AAC:0442, OVL14's placing of script 4210). Without them
the room area stays as the scene left it. tests/nistest.c built with -DNIS_ENGINE (tools/nisrun.py does) links the
whole program and sets the room hook to the shell's `shell_nis_room` (with DS:2BA6 = 1, as 0AAC:0274 sets it).

0AAC:0376 (room, level): 169B:018E; DS:0998 = DS:5CEC = level; the game's offscreen port DS:5CC2 = the current port (the
scene's screen, so the room is drawn straight into it); 1286:02EE (the level), the kind's scenery file opened,
1286:00A2 (the tiles, colours 0x40.. unless 0AAC:00AE), 1286:043A if the kind changed, 1286:03B6, 1286:0592; drawn room
0, DS:6B6D = room, 0823:0E72(1) (the room's description and its hook: level 14 room 1's 33FD:145E fills the port
with 0x8B, the sky), 169B:0430 (the whole redraw); 1286:0EC6(4), (1), 0EAC; the file closed; DS:43FD (the level kind)
= 0; DS:5CC2 put back; 2A31:0D03. Compared (2026-09-24): the room itself is exact in both (scene 2: every difference
of the room phase lies in the horse's animation, e.g. shot 2604: 725 px, all in x 119..195, y 102..185); scene 3's
sky needed the description hook's fill (shell_nis_room lets the room hooks draw: `nis_room_on`) and its horse statue
(the extra piece 0x6372) is off while DS:2BA6 is set. What remains is timing: the game spends ~45 frames loading
the level before its fade (scene 3 shots 1527..1563 black in the game), and anim 11's horse is 2-3 px off from ours.

## Verification (tests/nistest.c, tools/nisrun.py)

Captures (oracle, `tools/nisoracle.py NAME FIRST LAST STEP`, then `cap.sh NAME "prince yippeeyahoo NISn"`, the cheat
replaying scene n; `prince` alone for the intro): N20..N28, A9, A10, B11, C1, C2, C3, C5, C6b, INTRO (frames 60..17000,
every 3rd). The test matches each shot to our frame the same distance after the last checkpoint both sides passed
(anim frames, fades, sounds, texts, dissolves, anims, scenes, cue points, paired by type and ordinal) and also looks
+-10 frames around it. In every capture the checkpoint counts agree with the game's (for 1 the oracle has one more
sound: the cheat's music for the next loop).

| scene | shots | exact synced | exact within +-10 | remaining |
|---|---|---|---|---|
| 7 (intro 1) | 2249 | 2025 | 2218 | dissolve intermediate steps, fade phase |
| 4 (title) | 553 | 462 | 517 | tearing, fade phase at the end |
| 8 (intro 2) | 1840 | 1505 | 1700 | long dissolves (level off by a tick), fades |
| 20..28 | 3052 | 2773 | 3042 | fade phase (a frame), tearing |
| 9 | 1413 | 1267 | 1410 | fades during dense MIDI (lost ticks) |
| 10 | 1870 | 1707 | 1855 | dissolve steps, 2 torn shots |
| 11 (ending) | 1587 | 1358 | 1513 | tearing, fade phase around callbacks, 1-2 frame phase |
| 1, 2, 3, 5 | 2654 | 1992 | 2340 | 2, 3: the game's load time around the room (engine hook) not modelled; anim 11's horse; tearing |
| 6 | 1543 | 6 | 29 | 10 px: the item's palette bank 15 (from the game); 2 px uninitialised memory |

"Tearing": the game's shot caught the copy of an anim frame to the screen half done. All mismatches examined are
timing (they vanish at a nearby frame) except the ones listed for 2, 3 and 6.

## Integration

- The core should call `play_scene(n)` where the game does (0AAC:0274: 0823:01CA for the intro (7, 4, 8, stopping at
  a key), 0AAC:0120 on entering a level (see the table above; 0AAC:000E), 169B:123E (28), the 0823 main loop (11 after
  level 14), OVL14 level 8 room 9 (6)), i.e. `nis_open(dir, n)` and `nis_step` per video frame until 0, then carry on
  with 0AAC:0274's tail (0FB3:293A, 1286:07CE, DS:2BA6 = 0, the level restore of 0AAC:0376/0442 for 1, 2, 3).
  Game state the scenes set: DS:016A (last daughter scene, set by 0AAC:0120 before), DS:2087 (cue), the music (25011 runs
  from 7 into 4 and 8; a scene may leave its music playing).
- src/dat.c: `dat_find` reports the size one byte short (the body is the whole size field after the checksum byte);
  nis.c adds it back. Palettes (PALT, 768 bytes) and MIDI files need the last byte.
- 0AAC:0080 (black-out before a scene) reads an uninitialised local for n = 4; the model always blacks out.
- Workspace: `~/pop2dec/work/ovl00_2D3E.asm` (the OVL00 listing, synced on functions and jump tables),
  `tools/nispseudo.py` (call-level pseudo-code of a range), `nisscr.py`, `nismidi.py`, `nisdat.py`.

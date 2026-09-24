# Story scenes (NIS)

Reconstruction: `source/nis.c` (API in `source/nis.h`), test `tests/nistest.c`, tools `tools/nis*.py`.
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
long (source/dat.c's `dat_find` reports one byte less).

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
- 194C:840E plays a sound by id, 8426 tells whether one plays, 83D2 stops, 805C preloads (loads the resource and, for a
  digitized one, unpacks it: 194C:8118 / 8466). A digitized sound plays for length x trunc(1000000 / rate) us (the Sound
  Blaster's time constant: 90 us at 11000 Hz); its end (the DSP's interrupt, 194C:3422) frees the channel at once
  (the oracle: 80 frames for 26001's 12681 samples, 298 for 27001's 47233, to the frame).

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
once per (mode, area) (one cached entry, DS:6962..) to pick the first level `255 / (2*dur/elapsed + 1)` (0 steps under
6); each step (33B9:0300) sets the level to 255 * ticks elapsed / dur and dissolve-copies with the seed reset to 0x4583
(mode 1); the last copies all. Steps run back to back, so their number depends on the machine's speed. The copy
(194C:13E6) costs 10 instructions a pixel on the LCG's low byte, 18 on the high one (14 on average, the model's figure),
9 a row; the CopyBits to the screen w/2 more a row (rep movsw).

The timing steps are not quite steps: the record (0x52 bytes from the C heap, 2812:003B, never cleared) has its start
tick (+0x3A) and level (+0x3E = 0xFF) left as they are, so 33B9:0300 compares the tick with whatever the heap block held
before. The oracle (probe 33B9:0335, the elapsed time in DX:AX): the intro's first dissolve F0F1F283 (negative: two real
dissolve copies, 108000 cycles each), its fifth 73733913 (past the duration: two plain copies, ~7000 cycles), transition
6's first two 1111314F and 0000033F (plain copies). The model always times two real steps; where the game copied
(transition 6's first dissolve: ~5 frames sooner, the model's ~100 mismatching shots at its start) this is uninitialised
memory, not modelled.

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

## The model (source/nis.c)

- The scene code runs as a coroutine (ucontext, 1 MB stack) that gives control back to `nis_step` wherever the game
  busy-waits. Time is counted in CPU cycles of the reference machine, the oracle's DOSBox-X: 22000 cycles/ms, one cycle
  an instruction, an 8-bit IN 22 more and an OUT 16 more (its ISA I/O delay). Video frames: 22000000 x 800 x 449 /
  25175000 = 313898.7 cycles (mode 13h, 70.0863 Hz), frame g beginning at its retrace; the PIT: the 60 Hz tick (divisor
  19886) or, while a song plays, the sequencer at 240 Hz (divisor 4971, chaining to the tick with a 16.16 ratio), its
  requests counted exactly from the last reprogramming (fractions of a cycle kept).
- The scene's own work costs cycles by a model of each routine, measured in the oracle (probes on the routines' entry
  and exit, `n=` being the instruction count): a resource read from its file ~5.7 cycles a byte + 2150 (194C:6F4C;
  380 when still in memory: the resources of a file stay until it closes, the sounds' until an anim's images purge
  them); an LZG row-run image ~27 cycles an unpacked byte + 1.7 a literal + 31 a match (194C:0AFE, median error 1%),
  a packed-pixel one ~15 a pixel; a font 38.5 a byte more (194C:4F8C: ~130000 for fonts 10 and 11); a digitized sound's
  preparation the decoder's own instruction count (194C:8466, 8 a bit read, per code 3..13) + 2.35 a sample (the oracle
  within 0.2%); mirroring an image (0823:1447 / 1414) from their code; the fade's step 1152 a bank; SetPalette 19
  instructions and 3 OUTs a colour, GetPalette 19 and 3 INs (with the interrupts off: a timer request waits for its
  end); blits, fills, glyphs and the dissolve's copy (14 a pixel, from 194C:13E6).
- The interrupts take their time. The sequencer's handler (194C:2F10) sends each due MIDI event to the FM driver
  (MIDI.DRV, docs/AUDIO.md; the model keeps the driver's voice allocation and channel map to know how many registers a
  call writes) and every OPL register write spins through the driver's two delay loops (+0x8AF, [0x147] = 1033 and
  [0x149] = 517 turns of 3 instructions in the oracle): 4713 cycles a write, 13 for a note-on, so the first chord of a
  song holds the scene for most of a frame (the oracle: 243871 instructions for 4 note-ons, the model 245568). A song's
  start and stop reset the driver (18 writes, ~85000 cycles). While a handler runs the scene is frozen, a retrace that
  begins and ends inside it is not seen by the polling loop (the retrace bit, 3DA bit 3, lasts 2 of the 449 lines), and
  the PIT's next requests wait: the first stays pending in the PIC, the others are lost (ticks go missing). This
  replaces the former optional `NIS_ISR_MISS`.
- Reprogramming the PIT (194C:7D76 / 7E37, mode 3) sets its output high: in DOSBox-X that is an interrupt at once when
  the old count was in the second half of its period (the oracle's N26: the sequencer's first interrupt 23 instructions
  after the OUTs; N21: none, the tick having just come).
- The start: the NISn cheat calls play_scene ~114000 cycles after a retrace and 6000 after a tick (the frames of the
  oracle's first ticks bound that tick to 89600..126300 cycles after the retrace); the intro ~92000 after, with the tick
  229000 later. (Only the phases of the first frames depend on it.)
- `nis_step` output: the screen as the beam scans it (after the retrace 37 blank lines, then the 200 rows each scanned
  twice: a change the scene makes during the frame shows below the beam at once and above it from the next frame; a
  CopyBits, fill or dissolve copy to the screen proceeds row by row at its cost), the palette as set after the frame's
  retrace (the scenes write it right after their retrace waits).
- Sounds: MIDI (timing of cue points, the end, the tempo bug, looping songs), digitized (the Sound Blaster's rate).
  Events are reported through `nis_set_sound_callback` (kind, SND id); nothing is played.
- Known not modelled: which way the dissolve's timing steps go (uninitialised memory, above); the C heap's garbage in
  general; the model's cycle counts are exact to a few percent, so an event within a few thousand cycles of a frame
  boundary (or of a tick, which decides e.g. whether a fade step chains through an anim's redraw) can fall a frame off.

Scene functions: `scene_tree` (20..28, 2D7D:04F9, callback 2D7D:0472), `scene_story1` (7, 2D7D:259C), `scene_story2`
(8, 2D7D:2E27), `scene_story3` (9, 2D7D:1C0F), `scene_story4` (10, 2D7D:4A95, callback 2D7D:5332), `scene_end` (11,
2D7D:41B1, with the prologue `scene_final_intro` 2D7D:466B from FINAL.DAT, callbacks 2D7D:474C / 18E4), `scene_trans`
(1..6, 2D7D:0708; the title 4 with callback 2D7D:536E; 5 = 2D7D:117A, 6 = 02EAAD). `play_scene` = 0AAC:0274's
black-out/erase wrapper; `NIS_INTRO` plays 7, 4, 8 with the music running on.

Engine hooks (nis.h): `nis_set_room_hook` for 0AAC:0376 (transitions 2 and 3 draw a game room: level 10 room 22,
level 14 room 1), `nis_set_kid` for the kid of transition 6 (0AAC:0442, OVL14's placing of script 4210),
`nis_set_palette` for the DAC as the scene finds it (0AAC:0274 blacks it out but for 1, 6 and 100: transition 6 sets
colours 0..0xDF only, and its item, image 0xB of PRINCE.DAT's list 1000, is coloured from bank 15 (0xF1, 0xF2, 0xF5,
0xF6) as the game left it; in the NISn cheat's run that is still the BIOS's mode 13h palette, the model's default,
(11,16,12) etc.; in the game the level's PALS 1000). Without the room hook the room area stays as the scene left it. tests/nistest.c built with -DNIS_ENGINE (tools/nisrun.py does) links the
whole program and sets the room hook to the shell's `shell_nis_room` (with DS:2BA6 = 1, as 0AAC:0274 sets it).

0AAC:0376 (room, level): 169B:018E; DS:0998 = DS:5CEC = level; the game's offscreen port DS:5CC2 = the current port (the
scene's screen, so the room is drawn straight into it); 1286:02EE (the level), the kind's scenery file opened,
1286:00A2 (the tiles, colours 0x40.. unless 0AAC:00AE), 1286:043A if the kind changed, 1286:03B6, 1286:0592; drawn room
0, DS:6B6D = room, 0823:0E72(1) (the room's description and its hook: level 14 room 1's 33FD:145E fills the port
with 0x8B, the sky), 169B:0430 (the whole redraw); 1286:0EC6(4), (1), 0EAC; the file closed; DS:43FD (the level kind)
= 0; DS:5CC2 put back; 2A31:0D03. Compared (2026-09-24): the room itself is exact in both; scene 3's sky needed the
description hook's fill (shell_nis_room lets the room hooks draw: `nis_room_on`) and its horse statue (the extra piece
0x6372) is off while DS:2BA6 is set. Its time (the oracle, probes 0AAC:0376 / 03F6 / 042C, the scene's own cycles with
the interrupts' taken out): loading the level and drawing the room 15.06 M cycles for level 10 room 22, 8.83 M for
level 14 room 1, then 2A31:0D03 reloading the scenes' overlay that the engine's code displaced 3.82 M (the ~45 frames
before transition 3's fade). The model charges these measured figures after the hook.

Anim 11's horse (transition 3) was not off by pixels: its outline is colour 0xFF of the image's bank 15, and with the
shape list's mask 0xFFFE (15 banks) 194C:122C maps image bank 15 through the 16th entry of its table on the stack
(bp-0x22..), which it never sets: the game's leaves 0 there (outline in colour 0x0F, (15,5,12)), the model took 0xF0.
The model now maps the missing banks to 0 (verified: the horse exact, scene 3 +70 shots).

## Verification (tests/nistest.c, tools/nisrun.py)

Captures (oracle, `tools/nisoracle.py NAME FIRST LAST STEP`, then `cap.sh NAME "prince yippeeyahoo NISn"`, the cheat
replaying scene n; `prince` alone for the intro): N20..N28, A9, A10, B11, C1, C2, C3, C5, C6b, INTRO (frames 60..17000,
every 3rd). The oracle's frames are not retrace to retrace: oracle-run's `dosdrv_frame` runs the emulator until its
millisecond clock reaches floor((f + 1) T) (T = 1000 / 70.0863 ms), and the monitor's retraces come at psi + f T with
psi = 2.87 ms (the polling loops' retrace probe 194C:7A3D, in the frames where the game only polls: median of 300..680
per capture, 2.76..2.88), so the retrace of frame f lies 0.20..0.27 into it. A shot of frame F is the picture scanned
after the retrace of frame F - 1 (a fade's step written at the retrace of frame 99 shows first in shot 100). The test
puts each of our checkpoints (anim frames, fades, sounds, texts, dissolves, anims, scenes, cue points, paired by type
and ordinal) at its time in that numbering and compares a shot with our frame for the retrace before it, counted from
the last checkpoint both sides passed; it also looks +-window frames around it. In every capture the checkpoint counts
agree with the game's (for 1 the oracle has one more sound: the cheat's music for the next loop).

`nisrun.py [window]` (2026-09-24; before: the model and test of the previous revision):

| scene | shots | exact synced | exact within +-1 | exact within +-10 | before (synced / +-1 / +-10) |
|---|---|---|---|---|---|
| 7 (intro 1) | 2249 | 2040 | 2116 | 2202 | 2025 / 2114 / 2218 |
| 4 (title) | 553 | 460 | 507 | 521 | 462 / 505 / 517 |
| 8 (intro 2) | 1840 | 1534 | 1642 | 1689 | 1505 / 1604 / 1700 |
| 20..28 | 3052 | 2896 | 3024 | 3045 | 2773 / 3022 / 3042 |
| 9 | 1414 | 1311 | 1381 | 1414 | 1267 / 1326 / 1410 |
| 10 | 1871 | 1740 | 1805 | 1850 | 1707 / 1777 / 1855 |
| 11 (ending) | 1587 | 1314 | 1497 | 1523 | 1358 / 1444 / 1513 |
| 1 | 880 | 731 | 808 | 828 | 707 / 783 / 820 |
| 2 | 897 | 746 | 805 | 856 | 670 / 694 / 753 |
| 3 | 660 | 536 | 623 | 642 | 420 / 486 / 552 |
| 5 | 219 | 194 | 215 | 215 | 195 / 207 / 215 |
| 6 | 1543 | 1381 | 1397 | 1414 | 6 / 26 / 29 |
| all | 16765 | 14883 | 15820 | 16199 | 13095 / 13988 / 14624 |

What remains:
- Timing phase (most of it, exact within +-1 or a few frames): an event within a few thousand cycles of a frame
  boundary or of a tick lands a frame off, e.g. a fade's first step (the trees' fades), the ticks that pace a fade
  inside an anim's redraw (transition 2: in the game a fade step chains through 13 members of one redraw, a step a
  frame, delaying the anim's next frame by 7 ticks; the model does the same, at another redraw), the number of
  dissolve steps and the calibration's ticks (the intro's 28800-pixel dissolves: the game 3 and 2 ticks, the model 2
  and 3). The model's own cycle counts are within a few percent of the oracle's, which is not enough to put every such
  event on the right side of a boundary hundreds of frames in.
- Transition 6: the dissolve's timing steps (the record's uninitialised start tick, above): the game did plain copies,
  the model dissolve steps: its first dissolve ~5 frames late (~100 shots, exact at +3..+5); 2 pixels, (26,69) and
  (295,191), in the last picture (~40 shots): no image covers them in the port the game draws it in (194C:3890 does not
  clear a new port's memory), so they show what the heap held there (colours 0x90 and 0x76 of PALT 4211 in the
  oracle, 0 in the model).
- Tearing: the game's shot caught a copy to the screen half done where the model's copy runs at another speed.

Changed on 2026-09-24 (with the evidence above): the test's frame mapping (the oracle's millisecond frames and the
shot/retrace relation, instead of a flat checkpoint offset the model's costs had been tuned to); the cycle model redone
from the oracle's probes (resource reads, image and sound unpacking, fonts, mirroring, palette I/O, the fade's step; the
fade's former 300000-cycle set-up was really the first sequencer interrupt of a song); the timer interrupts' own time
(the FM driver's register writes), the PIC's pending/lost requests, the retraces missed under a handler, the PIT
reprogramming's immediate interrupt, exact PIT/frame periods; digitized sounds' length at the Sound Blaster's rate; the
beam; scene 7's five preloads (805C at 2D7D:28C6, 2904, 29EE, 2D6D, 2DB2) that the transcription lacked; the level
load of 0AAC:0376; the scene's start phase; the palette the scene finds (`nis_set_palette`); 194C:122C's unset bank
entries (0, not the bank itself).

## Integration

- The core should call `play_scene(n)` where the game does (0AAC:0274: 0823:01CA for the intro (7, 4, 8, stopping at
  a key), 0AAC:0120 on entering a level (see the table above; 0AAC:000E), 169B:123E (28), the 0823 main loop (11 after
  level 14), OVL14 level 8 room 9 (6)), i.e. `nis_open(dir, n)` and `nis_step` per video frame until 0, then carry on
  with 0AAC:0274's tail (0FB3:293A, 1286:07CE, DS:2BA6 = 0, the level restore of 0AAC:0376/0442 for 1, 2, 3).
  Game state the scenes set: DS:016A (last daughter scene, set by 0AAC:0120 before), DS:2087 (cue), the music (25011 runs
  from 7 into 4 and 8; a scene may leave its music playing).
- `nis_set_palette(render_palette)` before `nis_open` (the shell does): the colours a scene does not set are the game's.
- source/dat.c: `dat_find` reports the size one byte short (the body is the whole size field after the checksum byte);
  nis.c adds it back. Palettes (PALT, 768 bytes) and MIDI files need the last byte.
- 0AAC:0080 (black-out before a scene) reads an uninitialised local for n = 4; the model always blacks out.
- Workspace: `~/pop2dec/work/ovl00_2D3E.asm` (the OVL00 listing, synced on functions and jump tables),
  `tools/nispseudo.py` (call-level pseudo-code of a range), `nisscr.py`, `nismidi.py`, `nisdat.py`.

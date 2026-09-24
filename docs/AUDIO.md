# Sound playback (src/audio.c)

How the DOS game makes sound, reconstructed from segment 194C of PRINCE.EXE and the driver files it loads, and how
`src/audio.c` reproduces it for a frontend. The game logic's side (which sounds are asked for, when, and which answers
"is it playing" steer the game) is `src/sound.c` (FINDINGS 5.11); this file is the drivers underneath.

Addresses: `194C:xxxx` in PRINCE.EXE (DS = 3B25); `MIDI.DRV+x` / `DIGI.DRV+x` are file offsets (a driver runs with
CS = its load paragraph - 0x10, so offsets equal file offsets; in the oracle MIDI.DRV sits at 4BD9:0000, DIGI.DRV at
4B5C:0000, PRESETS.DEF at 4C71:000A).

## 1. Devices and files

The CD's setup leaves a Sound Blaster Pro configuration: CONFIG.DAT (32 bytes, loaded verbatim at DS:80FC, pointer
DS:1FB8) has digital type 1 (word +6) and MIDI type 0x21 (word +8), port 0x220 (+0xE). `DIGI.DRV` = `SNDDRVRS/DSB_PRO.DRV`
(Sound Blaster Pro DSP), `MIDI.DRV` = `SNDDRVRS/MSB_PRO.DRV` (Sound Blaster Pro FM = an OPL2 driver writing ports
388h/389h), `PRESETS.DEF` = `SNDDRVRS/PRESET33.DEF` (the FM instrument bank; 0x21 = 33). The other drivers on the CD
(MMPU401.DRV with PRESET40/41 for MPU-401 synthesizers, MCMS101.DRV, PRESET32 for AdLib) are not reconstructed. The
oracle's DOSBox-X emulates an SB Pro 2 (`sbtype sbpro2`, IRQ 7, DMA 1, OPL3 by the default DBOPL emulator).

194C:2B8C loads the drivers (`DIGI.DRV` if the digital type is nonzero, `MIDI.DRV` if the MIDI type is nonzero,
PRESETS.DEF if the MIDI type is nonzero; types >= 0x28 get no bank), 194C:31BE / 319A call their detection (function 0),
194C:31E2 initialises: DIGI.DRV function 1 (sets DS:2085 bit 0, DS:2094 = its entry, 2098/209A/209C = async flag 1,
min rate 3920, max rate 65535), MIDI.DRV function 1 (bit 1, DS:20AA entry, 20AE = its timer rate 0) and function 6 with
the bank, then volume 15 (194C:3380). With both types 0 no driver loads (DS:2085 = 0) and the game uses the PC speaker
player built into 194C. The level sounds come from DIGISND.DAT / MIDISND.DAT (or IBMSND.DAT for the speaker), the story
scenes' from NISDIGI.DAT / NISMIDI.DAT (NISIBM.DAT; NIS3VC.DAT belongs to a driver not on this setup).

## 2. Sound resources

Tag "SND" (stored "DNS\0" in the DAT index), id 10000 + sound number for the level sounds (DIGISND/MIDISND/IBMSND have
disjoint numbers), 25000+ for the scenes, 65534 = the "press a key" blink beep (169B:0B9C). `tools/audiodat.py` lists
them. Byte 0: bits 0-6 the kind (194C:3339), bit 7 loop.

**Kind 1, digitized** (header 10 bytes, then 8-bit unsigned mono PCM):
`+1` rate (word, Hz: 11000, some scene sounds 10500), `+3` 8 (0xFF: packed), `+4` length, `+6` loop start, `+8` loop end
(0 = the length). Packed resources (level sounds 0x20 0x26 0x2F 0x31 0x36 0x258 and every NISDIGI sound) hold at +0xA a
word (the unpacked length) and a bit stream; 194C:8118 unpacks them once when the resource is loaded (194C:8466, byte 3
becomes 8, `+4` the length) and resamples rates below 3920 Hz (194C:84D8, a box filter; never needed by the data).
The stream (MSB first): the first sample as a byte; then in mode 0 a 3-bit code c: 7 adds 0x80 to the sample, else
deltas of c + 2 bits follow, each added to the sample (values above 2^(bits-1) are negative, two's complement in `bits`
bits), until the escape value 2^(bits-1) returns to mode 0. Verified: the C unpacker's output is byte-identical to the
unpacked resources found in the oracle's memory (10049, 26001, 26002, 26004).

**Kind 2, MIDI**: a standard MIDI file (format 0 or 1; all game pieces are format 0, 1 track, 480 ticks per quarter)
from +1, or "MSeq" at +1 followed by events sent at once (194C:36DC; none in the data). 11 level pieces loop (bit 7:
ambient loops and level 14's room music).

**Kind 0, PC speaker**: `+1` the player's timer rate (Hz, mostly 72), then 3-byte entries {frequency word, duration byte
(timer ticks)}: frequency 0 = rest, 1 = vibrato depth (the byte), 2 = cue mark (DS:2087 = the byte), 3..0x12 = end
(loop to +3 if bit 7), else a tone of that frequency (PIT channel 2 divisor 0x1234DC / f). 3-byte placeholders (rate 0)
exist for sounds only other devices play.

## 3. The library (194C)

| routine | what |
|---|---|
| 805C(id) | resource handle ("SND", id), loading and preparing it (8118) |
| 840E(id) | 805C + 8092 |
| 8092(handle) | if the resource is playing (33CE) stop it (3320), then 3339 |
| 3339(res, callback) | by kind: 0 speaker 370A, 1 digital 35E5, 2 MIDI 3668 |
| 83D2(id) | 3320: stop the channels playing that resource (id 0: all) |
| 8396(id) → 32C6 | release: a looping sound plays to its end (DS:208A / 20A0 / 20B2 = 1; id 0: all) |
| 8426(id) → 33CE | playing: the busy flag of the channel whose resource it is (id 0: any) |
| 3380(v) | volume 0..15 (DS:2086; the game toggles 15 / 0 on Alt+S, 0823:0AF0): DIGI.DRV function 4, MIDI.DRV function 5, speaker gate |

Three channels, each {busy flag, resource, notify callback}: digital DS:2088 (208A loop stage, 208C resource),
MIDI DS:209E (20A0 release, 20A2 resource), speaker DS:20B0 (20B2 release, 20B4 resource). DS:2087 is the cue byte.

**Timers** (194C:7D76 add, 7E37 remove; list head DS:24E8): each new timer becomes the IRQ 0 handler with PIT divisor
0x1234DD / rate (65536 at 18 Hz and below) and calls the previous one at the right average rate through a 16.16
fraction (record +0xA). The game's own 60 Hz timer (DS:24EC, 194C:7EE7, divisor 19886) sits under them.

**Digital (35E5)**: needs DS:2085 bit 0; stops the channel, busy = 1, stage (208A) = 0, calls DIGI.DRV function 5
(no-op in DSB_PRO) and plays with function 7 (buffer +0xA, rate +1, length +4, or the loop end +8 for a looping sound).
The driver's completion (int 15h AX=91F0, below) calls 194C:3422: a looping sound with stage 0 plays again from +6 to the
loop end; after a release (stage 1) the tail +8..+4 plays once (stage -1); otherwise busy = 0.

**MIDI (3668)**: needs bit 1; cue = 0, stops the MIDI channel (3579: when it was busy, the timer goes and the driver is
reset), busy = 1, release = 0, 2DF0 starts the file:
- driver function 4 (reset); channel map (DS:2073, 16 bytes, identity at start) loses its mute bits; DS:2071 = 0;
- header "MThd", format <= 1, division without bit 15 (DS:1FBC); default tempo 500000 (DS:2065);
- each track (7-byte records at DS:1FE8: 32-bit tick count to its next event, pointer, running status) reads its first
  delta; a delta of 0 plays the track's first events at once;
- a timer at 240 Hz (DS:1FBA; divisor 4971) with 194C:2F10.
2F10 adds the 16.16 increment `inc = (256000000/240 << 16) / (tempo*256/division)` (194C:2FB4, each division
truncating) to a fraction (DS:1FBE, reset at the start); the whole ticks are subtracted from every track's count, and a
track whose count is <= 0 plays events (2FFA) until the next delta leaves it positive. Details that matter:
- 194C:3180 reads a delta with a quirk: on a continuation byte the high and low words of the partial value are shifted
  separately, which loses a bit once the value reaches the high word (4-byte deltas, >= 2^21 ticks; none in the data);
- tempo meta events (track 0 only) are read as the high byte and then a little-endian word (194C:3171): 0C 35 00 is
  0x0C0035, not 0x0C3500;
- meta 7 (cue point) stores its first byte in DS:2087; end of track on track 0 ends the piece (the timer goes, the
  driver is reset, then 34BA: a looping piece not released starts over through 2DF0, else busy = 0); other tracks stop;
- channel events go to the driver with the channel through the map; note-ons of a channel with map bit 7 are dropped,
  and all events of mapped channel 15 while DS:2071 = 1;
- system exclusive `00 00 34 dev cmd x ...` with dev 0 or 0x21 (the MIDI type) are sequencer commands: 0 pass to the
  driver, 1/2 mute/unmute note-ons of channel x, 3 remap channel x to the next byte, 4 identity map (channels 0..14),
  5 unmute (x != 0) or mute (x = 0) as many channels as the message is long, 6/7 drop / keep channel 15. Other sysex go
  to the driver.

**Speaker (370A)**: stops the speaker channel (and the digital one if there is no digital device), sets PIT channel 2 to
mode 3, adds a timer at the resource's rate with 194C:377B and calls it once at once (int 8). 377B: when the duration
counter (DS:20E2) runs out, the next entries are read (37E3); a tone sets the gate (port 61 bits 0-1, only with volume
on) and the divisor; with a vibrato depth d the divisor then moves each tick by a 8.8 step ((divisor >> 7) / d), the
direction flipping every 2d ticks (the first time after d).

## 4. MIDI.DRV (Sound Blaster Pro FM)

Entry +0x100: AL 0..7 = function, AL >= 0x80 = a MIDI status (AH channel, DL/DH data), dispatched by AL >> 4 through
the table at +0x123. Functions: 0 detect (OPL timer test through 388h and PIT 2), 1 init (times the register-write delay
loops against the PIT, writes 01=00, BD=00, 01=20 (waveform select on), 1 built-in instrument, reset), 2/4 reset, 5
volume, 6 bank (AH count, ES:BX 16-byte instruments), 3/7 nothing. Every register write goes through +0x8AF, which
keeps a shadow copy (+0x16B + register).

Instrument (16 bytes): +0/+1 A0/B0 values for rhythm voices, +2 C0 (feedback/connection), +3..7 modulator
20/40/60/80/E0, +8..0xC carrier, +0xD rhythm instrument (1 bass drum, 2..5 other rhythm sounds; none in PRESET33).

- reset (+0x40E): A0..A8 and B0..B8 = 0; channel n plays instrument n (< count, else 0), transposition 0; voices free;
  9 melodic voices (rhythm mode is never turned on); RPN "none"; bends 0.
- note on (+0x54A; velocity 0 = note off): nothing while the volume floor is 0x3F or the channel's instrument is past the
  bank. A free voice (no note) is searched round robin from a pointer that advances by one per note-on whatever is
  found; with none free the note is dropped. The voice gets the instrument (+0x6AC: C0, then both operators' five
  registers; the carrier's level, and the modulator's when C0 bit 0 is set, scaled by velocity:
  `l' = clamp((l + 64) * 225 / (velocity + 161), 64, 127) - 64`, then at least the volume floor, KSL bits kept), then
  the frequency (+0x7E6) with key on.
- frequency: `n = note - 31 + transposition + bend semitones` (8-bit; while negative +12), block = n / 12, F-number
  from the table +0x14F {1E3, 200, 21E, 23F, 261, 285, 2AB, 2D4, 300, 32E, 35E, 390, 3C7, 3FF} (index semitone + 1),
  moved towards the next / previous entry by the bend fraction (only the low byte of the difference is used);
  B0 = block << 2 | F-number high | 0x20.
- note off (+0x5AE): the first voice with that note and channel gets B0 without key on.
- controllers (+0x60C): only 100 (RPN LSB, value 0 selects the bend range), 6 (data entry: the range, 12 at load),
  38 (data entry LSB, unused).
- pitch bend (+0x637): value - 0x2000 scaled to semitones.fraction by the range; the channel's sounding voices
  are retuned.
- program change (+0x4F2): below the bank's count.
- sysex (+0x3DE): `00 00 34 (00|01) 00 t` sets the transposition to t (7-bit signed).
- volume (+0x449): floor = 0x3F - 4v (v < 15), 0 at 15; every carrier level (and additive modulator) below the floor is
  raised to it (never lowered back; new notes use the floor).

## 5. DIGI.DRV (Sound Blaster Pro DSP)

Entry +0x100, table +0x118: 0 detect (DSP reset, E0 test, version E1, SB Pro mixer and OPL at base+8), 1 init (hooks
int 15h: +0x234 calls the callback given with the play call when AX = 91F0), 2 shutdown, 3 stop (+0x776: DSP D0,
mask DMA 1 and IRQ 7), 4 speaker by volume (D1 on / D3 off, a running transfer paused with D0 and resumed with D4), 5/6
nothing, 7 play (+0x6BE: ES:BX buffer, CX length, DX rate, SI:DI callback). Play: DSP 40 with the time constant
256 - trunc(1000000 / rate) (11000 Hz plays at 1000000 / 90 = 11111 Hz), IRQ 7 hooked, DMA channel 1 single-cycle
reads (mode 0x49), DSP 14 (8-bit single-cycle output) with length - 1, split at 64 KiB physical pages (the IRQ handler
+0x526 programs the next piece). After the last piece: channel released, int 15h AX=91F0 -> 194C:3422.
Measured in the oracle: a sound's busy time = length x 90 us + about 4 ms.

## 6. src/audio.c

`audio_init(dir, caps)` (caps 3 = the CD setup; 0 = PC speaker), `audio_add_file(path)` for the scene DATs,
`audio_request(id)` (840E), `audio_request_res(id, bytes, len)`, `audio_stop(id)` (83D2), `audio_release(id)`
(8396), `audio_playing(id)` (8426), `audio_volume(v)` (3380), `audio_cue()`, `audio_render(pcm, frames, rate)` (mono
s16). The code follows the routines above one to one (same state, same order of register writes); the FM chip is Nuked
OPL3 1.8 (`src/audio_opl3.c/.h`, unmodified but for the include name; LGPL 2.1 or later, compatible with this GPL
project), used in OPL2 mode like the OPL3 of an SB Pro 2. Timing: an audio clock in PIT ticks (1193182 Hz); the MIDI
interrupt every 4971 ticks from the start call, the speaker player at its own rate, digital samples at 1000000 / q Hz
(linear interpolation). Requests take effect at the current audio time. Mixing as DOSBox-X does: FM x 1.5 (adlib.cpp
`SetScale(1.5)`), DAC x 1 ((s - 128) << 8, muted by DSP D3), speaker a +-4850 square wave; then a ~14 Hz high-pass
(`audio_dc_block`) because the FM output of these instruments carries a large DC offset.

Frontend wiring (sdl/ is not part of this work): open an SDL audio device (mono s16, e.g. 44100 Hz) whose callback calls
`audio_render`; set the core's hooks (src/sound.c) `sound_start_hook = n -> audio_request(10000 + n)` and
`sound_stop_hook = n -> audio_stop(n == -10000 ? 0 : 10000 + n)`, calling them under SDL_LockAudioDevice; the story
scenes' sound events (src/nis.c `snd_event`) map to `audio_request(id)` / `audio_stop(id)` after `audio_add_file` of
NISDIGI.DAT and NISMIDI.DAT. Not yet driven by the core: the blink beep 65534 (169B:0B9C: every 12 ticks of the
"press a key" countdown DS:5CDA, when DS:5CDA % 12 == 3 below 0x78), the sound toggle (Alt+S -> `audio_volume(15/0)`).

## 7. Verification (tests/audiotest.c)

Captures (`tools/audioscript.py` adds these probes to a script; `~/pop2dec/oracle/AU*.script`):

    probe 1611 053C a_req SS0000 10     probe 194C 840E a_reqid SS0000 10   probe 194C 83D2 a_stop SS0000 10
    probe 194C 8396 a_rel SS0000 10     probe 194C 3380 a_vol SS0000 8      probe 194C 3668/35E5/370A a_midistart/...
    probe 194C 2F10 a_isr               probe 194C 3055 a_ev                probe 4BD9 08AF a_opl
    probe 4B5C 02D2 a_dsp               probe 4B5C 0432 a_dsp               probe 4B5C 0526 a_irq
    probe 194C 3422 a_digiend           probe 194C 34BA a_midiend
    (speaker: probe 194C 377B a_spkisr; 37DF/380A/35D8/33C8 a_gate; 3839 a_div)

`audiotest PRINCE2_DIR CAPTURE-snap.txt` feeds the capture's inputs (requests, stops, releases, volume, each MIDI timer
interrupt, each digital completion, each speaker tick) to the C driver in order and compares its outputs, kind by kind
in order, with the DOS drivers': every OPL register write, every MIDI event the sequencer hands the driver, every DSP
byte (transfers split at DMA pages merged; a transfer stopped inside its first piece shows only that piece's length),
the MIDI end callbacks, the speaker's gate and divisor. A request that loads its resource from disk starts after
interrupts have run, so it is moved to its channel start. Results (2026-09-24), every capture identical:

| capture | inputs | OPL writes | MIDI events | DSP bytes | other |
|---|---|---|---|---|---|
| AU1 (level 1, E1_1 keys) | 15489 | 28590 | 5094 | 193 | |
| AU2 | 54 | 0 | 0 | 162 | |
| AU4..AU14 (E<L>_1 keys) | 3995..14457 each | 1978..12577 | 379..2335 | 130..247 | 62 MIDI ends (loops restarted), digital loops (AU12/13) |
| AUV8 (Alt+S twice) | 5946 | 2354 | 582 | 74 | volume 0 / 15 |
| SPK8 (`~/pop2dec/oracle/popspk.hdd`: pop.hdd with CONFIG.DAT's types 0; run oracle-run with `--rom` on it) | 1316 | - | - | - | 1362 speaker gate/divisor writes incl. vibrato |

`audiotest DIR --pcm CAPTURE-snap.txt AUDIO.raw [PREFIX]` renders the capture's requests in real time (placed by
instruction count inside the frame) and compares 10 ms RMS envelopes with the oracle's mixer output (oracle-run
`audio F1 F2 PATH`, added on the pop2-tracer branch): AUW8 (level 8, 28.5 s) correlation 0.915, mean RMS 2234 (DOSBox-X)
/ 2347 (C); AUV8 0.933; SPK8 (speaker) 0.985, 2358 / 2359. FM windows alone matched the DOSBox level only with the 1.5
FM scale; DBOPL and Nuked differ in detail, not in level. `audiotest DIR --wav OUT.wav SECONDS ID...` renders sounds.

Speaker captures need `AUDIO_CAPS=0` for audiotest. Build: `meson compile -C build` (build/tests/audiotest).

The pieces (MIDISND + NISMIDI, 204 files) contain notes, program changes, pitch bends, controllers 1 and 11 (which the
driver ignores), tempo, cue, text/time/key-signature metas, and sysex commands for device 0x21 (mute/unmute channels,
typically channel 9, the General MIDI drum channel) plus a few for device 1 (ignored by this driver); no delta is longer
than 2 bytes; every file's last byte (the end-of-track length 00) lies just past the resource, never read. Not covered by
the captures: controller events, MSeq resources and rhythm instruments (none in the data), the resampler (no rate below
3920 Hz).

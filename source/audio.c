/* Sound playback: the sound library of segment 194C and the drivers it loads (MIDI.DRV, DIGI.DRV), rendering PCM.
 * Addresses: 194C:xxxx = PRINCE.EXE (DS = 3B25); MIDI.DRV / DIGI.DRV offsets are file offsets (the drivers run with
 * CS = load address - 0x10, so an offset is also the file offset; in the oracle MIDI.DRV sits at 4BD9:0000 and DIGI.DRV
 * at 4B5C:0000). docs/AUDIO.md describes the formats and the behaviour; this file follows the code closely.
 *
 * Three channels, each remembering its resource and a busy flag (194C:33CE answers "playing" from them):
 *   digitized (DS:2088..): 8-bit unsigned PCM through the SB Pro DSP (DIGI.DRV), loop points, packed samples;
 *   MIDI      (DS:209E..): a standard MIDI file played by a 240 Hz timer interrupt (194C:2DF0/2F10/2FFA) into the
 *                          driver, which is an OPL2 FM synthesizer with a 128-instrument bank (PRESETS.DEF);
 *   speaker   (DS:20B0..): PC speaker note lists (IBMSND.DAT) on their own timer (194C:370A/377B).
 * The FM chip is emulated by Nuked OPL3 (source/audio_opl3.c, LGPL 2.1+, used in OPL2 mode as on a Sound Blaster Pro 2). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio.h"
#include "audio_opl3.h"
#include "dat.h"

#define PIT_HZ 1193182u
void (*audio_trace)(int what, int a, int b);
int audio_manual_clock;
int audio_gain_fm = 384, audio_gain_digi = 256, audio_gain_speaker = 256;   /* DOSBox-X: the FM channel at 1.5 (adlib.cpp SetScale), the DAC at 1 */
int audio_dc_block = 1;
#define TRACE(w, a, b) do { if (audio_trace) audio_trace((w), (a), (b)); } while (0)

/* ------------------------------------------------------------------------------------------------ resources */
typedef struct { uint16_t id; uint8_t *d; uint32_t n; } snd_res;   /* a prepared resource (194C:8118 applied) */
static snd_res *cache; static int ncache, capcache;
static dat_file files[8]; static int nfiles;
static uint8_t bank[1 + 128 * 16]; static int bank_ok;             /* PRESETS.DEF: count byte + 16-byte instruments */
static int caps = 3;                                               /* DS:2085 */
static uint8_t volume;                                             /* DS:2086 (15 after the start) */
static uint8_t cue;                                                /* DS:2087 */

static unsigned rd8(const snd_res *r, uint32_t o) { return o < r->n ? r->d[o] : 0; }
static unsigned rd16(const snd_res *r, uint32_t o) { return rd8(r, o) | rd8(r, o + 1) << 8; }
static void wr16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

/* 194C:8466: packed samples. A word (the unpacked length), the first sample, then a bit stream (MSB first) of deltas:
 * in mode 0 a 3-bit code c: 7 = add 0x80 to the sample, else switch to (c + 2)-bit deltas; a delta equal to the
 * escape 2^(bits-1) returns to mode 0, larger ones are negative (two's complement in `bits` bits). */
static uint32_t unpack(const uint8_t *s, uint32_t sn, uint8_t *o, uint32_t on)
{
	if (sn < 3) return 0;
	uint32_t n = s[0] | s[1] << 8, si = 2, di = 0; if (n > on) n = on;
	uint8_t mask = 0x80, bl = 0, dl = 0, dh = 0, al = s[si++];
	o[di++] = al;
	while (di < n) {
		uint8_t cl = 0, ch = bl ? bl : 3;
		while (ch--) { cl = (uint8_t)(cl << 1); if (si < sn && (s[si] & mask)) cl++; mask = (uint8_t)(mask >> 1 | mask << 7); if (mask == 0x80) si++; }
		if (bl) {
			if (cl == dl) { bl = 0; continue; }
			if (cl > dl) cl |= dh;
			al = (uint8_t)(al + cl); o[di++] = al;
		} else if (cl == 7) { al = (uint8_t)(al + 0x80); o[di++] = al; }
		else { bl = (uint8_t)(cl + 2); uint16_t dx = (uint16_t)(0xFC02u << cl); dl = (uint8_t)dx; dh = (uint8_t)(dx >> 8); }
		if (si > sn + 1) break;
	}
	return di;
}
/* 194C:84D8: box-filter resampling of n samples to m (only for rates outside the driver's 3920..65535 Hz; unused by
 * the game's data, whose rates are 10500 and 11000) */
static void resample(const uint8_t *s, uint32_t n, uint8_t *o, uint32_t m)
{
	if (!n || !m) return;
	unsigned step = (unsigned)((uint64_t)m * 256 / n);   /* 8.8 output samples per input sample (rounded like the original) */
	{ uint32_t q = m / n, r = m % n; uint32_t f = (uint32_t)(((uint64_t)r << 16) / n); unsigned fr = (f >> 8) + ((f >> 7) & 1); step = (q << 8) + (fr & 0x1FF); }
	uint32_t si = 0, di = 0; unsigned bx = 0x100, dx = 0;
	while (di < m && si < n) {
		unsigned al = s[si], cx = step;
		if (bx > cx) { dx += al * (cx & 0xFF); bx -= cx; si++; continue; }
		if (bx & 0xFF) { unsigned v = al * (bx & 0xFF) + dx; unsigned hi = (v >> 8) + ((v >> 7) & 1); o[di++] = hi > 255 ? 255 : (uint8_t)hi; if (di >= m) break; cx -= bx; }
		bx = 0x100; dx = 0;
		for (unsigned k = cx >> 8; k && di < m; k--) o[di++] = (uint8_t)al;
		if (cx & 0xFF) { dx += al * (cx & 0xFF); bx -= cx & 0xFF; }
		si++;
	}
	while (di < m) o[di++] = 0x80;
}
/* 194C:8118: a digitized resource is prepared once when loaded: packed samples (byte 3 = 0xFF) unpacked (byte 3 := 8,
 * word 4 := length), rates below 3920 Hz resampled */
static void prepare(snd_res *r)
{
	if (!(rd8(r, 0) & 1) || r->n < 10) return;
	if (rd8(r, 3) == 0xFF) {
		uint32_t n = rd16(r, 10); uint8_t *d = calloc(n + 0x10A, 1);
		memcpy(d, r->d, 10); unpack(r->d + 10, r->n - 10, d + 10, n);
		d[3] = 8; wr16(d + 4, n); free(r->d); r->d = d; r->n = n + 10;
	}
	unsigned rate = rd16(r, 1);
	if (rate < 3920) {
		uint32_t n = rd16(r, 4), ratio = (uint32_t)(((uint64_t)3920 << 16) / (rate ? rate : 1)), m = (uint32_t)(((uint64_t)ratio * n) >> 16);
		uint8_t *d = calloc(m + 0x10A, 1); memcpy(d, r->d, 10); resample(r->d + 10, n < r->n - 10 ? n : r->n - 10, d + 10, m);
		wr16(d + 1, 3920); wr16(d + 4, m); wr16(d + 6, (unsigned)(((uint64_t)ratio * rd16(r, 6)) >> 16)); wr16(d + 8, (unsigned)(((uint64_t)ratio * rd16(r, 8)) >> 16));
		free(r->d); r->d = d; r->n = m + 10;
	}
}
static snd_res *res_add(uint16_t id, const uint8_t *p, uint32_t n)
{
	if (ncache == capcache) { capcache = capcache ? capcache * 2 : 64; cache = realloc(cache, capcache * sizeof *cache); }
	snd_res *r = &cache[ncache++]; r->id = id; r->d = malloc(n ? n : 1); memcpy(r->d, p, n); r->n = n;
	prepare(r); return r;
}
/* 194C:805C: the resource ("SND", id), loaded and prepared on first use */
static snd_res *res_get(uint16_t id)
{
	for (int i = 0; i < ncache; i++) if (cache[i].id == id) return &cache[i];
	for (int f = 0; f < nfiles; f++) { uint16_t n; const uint8_t *p = dat_find(&files[f], "DNS", id, &n); if (p) return res_add(id, p, n); }
	return NULL;
}

/* ------------------------------------------------------------------------------------------------ the FM driver (MIDI.DRV) */
static opl3_chip opl; static uint32_t opl_rate;
static struct {
	uint8_t reg[256];             /* 0x16B: shadow of every register written */
	uint8_t note[9], chan[9];     /* 0x27F / 0x288: voice's note (0 = free) and channel */
	uint8_t b0[9];                /* 0x291: voice's last B0 value */
	uint8_t prog[16];             /* 0x26D: channel -> instrument */
	uint8_t bend[16][2];          /* 0x2F2: channel's bend (fraction, semitones) */
	int16_t bdir[16];             /* 0x312: bend direction 1 / -1 / 0 */
	uint8_t voices;               /* 0x2AF: 9 */
	uint8_t floor_;               /* 0x2B3: minimum attenuation (volume); 0x3F = silent */
	int8_t transpose;             /* 0x2B4 */
	uint8_t rr;                   /* 0x14D: next voice to try */
	uint8_t cur;                  /* 0x143: voice being set */
	uint8_t c_chan, c_note, c_vel;/* 0x2B0..0x2B2 */
	uint8_t rpn, rpn_lsb, range;  /* 0x332 (0xFF none), 0x333, 0x334 bend range (12) */
	const uint8_t *bank; uint8_t count;   /* 0x29B / 0x29A */
} fm;
static const uint8_t op_slot[18] = {0x00,0x03, 0x01,0x04, 0x02,0x05, 0x08,0x0B, 0x09,0x0C, 0x0A,0x0D, 0x10,0x13, 0x11,0x14, 0x12,0x15};   /* 0x2B6 */
static const uint8_t carriers9[] = {0x03,0x04,0x05,0x0B,0x0C,0x0D,0x13,0x14,0x15,0xFF};           /* 0x2C8 */
static const uint8_t carriers6[] = {0x03,0x04,0x05,0x0B,0x0C,0x0D,0x13,0x11,0x14,0x12,0x15,0xFF}; /* 0x2D2 (rhythm mode) */
static const uint8_t perc_slot[6] = {0x00,0x00,0x14,0x12,0x15,0x11};   /* 0x2DE */
static const uint8_t perc_voice[6] = {0x00,0x00,0x07,0x08,0x08,0x07};  /* 0x2E4 */
static const uint8_t perc_bit[6] = {0x00,0x10,0x08,0x04,0x02,0x01};    /* 0x2EA */
static const uint16_t fnum[14] = {0x1E3,0x200,0x21E,0x23F,0x261,0x285,0x2AB,0x2D4,0x300,0x32E,0x35E,0x390,0x3C7,0x3FF};   /* 0x14F (fnum[1] = C) */
static const uint8_t default_instr[16] = {0x00,0x0A,0x0C,0x32,0x1B,0x81,0x2F,0x00,0x22,0x03,0x82,0x2B,0x00,0x00,0x00,0x00};   /* 0x29F */

/* 0x8AF: register write (the shadow copy, then 0x388/0x389 with the delay loops) */
static void opl_w(uint8_t r, uint8_t v) { fm.reg[r] = v; OPL3_WriteReg(&opl, r, v); TRACE(AUDIO_T_OPL, r, v); }
/* 0x537 / 0x51C: silence every voice (and leave rhythm mode on with fewer voices) */
static void fm_silence(void) { for (int v = 0; v < 9; v++) { opl_w((uint8_t)(0xA0 + v), 0); opl_w((uint8_t)(0xB0 + v), 0); } if (fm.voices != 9) opl_w(0xBD, 0x20); }
/* 0x501: channel n plays instrument n; no transposition */
static void fm_programs(void) { fm.transpose = 0; for (int c = 0; c < 16; c++) fm.prog[c] = c < fm.count ? (uint8_t)c : 0; }
/* 0x40E (functions 2 and 4): reset */
static void fm_reset(void)
{
	fm_silence(); fm_programs();
	memset(fm.chan, 0, 9); memset(fm.note, 0, 9);
	fm.voices = 9; fm.rpn = 0xFF; fm.rpn_lsb = 0x0C;
	memset(fm.bend, 0, sizeof fm.bend); memset(fm.bdir, 0, sizeof fm.bdir);
}
/* 0x339 (function 1): after timing the delay loops, waveform select on, the one built-in instrument */
static void fm_init(void)
{
	memset(&fm, 0, sizeof fm); fm.voices = 9; fm.rpn = 0xFF; fm.range = 12;
	opl_w(0x01, 0x00); opl_w(0xBD, 0x00); opl_w(0x01, 0x20);
	fm.bank = default_instr; fm.count = 1;
	fm_reset();
}
/* 0x449 (function 5): volume 0..15 -> minimum attenuation of the carriers (and of additive modulators) */
static void fm_volume(uint8_t v)
{
	uint8_t a = v < 15 ? (uint8_t)(v << 2) : 0x3F;
	fm.floor_ = (uint8_t)(0x3F - a);
	const uint8_t *t = fm.voices == 9 ? carriers9 : carriers6; int k = 0;
	for (; *t != 0xFF; t++) {
		uint8_t r = (uint8_t)(*t + 0x40), c = fm.reg[r];
		if ((c & 0x3F) < fm.floor_) opl_w(r, (uint8_t)(fm.floor_ | (c & 0xC0)));
		if (k < fm.voices) {
			int v2 = k++;
			if (fm.reg[0xC0 + v2] & 1) { uint8_t r2 = (uint8_t)(op_slot[2 * v2] + 0x40), c2 = fm.reg[r2]; if ((c2 & 0x3F) < fm.floor_) opl_w(r2, (uint8_t)(fm.floor_ | (c2 & 0xC0))); }
		}
	}
}
/* 0x6DB: one operator's five registers from the instrument (level scaled by velocity when `scale`) */
static void fm_operator(uint8_t slot, const uint8_t *e, int scale)
{
	opl_w((uint8_t)(0x20 + slot), e[0]);
	uint8_t l = e[1];
	if (scale) {
		unsigned a = (unsigned)((l & 0x3F) + 0x40) * 0xE1 / (fm.c_vel + 0xA1u);
		if (a < 0x40) a = 0x40;
		if (a > 0x7F) a = 0x7F;
		a -= 0x40; if (a < fm.floor_) a = fm.floor_;
		l = (uint8_t)((l & 0xC0) | a);
	}
	opl_w((uint8_t)(0x40 + slot), l);
	opl_w((uint8_t)(0x60 + slot), e[2]); opl_w((uint8_t)(0x80 + slot), e[3]); opl_w((uint8_t)(0xE0 + slot), e[4]);
}
/* 0x6AC: voice v takes instrument e */
static void fm_instrument(int v, const uint8_t *e)
{
	opl_w((uint8_t)(0xC0 + v), e[2]);
	fm_operator(op_slot[2 * v], e + 3, e[2] & 1);
	fm_operator(op_slot[2 * v + 1], e + 8, 1);
}
/* 0x7E6: voice fm.cur plays `note` of channel fm.c_chan (bend, transposition), key on */
static void fm_freq(uint8_t note)
{
	int c = fm.c_chan; int16_t dir = fm.bdir[c]; uint8_t frac = fm.bend[c][0];
	int8_t al = (int8_t)(uint8_t)(note - 0x1F + fm.transpose + fm.bend[c][1]);
	while (al < 0) al = (int8_t)(al + 12);
	uint8_t s = (uint8_t)al, blk = 0;
	while (s >= 12) { s -= 12; blk++; }
	unsigned f = fnum[s + 1];
	if (dir > 0) f += ((unsigned)frac * ((fnum[s + 2] - fnum[s + 1]) & 0xFF)) >> 8;
	else if (dir < 0) f -= ((unsigned)frac * ((fnum[s + 1] - fnum[s]) & 0xFF)) >> 8;
	uint8_t b = (uint8_t)(blk << 2 | f >> 8 | 0x20);
	opl_w((uint8_t)(0xA0 + fm.cur), (uint8_t)f); fm.b0[fm.cur] = b; opl_w((uint8_t)(0xB0 + fm.cur), b);
}
/* 0x879: a free voice, searched round robin from fm.rr (which moves on by one either way); 0xFF: none */
static uint8_t fm_alloc(void)
{
	uint8_t v = fm.rr, r = 0xFF;
	for (int k = 0; k < fm.voices; k++) { if (!fm.note[v]) { r = v; break; } if (++v >= fm.voices) v = 0; }
	if (++fm.rr >= fm.voices) fm.rr = 0;
	return r;
}
static const uint8_t *fm_entry(int c) { return fm.bank + 16 * fm.prog[c]; }
/* 0x773: rhythm instruments (none in PRESETS.DEF) */
static void fm_perc(const uint8_t *e, uint8_t p)
{
	if (p == 1) {
		fm.cur = 6; fm_instrument(6, e);
		opl_w(0xA6, e[0]); opl_w(0xB6, (uint8_t)(e[1] & 0xDF)); opl_w(0xBD, (uint8_t)(fm.reg[0xBD] | 0x10));
	} else if (p < 6) {
		fm_operator(perc_slot[p], e + 8, 0);
		uint8_t v = perc_voice[p];
		opl_w((uint8_t)(0xA0 + v), e[0]); opl_w((uint8_t)(0xB0 + v), (uint8_t)(e[1] & 0xDF)); opl_w((uint8_t)(0xC0 + v), e[2]);
		opl_w(0xBD, (uint8_t)(fm.reg[0xBD] | perc_bit[p]));
	}
}
/* 0x5AE: note off (the first voice with this note and channel; else a rhythm instrument's bit) */
static void fm_note_off(uint8_t c, uint8_t n)
{
	fm.c_chan = c; fm.c_note = n;
	for (int v = 0; v < fm.voices; v++)
		if (fm.note[v] == n && fm.chan[v] == c) { fm.note[v] = 0; opl_w((uint8_t)(0xB0 + v), (uint8_t)(fm.b0[v] & 0xDF)); return; }
	uint8_t p = fm_entry(c)[13];
	if (p && p < 6) opl_w(0xBD, (uint8_t)(fm.reg[0xBD] & ~perc_bit[p]));
}
/* 0x54A: note on */
static void fm_note_on(uint8_t c, uint8_t n, uint8_t vel)
{
	fm.c_chan = c; fm.c_note = n; fm.c_vel = vel;
	if (!vel) { fm_note_off(c, n); return; }
	if (fm.floor_ == 0x3F || fm.prog[c] >= fm.count) return;
	const uint8_t *e = fm_entry(c);
	if (e[13]) { fm_perc(e, e[13]); return; }
	uint8_t v = fm_alloc(); if (v == 0xFF) return;
	fm.cur = v; fm_instrument(v, e);
	fm.chan[v] = c; fm.note[v] = n;
	fm_freq(n);
}
/* 0x60C: controllers: only RPN 0 (pitch bend range) through data entry */
static void fm_control(uint8_t k, uint8_t val)
{
	if (k == 0x64) { if (!val) fm.rpn = 0; return; }
	if (fm.rpn == 0xFF) return;
	if (k == 6) fm.range = val; else if (k == 0x26) fm.rpn_lsb = val;
}
/* 0x637: pitch bend -> (semitones, fraction) by the range; voices of the channel retuned */
static void fm_bend(uint8_t c, uint8_t lsb, uint8_t msb)
{
	int d = (msb << 7 | (lsb & 0x7F)) - 0x2000; int16_t dir; uint8_t lo, hi;
	if (!d) { dir = 0; lo = hi = 0; }
	else if (d > 0) { dir = 1; uint32_t p = (uint32_t)(fm.range << 8) * (uint16_t)(d << 3); lo = (uint8_t)(p >> 16); hi = (uint8_t)(p >> 24); }
	else {
		dir = -1; d = -d;
		if (d == 0x2000) { hi = fm.range; lo = 0; }
		else { uint32_t p = (uint32_t)(fm.range << 8) * (uint16_t)(d << 3); lo = (uint8_t)(p >> 16); hi = (uint8_t)(p >> 24); }
		hi = (uint8_t)-hi;
	}
	fm.bend[c][0] = lo; fm.bend[c][1] = hi; fm.bdir[c] = dir;
	for (int v = 0; v < fm.voices; v++) if (fm.note[v] && fm.chan[v] == c) { fm.c_chan = c; fm.cur = (uint8_t)v; fm_freq(fm.note[v]); }
}
/* 0x3DE: system exclusive 00 00 34 (00|01) 00 t: transpose by t (7-bit signed) */
static void fm_sysex(const snd_res *r, uint32_t o)
{
	if (rd16(r, o) || rd8(r, o + 2) != 0x34 || rd8(r, o + 3) > 1 || rd8(r, o + 4)) return;
	fm.transpose = (int8_t)(uint8_t)(rd8(r, o + 5) << 1) >> 1;
}
/* the driver's entry (0x100): functions 0..7, or a MIDI status in al (ah channel, dl/dh data) */
static void fm_call(uint8_t al, uint8_t ah, uint8_t d1, uint8_t d2, const snd_res *r, uint32_t o)
{
	switch (al >= 8 ? al >> 4 : al) {
	case 2: case 4: fm_reset(); break;
	case 5: fm_volume(ah); break;
	case 8: fm_note_off(ah, d1); break;
	case 9: fm_note_on(ah, d1, d2); break;
	case 0xB: fm_control(d1, d2); break;
	case 0xC: if (d1 < fm.count) fm.prog[ah] = d1; break;
	case 0xE: fm_bend(ah, d1, d2); break;
	case 0xF: fm_sysex(r, o); break;
	}
}

/* ------------------------------------------------------------------------------------------------ the MIDI sequencer (194C:2DF0..3180) */
typedef struct { int32_t count; uint32_t p; uint8_t rs; } track;   /* 7-byte records at DS:1FE8: ticks to the next event, pointer, running status */
#define MAXTR 16
static struct {
	int busy, release;            /* DS:209E / 20A0 */
	snd_res *res;                 /* DS:20A2 */
	int timer;                    /* the 240 Hz timer installed (DS:1FD0 in the timer list) */
	uint64_t next;                /* its next interrupt (audio time) */
	uint16_t division;            /* DS:1FBC */
	uint32_t inc; uint16_t frac;  /* DS:1FC0 (16.16 ticks per interrupt) / 1FBE */
	int ntr; track tr[MAXTR];     /* DS:2063 / 1FE8 */
	uint8_t map[16];              /* DS:2073: channel map (bit 7: note-ons muted) */
	int skip15;                   /* DS:2071 */
	uint8_t status;               /* DS:2070 */
} sq;
static const uint8_t map_init[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
#define MIDI_RATE 240u                                    /* DS:1FBA */
#define MIDI_DIV (0x1234DDu / MIDI_RATE)                  /* 194C:7D76: PIT divisor 4971 */

/* 194C:3180: a variable-length number, as the original reads it (on a continuation byte the high and low words shift
 * separately: a 4-byte number loses a bit) */
static uint32_t vlq(const snd_res *r, uint32_t *p)
{
	uint16_t ax = 0, dx = 0;
	for (int k = 0; k < 5; k++) {
		dx = (uint16_t)((dx & 0xFF) << 8 | ax >> 8); ax = (uint16_t)(ax << 8);
		uint8_t b = (uint8_t)rd8(r, (*p)++); ax |= (uint8_t)(b << 1);
		if (b & 0x80) { dx >>= 1; ax >>= 1; }
		else { uint32_t v = (uint32_t)dx << 16 | ax; return v >> 1; }
	}
	return (uint32_t)dx << 16 | ax;
}
/* 194C:7C68: 16.16 division */
static uint32_t fixdiv(uint32_t a, uint32_t b) { return b ? (uint32_t)(((uint64_t)a << 16) / b) : 0xFFFFFFFFu; }
/* 194C:2FB4: tempo (microseconds per quarter) -> ticks per interrupt */
static void set_tempo(uint32_t t) { sq.inc = fixdiv(fixdiv(0x0F424000u, MIDI_RATE << 16), fixdiv(t << 8, (uint32_t)sq.division << 16)); }
static void seq_driver(uint8_t al, uint8_t ah, uint8_t d1, uint8_t d2, const snd_res *r, uint32_t o)
{
	TRACE(AUDIO_T_MIDI, al | (al < 0xF0 ? ah : 0), d1 | d2 << 8);
	fm_call(al, ah, d1, d2, r, o);
}
/* 194C:2FFA: the track's events due now (and then while the next delta leaves the count <= 0); 1: track 0 ended */
static int seq_events(const snd_res *r, track *t, int first)
{
	for (;;) {
		uint8_t st = (uint8_t)rd8(r, t->p++);
		if (st == 0xFF) {                                     /* 194C:312E meta events */
			uint8_t ty = (uint8_t)rd8(r, t->p++);
			if (ty == 0x2F) { t->p -= 2; if (first) return 1; t->count = 0x7FFFFFFF; return 0; }
			if (ty == 7) { uint32_t n = vlq(r, &t->p); cue = (uint8_t)rd8(r, t->p); t->p += n; }
			else {
				if (ty == 0x51 && first) set_tempo(rd8(r, t->p + 1) << 16 | rd8(r, t->p + 3) << 8 | rd8(r, t->p + 2));   /* 194C:3171: bytes 2 and 3 swapped */
				t->p += vlq(r, &t->p);
			}
		} else {
			sq.status = st;
			if (st >= 0xF0) {                                 /* 194C:306E system exclusive */
				uint32_t n = vlq(r, &t->p), b = t->p; t->p += n;
				unsigned dev = rd8(r, b + 3);
				if (!rd16(r, b) && rd8(r, b + 2) == 0x34 && (dev == 0 || dev == 0x21)) {   /* 00 00 34 dev cmd x: commands for this device (0x21 = CONFIG.DAT's FM type) */
					uint8_t cmd = (uint8_t)rd8(r, b + 4), x = (uint8_t)rd8(r, b + 5);
					switch (cmd) {
					case 0: seq_driver(0xF0, x, 0, 0, r, b); break;
					case 1: if (x < 16) sq.map[x] |= 0x80; break;
					case 2: if (x < 16) sq.map[x] &= 0x7F; break;
					case 3: if (x < 16) sq.map[x] = (uint8_t)((sq.map[x] & 0x80) | rd8(r, b + 6)); break;
					case 4: for (int i = 0; i < 15; i++) sq.map[i] = (uint8_t)((sq.map[i] & 0x80) | i); break;
					case 5: for (uint32_t i = 0; i < n && i < 16; i++) { if (x) sq.map[i] &= 0x7F; else sq.map[i] |= 0x80; } break;
					case 6: sq.skip15 = 1; break;
					case 7: sq.skip15 = 0; break;
					}
				} else seq_driver(st, 0, 0, 0, r, b);
			} else {
				if (!(st & 0x80)) { t->p--; st = t->rs; }
				t->rs = st;
				uint8_t ty = st & 0xF0, d1 = (uint8_t)rd8(r, t->p++), d2 = 0;
				if (ty != 0xC0 && ty != 0xD0) d2 = (uint8_t)rd8(r, t->p++);
				uint8_t m = sq.map[st & 0x0F];
				if (!(ty == 0x90 && (m & 0x80)) && !((m & 0x0F) == 0x0F && sq.skip15) && ty >= 0x80) seq_driver(ty, m & 0x0F, d1, d2, r, 0);
			}
		}
		t->count += (int32_t)vlq(r, &t->p);                 /* 194C:3059 */
		if (t->count > 0) return 0;
		if (t->p >= r->n + 4) return first;
	}
}
static void midi_end(void);
/* 194C:2DF0: start the MIDI file at resource offset 1 */
static void seq_start(snd_res *r)
{
	fm_call(4, 0, 0, 0, r, 0);
	for (int i = 0; i < 16; i++) sq.map[i] &= 0x0F;
	sq.skip15 = 0;
	uint32_t p = 1;
	if (memcmp(r->d + 1, "MThd", 4) || r->n < 15) return;
	uint32_t chunk = p + 8 + (rd8(r, p + 6) << 8 | rd8(r, p + 7));
	unsigned fmt = rd8(r, p + 8) << 8 | rd8(r, p + 9), ntr = rd8(r, p + 10) << 8 | rd8(r, p + 11), div = rd8(r, p + 12) << 8 | rd8(r, p + 13);
	if (fmt > 1 || !ntr || (div & 0x8000)) return;
	if (ntr > MAXTR) ntr = MAXTR;
	sq.ntr = (int)ntr; sq.division = (uint16_t)div; sq.frac = 0;
	set_tempo(500000);                                        /* DS:2065 */
	for (unsigned t = 0; t < ntr; t++) {
		int is_trk;
		do { is_trk = !memcmp(r->d + (chunk < r->n ? chunk : 0), "MTrk", 4) && chunk + 8 <= r->n; p = chunk + 8; chunk = p + (rd8(r, chunk + 6) << 8 | rd8(r, chunk + 7)); } while (!is_trk && chunk < r->n);
		track *k = &sq.tr[t]; k->rs = 0; k->p = p;
		k->count = (int32_t)vlq(r, &k->p);
		if (!k->count) seq_events(r, k, t == 0);
	}
	sq.timer = 1;
}
static uint64_t now;                                          /* audio time: PIT clocks << 16 */
static void timer_start(uint64_t *next, uint32_t div) { *next = now + ((uint64_t)div << 16); }
/* 194C:2EFD: the timer removed, the driver reset */
static void seq_stop_timer(void) { sq.timer = 0; fm_call(4, 0, 0, 0, NULL, 0); }
/* 194C:2F10: the 240 Hz interrupt */
void audio_midi_irq(void)
{
	TRACE(AUDIO_T_TICK, 0, 0);
	if (!sq.timer || !sq.res) return;
	uint32_t f = (uint32_t)sq.frac + (sq.inc & 0xFFFF); sq.frac = (uint16_t)f;
	uint16_t n = (uint16_t)((sq.inc >> 16) + (f >> 16));
	if (!n) return;
	for (int t = 0; t < sq.ntr; t++) {
		track *k = &sq.tr[t];
		k->count -= n;
		if (k->count <= 0 && seq_events(sq.res, k, t == 0)) { seq_stop_timer(); midi_end(); return; }
	}
}
/* 194C:34BA: end of the piece: a looping one (byte 0 bit 7) starts over unless released */
static void midi_end(void)
{
	TRACE(AUDIO_T_MIDI_END, 0, 0);
	if ((sq.res->d[0] & 0x80) && !sq.release) { seq_stop_timer(); seq_start(sq.res); if (sq.timer) timer_start(&sq.next, MIDI_DIV); }
	else sq.busy = 0;
}
/* 194C:3579: stop the MIDI channel (r NULL: whatever plays) */
static void midi_stop(const snd_res *r)
{
	if (r && r != sq.res) return;
	int was = sq.busy; sq.busy = 0;
	if (was) { cue = 0; seq_stop_timer(); }
}
/* 194C:3668 */
static int midi_start(snd_res *r)
{
	if (!(caps & AUDIO_CAP_MIDI)) return 0;
	if (r->n >= 5 && !memcmp(r->d + 1, "MSeq", 4)) {   /* 194C:36DC: a list of events sent at once */
		track t = {0, 5, 0}; seq_events(r, &t, 0); return 0;
	}
	cue = 0; midi_stop(NULL);
	sq.res = r; sq.busy = 1; sq.release = 0;
	seq_start(r);
	if (sq.timer) timer_start(&sq.next, MIDI_DIV);
	return sq.busy;
}

/* ------------------------------------------------------------------------------------------------ digitized sounds (194C:35E5 + DIGI.DRV) */
static struct {
	int busy, stage;              /* DS:2088 / 208A (0 loop, 1 release requested, -1 playing the tail) */
	snd_res *res;                 /* DS:208C */
	int active;                   /* DIGI.DRV 0x260: a transfer runs */
	int speaker;                  /* DSP speaker on (D1) / off (D3) */
	uint32_t off, len;            /* the transfer: resource offset of its first sample, length */
	unsigned q;                   /* the DSP time constant's period in microseconds (1000000 / rate, truncated) */
	uint64_t pos;                 /* position in the transfer, samples << 32 */
} dg = {0, 0, NULL, 0, 1, 0, 0, 90, 0};
static void dsp(uint8_t b) { TRACE(AUDIO_T_DSP, b, 0); }
/* DIGI.DRV 0x6BE (function 7): play len samples at rate: DSP 40 (time constant), DMA, DSP 14 (8-bit single cycle) */
static void digi_play(uint32_t off, uint32_t len, unsigned rate)
{
	if (dg.active) return;
	dg.active = 1; dg.q = (unsigned)(1000000u / (rate ? rate : 1)) & 0xFF; if (!dg.q) dg.q = 256;
	dsp(0x40); dsp((uint8_t)-dg.q);
	dg.off = off; dg.len = len; dg.pos = 0;
	dsp(0x14); dsp((uint8_t)(len - 1)); dsp((uint8_t)((len - 1) >> 8));
}
/* DIGI.DRV 0x776 (function 3): halt (DSP D0) and release the channel */
static void digi_halt(void) { if (dg.active) { dsp(0xD0); dg.active = 0; } }
/* 194C:3422: the transfer's end (DIGI.DRV raises int 15h ax=91F0 from its IRQ) */
static void digi_end(void)
{
	TRACE(AUDIO_T_DIGI_END, 0, 0);
	snd_res *r = dg.res; if (!r) return;
	if (r->d[0] & 0x80) {
		unsigned rate = rd16(r, 1);
		if (dg.stage == 0) {
			unsigned e = rd16(r, 8); if (!e) e = rd16(r, 4);
			unsigned s = rd16(r, 6);
			digi_play(10 + s, (uint16_t)(e - s), rate); return;
		}
		if (dg.stage > 0) {
			unsigned s = rd16(r, 8), n = (uint16_t)(rd16(r, 4) - s);
			if (s && n) { dg.stage = -dg.stage; digi_play(10 + s, n, rate); return; }
		}
	}
	dg.busy = 0;
}
void audio_digi_irq(void) { if (!dg.active) return; dg.active = 0; digi_end(); }
/* 194C:354A */
static void digi_stop(const snd_res *r)
{
	if (r && r != dg.res) return;
	int was = dg.busy; dg.busy = 0;
	if (was) digi_halt();
}
/* 194C:35E5 */
static int digi_start(snd_res *r)
{
	if (!(caps & AUDIO_CAP_DIGI)) return 0;
	digi_stop(NULL);
	dg.busy = 1; dg.stage = 0; dg.res = r;
	unsigned n = rd16(r, 4);
	if (r->d[0] & 0x80) { unsigned e = rd16(r, 8); if (e) n = e; }
	digi_play(10, n, rd16(r, 1));
	return dg.busy;
}
/* DIGI.DRV 0x19E (function 4): speaker on / off by the volume (a running transfer paused around it) */
static void digi_volume(uint8_t v) { if (dg.active) dsp(0xD0); dsp(v ? 0xD1 : 0xD3); dg.speaker = v != 0; if (dg.active) dsp(0xD4); }

/* ------------------------------------------------------------------------------------------------ PC speaker (194C:370A / 377B) */
static struct {
	int busy, release;            /* DS:20B0 / 20B2 */
	snd_res *res;                 /* DS:20B4 */
	uint32_t p;                   /* DS:20C8: next 3-byte entry */
	uint8_t dur, vib;             /* DS:20E2 / 20E3 */
	uint16_t vcount;              /* DS:20DE */
	uint16_t step;                /* DS:20E0: vibrato step (8.8) */
	uint8_t vfrac;                /* DS:20E4 */
	uint16_t div;                 /* DS:20DC: PIT channel 2 divisor */
	int gate;                     /* port 61 bits 0-1 */
	int timer; uint32_t tdiv; uint64_t next;
	uint64_t phase;               /* square wave phase (PIT clocks << 16) */
} sp;
static void spk_gate(int g) { sp.gate = g; TRACE(AUDIO_T_SPEAKER, 0, g); }        /* port 61 bits 0-1 */
static void spk_div(uint16_t d) { sp.div = d; TRACE(AUDIO_T_SPEAKER, 1, d); }     /* PIT channel 2 (square wave) divisor */
static void spk_stop(const snd_res *r)
{
	if (r && r != sp.res) return;
	int was = sp.busy; sp.busy = 0;
	if (was) { cue = 0; sp.timer = 0; spk_gate(0); }
}
/* 194C:37E3: the next entries (frequency word, duration byte); 0 = rest, 1 = vibrato depth, 2 = cue, 3..0x12 = end */
static void spk_next(void)
{
	for (int guard = 0; guard < 10000; guard++) {
		snd_res *r = sp.res; uint32_t e = sp.p; sp.p += 3;
		unsigned f = rd16(r, e); uint8_t al = (uint8_t)rd8(r, e + 2); sp.dur = al;
		if (!f) { spk_gate(0); return; }
		if (f < 0x13) {
			if (f == 1) { sp.vib = al; continue; }
			if (f == 2) { cue = al; continue; }
			if ((r->d[0] & 0x80) && !sp.release) { sp.p = 3; continue; }
			spk_stop(NULL); return;
		}
		spk_gate(volume ? 3 : 0);
		uint16_t d = (uint16_t)(0x1234DCu / f);
		if (sp.vib) {
			sp.vcount = sp.vib;
			uint16_t a = (uint16_t)(d >> 7); uint8_t q1 = (uint8_t)(a / sp.vib), rm = (uint8_t)(a % sp.vib);
			uint8_t q2 = (uint8_t)(((unsigned)rm << 8) / sp.vib);
			sp.step = (uint16_t)(q1 << 8 | q2); sp.vfrac = 0;
		}
		spk_div(d); return;
	}
}
/* 194C:377B: the note list's timer interrupt */
void audio_speaker_irq(void)
{
	if (!sp.busy) return;
	if (--sp.dur == 0) { spk_next(); return; }
	if (!sp.vib) return;
	if (--sp.vcount == 0) { sp.vcount = (uint16_t)(sp.vib * 2); sp.step = (uint16_t)-sp.step; }
	unsigned f = (unsigned)sp.vfrac + (sp.step & 0xFF); sp.vfrac = (uint8_t)f;
	uint8_t hi = (uint8_t)((sp.step >> 8) + (f >> 8));
	if (!hi) return;
	spk_div((uint16_t)(sp.div + (int8_t)hi));
}
/* 194C:370A */
static int spk_start(snd_res *r)
{
	spk_stop(NULL);
	if (dg.busy && !(caps & AUDIO_CAP_DIGI)) digi_stop(NULL);
	sp.res = r; sp.p = 3;
	unsigned rate = rd16(r, 1);
	sp.tdiv = rate > 0x12 ? 0x1234DDu / rate : 0x10000; sp.timer = 1; timer_start(&sp.next, sp.tdiv);
	sp.vib = 0; sp.dur = 1; sp.busy = 1; sp.release = 0;
	if (r->n < 6) { spk_stop(NULL); return 0; }
	audio_speaker_irq();                                     /* int 8: the first entry at once */
	return sp.busy;
}

/* ------------------------------------------------------------------------------------------------ the library API */
/* 194C:3339: by kind (byte 0 bits 0-6): 0 speaker, 1 digitized, 2 MIDI */
static int dispatch(snd_res *r)
{
	switch (r->d[0] & 0x7F) {
	case 0: return spk_start(r);
	case 1: return digi_start(r);
	case 2: return midi_start(r);
	}
	return 0;
}
/* 194C:3320: stop the channels playing r (NULL: all) */
static void stop_res(const snd_res *r) { digi_stop(r); midi_stop(r); spk_stop(r); }
/* 194C:33CE */
static int playing_res(const snd_res *r)
{
	if (!r) return dg.busy | sq.busy | sp.busy;
	if (r == dg.res) return dg.busy;
	if (r == sq.res) return sq.busy;
	if (r == sp.res) return sp.busy;
	return 0;
}
/* 194C:8092 */
static int play_res(snd_res *r) { if (!r) return 0; if (playing_res(r)) stop_res(r); return dispatch(r); }
int audio_request(uint16_t id) { return play_res(res_get(id)); }
int audio_request_res(uint16_t id, const uint8_t *res, uint32_t len)
{
	for (int i = 0; i < ncache; i++) if (cache[i].id == id) return play_res(&cache[i]);
	return play_res(res_add(id, res, len));
}
void audio_stop(uint16_t id) { if (!id) { stop_res(NULL); return; } snd_res *r = res_get(id); if (r) stop_res(r); }
/* 194C:8396 -> 32C6 */
void audio_release(uint16_t id)
{
	snd_res *r = id ? res_get(id) : NULL; if (id && !r) return;
	if (!r || r == dg.res) dg.stage = 1;
	if (!r || r == sq.res) sq.release = 1;
	if (!r || r == sp.res) sp.release = 1;
}
int audio_playing(uint16_t id) { if (!id) return playing_res(NULL); snd_res *r = res_get(id); return r ? playing_res(r) : 0; }
/* 194C:3380 */
void audio_volume(int v)
{
	if (v > 15) v = 15;
	if ((uint8_t)v == volume) return;
	volume = (uint8_t)v;
	if (caps & AUDIO_CAP_DIGI) digi_volume(volume);
	if (caps & AUDIO_CAP_MIDI) fm_call(5, volume, 0, 0, NULL, 0);
	if (sp.busy) spk_gate(volume ? 3 : 0);
}
int audio_cue(void) { return cue; }
const uint8_t *audio_resource(uint16_t id, uint32_t *len) { snd_res *r = res_get(id); if (len) *len = r ? r->n : 0; return r ? r->d : NULL; }

int audio_add_file(const char *path)
{
	if (nfiles >= 8 || !dat_open(&files[nfiles], path)) return 0;
	nfiles++; return 1;
}
/* 194C:2B8C / 31E2: drivers loaded and initialised (DIGI.DRV first, then MIDI.DRV with PRESETS.DEF), volume 15 */
int audio_init(const char *dir, int c)
{
	audio_shutdown();
	caps = c & 3; char p[1024];
	opl_rate = 49716; OPL3_Reset(&opl, opl_rate);
	memset(&dg, 0, sizeof dg); memset(&sq, 0, sizeof sq); memset(&sp, 0, sizeof sp); dg.q = 90;
	memcpy(sq.map, map_init, 16); volume = 0; cue = 0; now = 0;
	if (caps & AUDIO_CAP_DIGI) { dg.speaker = 1; snprintf(p, sizeof p, "%s/DIGISND.DAT", dir); audio_add_file(p); }
	if (caps & AUDIO_CAP_MIDI) {
		snprintf(p, sizeof p, "%s/MIDISND.DAT", dir); audio_add_file(p);
		fm_init();
		snprintf(p, sizeof p, "%s/PRESETS.DEF", dir); FILE *f = fopen(p, "rb");
		if (f) { bank_ok = fread(bank, 1, sizeof bank, f) == sizeof bank; fclose(f); }
		if (bank_ok) { fm.bank = bank + 1; fm.count = bank[0]; fm_programs(); }   /* function 6 */
	}
	if (!caps) { snprintf(p, sizeof p, "%s/IBMSND.DAT", dir); audio_add_file(p); }
	audio_volume(15);
	return nfiles > 0;
}
void audio_shutdown(void)
{
	for (int i = 0; i < ncache; i++) free(cache[i].d);
	free(cache); cache = NULL; ncache = capcache = 0;
	for (int f = 0; f < nfiles; f++) free(files[f].data);
	nfiles = 0; bank_ok = 0;
}

/* ------------------------------------------------------------------------------------------------ rendering */
static void opl_set_rate(uint32_t rate)
{
	if (rate == opl_rate) return;
	opl_rate = rate; OPL3_Reset(&opl, rate);
	OPL3_WriteReg(&opl, 0x01, fm.reg[0x01]);
	for (int r = 0x20; r < 0x100; r++) if ((r & 0xF0) != 0xB0) OPL3_WriteReg(&opl, (uint16_t)r, fm.reg[r]);
	for (int r = 0xB0; r < 0xB9; r++) OPL3_WriteReg(&opl, (uint16_t)r, fm.reg[r]);
	OPL3_WriteReg(&opl, 0xBD, fm.reg[0xBD]);
}
void audio_render(int16_t *pcm, int frames, int rate)
{
	if (rate <= 0) return;
	opl_set_rate((uint32_t)rate);
	uint64_t step = ((uint64_t)PIT_HZ << 16) / (unsigned)rate;
	uint64_t dstep = (uint64_t)((1000000.0 / dg.q) / rate * 4294967296.0);
	for (int i = 0; i < frames; i++) {
		uint64_t end = now + step;
		while (!audio_manual_clock) {                        /* the timer interrupts due before this sample */
			uint64_t t = UINT64_MAX; int which = 0;
			if (sq.timer && sq.next < t) { t = sq.next; which = 1; }
			if (sp.timer && sp.next < t) { t = sp.next; which = 2; }
			if (t > end) break;
			now = t;
			if (which == 1) { sq.next += (uint64_t)MIDI_DIV << 16; audio_midi_irq(); }
			else { sp.next += (uint64_t)sp.tdiv << 16; audio_speaker_irq(); }
		}
		now = end;
		int16_t b[2]; OPL3_GenerateResampled(&opl, b);
		int32_t s = (int32_t)(((b[0] + b[1]) / 2) * audio_gain_fm) >> 8;
		if (dg.active) {
			uint32_t k = (uint32_t)(dg.pos >> 32);
			if (k >= dg.len) {
				if (!audio_manual_clock) { dg.active = 0; digi_end(); dstep = (uint64_t)((1000000.0 / dg.q) / rate * 4294967296.0); }
			} else {
				const snd_res *r = dg.res; uint32_t fr = (uint32_t)(dg.pos >> 16) & 0xFFFF;
				int a = (int)rd8(r, dg.off + k) - 128, c = k + 1 < dg.len ? (int)rd8(r, dg.off + k + 1) - 128 : a;
				int v = a * 256 + (int)(((int64_t)(c - a) * 256 * fr) >> 16);
				if (dg.speaker) s += (v * audio_gain_digi) >> 8;
				dg.pos += dstep;
			}
		}
		if (sp.busy && sp.gate == 3) {
			uint64_t per = (uint64_t)(sp.div ? sp.div : 0x10000) << 16;
			sp.phase = (sp.phase + step) % per;
			s += ((sp.phase < per / 2 ? 4850 : -4850) * audio_gain_speaker) >> 8;   /* DOSBox-X's speaker level */
		}
		if (audio_dc_block) { static int32_t xin, yout; int32_t y = s - xin + (int32_t)(((int64_t)yout * 32702) >> 15); xin = s; yout = y; s = y; }   /* the card's output coupling (~14 Hz high-pass) */
		pcm[i] = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
	}
}

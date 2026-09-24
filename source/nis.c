/* The story scenes (NIS). Transcribed from overlay OVL00 (2D3E, 2D7D, 32D4, 33B9) and the parts of the resident
 * graphics/resource/sound library (194C, 25A1, 2583, 2631, 2797) they use. docs/NIS.md describes the formats.
 *
 * The scene code of the game is straight-line C with blocking waits (for the 60 Hz timer tick, the vertical retrace,
 * a MIDI cue point or the end of a sound). It runs here as a coroutine (coro.h) that yields to nis_step whenever the
 * original would busy-wait; nis_step advances the clock by one video frame and resumes it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coro.h"
#include "dat.h"
#include "nis.h"

#define W 320
#define H 200

/* ------------------------------------------------------------------ resources (194C:6F4C, 2797:01D4 / 01B6) */
/* The open resource files form a chain searched from the most recently opened one. */
#define MAXFILES 16
static struct { char name[16]; dat_file f; } files[MAXFILES];
static int nfiles;
static char g_dir[512];
static dat_file cache_f[MAXFILES]; static char cache_n[MAXFILES][16]; static int ncache;

static dat_file *dat_load(const char *name)
{
	for (int i = 0; i < ncache; i++) if (!strcmp(cache_n[i], name)) return &cache_f[i];
	if (ncache == MAXFILES) return NULL;
	char p[640]; snprintf(p, sizeof p, "%s/%s", g_dir, name);
	if (!dat_open(&cache_f[ncache], p)) return NULL;
	snprintf(cache_n[ncache], 16, "%s", name);
	return &cache_f[ncache++];
}
static void res_open(const char *name)
{
	for (int i = 0; i < nfiles; i++) if (!strcmp(files[i].name, name)) return;
	dat_file *d = dat_load(name); if (!d || nfiles == MAXFILES) return;
	snprintf(files[nfiles].name, 16, "%s", name); files[nfiles].f = *d; nfiles++;
}
static void res_forget(const char *file);
static void res_close(const char *name)
{
	res_forget(name);
	for (int i = 0; i < nfiles; i++) if (!strcmp(files[i].name, name)) { memmove(&files[i], &files[i + 1], (nfiles - i - 1) * sizeof files[0]); nfiles--; return; }
}
/* tag as written in the game ("SHAP", "PALT", "_SCR"...; '_' = NUL) */
static const uint8_t *res_find(const char *tag, int id, int *size)
{
	char t[4]; for (int i = 0; i < 4; i++) t[i] = tag[3 - i] == '_' ? 0 : tag[3 - i];
	for (int i = nfiles - 1; i >= 0; i--) {
		uint16_t sz; const uint8_t *p = dat_find(&files[i].f, t, (uint16_t)id, &sz);
		if (p) { if (size) *size = sz + 1; return p; }     /* (dat_find's size is one short: the body is the whole size field) */
	}
	if (size) *size = 0;
	return NULL;
}
/* 194C:6F4C: a resource for the game: read from its file the first time (~5.7 cycles a byte: the DOS read and the
 * checksum), then found in memory (~380 cycles) until the file is closed */
static void cpu(double n);
static struct { char tag[4]; int id; char file[16]; } res_mem[512]; static int n_res_mem;
static const uint8_t *res_get(const char *tag, int id, int *size)
{
	int sz; const uint8_t *r = res_find(tag, id, &sz);
	if (size) *size = sz;
	if (!r) return NULL;
	for (int i = 0; i < n_res_mem; i++) if (res_mem[i].id == id && !memcmp(res_mem[i].tag, tag, 4)) { cpu(380); return r; }
	if (n_res_mem < 512) { memcpy(res_mem[n_res_mem].tag, tag, 4); res_mem[n_res_mem].id = id; res_mem[n_res_mem].file[0] = 0;
		for (int i = nfiles - 1; i >= 0; i--) { uint16_t z; char t[4]; for (int k = 0; k < 4; k++) t[k] = tag[3 - k] == '_' ? 0 : tag[3 - k];
			if (dat_find(&files[i].f, t, (uint16_t)id, &z)) { snprintf(res_mem[n_res_mem].file, 16, "%s", files[i].name); break; } }
		n_res_mem++; }
	cpu(2150 + sz * 5.7);
	return r;
}
static void res_forget(const char *file)       /* (closing a file frees its resources) */
{
	int k = 0;
	for (int i = 0; i < n_res_mem; i++) if (strcmp(res_mem[i].file, file)) res_mem[k++] = res_mem[i];
	n_res_mem = k;
}
static int rd16(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }
static int ru16(const uint8_t *p) { return p[0] | p[1] << 8; }

/* ------------------------------------------------------------------ time: frames, ticks, the coroutine */
/* Time runs in CPU cycles of the reference machine (the oracle's DOSBox-X: 22000 cycles/ms, one cycle an instruction,
 * an 8-bit IN 22 more and an OUT 16 more): video frames at 70.086 Hz (313900 cycles), the timer tick at 60 Hz (366670
 * cycles; the MIDI sequencer at 4x). The scene's own work costs cycles by a model of its routines (cpu()), so long
 * operations (unpacking images, dissolving, drawing text) take time as in the game; busy waits sleep until the next
 * interrupt or frame. The timer interrupts take their time too: the sequencer's (194C:2F10) sends the MIDI events to
 * the FM driver, whose every OPL register write spins through two delay loops (~4700 cycles), so a chord takes most of
 * a frame. Meanwhile the scene is frozen, a retrace can pass unseen by the polling loop, and the PIT's next interrupts
 * wait (one stays pending, the others are lost: ticks go missing). */
#define FRAME_CYC 313899u             /* 22000000 * 800 * 449 / 25175000 (mode 13h: 25.175 MHz, 800 x 449 dots) */
#define TICK_DIV 19886u               /* the 60 Hz tick's PIT divisor */
#define MUSIC_DIV 4971u               /* the MIDI sequencer's (240 Hz) */
#define TICK_CYC 366660u              /* (its period in cycles, 19886 * 22000000 / 1193182) */
#define MUSIC_CYC 91656u
static uint64_t fstart(uint64_t g) { return g * 7902400000ull / 25175ull; }   /* the retrace that begins frame g */
#define TICK_RATIO 16382u             /* 16.16 fdiv(4971, 19886): the tick chained from the sequencer's interrupt */
#define RETRACE_CYC 1398u             /* the vertical retrace bit (3DA bit 3) stays set 2 of the 449 lines of a frame */
#define IN_CYC 23                     /* an IN instruction (with DOSBox-X's 8-bit I/O delay) */
#define OUT_CYC 17                    /* an OUT */
/* the interrupt handlers' costs (measured in the oracle) */
#define ISR_TICK_CYC 60               /* 194C:7EE7 (the 60 Hz tick, its EOI) */
#define ISR_CHAIN_CYC 45              /* ... chained from the sequencer's handler */
#define ISR_SEQ_IDLE_CYC 61           /* 194C:2F10 when no MIDI tick is due */
#define ISR_SEQ_CYC 96                /* ... with the tracks' counts stepped */
#define ISR_EVENT_CYC 99              /* a MIDI event read and handed to the driver */
#define OPL_WRITE_CYC 4713            /* MIDI.DRV+0x8AF: two OUTs and the delay loops ([0x147] 1033 + [0x149] 517 turns of 3) */
static coro *scene_coro;   /* (coro.h: the scene runs as a coroutine of whoever steps it) */
static int scene_done, scene_result, g_abort;
static uint32_t g_frame, g_tick;
static uint64_t g_time;                /* cycles */
static uint64_t next_isr;              /* the PIT's next interrupt request */
static uint64_t pit_base; static uint32_t pit_k, pit_div;   /* its count restarted at pit_base: request k at pit_base + k periods */
static void pit_next(void) { pit_k++; next_isr = pit_base + (uint64_t)pit_k * pit_div * 22000000ull / 1193182ull; }
static int isr_music;                  /* the PIT runs at the sequencer's rate (a song plays) */
static uint32_t isr_acc;               /* DS:1FDE: the chained tick's accumulator */
static uint32_t isr_lost;              /* interrupt requests lost while a handler ran */
static nis_tick_fn tick_fn; static void *tick_user;
static nis_sound_fn sound_fn; static void *sound_user;
static uint16_t countdown[4];          /* DS:24DC.. the four countdown timers of the timer interrupt (194C:7EE7) */
static int cur_scene;
static double cpu_scale = 1.0;
static nis_event_fn event_fn; static void *event_user;
static void event(int ev) { if (event_fn) event_fn(ev, g_frame, event_user); }
static int trace_on = -1;
static void trace(const char *what, int a, int b)
{
	if (trace_on < 0) trace_on = getenv("NIS_TRACE") != NULL;
	if (trace_on) fprintf(stderr, "[nis] frame %u tick %u (+%u): %s %d %d\n", g_frame, g_tick, (unsigned)(g_time - fstart(g_frame)), what, a, b);
}
static void do_tick(void);
static uint32_t mus_interrupt(void);
static uint64_t digi_wake(void);
static uint64_t frame_end(void) { return fstart(g_frame + 1); }
/* 194C:7D76 / 7E37: adding or removing the sequencer's timer reprograms the PIT (mode 3), restarting its count. In
 * DOSBox-X the counter's output goes high again: when it was low (the second half of a square wave period) that is an
 * edge, and the new handler runs at once. */
static void pit_music(int on)
{
	if (on == isr_music) return;
	uint64_t per = isr_music ? MUSIC_CYC : TICK_CYC;
	int edge = !tick_fn && next_isr > g_time && next_isr - g_time <= per / 2;
	isr_music = on; isr_acc = 0;
	pit_base = g_time; pit_k = 0; pit_div = on ? MUSIC_DIV : TICK_DIV; pit_next();
	if (edge) { next_isr = g_time; pit_k = 0; }
}
/* one timer interrupt (at its request, or as soon as the handler before it has returned): with a song, the sequencer
 * (194C:2F10) which chains to the tick (194C:7EE7) every ~4th time */
static void do_isr(void)
{
	if (next_isr > g_time) g_time = next_isr;
	uint64_t per = isr_music ? MUSIC_CYC : TICK_CYC;
	uint32_t cost;
	pit_next();
	if (isr_music) {
		cost = mus_interrupt();
		isr_acc += TICK_RATIO;
		if (isr_acc >> 16) { isr_acc &= 0xFFFF; do_tick(); cost += ISR_CHAIN_CYC; }
	} else { do_tick(); cost = ISR_TICK_CYC; }
	if (cost > 1000) trace("isr", (int)cost, (int)(next_isr - per - g_time));
	g_time += cost;
	if (!isr_music && per == MUSIC_CYC) return;      /* (the song ended in the handler: the PIT was reprogrammed) */
	/* the requests while it ran: the first stays pending in the PIC, the others are lost */
	if (next_isr <= g_time) for (;;) { uint64_t n = next_isr; uint32_t k = pit_k; pit_next(); if (next_isr > g_time) { next_isr = n; pit_k = k; break; } isr_lost++; }
}
/* the scene idles (polls) until time t: the interrupts on the way happen (the scene resumes when the last returns) */
static void advance(uint64_t t)
{
	if (tick_fn) { g_time = t > g_time ? t : g_time; return; }     /* (ticks come from the caller, a frame at a time) */
	while (next_isr <= (t > g_time ? t : g_time)) do_isr();
	if (t > g_time) g_time = t;
}
static void end_frame(void) { coro_yield(scene_coro); }
/* work of n cycles (the interrupts on the way add theirs) */
static void cpu(double n)
{
	uint64_t left = (uint64_t)(n * cpu_scale);
	if (tick_fn) g_time += left;
	else for (;;) {
		if (next_isr > g_time + left) { g_time += left; break; }
		if (next_isr > g_time) { left -= next_isr - g_time; g_time = next_isr; }
		do_isr();
	}
	while (g_time >= frame_end()) end_frame();
}
/* a busy wait polling for a change: sleep until the next interrupt or the next frame */
static void yield(void)
{
	uint64_t f = frame_end(), t = tick_fn ? f : next_isr;
	uint64_t d = digi_wake(); if (d > g_time && d < t) t = d;                /* (the DSP's interrupt ends a digitized sound) */
	if (t < f) advance(t); else { advance(f); end_frame(); }
}
/* 194C:7A26: wait for the vertical retrace (returns at once while it lasts); the polling misses a retrace that passes
 * while an interrupt handler runs */
static void wait_retrace(void)
{
	for (;;) {
		uint64_t start = fstart(g_frame);
		if (g_time >= start && g_time - start < RETRACE_CYC) return;
		advance(frame_end()); end_frame();
		if (tick_fn) return;
	}
}
/* 2797:009C: the key check */
static int key_pressed(void) { return g_abort; }

/* ------------------------------------------------------------------ sound model */
/* 194C:2DF0 / 2F10 / 2FFA: the MIDI sequencer, run at 240 Hz (the timer chain calls the 60 Hz tick every 4th time), and
 * the FM driver under it (MIDI.DRV, docs/AUDIO.md 4; played for real by source/audio.c). Only their timing is modelled:
 * cue points (meta event 7 sets DS:2087 to the text's first byte), the end of the song (end of track 0; a looping one
 * starts over) and the time the interrupt handler spends, which is the driver's OPL register writes. Digitized sounds
 * (type 1) play for their length at the Sound Blaster's rate. */
typedef struct { const uint8_t *p, *end; int32_t delta; uint8_t running; int done; } mtrack;
static struct {
	int playing, id, loop; const uint8_t *res; int size;
	mtrack tr[16]; int ntr; int division; uint32_t inc, acc;
	uint8_t map[16]; int skip15;        /* DS:2073 (bit 7: note-ons muted), DS:2071 */
} mus;
/* MIDI.DRV's state as far as it decides how many registers a call writes */
static struct { uint8_t note[9], chan[9], prog[16], rr; int count; } fm;
static uint32_t opl_writes;
static int fm_bank_count = 1;
static struct { int playing, id; uint64_t end; } digi;
static uint8_t cue;                     /* DS:2087 */

/* MIDI.DRV functions 2 / 4 (+0x40E): silence the nine voices (A0..A8, B0..B8), channel n plays instrument n */
static void fm_reset(void)
{
	opl_writes += 18;
	memset(fm.note, 0, sizeof fm.note); memset(fm.chan, 0, sizeof fm.chan);
	for (int c = 0; c < 16; c++) fm.prog[c] = (uint8_t)(c < fm.count ? c : 0);
}
/* +0x5AE: the first voice with this note and channel keys off (B0) */
static void fm_note_off(int c, int n)
{
	for (int v = 0; v < 9; v++) if (fm.note[v] == n && fm.chan[v] == c) { fm.note[v] = 0; opl_writes++; return; }
}
/* +0x54A: a free voice (round robin from +0x14D, which moves on either way) takes the channel's instrument (C0 and the
 * two operators' five registers) and the note's frequency (A0, B0); none free: the note is dropped */
static void fm_note_on(int c, int n, int vel)
{
	if (!vel) { fm_note_off(c, n); return; }
	if (fm.prog[c] >= fm.count) return;
	int v = fm.rr, found = -1;
	for (int k = 0; k < 9; k++) { if (!fm.note[v]) { found = v; break; } if (++v >= 9) v = 0; }
	if (++fm.rr >= 9) fm.rr = 0;
	if (found < 0) return;
	fm.chan[found] = (uint8_t)c; fm.note[found] = (uint8_t)n;
	opl_writes += 13;
}
static void fm_event(int ty, int c, int d1)
{
	switch (ty) {
	case 0x80: fm_note_off(c, d1); break;
	case 0xC0: if (d1 < fm.count) fm.prog[c] = (uint8_t)d1; break;
	case 0xE0: for (int v = 0; v < 9; v++) if (fm.note[v] && fm.chan[v] == c) opl_writes += 2; break;   /* +0x637: retuned */
	}
}
static uint32_t mus_events_sent;
static uint32_t fdiv16(uint32_t a, uint32_t b) { return b ? (uint32_t)(((uint64_t)a << 16) / b) : 0; }   /* 194C:7C68 */
static void mus_tempo(uint32_t tempo) { mus.inc = fdiv16(fdiv16(0x0F424000u, 240u << 16), fdiv16(tempo << 8, (uint32_t)mus.division << 16)); }   /* 194C:2FB4 */
static uint32_t varlen(const uint8_t **pp, const uint8_t *end)
{
	uint32_t v = 0; const uint8_t *p = *pp;
	while (p < end) { uint8_t b = *p++; v = v << 7 | (b & 0x7F); if (!(b & 0x80)) break; }
	*pp = p; return v;
}
/* one track's events due now; returns 1 when track 0 ended the song */
static int mus_events(int t)
{
	mtrack *k = &mus.tr[t];
	do {
		if (k->p >= k->end) { k->delta = 0x7FFFFFFF; return t == 0; }
		uint8_t b = *k->p++;
		if (b == 0xFF) {
			uint8_t type = *k->p++; const uint8_t *q = k->p; uint32_t len = varlen(&q, k->end);
			if (type == 0x2F) { if (t == 0) return 1; k->delta = 0x7FFFFFFF; return 0; }
			if (type == 0x07 && len) { cue = q[0]; trace("cue", cue, mus.id); event(NIS_EV_CUE); }
			/* (194C:316B reads the tempo's low two bytes as a little-endian word: the middle and low bytes swap) */
			if (type == 0x51 && t == 0 && len >= 3) mus_tempo((uint32_t)q[0] << 16 | q[2] << 8 | q[1]);
			k->p = q + len;
		} else if (b == 0xF0 || b == 0xF7) {        /* 194C:306E: 00 00 34 dev cmd x: the sequencer's commands */
			const uint8_t *q = k->p; uint32_t len = varlen(&q, k->end);
			if (len >= 6 && q + 6 <= k->end && !q[0] && !q[1] && q[2] == 0x34 && (q[3] == 0 || q[3] == 0x21)) {
				int x = q[5];
				switch (q[4]) {
				case 0: mus_events_sent++; break;
				case 1: if (x < 16) mus.map[x] |= 0x80; break;
				case 2: if (x < 16) mus.map[x] &= 0x7F; break;
				case 3: if (x < 16 && len >= 7) mus.map[x] = (uint8_t)((mus.map[x] & 0x80) | q[6]); break;
				case 4: for (int i = 0; i < 15; i++) mus.map[i] = (uint8_t)((mus.map[i] & 0x80) | i); break;
				case 5: for (uint32_t i = 0; i < len && i < 16; i++) { if (x) mus.map[i] &= 0x7F; else mus.map[i] |= 0x80; } break;
				case 6: mus.skip15 = 1; break;
				case 7: mus.skip15 = 0; break;
				}
			} else mus_events_sent++;
			k->p = q + len;
		} else {
			if (b < 0x80) { k->p--; b = k->running; }
			k->running = b;
			int ty = b & 0xF0, d1 = k->p < k->end ? k->p[0] : 0, d2 = k->p + 1 < k->end ? k->p[1] : 0;
			k->p += (ty == 0xC0 || ty == 0xD0) ? 1 : 2;
			uint8_t m = mus.map[b & 0x0F];
			if (!(ty == 0x90 && (m & 0x80)) && !((m & 0x0F) == 0x0F && mus.skip15)) {
				mus_events_sent++;
				if (ty == 0x90) fm_note_on(m & 0x0F, d1, d2); else fm_event(ty, m & 0x0F, d1);
			}
		}
		k->delta += (int32_t)varlen(&k->p, k->end);
	} while (k->delta <= 0);
	return 0;
}
static void snd_event(int kind, int id) { if (sound_fn) sound_fn(kind, id, sound_user); }
static void pit_music(int on);
/* the cycles the driver calls and events so far cost (and the counts restart) */
static uint32_t mus_cost(void)
{
	uint32_t c = opl_writes * OPL_WRITE_CYC + mus_events_sent * ISR_EVENT_CYC;
	opl_writes = 0; mus_events_sent = 0;
	return c;
}
/* 194C:3579 / 2EFD: stop the song: its timer goes (the PIT back to 60 Hz), the driver is reset */
static void mus_halt(void) { if (mus.playing) snd_event(NIS_SND_STOP, mus.id); mus.playing = 0; pit_music(0); fm_reset(); }
static void mus_stop(void) { if (mus.playing) { cue = 0; mus_halt(); cpu(mus_cost()); } }   /* (cpu: then any interrupt pending) */
/* 194C:2DF0: the driver reset, the tracks, their first events (delta 0) */
static int mus_begin(void)
{
	fm_reset();
	for (int i = 0; i < 16; i++) mus.map[i] &= 0x0F;
	mus.skip15 = 0;
	const uint8_t *p = mus.res + 1, *end = mus.res + mus.size;   /* (byte 0: the sound type, 2 = MIDI; bit 7 = loop) */
	if (mus.size < 15 || memcmp(p, "MThd", 4)) return 0;
	int ntr = p[10] << 8 | p[11]; mus.division = p[12] << 8 | p[13];
	if (mus.division & 0x8000) return 0;
	p += 8 + (p[4] << 24 | p[5] << 16 | p[6] << 8 | p[7]);
	mus.ntr = 0; mus_tempo(500000); mus.acc = 0;
	for (int t = 0; t < ntr && t < 16 && p + 8 <= end; t++) {
		uint32_t len = (uint32_t)p[4] << 24 | p[5] << 16 | p[6] << 8 | p[7];
		mtrack *k = &mus.tr[mus.ntr++]; k->p = p + 8; k->end = p + 8 + len > end ? end : p + 8 + len; k->running = 0;
		k->delta = (int32_t)varlen(&k->p, k->end); k->done = 0;
		p += 8 + len;
	}
	for (int t = 0; t < mus.ntr; t++) if (mus.tr[t].delta == 0 && mus_events(t)) return 0;
	return 1;
}
/* 194C:3668: start a song (the one playing is stopped first) */
static void mus_start(int id, const uint8_t *r, int size)
{
	cue = 0;
	if (mus.playing) { mus_halt(); }
	mus.res = r; mus.size = size; mus.id = id; mus.loop = r[0] & 0x80;
	int ok = mus_begin();
	cpu(mus_cost());
	if (!ok) return;
	mus.playing = 1;
	snd_event(NIS_SND_MUSIC, id);
	pit_music(1);                        /* (the timer is installed last) */
	cpu(0);                              /* (an interrupt the reprogramming raised runs at once) */
}
/* 194C:2F10: the sequencer's interrupt; returns the cycles it took */
static uint32_t mus_interrupt(void)
{
	if (!mus.playing) return ISR_SEQ_IDLE_CYC;
	uint32_t a = (mus.acc & 0xFFFF) + (mus.inc & 0xFFFF);
	uint32_t n = (mus.inc >> 16) + (a >> 16); mus.acc = a & 0xFFFF;
	if (!n) return ISR_SEQ_IDLE_CYC;
	for (int t = 0; t < mus.ntr; t++) {
		mtrack *k = &mus.tr[t];
		if (k->delta == 0x7FFFFFFF) continue;
		k->delta -= (int32_t)n;
		if (k->delta <= 0 && mus_events(t)) {
			/* 194C:34BA: the end; a looping song starts over (its timer anew) */
			int id = mus.id;
			mus_halt();
			if (mus.loop) {
				if (mus_begin()) { mus.playing = 1; mus.id = id; snd_event(NIS_SND_MUSIC, id); pit_music(1); }
			}
			break;
		}
	}
	return ISR_SEQ_CYC + mus_cost();
}
/* 194C:35E5 -> DIGI.DRV function 7: the Sound Blaster plays the samples at 1000000 / trunc(1000000 / rate) Hz (its time
 * constant); the end (the DSP's interrupt, 194C:3422) frees the channel at once */
static void digi_start(int id, const uint8_t *r, int size)
{
	/* header: type, rate word, (FF), length word (0 here), 4 more; at +10 the packed samples, their unpacked length first */
	int rate = ru16(r + 1); if (rate <= 0) rate = 11000;
	int n = ru16(r + 4); if (!n && size >= 12) n = ru16(r + 10);
	digi.playing = 1; digi.id = id; digi.end = g_time + (uint64_t)n * (1000000 / rate) * 22;
	snd_event(NIS_SND_DIGI, id);
}
static uint64_t digi_wake(void) { return digi.playing ? digi.end : 0; }
static void digi_poll(void) { if (digi.playing && g_time >= digi.end) { digi.playing = 0; snd_event(NIS_SND_STOP, digi.id); } }
static const uint8_t *snd_res(int id, int *size) { return res_get("_SND", id, size); }
/* 194C:8466: the packed samples' decoder, for its cost: per round 7 instructions (8 in mode 0), 8 a bit read (9 for
 * a 1), then 4 more and 5 (a delta), 6 (the escape back to mode 0), 3 (+0x80) or 5 (a new delta width) */
static double unpack_cost(const uint8_t *s, int sn)
{
	if (sn < 3) return 0;
	int n = s[0] | s[1] << 8, si = 3, di = 1; uint8_t mask = 0x80, bl = 0, dl = 0; double c = 0;
	for (;;) {
		c += 2;
		if (di >= n) break;
		c += bl ? 3 : 4;
		int ch = bl ? bl : 3; uint8_t cl = 0;
		while (ch--) { cl = (uint8_t)(cl << 1); c += 8; if (si < sn && (s[si] & mask)) { cl++; c++; } mask = (uint8_t)(mask >> 1 | mask << 7); if (mask == 0x80) si++; }
		c += 4;
		if (bl) { if (cl == dl) { bl = 0; c += 2; continue; } c += cl > dl ? 5 : 4; di++; }
		else if (cl == 7) { c += 3; di++; }
		else { c += 5; bl = (uint8_t)(cl + 2); dl = (uint8_t)(0xFC02u << cl); }
		if (si > sn + 1) break;
	}
	return c;
}
/* 194C:805C: load a sound (once): the resource read (194C:6F4C, ~5.6 cycles a byte), a digitized one unpacked for the
 * driver (194C:8118: the decoder, then ~2.35 cycles a sample) */
static int snd_loaded[64], n_snd_loaded;
static void sound_load(int id)
{
	for (int i = 0; i < n_snd_loaded; i++) if (snd_loaded[i] == id) { res_get("_SND", id, NULL); return; }
	if (n_snd_loaded < 64) snd_loaded[n_snd_loaded++] = id;
	int size; const uint8_t *r = snd_res(id, &size);
	if (r && (r[0] & 0x7F) == 1 && size > 12 && r[3] == 0xFF) cpu(unpack_cost(r + 10, size - 10) + ru16(r + 10) * 2.35);
}
static void sound_preload(int id) { sound_load(id); }          /* 194C:805C */
/* 194C:840E: play a sound by id (type 1 digitized, 2 MIDI; type 0 PC speaker is not used by the scenes) */
static void sound_play(int id)
{
	trace("sound", id, 0); event(NIS_EV_SOUND);
	sound_preload(id);
	int size; const uint8_t *r = res_find("_SND", id, &size);
	if (!r) return;
	if ((r[0] & 0x7F) == 2) mus_start(id, r, size);
	else if ((r[0] & 0x7F) == 1) digi_start(id, r, size);
}
/* 194C:8426: is sound id playing */
static int sound_playing(int id)
{
	digi_poll();
	if (mus.playing && mus.id == id) return 1;
	if (digi.playing && digi.id == id) return 1;
	return 0;
}
static int any_sound_playing(void) { digi_poll(); return mus.playing || digi.playing; }   /* 194C:33CE(NULL) */
/* 194C:83D2: stop sound id (0: all) */
static void sound_stop(int id)
{
	if (!id || (mus.playing && mus.id == id)) mus_stop();
	if (!id || (digi.playing && digi.id == id)) { if (digi.playing) snd_event(NIS_SND_STOP, digi.id); digi.playing = 0; }
}
static void sound_stop_all(void) { sound_stop(0); }    /* 194C:3320(0,0) */

/* ------------------------------------------------------------------ the VGA palette (194C:79A3 / 7969) */
static uint8_t dac[768];
/* the DAC as the scene begins: the game's palette (0AAC:0274 blacks it out first but for 1, 6 and 100; transition 6
 * sets colours 0..0xDF only, so its item (image set 0, bank 15) shows in the game's colours). nis_set_palette gives it;
 * by default the colours the NISn cheat leaves untouched at 0xE0..0xFF: the BIOS's mode 13h palette (the rest black) */
static uint8_t init_dac[768]; static int init_dac_set;
static const uint8_t bios_e0[24][3] = {   /* INT 10h mode 13h default DAC, colours 0xE0..0xF7 (0xF8..0xFF black) */
	{0x0b,0x0b,0x10},{0x0c,0x0b,0x10},{0x0d,0x0b,0x10},{0x0f,0x0b,0x10},{0x10,0x0b,0x10},{0x10,0x0b,0x0f},{0x10,0x0b,0x0d},{0x10,0x0b,0x0c},
	{0x10,0x0b,0x0b},{0x10,0x0c,0x0b},{0x10,0x0d,0x0b},{0x10,0x0f,0x0b},{0x10,0x10,0x0b},{0x0f,0x10,0x0b},{0x0d,0x10,0x0b},{0x0c,0x10,0x0b},
	{0x0b,0x10,0x0b},{0x0b,0x10,0x0c},{0x0b,0x10,0x0d},{0x0b,0x10,0x0f},{0x0b,0x10,0x10},{0x0b,0x0f,0x10},{0x0b,0x0d,0x10},{0x0b,0x0c,0x10},
};
/* 194C:79A3: set count colours from first (src NULL: black), after a retrace wait if asked */
static void setpal(int first, int count, const uint8_t *src, int wait)
{
	event(NIS_EV_SETPAL);
	if (wait) wait_retrace();
	for (int i = 0; i < count * 3 && first * 3 + i < 768; i++) dac[first * 3 + i] = src ? src[i] & 0x3F : 0;
	cpu(40 + count * (src ? 19 + 3 * OUT_CYC : 16 + 3 * OUT_CYC));        /* (3 OUTs a colour, with nops between) */
}
static void getpal(uint8_t *dst, int count, int first)    /* 194C:7969 (waits for the retrace too) */
{
	wait_retrace();
	memcpy(dst, dac + first * 3, count * 3);
	g_time += 40 + count * (19 + 3 * IN_CYC);                /* (the reading runs with the interrupts off: they wait) */
	cpu(0);
}

/* ------------------------------------------------------------------ ports and rectangles (the QuickDraw-like 194C) */
typedef struct { int t, l, b, r; } rect;
typedef struct port {
	uint8_t *bits; rect bounds; int rowbytes;     /* +0, +4, +0xC */
	rect portRect, clip;                          /* +0x10, +0x18 (clip in pixel coordinates: bounds-relative) */
	int bg, penv, penh, fg, font;                 /* +0x20, +0x22/24, +0x28, +0x2A */
} port;
static uint8_t screen_bits[W * H];
/* The monitor: after the retrace that begins a frame (its start here) come 37 more blank lines (to line 449 of 449),
 * then the 200 rows, each scanned twice (lines 0..399). A row shows what video memory holds when the beam passes it,
 * so a change the scene makes during the frame shows below the beam at once and above it from the next frame. */
static uint8_t scan_bits[W * H];        /* the frame being shown */
static int scan_row;                    /* rows of it scanned so far */
static uint64_t row_time(int y) { return fstart(g_frame) + (uint64_t)(37 + 2 * y) * FRAME_CYC / 449; }
static void beam(uint64_t t)            /* the beam has come this far at time t: those rows show the screen as it is */
{
	while (scan_row < H && row_time(scan_row) <= t) { memcpy(scan_bits + scan_row * W, screen_bits + scan_row * W, W); scan_row++; }
}
static port screen_port = { screen_bits, {0, 0, H, W}, W, {0, 0, H, W}, {0, 0, H, W}, 0, 0, 0, 15, 0 };
static port *the_port = &screen_port;             /* DS:2450 */
static const rect R_SCREEN = {0, 0, 200, 320};    /* DS:1F2A */
static const rect R_GAME __attribute__((unused)) = {0, 0, 192, 320};      /* DS:097E */
static const rect R_PICTURE = {32, 40, 152, 280}; /* DS:13B0: the story pictures' frame */

static int empty_rect(const rect *r) { return !(r->l < r->r && r->t < r->b); }     /* 194C:4D50 */
static rect offset_rect(rect r, int dv, int dh) { r.t += dv; r.l += dh; r.b += dv; r.r += dh; return r; }   /* 194C:50EC */
static int sect_rect(const rect *a, const rect *b, rect *d)                     /* 194C:5266 */
{
	int l = a->l > b->l ? a->l : b->l, r = a->r < b->r ? a->r : b->r;
	int t = a->t > b->t ? a->t : b->t, bo = a->b < b->b ? a->b : b->b;
	if (l < r && t < bo) { d->t = t; d->l = l; d->b = bo; d->r = r; return 1; }
	memset(d, 0, sizeof *d); return 0;
}
static rect union_rect(const rect *a, const rect *b)                            /* 194C:647E */
{
	rect d = { a->t < b->t ? a->t : b->t, a->l < b->l ? a->l : b->l, a->b > b->b ? a->b : b->b, a->r > b->r ? a->r : b->r };
	return d;
}
/* 194C:399C: clip = portRect & bounds, in pixel coordinates */
static void port_reset_clip(port *p)
{
	sect_rect(&p->portRect, &p->bounds, &p->clip);
	p->clip = offset_rect(p->clip, -p->bounds.t, -p->bounds.l);
}
/* 194C:52E8 SetOrigin: move the local coordinates so that portRect's top left is (v, h) */
static void set_origin(port *p, int v, int h)
{
	int dv = v - p->portRect.t, dh = h - p->portRect.l;
	p->portRect = offset_rect(p->portRect, dv, dh); p->bounds = offset_rect(p->bounds, dv, dh);
}
/* 194C:3890 NewPort: an offscreen 8-bit port for rect (portRect = bounds = rect) */
static port *port_new(rect r)
{
	port *p = calloc(1, sizeof *p);
	*p = screen_port;                                           /* 194C:4FC2: the defaults (DS:247E) */
	int h = r.b - r.t, w = r.r - r.l;
	p->portRect = (rect){0, 0, h, w};
	p->bounds = (rect){0, 0, h, w};
	p->rowbytes = w > 0 ? w : 1;
	p->bits = calloc((size_t)(h > 0 ? h : 1) * p->rowbytes, 1);
	set_origin(p, r.t, r.l);
	port_reset_clip(p);
	return p;
}
static void port_free(port *p) { if (p && p != &screen_port) { free(p->bits); free(p); } }
/* 194C:4C34 / 4C9A: intersect the clip with a rect (saving it) / restore it */
static rect clip_push(const rect *r)
{
	rect saved = the_port->clip;
	if (!r) memset(&the_port->clip, 0, sizeof(rect));
	else { rect q = offset_rect(*r, -the_port->bounds.t, -the_port->bounds.l); sect_rect(&the_port->clip, &q, &the_port->clip); }
	return saved;
}
static void clip_pop(rect saved) { the_port->clip = saved; }
/* 194C:6632 -> 6C64: fill rect of the current port with a colour (mode 0) */
static void fill_rect(const rect *r, int color)
{
	port *p = the_port;
	int l = r->l - p->bounds.l, rr = r->r - p->bounds.l, t = r->t - p->bounds.t, b = r->b - p->bounds.t;
	if (l < p->clip.l) l = p->clip.l;
	if (rr > p->clip.r) rr = p->clip.r;
	if (t < p->clip.t) t = p->clip.t;
	if (b > p->clip.b) b = p->clip.b;
	if (rr <= l || b <= t) return;
	double row = 20 + (rr - l) / 2.0;
	for (int y = t; y < b; y++) { if (p->bits == screen_bits) beam(g_time + (uint64_t)((y - t) * row)); memset(p->bits + y * p->rowbytes + l, color, rr - l); }
	cpu((b - t) * row);
}
/* 194C:4CB0 -> 6698 CopyBits (mode 0) */
static void copy_bits(port *dst, port *src, const rect *dr, const rect *sr)
{
	int st = sr->t - src->bounds.t, dxn = 0; if (st < 0) { dxn = st; st = 0; }
	int sl = sr->l - src->bounds.l, cxn = 0; if (sl < 0) { cxn = sl; sl = 0; }
	int ob = sr->b - src->bounds.b; if (ob < 0) ob = 0;
	int orr = sr->r - src->bounds.r; if (orr < 0) orr = 0;
	int dt = dr->t - dxn - dst->bounds.t; if (dt < 0) { st -= dt; dt = 0; }
	int dl = dr->l - cxn - dst->bounds.l; if (dl < 0) { sl -= dl; dl = 0; }
	int db = dr->b - ob; if (db > dst->bounds.b) db = dst->bounds.b; db -= dst->bounds.t;
	int drr = dr->r - orr; if (drr > dst->bounds.r) drr = dst->bounds.r; drr -= dst->bounds.l;
	if (dst == the_port) {
		if (dst->clip.t > dt) { st += dst->clip.t - dt; dt = dst->clip.t; }
		if (dst->clip.l > dl) { sl += dst->clip.l - dl; dl = dst->clip.l; }
		if (db > dst->clip.b) db = dst->clip.b;
		if (drr > dst->clip.r) drr = dst->clip.r;
	}
	int h = db - dt, w = drr - dl;
	if (h <= 0 || w <= 0) return;
	double row = 20 + w / 2.0;
	for (int y = 0; y < h; y++) { if (dst->bits == screen_bits) beam(g_time + (uint64_t)(y * row)); memmove(dst->bits + (dt + y) * dst->rowbytes + dl, src->bits + (st + y) * src->rowbytes + sl, w); }
	cpu(h * row);
}

/* ------------------------------------------------------------------ images (SHAP) and shape lists (SHPL) */
/* An unpacked image (194C:0AFE): kind 1 = the 8-bit row-run form the blitter 2583:0006 reads (rows of a length word
 * and packets: 0..7F copy n+1 bytes, 80..FF repeat the next byte (n & 7F)+1 times); kind 2 = one byte per pixel
 * (stride bytes a row), drawn by 194C:6D42. */
typedef struct { int kind, h, w, stride, flags; uint8_t *data; int size; } img;
typedef struct { int first, count, mask, ncolors; } shplist;

static uint32_t lzg_lits, lzg_matches;   /* (for the cost) */
static const uint8_t *unpack_lzg(uint8_t *dest, int total, const uint8_t *src, const uint8_t *end)
{
	uint8_t win[0x400]; memset(win, 0, sizeof win);
	int wpos = 0x400 - 0x42, pos = 0; unsigned mask = 0;
	while (pos < total && src < end) {
		mask >>= 1;
		if (!(mask & 0xFF00)) mask = *src++ | 0xFF00u;
		if (mask & 1) { if (src >= end) break; lzg_lits++; uint8_t b = *src++; win[wpos] = b; wpos = (wpos + 1) & 0x3FF; dest[pos++] = b; }
		else {
			if (src + 1 >= end) break;
			unsigned w = src[0] << 8 | src[1]; src += 2;
			int from = w & 0x3FF, len = (w >> 10) + 3; lzg_matches++;
			for (int i = 0; i < len && pos < total; i++) { uint8_t b = win[from]; from = (from + 1) & 0x3FF; win[wpos] = b; wpos = (wpos + 1) & 0x3FF; dest[pos++] = b; }
		}
	}
	return src;
}
static const uint8_t *unpack_rle(uint8_t *dest, int total, const uint8_t *src, const uint8_t *end)
{
	int pos = 0;
	while (pos < total && src < end) {
		int8_t c = (int8_t)*src++;
		if (c >= 0) for (int i = 0; i <= c && src < end && pos < total; i++) dest[pos++] = *src++;
		else { uint8_t b = src < end ? *src++ : 0; for (int i = 0; i < -c && pos < total; i++) dest[pos++] = b; }
	}
	return src;
}
/* 194C:122C: the 8-bit images' colours are moved to the shape list's palette banks (mask bit n = bank n; the image's
 * bank k goes to the k-th bank of the mask); colour 0 stays */
static void remap_rows(img *im, int mask)
{
	uint8_t tab[16] = {0}; int n = 0;
	for (int b = 0; b < 16; b++) if (mask & (1 << b)) tab[n++] = (uint8_t)(b << 4);
	/* (the table is on 194C:122C's stack, bp-0x22, set only for the mask's banks: the game leaves 0 in the rest, e.g. transition
	 * 3's horse outline, image bank 15 with mask 0xFFFE, in colour 0x0F) */
	for (int k = n; k < 16; k++) tab[k] = 0;
	uint8_t *p = im->data, *end = im->data + im->size;
	for (int y = 0; y < im->h && p + 2 <= end; y++) {
		uint8_t *q = p + 2; int x = 0;
		while (x < im->w && q < end) {
			uint8_t c = *q++; int n2 = (c & 0x7F) + 1;
			if (c & 0x80) { if (*q) *q = (*q & 15) | tab[*q >> 4]; q++; }
			else for (int i = 0; i < n2 && q < end; i++, q++) if (*q) *q = (*q & 15) | tab[*q >> 4];
			x += n2;
		}
		p = q;
	}
}
static img *img_unpack(const uint8_t *r, int size, int mask)
{
	if (size < 6) return NULL;
	img *im = calloc(1, sizeof *im);
	im->h = ru16(r); im->w = ru16(r + 2); im->flags = ru16(r + 4);
	int method = (im->flags >> 8) & 0xF;
	if (im->flags & 0x7F) {                                     /* 194C:0ED4: the row-run form */
		/* in chunks of 0xE37E / ((w + 1) * 2) rows, each a length word and its own packed stream */
		int per = 0xE37E / ((im->w + 1) * 2); if (per < 1) per = 1;
		const uint8_t *src = r + 6, *end = r + size;
		im->kind = 1; im->size = 0; im->data = NULL;
		for (int rows = 0; rows < im->h && src + 2 <= end; rows += per) {
			int total = ru16(src); src += 2;
			im->data = realloc(im->data, im->size + total + 16); memset(im->data + im->size, 0, total + 16);
			if (method == 3) src = unpack_lzg(im->data + im->size, total, src, end);
			else if (method == 1) src = unpack_rle(im->data + im->size, total, src, end);
			else { memcpy(im->data + im->size, src, end - src < total ? end - src : total); src += total; }
			im->size += total;
		}
		if ((im->flags & 0x8000) && ((im->flags >> 8) & 0x70) == 0x70) remap_rows(im, mask);
		return im;
	}
	/* 194C:0B28: packed pixels (1..8 bits), unpacked to one byte a pixel */
	int depth = ((im->flags >> 12) & 7) + 1, stride = (depth * im->w + 7) / 8, total = stride * im->h;
	uint8_t *packed = calloc(total + 1, 1);
	const uint8_t *src = r + 6, *end = r + size;
	if (method == 0) memcpy(packed, src, size - 6 < total ? size - 6 : total);
	else if (method == 1 || method == 3) { if (method == 1) unpack_rle(packed, total, src, end); else unpack_lzg(packed, total, src, end); }
	else if (method == 2 || method == 4) {                   /* by columns */
		uint8_t *tmp = calloc(total + 1, 1);
		if (method == 2) unpack_rle(tmp, total, src, end); else unpack_lzg(tmp, total, src, end);
		for (int k = 0; k < total; k++) packed[(k % im->h) * stride + k / im->h] = tmp[k];
		free(tmp);
	}
	im->kind = 2; im->stride = im->w; im->data = calloc((size_t)im->w * im->h + 1, 1);
	uint8_t tab[16] = {0}; int n = 0;
	for (int b = 0; b < 16; b++) if (mask & (1 << b)) tab[n++] = (uint8_t)(b << 4);
	for (int y = 0; y < im->h; y++) for (int x = 0; x < im->w; x++) {
		int bit = x * depth, byte = packed[y * stride + bit / 8], shift = 8 - depth - bit % 8;
		int c = (byte >> shift) & ((1 << depth) - 1);
		if (depth <= 4 && c) c |= tab[0];                      /* (the colours go to the list's first bank) */
		im->data[y * im->w + x] = (uint8_t)c;
	}
	free(packed);
	return im;
}
static void img_free(img *im) { if (im) { free(im->data); free(im); } }
/* 2D7D:000E / 26BC:0034: load a shape list, with the palette-bank mask stored in its header */
static shplist shpl_load(int id, int mask)
{
	shplist l = {0, 0, 0, 0};
	int size; const uint8_t *r = res_get("SHPL", id, &size);
	if (!r) { fprintf(stderr, "nis: no shape list %d\n", id); return l; }
	l.first = ru16(r); l.count = ru16(r + 2); l.mask = mask & 0xFFFF; l.ncolors = r[6];
	return l;
}
/* 25A1:00FA: the unpacked image n (1-based) of a list */
static img *shape_get(const shplist *l, int n)
{
	int size; const uint8_t *r = res_get("SHAP", l->first + n - 1, &size);
	if (!r) { fprintf(stderr, "nis: no shape %d\n", l->first + n - 1); return NULL; }
	lzg_lits = lzg_matches = 0;
	img *im = img_unpack(r, size, l->mask);
	/* 194C:0AFE (the oracle: an LZG row-run image ~27 cycles an unpacked byte, 1.7 more a literal, 31 a match; a
	 * packed-pixel one ~15 a pixel) */
	double c = !im ? 0 : im->kind == 1 ? (lzg_lits || lzg_matches ? im->size * 27.0 + lzg_lits * 1.7 + lzg_matches * 30.7 : im->size * 31.7) : (double)im->w * im->h * 15;
	trace("shape", l->first + n - 1, (int)c);
	cpu(c);
	return im;
}
/* 2583:0006: draw a row-run image at (x, y) of the current port; mode 0 copies, other modes leave the runs of colour
 * 0 transparent */
static void draw_rowrun(int mode, int x, int y, const img *im)
{
	port *p = the_port;
	x -= p->bounds.l; int skipx = 0;
	if (x < p->clip.l) { skipx = p->clip.l - x; x = p->clip.l; }
	int xr = x + im->w - skipx; if (xr > p->clip.r) xr = p->clip.r;
	int width = xr - x; if (width <= 0) return;
	y -= p->bounds.t; int skipy = 0;
	if (y < p->clip.t) { skipy = p->clip.t - y; y = p->clip.t; }
	int yb = y + im->h - skipy; if (yb > p->clip.b) yb = p->clip.b;
	int rows = yb - y; if (rows <= 0) return;
	if (p->bits == screen_bits) beam(g_time);
	const uint8_t *s = im->data, *end = im->data + im->size;
	for (int k = 0; k < skipy && s + 2 <= end; k++) s += 2 + ru16(s);
	for (int k = 0; k < rows && s + 2 <= end; k++) {
		const uint8_t *next = s + 2 + ru16(s), *q = s + 2;
		uint8_t *d = p->bits + (y + k) * p->rowbytes + x;
		int cx = skipx, n = 0, lit = 0; uint8_t v = 0;
		/* skip the clipped left part */
		while (cx > 0 && q < end) {
			uint8_t c = *q++; n = (c & 0x7F) + 1;
			if (c & 0x80) { v = *q++; lit = 0; } else { lit = 1; q += n; }
			if (n > cx) { n -= cx; if (lit) q -= n; cx = 0; break; }
			cx -= n; n = 0;
		}
		int left = width;
		for (;;) {
			if (!n) {
				if (q >= end) break;
				uint8_t c = *q++; n = (c & 0x7F) + 1;
				if (c & 0x80) { v = *q++; lit = 0; } else lit = 1;
			}
			int m = n < left ? n : left;
			if (lit) { memcpy(d, q, m); q += n; }
			else if (mode == 0 || v) memset(d, v, m);
			d += m; left -= m; n = 0;
			if (left <= 0) break;
		}
		s = next;
	}
	cpu(rows * (30 + width));
}
/* 194C:6D42: draw a one-byte-a-pixel image; mode 0 copy, 10 skips colour 0, 8 paints the non-zero pixels in colour 0,
 * others combine (1 and, 2 or, 3 xor; 4..7 the same with the source inverted) */
static void draw_chunky(int mode, int x, int y, const img *im)
{
	port *p = the_port;
	x -= p->bounds.l; int skipx = 0;
	if (x < p->clip.l) { skipx = p->clip.l - x; x = p->clip.l; }
	int xr = x + im->w - skipx; if (xr > p->clip.r) xr = p->clip.r;
	int width = xr - x; if (width <= 0) return;
	y -= p->bounds.t; int skipy = 0;
	if (y < p->clip.t) { skipy = p->clip.t - y; y = p->clip.t; }
	int yb = y + im->h - skipy; if (yb > p->clip.b) yb = p->clip.b;
	int rows = yb - y; if (rows <= 0) return;
	if (p->bits == screen_bits) beam(g_time);
	uint8_t inv = (mode & 7) >= 4 ? 0xFF : 0;
	for (int k = 0; k < rows; k++) {
		const uint8_t *s = im->data + (skipy + k) * im->stride + skipx;
		uint8_t *d = p->bits + (y + k) * p->rowbytes + x;
		for (int i = 0; i < width; i++) {
			uint8_t c = s[i];
			if (mode == 8) { if (c) d[i] = 0; }
			else if (mode == 10) { if (c) d[i] = c; }
			else if (mode == 0) d[i] = c;
			else switch (mode & 3) { case 0: d[i] = c ^ inv; break; case 1: d[i] &= c ^ inv; break; case 2: d[i] |= c ^ inv; break; case 3: d[i] ^= c ^ inv; break; }
		}
	}
	cpu(rows * (20 + width * 5.0));
}
static void draw_img(const img *im, int x, int y, int mode)
{
	if (!im) return;
	if (im->kind == 1) draw_rowrun(mode, x, y, im); else draw_chunky(mode, x, y, im);
}
/* 26BC:0876: draw image n of a list at (x, y) */
static void draw_shape(const shplist *l, int n, int x, int y, int mode)
{
	img *im = shape_get(l, n);
	draw_img(im, x, y, mode);
	img_free(im);
}

/* ------------------------------------------------------------------ waits (2797) */
/* 2797:0158: countdown timer k = n, then wait for it (or a key) */
static int timer_wait(int k, int n)
{
	countdown[k] = (uint16_t)n;
	for (;;) { int r = key_pressed(); if (!countdown[k]) return 0; if (r) return r; yield(); }
}
/* 2797:0104: wait for countdown timer k (or a key) */
static int wait_countdown(int k)
{
	for (;;) { int r = key_pressed(); if (!countdown[k]) return 0; if (r) return r; yield(); }
}
/* 2797:0176: wait while sound id (0: any) plays */
static int wait_sound(int id)
{
	int r = 0;
	while ((id ? sound_playing(id) : any_sound_playing()) && !r) { r = key_pressed(); if (!r) yield(); }
	return r;
}
/* 2D7D:0135: wait for MIDI cue point c (returns at once when no music plays) */
static int wait_cue(int c)
{
	for (;;) {
		if (!any_sound_playing()) return 0;
		if (key_pressed()) return 1;
		if (cue >= c) return 0;
		yield();
	}
}
static uint32_t timer_target;          /* DS:2B1C */
/* 2D7D:009C: play a (digitized) sound; without the digitizer the game times out after n ticks instead */
static void play_or_time(int id, int n) { (void)n; sound_play(id); }
/* 2D7D:00E4: wait for it (or the time out) */
static int wait_or_time(int id) { return wait_sound(id); }
static int say(int id, int n) { play_or_time(id, n); return wait_or_time(id); }      /* 2D7D:00CA */
static void show_text(int base, int n);
/* 2D7D:4119: a caption with its narration: text t of STRL base, sound base+off */
static void sound_preload(int id);
static int say_text(int base, int t, int off, int n) { sound_preload(base + off); show_text(base, t); return say(base + off, n); }

/* ------------------------------------------------------------------ palette fades (2631) */
typedef struct { int delay, step, mask; uint8_t target[768], out[768], start[768]; } fade_t;
static double fade_alloc_cyc = 900;     /* (194C:19DC allocates the fade's 0x916 bytes, 2812:1F36 clears them) */
static int fade_countdown(void) { return countdown[3]; }       /* DS:24E2 */
/* 2631:0296: set the palette banks of mask from pal (NULL: black) */
static void setpal_banks(const uint8_t *pal, int mask)
{
	mask &= 0xFFFF;
	if (mask == 0xFFFF) { setpal(0, 256, pal, 1); return; }
	if (mask == 0xFFFE) { setpal(0x10, 0xF0, pal ? pal + 0x30 : NULL, 1); return; }
	if (mask == 0xFFF8) { setpal(0x30, 0xD0, pal ? pal + 0x90 : NULL, 1); return; }
	int n = 0;
	for (int b = 0; b < 16; b++) if (mask & (1 << b)) { setpal(b * 16, 16, pal ? pal + b * 0x30 : NULL, (n & 3) == 0); n++; }
}
/* 2631:023A: read the palette banks of mask */
static void getpal_banks(uint8_t *dst, int mask)
{
	mask &= 0xFFFF;
	if (mask == 0xFFFF) { getpal(dst, 256, 0); return; }
	for (int b = 0; b < 16; b++) if (mask & (1 << b)) getpal(dst + b * 0x30, 16, b * 16);
}
/* 2631:03E6: a fade from the current palette to target (NULL: black) over 64 steps, one every delay ticks */
static fade_t *fade_new(int delay, int mask, const uint8_t *target)
{
	fade_t *f = calloc(1, sizeof *f);
	cpu(fade_alloc_cyc);
	f->mask = mask & 0xFFFF; f->delay = delay;
	getpal_banks(f->start, f->mask);
	memcpy(f->out, f->start, 768);
	if (target) memcpy(f->target, target, 768);
	countdown[3] = 0;
	return f;
}
/* 2631:04E0: one step when the countdown has run out; 1 when done */
static int fade_step(fade_t *f)
{
	if (fade_countdown()) return 0;
	countdown[3] = (uint16_t)f->delay;
	int s = ++f->step;
	for (int b = 0; b < 16; b++) if (f->mask & (1 << b)) cpu(1152);       /* (the oracle: 18429 cycles for 16 banks) */
	for (int b = 0; b < 16; b++) if (f->mask & (1 << b))
		for (int i = b * 48; i < b * 48 + 48; i++) {
			int d = ((int)f->target[i] - (int)f->start[i]) * s;
			int16_t v = (int16_t)d; int neg = v < 0; int a = neg ? -v : v; a >>= 6;
			f->out[i] = (uint8_t)(f->start[i] + (neg ? -a : a));
		}
	setpal_banks(f->out, f->mask);
	return s == 64;
}
static void fade_free(fade_t *f) { free(f); }
/* the PALT resources are 767 bytes: the 768th comes from whatever follows */
static void palt_get(int id, uint8_t *pal)
{
	int size; const uint8_t *r = res_get("PALT", id, &size);
	memset(pal, 0, 768);
	if (r) memcpy(pal, r, size < 768 ? size : 768);
	else fprintf(stderr, "nis: no palette %d\n", id);
}
/* 2631:037E: fade and wait (a key ends it early: 1) */
static int fade_to(int palt, int mask, int delay)
{
	trace("fade", mask, palt); event(NIS_EV_FADE);
	uint8_t pal[768]; if (palt) palt_get(palt, pal);
	fade_t *f = fade_new(delay, mask, palt ? pal : NULL);
	int r = 0;
	if (memcmp(f->start, f->target, 768)) for (;;) {
		if (fade_step(f)) break;
		if ((r = key_pressed())) break;
		if (fade_countdown()) yield();
	}
	fade_free(f);
	return r;
}
/* 2631:064A: fade to black and clear the screen */
static void fade_out_clear(void)
{
	int a = g_abort; g_abort = 0;     /* (the key handler DS:1F32 is off meanwhile) */
	fade_to(0, 0xFFFF, 0);
	g_abort = a;
	fill_rect(&R_SCREEN, 0);
}

/* ------------------------------------------------------------------ the animation player (OVL00 32D4) */
/* A cast of 32 members (0x22-byte records at anim+0x78) moved and flipped through images by a byte-code script
 * (_SCR resources: a screen rect, then the ops; see docs/NIS.md). The anim draws into its own offscreen port and
 * copies the changed ("dirty") rect to the screen once a frame. */
typedef struct {
	int cur, first, last;              /* +0 image (1-based, 0 = none), +2/+4 the range it cycles through */
	uint16_t flags;                    /* +6: bits 0-3 draw mode, 4-7 move period, 8-11 image period, 12 backwards, 13 ping-pong */
	uint8_t mcount, fcount;            /* +8, +9 */
	int32_t x, y, dx, dy, ax, ay;      /* +A, +E, +12, +16, +1A, +1E (16.16) */
} member;
struct anim;
typedef int (*anim_cb)(struct anim *a, int arg);
typedef struct anim {
	port *p;                           /* +0: the anim's own port */
	port *parent;                      /* +2C */
	port *bg;                          /* +2E: the saved background (NULL: fill with the parent's colour) */
	rect scr;                          /* +3E: where it shows (the script's header) */
	rect dirty;                        /* +46 */
	const uint8_t *script, *pc;        /* +4E */
	int32_t w52;                       /* +52: a repeat count / tick target / cue */
	int skipdraw;                      /* +56 */
	int nframes;                       /* +58 */
	int delay;                         /* +5A */
	uint32_t last;                     /* +5C: the tick of the last frame */
	anim_cb cb[4];                     /* +60 */
	int loops[4];                      /* +70 */
	member m[32];                      /* +78 */
	int count;                         /* +4BA */
	fade_t *fade;                      /* +4BC: a fade stepped along */
	img **shapes;                      /* +4C0 */
	int color16;                       /* DS:695E: a 16-colour shape list */
} anim;
static int scr_patch_id; static rect scr_patch;   /* 37F0:01AA rewrites the rect of this _SCR resource (transition 6) */
static int draw_limit;                 /* DS:1E0A: images up to this one use the packed-pixel blitter (0x109 in the transitions) */
static int anim_cb_arg;                /* DS:6934: when set, callback 0 runs after every redraw */
static const uint8_t op_len[17][2] = { {0, 0}, {1, 4}, {2, 2}, {3, 2}, {4, 2}, {5, 2}, {6, 1}, {7, 0}, {0x10, 2}, {0x14, 4},
	{0x20, 4}, {0x40, 4}, {0x60, 4}, {0x80, 4}, {0xA0, 8}, {0xC0, 8}, {0xE0, 1} };   /* DS:1E34 / 1E46 */

static void member_reset(member *m)     /* 32D4:0BB0 */
{
	memset(m, 0, sizeof *m);
	m->flags = 2;
	m->x = (int32_t)0x80000000; m->y = (int32_t)0x80000000;
}
static int mx(const member *m) { return (int16_t)(m->x >> 16); }
static int my(const member *m) { return (int16_t)(m->y >> 16); }
static img *anim_img(anim *a, int n) { return n >= 1 && n <= a->count ? a->shapes[n - 1] : NULL; }
/* 32D4:0AFE: add a member's rect to the dirty rect */
static void anim_dirty(anim *a, member *m)
{
	if (!m->cur) return;
	img *im = anim_img(a, m->cur); if (!im) return;
	rect r = { my(m), mx(m), my(m) + im->h, mx(m) + im->w };
	if (!sect_rect(&a->p->portRect, &r, &r)) return;
	if (empty_rect(&a->dirty)) a->dirty = r; else a->dirty = union_rect(&a->dirty, &r);
}
/* 32D4:0A90 */
static void anim_draw_member(anim *a, member *m)
{
	if (!m->cur) return;
	img *im = anim_img(a, m->cur); if (!im) return;
	if (!a->color16 && m->cur > draw_limit && im->kind == 1) draw_rowrun(m->flags & 15, mx(m), my(m), im);
	else draw_img(im, mx(m), my(m), m->flags & 15);
}
/* 32D4:09D0: redraw the dirty rect: the background, then the members in order */
static void anim_draw(anim *a)
{
	rect saved = clip_push(&a->dirty);
	if (a->bg) copy_bits(a->p, a->bg, &a->p->portRect, &a->p->portRect);
	else fill_rect(&a->p->portRect, a->parent->bg);
	for (int i = 0; i < 32; i++) {
		anim_draw_member(a, &a->m[i]);
		if (i && a->nframes > 1 && a->fade && fade_step(a->fade)) { fade_free(a->fade); a->fade = NULL; }
	}
	clip_pop(saved);
	if (anim_cb_arg && a->cb[0]) a->cb[0](a, anim_cb_arg);
}
/* 32D4:0852: show a frame: redraw, copy the dirty rect to the screen, then move the members */
static uint32_t n_anim_frames;
static void anim_frame(anim *a)
{
	n_anim_frames++; event(NIS_EV_ANIM_FRAME);
	trace("anim_frame", a->nframes, (int)(a->pc - a->script));
	a->nframes++;
	a->last = g_tick;
	if (!a->skipdraw) anim_draw(a);
	a->skipdraw = 0;
	rect d = offset_rect(a->dirty, a->scr.t, a->scr.l);
	copy_bits(a->parent, a->p, &d, &a->dirty);
	memset(&a->dirty, 0, sizeof a->dirty);
	for (int i = 0; i < 32; i++) {
		member *m = &a->m[i];
		if (!m->cur) continue;
		if (m->first != m->last) {
			int old = m->fcount++;
			if ((m->flags >> 8 & 15) <= old) {
				m->fcount = 0;
				anim_dirty(a, m);
				m->cur += (m->flags & 0x1000) ? -1 : 1;
				if (m->flags & 0x2000) {
					if (m->cur < m->first) { m->cur = m->first + 1; m->flags &= ~0x1000; }
					else if (m->cur > m->last) { m->cur = m->last - 1; m->flags |= 0x1000; }
				} else if (m->cur < m->first) m->cur = m->last;
				else if (m->cur > m->last) m->cur = m->first;
				anim_dirty(a, m);
			}
		}
		int old = m->mcount++;
		if ((m->flags >> 4 & 15) <= old) {
			m->mcount = 0;
			if (m->dx || m->dy) { anim_dirty(a, m); m->x += m->dx; m->y += m->dy; anim_dirty(a, m); }
			m->dx += m->ax; m->dy += m->ay;
		}
	}
}
/* 32D4:0524: a new anim for a script, its images n = first..last of the list */
static anim *anim_new(int with_bg, int last, int first, const shplist *l, const uint8_t *script, int script_size)
{
	anim *a = calloc(1, sizeof *a);
	a->parent = the_port;
	for (int i = 0; i < 32; i++) member_reset(&a->m[i]);
	a->count = last - first + 1;
	a->shapes = calloc(a->count + 1, sizeof(img *));
	/* the images the script uses (32D4:05B5: scanning the ops) */
	uint8_t *used = calloc(a->count + 2, 1);
	const uint8_t *p = script + 8, *end = script + script_size;
	while (p < end && *p) {
		int k = 0; uint8_t op = *p;
		for (k = 0; k < 8; k++) if (op_len[k][0] == op) goto found;
		if ((op & 0xFC) == 0x10) { k = 8; goto found; }
		if ((op & 0xFC) == 0x14) { k = 9; goto found; }
		for (k = 10; k < 17; k++) if (op_len[k][0] == (op & 0xE0)) break;
		if ((op & 0xE0) == 0x80) {
			int f = rd16(p + 1), t = rd16(p + 3);
			if (f) for (int i = f; i <= t; i++) if (i <= last) used[i - 1] = 1;
		}
	found:
		if (k >= 17) break;
		p += op_len[k][1] + 1;
	}
	a->color16 = l->ncolors <= 0x10;
	for (int i = 0; i < a->count; i++) if (used[i]) a->shapes[i] = shape_get(l, first + i);
	free(used);
	a->scr = (rect){ rd16(script), rd16(script + 2), rd16(script + 4), rd16(script + 6) };
	a->p = port_new(a->scr);
	port *save = the_port; the_port = a->p;
	set_origin(a->p, 0, 0);
	a->dirty = a->p->portRect;
	a->script = script; a->pc = script + 8;
	if (with_bg) {
		a->bg = calloc(1, sizeof(port)); *a->bg = *a->p;
		a->bg->bits = calloc((size_t)(a->p->bounds.b - a->p->bounds.t) * a->p->rowbytes, 1);
		copy_bits(a->bg, a->parent, &a->p->portRect, &a->scr);
	}
	anim_draw(a);
	the_port = save;
	return a;
}
static void anim_free(anim *a)          /* 32D4:0038 */
{
	if (a->fade) fade_free(a->fade);
	for (int i = 0; i < a->count; i++) img_free(a->shapes[i]);
	free(a->shapes);
	if (a->bg) { free(a->bg->bits); free(a->bg); }
	port_free(a->p);
	free(a);
}
/* 32D4:00B8: run the script until it has to wait: 0 = waiting, 1 = the end, 2 = a callback asked to stop */
static int anim_run(anim *a)
{
	port *save = the_port; the_port = a->p;
	int ret = 0;
	for (;;) {
		const uint8_t *q = a->pc; uint8_t op = *q;
		int w1 = rd16(q + 1), w3 = rd16(q + 3);
		if (op < 8) {
			uint32_t due = a->last + (uint32_t)a->delay;
			switch (op) {
			case 0: if (due <= g_tick) ret = 1; goto out;
			case 1:
				if (due > g_tick) goto out;
				anim_frame(a);
				if (a->w52 == 0) { a->w52 = w1; a->delay = w3; }
				if (--a->w52 == 0) a->pc += 5;
				goto out;
			case 2:
				if (a->w52 == 0) { a->w52 = (int32_t)(g_tick + w1); if (a->last) a->last += w1; goto out; }
				if ((uint32_t)a->w52 > g_tick) goto out;
				a->w52 = 0; a->pc += 3; goto out;
			case 3: if (due > g_tick) goto out; sound_play(w1); a->w52 = 0; a->pc += 3; goto out;
			case 4: sound_stop(w1); a->pc += 3; goto out;
			case 5: if (!sound_playing(w1)) a->pc += 3; goto out;
			case 6:
				if ((a->w52 & 0xFF) == 0) a->w52 = q[1];
				if (cue && (a->w52 & 0xFF) > cue) goto out;
				a->w52 = 0; a->pc += 2; goto out;
			case 7: anim_draw(a); a->skipdraw = 1; a->pc += 1; goto out;
			}
		}
		if ((op & 0xFC) == 0x10) {
			anim_cb f = a->cb[op & 3];
			if (f && f(a, w1)) ret = 2;
			a->pc += 3; continue;
		}
		if ((op & 0xFC) == 0x14) {
			int *c = &a->loops[op & 3];
			if (*c == 0) *c = w1;
			else if (--*c == 0) { a->pc += 5; continue; }
			a->pc = q + w3 + 3; continue;
		}
		member *m = &a->m[op & 0x1F];
		switch (op & 0xE0) {
		case 0x20: {
			uint16_t old = m->flags; m->flags = (uint16_t)((m->flags & (uint16_t)w1) | (uint16_t)w3);
			if ((m->flags ^ old) & 0xF0) m->mcount = 0;
			if ((m->flags ^ old) & 0xF00) m->fcount = 0;
			a->pc += 5; continue; }
		case 0x40: anim_dirty(a, m); m->x += (int32_t)((uint32_t)w1 << 16); m->y += (int32_t)((uint32_t)w3 << 16); anim_dirty(a, m); a->pc += 5; continue;
		case 0x60: anim_dirty(a, m); m->x = (int32_t)((uint32_t)w1 << 16); m->y = (int32_t)((uint32_t)w3 << 16); anim_dirty(a, m); a->pc += 5; continue;
		case 0x80:
			anim_dirty(a, m); m->last = w3; m->first = w1; m->cur = w1;
			if (!w1) member_reset(m); else anim_dirty(a, m);
			a->pc += 5; continue;
		case 0xA0: m->dx = (int32_t)(ru16(q + 1) | (uint32_t)ru16(q + 3) << 16); m->dy = (int32_t)(ru16(q + 5) | (uint32_t)ru16(q + 7) << 16); a->pc += 9; continue;
		case 0xC0: m->ax = (int32_t)(ru16(q + 1) | (uint32_t)ru16(q + 3) << 16); m->ay = (int32_t)(ru16(q + 5) | (uint32_t)ru16(q + 7) << 16); a->pc += 9; continue;
		case 0xE0:
			if ((int8_t)q[1] > 0) {        /* stamp into the parent (the screen), at the anim's place */
				port *pp = a->parent; the_port = pp;
				m->x += (int32_t)((uint32_t)a->scr.l << 16); m->y += (int32_t)((uint32_t)a->scr.t << 16);
				anim_draw_member(a, m);
				m->x -= (int32_t)((uint32_t)a->scr.l << 16); m->y -= (int32_t)((uint32_t)a->scr.t << 16);
				the_port = a->p;
			}
			if (q[1] == 0) anim_draw_member(a, m);
			else if (a->bg) {                /* and into the saved background */
				port tmp = *a->p; a->p->bits = a->bg->bits;
				anim_draw_member(a, m);
				a->p->bits = tmp.bits;
			}
			a->pc += 2; continue;
		}
		ret = 1; goto out;
	}
out:
	the_port = save;
	return ret;
}
/* 32D4:0BE4: play the anim of script base+n (with the sounds its _PSL lists preloaded); abort: a key stops it (the
 * screen faded out); flags bit 1: then wait for the sounds, bit 0: then stop them; restore: put the background back
 * (or clear the rect). Returns 1 when stopped by a key or a callback. */
static int play_anim(const shplist *l, int base, int n, int with_bg, anim_cb cb0, int restore, int flags, int abort)
{
	event(NIS_EV_PLAY_ANIM);
	int size; const uint8_t *script = res_get("_SCR", base + n, &size);
	if (!script) { fprintf(stderr, "nis: no script %d\n", base + n); return 0; }
	uint8_t *patched = NULL;
	if (base + n == scr_patch_id && size >= 8) {          /* (OVL14 37F0:01AA rewrote the resource's rect) */
		patched = malloc(size); memcpy(patched, script, size);
		int v[4] = { scr_patch.t, scr_patch.l, scr_patch.b, scr_patch.r };
		for (int i = 0; i < 4; i++) { patched[2 * i] = (uint8_t)v[i]; patched[2 * i + 1] = (uint8_t)(v[i] >> 8); }
		script = patched;
	}
	anim *a = anim_new(with_bg, l->count, 1, l, script, size);
	a->cb[0] = cb0;
	n_snd_loaded = 0;            /* (the anim's images fill the heap: the sounds loaded before are purged, read and prepared again when played) */
	{ int k = 0; for (int i = 0; i < n_res_mem; i++) if (memcmp(res_mem[i].tag, "_SND", 4)) res_mem[k++] = res_mem[i]; n_res_mem = k; }
	{ int psz; const uint8_t *psl = res_get("_PSL", base + n, &psz);     /* (32D4:0C9D: the sounds it plays) */
	  if (psl) for (int i = 0; i < rd16(psl) && 2 + 2 * i + 2 <= psz; i++) sound_preload(ru16(psl + 2 + 2 * i)); }
	int r, stopped = 0;
	for (;;) {
		const uint8_t *pc = a->pc; int nf = a->nframes; int32_t w52 = a->w52; int step = a->fade ? a->fade->step : -1;
		r = anim_run(a);
		if (abort && (r == 2 || key_pressed())) { fade_out_clear(); sound_stop_all(); stopped = 1; break; }
		if (a->fade && fade_step(a->fade)) { fade_free(a->fade); a->fade = NULL; }
		if (r) break;
		if (pc == a->pc && nf == a->nframes && w52 == a->w52 && step == (a->fade ? a->fade->step : -1)) yield();   /* (the game polls) */
	}
	if (restore) {
		if (with_bg) { rect lr = offset_rect(a->scr, -a->scr.t, -a->scr.l); port *s = the_port; the_port = &screen_port; copy_bits(&screen_port, a->bg, &a->scr, &lr); the_port = s; }
		else { rect sr = a->scr; fill_rect(&sr, 0); }
	}
	anim_free(a);
	free(patched);
	if (flags & 2) wait_sound(0);
	if (flags & 1) sound_stop_all();
	return stopped;
}

/* ------------------------------------------------------------------ fonts and text (194C:07AC, 4CD2, 537D, 64FE) */
/* FONT: first char, last char, ascent, descent, leading, extra spacing (words at +2..+9), then an offset per char to
 * its glyph (height, width, a stride word, rows of 1 bit a pixel, (width + 7) / 8 bytes, high bit first). */
typedef struct { const uint8_t *r; int size, first, last, ascent, descent, leading, spacing; } font;
static font font_load(int id)                  /* 194C:4F8C */
{
	font f; memset(&f, 0, sizeof f);
	f.r = res_get("FONT", id, &f.size);
	if (!f.r) { fprintf(stderr, "nis: no font %d\n", id); return f; }
	cpu(f.size * 38.5);                        /* (the oracle: ~130000 cycles for the 2914 bytes of font 10 or 11) */
	f.first = f.r[0]; f.last = f.r[1]; f.ascent = rd16(f.r + 2); f.descent = rd16(f.r + 4); f.leading = rd16(f.r + 6); f.spacing = rd16(f.r + 8);
	return f;
}
static const uint8_t *glyph(const font *f, int c)
{
	if (!f->r || c > f->last || c < f->first) return NULL;
	return f->r + ru16(f->r + 10 + 2 * (c - f->first));
}
static const font *port_font;                  /* (the port's font, +2A: one current font suffices here) */
static int char_width(const font *f, int c) { const uint8_t *g = glyph(f, c); return g ? rd16(g + 2) + f->spacing : 0; }
static int text_width(const font *f, const char *s, int n) { int w = 0; for (int i = 0; i < n; i++) w += char_width(f, (uint8_t)s[i]); return w; }   /* 194C:53E2 */
/* 194C:6B06: a glyph at (x, y) in colour (mode 0: its set pixels painted) */
static void draw_glyph(const uint8_t *g, int x, int y, int color)
{
	port *p = the_port;
	int h = rd16(g), w = rd16(g + 2), stride = (w + 7) / 8;
	x -= p->bounds.l; int skipx = 0;
	if (x < p->clip.l) { skipx = p->clip.l - x; x = p->clip.l; }
	int xr = x + w - skipx; if (xr > p->clip.r) xr = p->clip.r;
	int width = xr - x; if (width <= 0) return;
	y -= p->bounds.t; int skipy = 0;
	if (y < p->clip.t) { skipy = p->clip.t - y; y = p->clip.t; }
	int yb = y + h - skipy; if (yb > p->clip.b) yb = p->clip.b;
	if (p->bits == screen_bits) beam(g_time);
	for (int k = 0; k < yb - y; k++) {
		const uint8_t *row = g + 6 + (skipy + k) * stride;
		uint8_t *d = p->bits + (y + k) * p->rowbytes + x;
		for (int i = 0; i < width; i++) { int bx = skipx + i; if (row[bx >> 3] >> (7 - (bx & 7)) & 1) d[i] = (uint8_t)color; }
	}
}
/* 194C:4CD2: n chars at the pen (v = the baseline), moving it */
static void draw_chars(const char *s, int n)
{
	const font *f = port_font; if (!f) return;
	for (int i = 0; i < n; i++) {
		const uint8_t *g = glyph(f, (uint8_t)s[i]); if (!g) continue;
		int x = the_port->penh;
		the_port->penh += rd16(g + 2) + f->spacing;
		draw_glyph(g, x, the_port->penv - f->ascent, the_port->fg);
		cpu(60 + rd16(g) * (20 + rd16(g + 2) * 5.0));
	}
}
/* 194C:537D: how many chars of s fit a line of width w (breaking after a space, before a space, after '-'; a CR ends it) */
static int line_break(int hjust, int w, int len, const char *s)
{
	int n = 0, brk = 0, width = 0;
	for (;;) {
		if (n == len) return n;
		width += char_width(port_font, (uint8_t)s[n]);
		if (width > w) return brk ? brk : n;
		char c = s[n++], next = n < len ? s[n] : 0;
		if (c == 0x0D) return n;
		if (c == '-') { brk = n; continue; }
		if (hjust > 0) { if (next == ' ' && c != ' ') brk = n; }
		else if (c == ' ' || next == ' ') brk = n;
	}
}
/* 194C:5334 / 64FE: a string laid out in rect; vjust / hjust: 0 centred, > 0 bottom / right, < 0 top / left */
static void text_box(const char *s, int vjust, int hjust, const rect *r)
{
	const font *f = port_font; if (!f) return;
	int save_v = the_port->penv, save_h = the_port->penh, save_fg = the_port->fg;
	int len = (int)strlen(s), w = r->r - r->l, h = r->b - r->t;
	int starts[64], lens[64], nl = 0, pos = 0;
	while (len > 0 && nl < 64) {
		int k = line_break(hjust, w, len, s + pos); if (!k) break;
		starts[nl] = pos; lens[nl] = k; nl++; pos += k; len -= k;
	}
	int lh = f->ascent + f->descent + f->leading, total = lh * nl - f->leading;
	int v = r->t;
	if (vjust == 0) v = r->t + (h >> 1) + (h & 1) - ((total >> 1) + (total & 1));
	else if (vjust > 0) v = r->t + h - total;
	the_port->penv = v + f->ascent;
	for (int i = 0; i < nl; i++) {
		const char *q = s + starts[i]; int n = lens[i];
		if (hjust < 0 && starts[i] > 0 && q[0] == ' ' && q[-1] != 0x0D) {
			char prev = q[-1]; q++; n--;
			if (n > 0 && q[0] == ' ' && prev == '.') { q++; n--; }
		}
		int tw = text_width(f, q, n), x = r->l;
		if (hjust == 0) x += (w >> 1) - (tw >> 1);
		else if (hjust > 0) x += w - tw;
		the_port->penh = x;
		draw_chars(q, n);
		the_port->penv += lh;
	}
	the_port->penv = save_v; the_port->penh = save_h; the_port->fg = save_fg;
}
/* 194C:8580: string n (1-based) of a STRL resource: a count byte, then NUL-terminated strings */
static const char *strl_get(int id, int n)
{
	int size; const uint8_t *r = res_get("STRL", id, &size);
	if (!r || n < 1 || r[0] < n) return "";
	const char *p = (const char *)r + 1;
	for (int i = 1; i < n; i++) p += strlen(p) + 1;
	return p;
}
/* ------------------------------------------------------------------ the caption bar (2D7D:01AF) */
static font font10, font11;                    /* DS:690C, 6910 */
static shplist list25000;                      /* DS:691A: the frame, the caption bar, the drop capitals */
static int text_top;                           /* DS:13CE: the bar at the top (10..50) instead of the bottom (157..197) */
static int text_saved;                         /* DS:13CC: the bar's background from the saved ports DS:2B20 / 2B22 */
static port *saved_bar_bottom, *saved_bar_top; /* DS:2B20, 2B22 */
static int extra_shape, extra_id, extra_x, extra_y;   /* DS:692C.. (a drop capital) */
static const rect R_BAR_BOTTOM = {157, 23, 197, 296}, R_BAR_TOP = {10, 23, 50, 296};   /* DS:13A0, 13A8 */
static void show_text(int base, int n)
{
	char str[330] = "";
	if (n) snprintf(str, sizeof str, "%s", strl_get(base, n));
	event(NIS_EV_TEXT);
	rect r = text_top ? R_BAR_TOP : R_BAR_BOTTOM;
	port *save = the_port, *p = port_new(r); the_port = p;
	int fg1, fg2;
	if (text_saved) {
		fg1 = 0x31; fg2 = 0x22;
		port *src = text_top ? saved_bar_top : saved_bar_bottom;
		if (src) copy_bits(p, src, &src->portRect, &src->portRect);
	} else {
		fg1 = 0x0E; fg2 = 0x0F;
		if (text_top) { if (saved_bar_top) copy_bits(p, saved_bar_top, &saved_bar_top->portRect, &saved_bar_top->portRect); }
		else draw_shape(&list25000, 2, 0, 0x9D, 0);
	}
	if (extra_shape) { draw_shape(&list25000, extra_id, extra_x, extra_y, 0); extra_shape = 0; }
	port_font = &font10;
	p->fg = 0;
	rect tr = r; tr.t += 3;
	rect sh = offset_rect(tr, 0, 1);
	text_box(str, 0, 0, &sh);                    /* a shadow one pixel right, in colour 0 */
	p->fg = fg1; text_box(str, 0, 0, &tr);
	port_font = &font11;
	p->fg = fg2; text_box(str, 0, 0, &tr);
	copy_bits(save, p, &r, &r);
	the_port = save;
	port_free(p);
}

/* ------------------------------------------------------------------ the sultan's-daughter scenes (2D7D:04F9) */
static int tree_n;                     /* DS:6924 */
static int tree_palt;                  /* DS:6922 */
/* 2D7D:0472: the scripts' callback 0: the music, then the fade in */
static int tree_cb(anim *a, int arg)
{
	(void)a; (void)arg;
	cue = 0;
	sound_play(tree_n + 26010);
	int r = fade_to(tree_palt, 0xFFFF, 0);
	if (r) return r;
	if (tree_n == 1) {
		port *s = the_port; the_port = &screen_port;
		r = say_text(26000, 1, 1, 0x45);
		if (!r) play_or_time(26002, 0x78);
		the_port = s;
	} else if (tree_n == 9) r = wait_cue(0x61);
	return r;
}
static int scene_tree(int n)
{
	res_open("NISDIGI.DAT"); res_open("NISMIDI.DAT");
	tree_n = n;
	res_open("NIS.DAT");
	shplist l = shpl_load(26000, 0xFFFF);
	list25000 = shpl_load(25000, 1);        /* 2D7D:016B: with the caption fonts */
	font10 = font_load(10); font11 = font_load(11);
	setpal(0, 256, NULL, 1);
	draw_shape(&list25000, 1, 0, 0, 0);
	if (n == 1) show_text(0, 0);
	draw_shape(&l, 2, 40, 32, 0);
	int p = (n - 1) / 3; if (p > 2) p = 2;
	tree_palt = 26001 + p;
	int r = play_anim(&l, 26000, n, 1, tree_cb, 0, 0, 1);
	if (!r) {
		if (n == 1) {
			r = wait_or_time(26002);
			if (!r) r = say_text(26000, 2, 4, 0x6E);
			if (!r) r = timer_wait(0, 0x3C);
			if (!r) { play_or_time(10500, 0x79); show_text(26000, 3); r = wait_or_time(10500); }
		} else if (n == 9) {
			r = wait_cue(0x62);
			if (!r) { draw_shape(&l, 15, 0x45, 0x38, 10); r = wait_cue(0x63); }
			if (!r) r = fade_to(26004, 0xFFFF, 2);
		}
	}
	if (!r) r = wait_sound(0);
	fade_out_clear();
	if (r) sound_stop_all();
	res_close("NIS.DAT");
	return r;
}

/* ------------------------------------------------------------------ dissolves (OVL00 33B9, 194C:13E6) */
static uint16_t dis_seed = 0x4583;     /* DS:1F1E */
static struct { int mode; int32_t area, elapsed; int valid; } dis_cache;   /* DS:6962.. (per mode and area) */
/* 194C:13E6: copy the pixels of src's rect whose pseudo-random byte is <= level (the bytes of a 16-bit LCG x*5+1,
 * low then high, from the seed DS:1F1E) */
static void dissolve_copy(int level, port *dst, port *src, const rect *dr, const rect *sr)
{
	int st = sr->t - src->bounds.t, dxn = 0; if (st < 0) { dxn = st; st = 0; }
	int sl = sr->l - src->bounds.l, cxn = 0; if (sl < 0) { cxn = sl; sl = 0; }
	int ob = sr->b - src->bounds.b; if (ob < 0) ob = 0;
	int orr = sr->r - src->bounds.r; if (orr < 0) orr = 0;
	int dt = dr->t - dxn - dst->bounds.t; if (dt < 0) { st -= dt; dt = 0; }
	int dl = dr->l - cxn - dst->bounds.l; if (dl < 0) { sl -= dl; dl = 0; }
	int db = dr->b - ob; if (db > dst->bounds.b) db = dst->bounds.b; db -= dst->bounds.t;
	int drr = dr->r - orr; if (drr > dst->bounds.r) drr = dst->bounds.r; drr -= dst->bounds.l;
	if (dst == the_port) {
		if (dst->clip.t > dt) { st += dst->clip.t - dt; dt = dst->clip.t; }
		if (dst->clip.l > dl) { sl += dst->clip.l - dl; dl = dst->clip.l; }
		if (db > dst->clip.b) db = dst->clip.b;
		if (drr > dst->clip.r) drr = dst->clip.r;
	}
	int h = db - dt, w = drr - dl;
	if (h <= 0 || w <= 0) return;
	uint16_t x = dis_seed; int phase = 0;
	for (int y = 0; y < h; y++) {
		if (dst->bits == screen_bits) beam(g_time + (uint64_t)(y * (20 + w * 14.0)));
		uint8_t *d = dst->bits + (dt + y) * dst->rowbytes + dl;
		const uint8_t *s = src->bits + (st + y) * src->rowbytes + sl;
		for (int i = 0; i < w; i++) {
			uint8_t v;
			if (phase == 0) v = x & 0xFF;
			else if (phase == 1) v = x >> 8;
			else { x = (uint16_t)(x * 5 + 1); phase = 0; v = x & 0xFF; }
			phase++;
			if (v <= level) d[i] = s[i];
		}
	}
	dis_seed = x;
	cpu(h * (20 + w * 14.0));
}
typedef struct { int mode; port *src, *dst; rect r; int32_t dur; uint32_t start; int level, lvl0, half; } dissolve_t;
/* 33B9:0300: one step: the level grows with the ticks since the start (0..255 over dur); 1 when done */
static int dissolve_step(dissolve_t *d)
{
	event(NIS_EV_DIS_STEP);
	int done = 0;
	if (d->level == 0) { d->start = g_tick; d->level = d->lvl0; }
	else {
		int32_t el = (int32_t)(g_tick - d->start);
		int32_t thr = d->dur - (d->half - (d->half >> 15)) / 2;
		if ((uint32_t)thr < (uint32_t)el) { d->level = 0xFF; done = 1; }
		else { d->level = (int)((int64_t)el * 255 / d->dur); if (!d->level) d->level = 1; }
	}
	port *save = the_port; the_port = d->dst;
	if (!done) { if (d->mode == 1) dis_seed = 0x4583; dissolve_copy(d->level, d->dst, d->src, &d->r, &d->r); }
	else copy_bits(d->dst, d->src, &d->r, &d->r);
	the_port = save;
	return done;
}
/* 33B9:0000: dissolve src into the current port's rect over dur ticks, showing each step on the screen */
static int dissolve(int32_t dur, const rect *r, port *src, int mode)
{
	event(NIS_EV_DISSOLVE);
	dissolve_t d = { mode, src, the_port, *r, dur, 0, 0, 0, 0 };
	/* 33B9:01AC: how many steps fit: two steps are timed once per mode and area (a few blits: under a tick here) */
	int32_t area = (int16_t)((r->r - r->l) * (r->b - r->t));
	int32_t el;
	if (dis_cache.valid && dis_cache.mode == mode && dis_cache.area == area) el = dis_cache.elapsed;
	else {
		/* (33B9:021A: two steps at level 0xFF into the source port itself, each a dissolve copy and a CopyBits, timed in
		 * ticks; 0 counts as 1) */
		uint32_t t0 = g_tick;
		int w = r->r - r->l, h = r->b - r->t;
		for (int k = 0; k < 2; k++) cpu(h * (20 + w * 14.0) + h * (20 + w / 2.0) + 500);
		el = (int32_t)(g_tick - t0); if (!el) el = 1;
		dis_cache.valid = 1; dis_cache.mode = mode; dis_cache.area = area; dis_cache.elapsed = el;
		trace("dis_cal", area, el);
	}
	int32_t n = 2 * dur / el;
	if (n <= 5) n = 0; else if (n > 255) n = 0xFE;
	d.lvl0 = 0xFF / (n + 1);
	d.half = dur / (0xFF / d.lvl0);
	int key = 0, done;
	trace("dissolve", r->r - r->l, r->b - r->t);
	do {                               /* (as fast as the machine goes) */
		key = key_pressed();
		done = dissolve_step(&d);
		copy_bits(&screen_port, d.dst, &d.r, &d.r);
		cpu(500);                      /* (the step's calls: time passes even when the rect is clipped away) */
	} while (!key && !done);
	return key;
}

/* ------------------------------------------------------------------ the story pictures (OVL00 2D7D:3EBF..4147) */
static int story_palt;                 /* DS:6906 */
static void pic_fill(port *p, int color) { port *s = the_port; the_port = p; fill_rect(&R_PICTURE, color); the_port = s; }   /* 2D7D:3EBF */
static void pic_draw(port *p, const shplist *l, int id, int x, int y, int mode) { port *s = the_port; the_port = p; draw_shape(l, id, x, y, mode); the_port = s; }   /* 3EEA */
static rect shape_rect(const shplist *l, int id, int x, int y)     /* 2D7D:4147 */
{
	int size; const uint8_t *r = res_get("SHAP", l->first + id - 1, &size);
	rect q = { y, x, y, x };
	if (r) { q.b = y + ru16(r); q.r = x + ru16(r + 2); }
	return q;
}
static void pic_show(port *p) { copy_bits(the_port, p, &R_PICTURE, &R_PICTURE); }                  /* 2D7D:3F9E */
/* 2D7D:3FBD: fade to black (but bank 0), show the picture, fade in to the story palette */
static int pic_fade_in(port *p, int delay)
{
	int r = fade_to(0, 0xFFFE, delay);
	if (!r) pic_show(p);
	if (!r) r = fade_to(story_palt, 0xFFFF, delay);
	return r;
}
/* 2D7D:3F52: black (but bank 0), the picture, 2 ticks, the story palette */
static void pic_flash(port *p)
{
	setpal_banks(NULL, 0xFFFE);
	pic_show(p);
	timer_wait(0, 2);
	uint8_t pal[768]; palt_get(story_palt, pal); setpal_banks(pal, 0xFFFF);
}
/* 2D7D:4011: dissolve port p into the screen's rect */
static int pic_dissolve(port *p, int dur, const rect *r)
{
	port *save = the_port, *t = port_new(*r);
	the_port = t;
	copy_bits(t, &screen_port, r, r);
	int k = dissolve(dur, r, p, 1);
	the_port = save;
	port_free(t);
	return k;
}

/* ------------------------------------------------------------------ scene 7: the story, part 1 (2D7D:259C) */
static int scene_story1(void)
{
	cue = 0;
	sound_play(25010);
	port *P = port_new(R_PICTURE);
	list25000 = shpl_load(25000, 1);
	shplist L = shpl_load(27000, 0xFFFF);
	list25000 = shpl_load(25000, 1); font10 = font_load(10); font11 = font_load(11);   /* (2D7D:016B) */
	setpal(0, 256, NULL, 1);
	draw_shape(&list25000, 1, 0, 0, 0);
	fill_rect(&R_PICTURE, 0xD4);
	show_text(0, 0);
	copy_bits(P, the_port, &R_PICTURE, &R_PICTURE);
	story_palt = 27001;
	rect R;
	int r = fade_to(story_palt, 0xFFFF, 0);
	if (!r) {
		pic_fill(P, 0xD4); pic_draw(P, &L, 4, 0x62, 0x3E, 10); R = shape_rect(&L, 4, 0x62, 0x3E);
		r = wait_cue(0x61);
		if (!r) r = pic_dissolve(P, 0x2D, &R);
		if (!r) r = timer_wait(0, 0x78);
	}
	if (!r) {
		pic_fill(P, 0xD4); R = shape_rect(&L, 4, 0x62, 0x3E);
		r = pic_dissolve(P, 0x2D, &R);
		if (!r) r = timer_wait(0, 0x30);
	}
	if (!r) {
		pic_draw(P, &L, 5, 0x4B, 0x44, 10); R = shape_rect(&L, 5, 0x4B, 0x44);
		r = wait_cue(0x62);
		if (!r) r = pic_dissolve(P, 0x2D, &R);
		if (!r) r = timer_wait(0, 0x78);
	}
	if (!r) {
		pic_fill(P, 0xD4); R = shape_rect(&L, 5, 0x4B, 0x44);
		r = pic_dissolve(P, 0x2D, &R);
		if (!r) r = timer_wait(0, 0x30);
	}
	if (!r) {
		pic_fill(P, 0); pic_draw(P, &L, 6, 0x28, 0x20, 0);
		r = fade_to(0, 0xFFFE, 0);
		if (!r) { pic_show(P); r = wait_cue(0x63); if (!r) r = fade_to(story_palt, 0xFFFF, 0); }
	}
	if (!r) {
		play_or_time(27001, 0x101);
		extra_id = 4; extra_x = 0x58; extra_y = 0xA7; extra_shape = 1;
		show_text(27000, 1);
		if (!r) r = timer_wait(0, 0x5A);
	}
	if (!r) { show_text(27000, 2); r = wait_or_time(27001); }
	if (!r) {
		countdown[0] = 0x2D;
		sound_preload(27002);                        /* (2D7D:28C6) */
		r = wait_countdown(0);
		if (!r) r = say_text(27000, 3, 2, 0xB8);
	}
	if (!r) {
		countdown[0] = 0x55;
		sound_preload(27003);                        /* (2D7D:2904) */
		wait_countdown(0);
		play_or_time(27003, 0xFE);
		countdown[0] = 0xAA;
		show_text(27000, 4);
		story_palt = 27002;
		pic_fill(P, 0); pic_draw(P, &L, 7, 0x27, 0x1F, 0);
		r = wait_countdown(0);
		if (!r) {
			show_text(27000, 5);
			r = fade_to(0, 0xFFFE, 0);
			if (!r) {
				pic_show(P);
				r = wait_cue(0x64);
				show_text(0, 0);
				if (!r) r = fade_to(story_palt, 0xFFFF, 0);
			}
		}
	}
	if (!r) { sound_preload(27004); r = wait_cue(0x65); if (!r) r = say_text(27000, 6, 4, 0xDF); if (!r) r = timer_wait(0, 0x14); }   /* (2D7D:29EE) */
	if (!r) r = say_text(27000, 7, 5, 0x87);
	if (!r) {
		r = wait_cue(0x66);
		play_or_time(27006, 0xC5);
		show_text(27000, 8);
		if (!r) r = timer_wait(0, 0x96);
	}
	if (!r) {
		pic_draw(P, &L, 8, 0x28, 0x1F, 10); R = shape_rect(&L, 8, 0x28, 0x1F);
		r = pic_dissolve(P, 0x2D, &R);
		if (!r) r = wait_or_time(27006);
		if (!r) r = timer_wait(0, 0x1E);
	}
	if (!r) { r = say_text(27000, 9, 7, 0x83); if (!r) r = timer_wait(0, 0x14); }
	if (!r) {
		show_text(0, 0);
		pic_fill(P, 0); pic_draw(P, &L, 10, 0x27, 0x1E, 0);
		r = pic_dissolve(P, 0x3C, &R_PICTURE);
		if (!r) r = timer_wait(0, 0x12);
	}
	if (!r) { r = say_text(27000, 10, 8, 0xFA); if (!r) r = timer_wait(0, 0x14); }
	if (!r) {
		play_or_time(27009, 0xEC);
		show_text(27000, 11);
		pic_draw(P, &L, 9, 0x27, 0x1E, 10);
		r = pic_dissolve(P, 0x2D, &R_PICTURE);
		if (!r) r = wait_or_time(27009);
	}
	if (!r) {
		story_palt = 27003;
		pic_fill(P, 0); pic_draw(P, &L, 12, 0x28, 0x20, 0);
		r = wait_sound(25010);
		if (!r) {
			show_text(0, 0);
			r = pic_fade_in(P, 0);
			cue = 0;
			if (!r) sound_play(25011);
		}
	}
	if (!r) { r = say_text(27000, 12, 10, 0x7C); if (!r) r = timer_wait(0, 0x14); }
	if (!r) { r = say_text(27000, 13, 11, 0x75); if (!r) r = timer_wait(0, 0x14); }
	if (!r) r = say_text(27000, 14, 12, 0xB0);
	if (!r) {
		show_text(0, 0);
		story_palt = 27004;
		pic_fill(P, 0); pic_draw(P, &L, 11, 0x28, 0x20, 0);
		r = wait_cue(0x61);
		if (!r) r = pic_fade_in(P, 0);
	}
	if (!r) { sound_preload(27013); r = wait_cue(0x62); if (!r) r = say_text(27000, 15, 13, 0xD0); if (!r) r = timer_wait(0, 0x14); }   /* (2D7D:2D6D) */
	if (!r) { sound_preload(27014); r = wait_cue(0x63); if (!r) r = say_text(27000, 16, 14, 0x91); if (!r) r = timer_wait(0, 0x5A); }   /* (2D7D:2DB2) */
	fade_out_clear();
	if (r) sound_stop_all();
	port_free(P);
	return r;
}
/* 2D7D:3F1C: an unpacked image into a port */
static void pic_img(port *p, const img *im, int x, int y, int mode) { port *s = the_port; the_port = p; draw_img(im, x, y, mode); the_port = s; }
/* ------------------------------------------------------------------ scene 8: the story, part 2 (2D7D:2E27) */
static int scene_story2(void)
{
	port *P = port_new(R_PICTURE);
	list25000 = shpl_load(25000, 1);
	shplist L = shpl_load(27000, 0xFFFF);
	list25000 = shpl_load(25000, 1); font10 = font_load(10); font11 = font_load(11);   /* (2D7D:016B) */
	setpal(0, 256, NULL, 1);
	draw_shape(&list25000, 1, 0, 0, 0);
	show_text(0, 0);
	story_palt = 27005;
	rect R10 = {0}, R48 = {0};
	int r = wait_cue(0x6A);
	if (!r) {
		r = fade_to(story_palt, 1, 2);
		if (!r) {
			pic_draw(P, &L, 0xD, 9, 0x1F, 0);
			pic_draw(P, &L, 0xE, 0xA0, 0x20, 10);
			pic_draw(P, &L, 0xF, 0xC1, 0x37, 10);
			R10 = shape_rect(&L, 0xF, 0xC1, 0x37);
			pic_show(P);
			sound_preload(25012);
			r = wait_sound(25011);
		}
		if (!r) { cue = 0; sound_play(25012); r = fade_to(story_palt, 0xFFFF, 0); }
	}
	if (!r) {
		extra_id = 3; extra_x = 0x63; extra_y = 0xA7; extra_shape = 1;
		sound_preload(27015);
		r = wait_cue(0x61);
		if (!r) {
			play_or_time(27015, 0xE7);
			show_text(27000, 0x11);
			r = timer_wait(0, 0x3C);
			if (!r) show_text(27000, 0x12);
		}
	}
	if (!r) {
		pic_fill(P, 0);
		pic_draw(P, &L, 0xD, 9, 0x1F, 0);
		pic_draw(P, &L, 0xE, 0xA0, 0x20, 0);
		pic_draw(P, &L, 0x10, 0xC0, 0x37, 10);
		R48 = shape_rect(&L, 0x10, 0xC0, 0x37);
		sound_preload(27016);
		wait_or_time(27015);
		r = wait_cue(0x62);
		if (!r) {
			play_or_time(27016, 0xA4);
			show_text(27000, 0x13);
			R10 = union_rect(&R10, &R48);
			r = pic_dissolve(P, 0x12C, &R10);
		}
	}
	if (!r) wait_sound(0);
	if (!r) {
		/* the prince runs in: 16 frames, the countdown paced (2D7D:3139) */
		show_text(0, 0);
		img *a = shape_get(&L, 0x11), *b = shape_get(&L, 0xD), *c = shape_get(&L, 0xE), *d = shape_get(&L, 0x10);
		sound_play(30503);
		for (int k = 1; k < 0x11; k++) {
			if (r) continue;
			countdown[0] = (uint16_t)(0xE - k < 0 ? 0 : 0xE - k);
			the_port = P;
			draw_img(a, 0x27, 0x20, 0);
			draw_img(b, 9 - 8 * k, 0x1F, 0);
			draw_img(c, (k + 0x14) * 8, 0x20, 0);
			draw_img(d, 0xC0, 0x37, 10);
			the_port = &screen_port;
			pic_show(P);
			r = wait_countdown(0);
		}
		img_free(d); img_free(c); img_free(b); img_free(a);
		if (!r) r = wait_sound(30503);
	}
	if (!r) { cue = 0; sound_play(25013); play_or_time(27017, 0xF0); show_text(27000, 0x14); }
	if (!r) {
		pic_fill(P, 0); pic_draw(P, &L, 0x12, 0x28, 0x20, 0);
		r = pic_dissolve(P, 0x3C, &R_PICTURE);
		if (!r) r = timer_wait(0, 0x2A);
	}
	if (!r) {
		show_text(27000, 0x15);
		pic_fill(P, 0); pic_draw(P, &L, 0x13, 0x27, 0x20, 0);
		r = pic_dissolve(P, 0x3C, &R_PICTURE);
		if (!r) r = timer_wait(0, 0x2A);
	}
	if (!r) r = wait_or_time(27017);
	if (!r) {
		pic_fill(P, 0); pic_draw(P, &L, 0x14, 0x28, 0x20, 0);
		r = pic_dissolve(P, 0x3C, &R_PICTURE);
		if (!r) r = timer_wait(0, 0x2A);
	}
	if (!r) {
		pic_fill(P, 0);
		pic_draw(P, &L, 0x15, 0x28, 0x1F, 0);
		pic_draw(P, &L, 0x18, 0x7E, 0x2C, 10);
		pic_draw(P, &L, 0x17, 0x41, 0x4F, 10);
		pic_draw(P, &L, 0x19, 0xA7, 0x32, 10);
		r = pic_dissolve(P, 0x3C, &R_PICTURE);
		if (!r) { show_text(0, 0); r = timer_wait(0, 0x2A); }
	}
	if (!r) { r = say_text(27000, 0x16, 0x12, 0xB6); if (!r) r = timer_wait(0, 0x14); }
	if (!r) {
		story_palt = 27006;
		show_text(0, 0);
		pic_fill(P, 0); pic_draw(P, &L, 0x21, 0x27, 0x20, 0); pic_draw(P, &L, 0x22, 0x28, 0x20, 10);
		if (!r) r = wait_cue(0x61);
		pic_flash(P);
		r = timer_wait(0, 0x30);
	}
	if (!r) r = wait_cue(0x62);
	if (!r) { r = say_text(27000, 0x17, 0x13, 0x112); if (!r) r = timer_wait(0, 0x14); }
	img *a = NULL, *b = NULL, *c = NULL;
	if (!r) {
		/* Jaffar's spell (2D7D:3519) */
		a = shape_get(&L, 0x15); b = shape_get(&L, 0x16); c = shape_get(&L, 0x19);
		story_palt = 27005;
		show_text(0, 0);
		pic_img(P, a, 0x28, 0x1F, 0); pic_draw(P, &L, 0x1A, 0x71, 0x33, 10); pic_img(P, b, 0x3A, 0x4E, 10); pic_img(P, c, 0xA7, 0x32, 10);
		r = wait_cue(0x64);
		if (!r) pic_flash(P);
	}
	static const int spell[5][4] = { {0x1C, 0x73, 0x32, 6}, {0x1D, 0x71, 0x30, 6}, {0x1E, 0x70, 0x30, 6}, {0x1F, 0x70, 0x2E, 6}, {0x20, 0x59, 0x2F, 0x12} };
	if (!r) { pic_img(P, a, 0x28, 0x1F, 0); pic_draw(P, &L, 0x1B, 0x71, 0x32, 10); pic_img(P, b, 0x3A, 0x4E, 10); pic_img(P, c, 0xA7, 0x32, 10); pic_show(P); }
	for (int k = 0; k < 5; k++) if (!r) {
		pic_img(P, a, 0x28, 0x1F, 0); pic_draw(P, &L, spell[k][0], spell[k][1], spell[k][2], 10); pic_img(P, b, 0x3A, 0x4E, 10); pic_img(P, c, 0xA7, 0x32, 10);
		pic_show(P);
		r = timer_wait(0, spell[k][3]);
	}
	if (a) { img_free(c); img_free(b); img_free(a); }
	if (!r) { r = say_text(27000, 0x18, 0x14, 0x81); if (!r) r = timer_wait(0, 0x14); }
	if (!r) {
		story_palt = 27006;
		show_text(0, 0);
		pic_fill(P, 0); pic_draw(P, &L, 0x23, 0x28, 0x20, 0);
		pic_flash(P);
		cue = 0; sound_play(25014);
		r = say_text(27000, 0x19, 0x15, 0xAE);
	}
	if (!r) {
		show_text(0, 0);
		pic_fill(P, 0); pic_draw(P, &L, 0x21, 0x27, 0x20, 0); pic_draw(P, &L, 0x24, 0x48, 0x27, 10);
		pic_flash(P);
	}
	if (!r) {
		countdown[0] = 0xC;
		pic_fill(P, 0); pic_draw(P, &L, 0x21, 0x27, 0x20, 0); pic_draw(P, &L, 0x25, 0x2E, 0x26, 10);
		r = wait_countdown(0);
		if (!r) {
			play_or_time(27022, 0x3C);
			show_text(27000, 0x1A);
			pic_show(P);
			r = wait_or_time(27022);
			if (!r) r = timer_wait(0, 0x1E);
		}
	}
	if (!r) {
		show_text(0, 0);
		pic_fill(P, 0); pic_draw(P, &L, 0x26, 0x29, 0x20, 0);
		pic_flash(P);
		if (!r) r = timer_wait(0, 0x12);
	}
	if (!r) { r = say_text(27000, 0x1B, 0x17, 0x62); if (!r) r = timer_wait(0, 0x14); }
	if (!r) {
		play_or_time(27024, 0xEC);
		show_text(27000, 0x1C);
		pic_fill(P, 0); pic_draw(P, &L, 0x21, 0x27, 0x20, 0); pic_draw(P, &L, 0x27, 0x28, 0x20, 10);
		pic_flash(P);
	}
	if (!r) r = wait_or_time(27024);
	if (!r) {
		show_text(0, 0);
		pic_fill(P, 0); pic_draw(P, &L, 0x2B, 0x28, 0x20, 0); pic_draw(P, &L, 0x28, 0x80, 0x1F, 10);
		R10 = shape_rect(&L, 0x28, 0x80, 0x1F);
		pic_flash(P);
		if (!r) r = timer_wait(0, 0x12);
	}
	if (!r) { r = say_text(27000, 0x1D, 0x19, 0x61); if (!r) r = timer_wait(0, 0x12); }
	if (!r) {
		pic_fill(P, 0); pic_draw(P, &L, 0x2B, 0x28, 0x20, 0); pic_draw(P, &L, 0x29, 0x7D, 0x1C, 10);
		R48 = shape_rect(&L, 0x29, 0x7D, 0x1C);
		cue = 0; sound_play(25015);
		play_or_time(27026, 0x5D);
		show_text(27000, 0x1E);
		R10 = union_rect(&R10, &R48);
		r = pic_dissolve(P, 0x4B, &R10);
		if (!r) r = wait_or_time(27026);
		if (!r) r = timer_wait(0, 0x1E);
	}
	if (!r) {
		pic_fill(P, 0); pic_draw(P, &L, 0x2B, 0x28, 0x20, 0); pic_draw(P, &L, 0x2A, 0x7D, 0x20, 10);
		R10 = shape_rect(&L, 0x2A, 0x7D, 0x20);
		R10 = union_rect(&R10, &R48);
		r = pic_dissolve(P, 0x4B, &R10);
		if (!r) r = timer_wait(0, 0x1E);
	}
	if (!r) {
		story_palt = 27007;
		show_text(0, 0);
		pic_fill(P, 0); pic_draw(P, &L, 0x2D, 0x28, 0x20, 0);
		pic_flash(P);
		r = timer_wait(0, 0x30);
	}
	if (!r) {
		pic_draw(P, &L, 0x2C, 0x52, 0x26, 10);
		R10 = shape_rect(&L, 0x2C, 0x52, 0x26);
		sound_preload(10049); sound_stop(0); sound_play(10049);
		r = pic_dissolve(P, 0x5A, &R10);
		if (!r) r = timer_wait(0, 0x1E);
		if (!r) r = fade_to(0, 0xFFFF, 2);
	}
	fade_out_clear();
	if (r) sound_stop_all();
	port_free(P);
	return r;
}
/* ------------------------------------------------------------------ scene 9: the Prince leaves (2D7D:1C0F) */
/* 2631:0296 with a PALT (194C:6F4C + 15B2: the story palette's handle, locked) */
static void story_setpal(int mask) { uint8_t pal[768]; palt_get(story_palt, pal); setpal_banks(pal, mask); }
static int scene_story3(void)
{
	sound_play(28100);
	uint8_t white[768]; memset(white, 0x3F, 0x2D0);               /* 2812:19D0 memset */
	port *P = port_new(R_PICTURE);
	list25000 = shpl_load(25000, 1);
	shplist L = shpl_load(28000, 0xFFFF);
	list25000 = shpl_load(25000, 1); font10 = font_load(10); font11 = font_load(11);   /* (2D7D:016B) */
	setpal(0, 256, NULL, 1);
	draw_shape(&list25000, 1, 0, 0, 0);
	draw_shape(&L, 2, 0x28, 0x20, 0);
	show_text(0, 0);
	story_palt = 28001;
	int r = wait_cue(0x61);
	if (!r) r = fade_to(story_palt, 0xFFFF, 2);
	if (!r) r = wait_cue(0x62);
	if (!r) {
		extra_id = 5; extra_x = 0x58; extra_y = 0x9F; extra_shape = 1;
		r = say_text(28000, 1, 1, 0xBA);
		if (!r) r = timer_wait(0, 0x14);
	}
	if (!r) {                                                    /* 1D3A */
		story_palt = 28002;
		pic_fill(P, 0); pic_draw(P, &L, 3, 0x26, 0x1F, 0); pic_draw(P, &L, 4, 0x25, 0x66, 10);
		r = wait_cue(0x63);
		show_text(0, 0);
		if (!r) r = pic_fade_in(P, 0);
		if (!r) r = timer_wait(0, 0x12);
	}
	if (!r) r = wait_cue(0x64);                                  /* 1DDD */
	if (!r) { r = say_text(28000, 2, 2, 0x83); if (!r) r = timer_wait(0, 0x14); }
	if (!r) r = wait_cue(0x65);
	if (!r) { r = say_text(28000, 3, 3, 0xC8); if (!r) r = timer_wait(0, 0x14); }
	if (!r) { show_text(0, 0); r = timer_wait(0, 0x48); }
	if (!r) { r = wait_cue(0x66); if (!r) play_or_time(28005, 0xEE); }
	if (!r) r = fade_to(0, 0xFFFE, 5);
	if (!r) r = wait_or_time(28005);
	if (!r) {                                                    /* 1ED3 */
		story_palt = 28002;
		pic_fill(P, 0); pic_draw(P, &L, 5, 0x28, 0x20, 0);
		r = wait_cue(0x67);
		if (!r) r = pic_fade_in(P, 3);
	}
	if (!r) r = wait_cue(0x68);
	if (!r) { play_or_time(10500, 0x79); show_text(28000, 4); r = wait_or_time(10500); }
	if (!r) r = wait_cue(0x69);
	if (!r) sound_play(30500);
	if (!r) {                                                    /* 1F9E: the lightning */
		show_text(0, 0);
		pic_draw(P, &L, 6, 0x29, 0x20, 10); pic_show(P);
		r = timer_wait(0, 6);
	}
	if (!r) { story_palt = 28004; story_setpal(0xFFFF); r = timer_wait(0, 0xC); }
	if (!r) { setpal(0x10, 0xF0, white, 1); r = timer_wait(0, 0xC); }
	if (!r) {                                                    /* 2059 */
		story_palt = 28005;
		pic_fill(P, 0); pic_draw(P, &L, 7, 0x28, 0x20, 0);
		sound_play(28101);
		pic_flash(P);
		if (!r) r = timer_wait(0, 0x12);
	}
	if (!r) { story_palt = 28002; story_setpal(0xFFFF); r = timer_wait(0, 6); }
	if (!r) {                                                    /* 2116 */
		pic_fill(P, 0); pic_draw(P, &L, 3, 0x27, 0x20, 0); pic_draw(P, &L, 8, 0x26, 0x56, 10);
		pic_show(P);
		if (!r) r = timer_wait(0, 0x72);
	}
	if (!r) {                                                    /* 2182 */
		story_palt = 28006;
		pic_fill(P, 0); pic_draw(P, &L, 9, 0x26, 0x1B, 0);
		r = wait_cue(0x62);
		if (!r) pic_flash(P);
	}
	if (!r) { pic_draw(P, &L, 10, 0x28, 0x21, 10); pic_show(P); if (!r) r = timer_wait(0, 6); }
	if (!r) {                                                    /* 2227 */
		sound_play(30500);                                       /* (when DS:2085 & 3: sound on, always) */
		pic_fill(P, 0); pic_draw(P, &L, 9, 0x26, 0x1B, 0); pic_draw(P, &L, 11, 0x28, 0x20, 10);
		pic_show(P);
		if (!r) r = timer_wait(0, 6);
	}
	if (!r) {
		pic_fill(P, 0); pic_draw(P, &L, 9, 0x26, 0x1B, 0); pic_draw(P, &L, 12, 0x27, 0x20, 10);
		pic_show(P);
		if (!r) r = timer_wait(0, 6);
	}
	if (!r) { pic_fill(P, 0); pic_draw(P, &L, 13, 0x27, 0x1F, 0); pic_show(P); if (!r) r = timer_wait(0, 6); }
	if (!r) { setpal(0x10, 0xF0, white, 1); countdown[0] = 0xC; r = timer_wait(0, 0xC); }
	if (!r) {                                                    /* 2392 */
		pic_fill(P, 0); pic_draw(P, &L, 17, 0x28, 0x1F, 0);
		r = wait_countdown(0);
		if (!r) {
			setpal_banks(NULL, 0xFFFE);
			r = pic_fade_in(P, 0);
			if (!r) r = timer_wait(0, 0x12);
		}
	}
	if (!r) r = wait_cue(0x63);
	if (!r) sound_play(10262);
	if (!r) { pic_draw(P, &L, 14, 0x43, 0x2D, 10); pic_show(P); if (!r) r = timer_wait(0, 6); }
	if (!r) {
		pic_fill(P, 0); pic_draw(P, &L, 17, 0x28, 0x1F, 0); pic_draw(P, &L, 15, 0x4E, 0x2C, 10);
		pic_show(P);
		if (!r) r = timer_wait(0, 6);
	}
	if (!r) {
		pic_fill(P, 0); pic_draw(P, &L, 17, 0x28, 0x1F, 0); pic_draw(P, &L, 16, 0x4D, 0x2E, 10);
		pic_show(P);
		if (!r) r = timer_wait(0, 0xC);
	}
	if (!r) { r = wait_cue(0x64); if (!r) r = fade_to(0, 0xFFFF, 4); }
	fade_out_clear();
	if (r) sound_stop_all();
	port_free(P);
	return r;
}

/* ------------------------------------------------------------------ scene 10: meanwhile, Jaffar (2D7D:4A95) */
static shplist list29000;              /* DS:6904 */
/* 2D7D:5332: the scripts' callback 0: the picture (image 11) into the anim's background (32D4:0E22 swaps the anim
 * port's bits with the background's, before and after) */
static int story4_cb(anim *a, int arg)
{
	(void)arg;
	uint8_t *b = a->p->bits; a->p->bits = a->bg->bits;
	draw_shape(&list29000, 11, 0x28 - a->scr.l, 0x20 - a->scr.t, 0);
	a->p->bits = b;
	return 0;
}
static int scene_story4(void)
{
	sound_play(29100);
	port *P = port_new(R_PICTURE);
	list25000 = shpl_load(25000, 1);
	list29000 = shpl_load(29000, 0xFFFF);
	shplist *L = &list29000;
	list25000 = shpl_load(25000, 1); font10 = font_load(10); font11 = font_load(11);   /* (2D7D:016B) */
	setpal(0, 256, NULL, 1);
	draw_shape(&list25000, 1, 0, 0, 0);
	show_text(0, 0);
	draw_shape(L, 2, 0x28, 0x20, 0);
	story_palt = 29001;
	rect R1, R2;
	int r = fade_to(story_palt, 0xFFFF, 0);
	if (!r) r = timer_wait(0, 0xF0);
	if (!r) {
		play_or_time(29001, 0x10E);
		extra_id = 6; extra_x = 0x60; extra_y = 0xA8; extra_shape = 1;
		show_text(29000, 1);
		if (!r) r = timer_wait(0, 0x3C);
	}
	if (!r) { show_text(29000, 2); r = wait_or_time(29001); if (!r) r = timer_wait(0, 0x5A); }
	if (!r) { r = say_text(29000, 3, 2, 0x9D); if (!r) r = timer_wait(0, 0x28); }
	if (!r) { r = say_text(29000, 4, 3, 0xBB); if (!r) r = timer_wait(0, 0x14); }
	if (!r) {                                                    /* 4C39 */
		show_text(0, 0);
		pic_fill(P, 0); pic_draw(P, L, 3, 0x28, 0x20, 0); pic_draw(P, L, 4, 0x33, 0x28, 10);
		R1 = shape_rect(L, 4, 0x33, 0x28);
		r = wait_cue(0x61);
		if (!r) r = pic_fade_in(P, 0);
		if (!r) r = timer_wait(0, 0x18);
	}
	if (!r) { r = say_text(29000, 5, 4, 0xA8); if (!r) r = timer_wait(0, 0x14); }
	if (!r) { r = say_text(29000, 6, 5, 0x89); if (!r) r = timer_wait(0, 0x14); }
	if (!r) { r = say_text(29000, 7, 6, 0xDC); if (!r) r = timer_wait(0, 0x3C); }
	if (!r) r = wait_cue(0x62);
	if (!r) r = say(30501, 0);
	if (!r) {                                                    /* 4D96 */
		show_text(0, 0);
		pic_fill(P, 0); pic_draw(P, L, 3, 0x28, 0x20, 0); pic_draw(P, L, 5, 0x32, 0x21, 10);
		R2 = shape_rect(L, 5, 0x32, 0x21);
		if (!r) sound_play(29007);
		rect U = union_rect(&R1, &R2);                           /* 194C:647E */
		r = pic_dissolve(P, 0xF, &U);
		if (!r) r = timer_wait(0, 0x1E);
	}
	if (!r) { pic_fill(P, 0); pic_draw(P, L, 8, 0x28, 0x20, 0); pic_flash(P); r = timer_wait(0, 0x3C); }
	if (!r) {                                                    /* 4E94 */
		pic_fill(P, 0); pic_draw(P, L, 3, 0x28, 0x20, 0); pic_draw(P, L, 5, 0x32, 0x21, 10);
		pic_flash(P);
		r = say_text(29000, 8, 8, 0x42);
		if (!r) r = timer_wait(0, 0x14);
	}
	if (!r) {                                                    /* 4F10 */
		sound_play(30500);
		pic_fill(P, 0); pic_draw(P, L, 3, 0x28, 0x20, 0); pic_draw(P, L, 6, 0x32, 0x21, 10);
		show_text(0, 0);
		pic_show(P);
		r = timer_wait(0, 6);
	}
	if (!r) { pic_fill(P, 0); pic_draw(P, L, 3, 0x28, 0x20, 0); pic_draw(P, L, 7, 0x28, 0x20, 10); pic_show(P); r = timer_wait(0, 6); }
	if (!r) {                                                    /* 4FEB */
		uint8_t white[768]; memset(white, 0x3F, 0x2D0);         /* 2812:19D0 memset */
		setpal(0x10, 0xF0, white, 1);
		sound_play(29101);
		r = timer_wait(0, 6);
	}
	if (!r) { pic_fill(P, 0); pic_draw(P, L, 9, 0x27, 0x1F, 0); pic_flash(P); r = timer_wait(0, 6); }
	if (!r) { pic_fill(P, 0); pic_draw(P, L, 8, 0x28, 0x20, 0); pic_show(P); r = timer_wait(0, 0x90); }
	if (!r) {                                                    /* 50C7 */
		pic_draw(P, L, 10, 0x32, 0x1F, 10);
		R1 = shape_rect(L, 10, 0x32, 0x1F);
		r = pic_dissolve(P, 0x4B, &R1);
		if (!r) r = timer_wait(0, 0x9C);
	}
	if (!r) r = fade_to(0, 0xFFFE, 2);
	if (!r) {                                                    /* 5149 */
		pic_fill(P, 0); pic_draw(P, L, 11, 0x28, 0x20, 0); pic_draw(P, L, 13, 0xBF, 0x59, 10);
		story_palt = 29002;
		if (!r) r = wait_sound(0);
		if (!r) sound_play(29102);
		if (!r) r = pic_fade_in(P, 2);
		if (!r) r = timer_wait(0, 0x3C);
	}
	if (!r) { r = say_text(29000, 9, 9, 0x11E); if (!r) r = timer_wait(0, 0x14); }
	if (!r) { r = say_text(29000, 10, 10, 0x11E); if (!r) r = timer_wait(0, 0x14); }
	if (!r) { r = say_text(29000, 11, 11, 0xC8); if (!r) r = timer_wait(0, 0x78); }
	if (!r) r = play_anim(L, 29000, 1, 1, story4_cb, 0, 0, 1);  /* (2A31:0CF9) */
	if (!r) { r = play_anim(L, 29000, 2, 1, story4_cb, 0, 0, 1); if (!r) r = fade_to(0, 0xFFFF, 2); }
	fade_out_clear();
	if (r) sound_stop_all();
	port_free(P);
	return r;
}
/* ------------------------------------------------------------------ scene 11: the ending (2D7D:41B1) */
static shplist trans_list;             /* DS:68FA */
static int trans_palt;                 /* DS:6928 */
static uint8_t end_pal[4][768];        /* DS:6918, 691C, 691E, 6920: PALT 30002..30005 */
static img *end_img;                   /* DS:690E: image 10 of the list 30000 (the last picture) */
static shplist end_list;               /* DS:6912 */
/* 194C:5194: a port holding a copy of the current port's rect */
static port *save_rect(const rect *r) { port *p = port_new(*r); copy_bits(p, the_port, r, r); return p; }
/* 194C:5164: put a saved rect (194C:5194) back into the current port, and free it */
static void restore_rect(port *p) { copy_bits(the_port, p, &p->portRect, &p->portRect); port_free(p); }
/* 32D4:0E22: swap the anim's bits with its background's (drawing then goes into the background) */
static void anim_swap_bg(anim *a) { uint8_t *t = a->p->bits; a->p->bits = a->bg->bits; a->bg->bits = t; }
/* 0823:1447: mirror a row-run image (each row's packets in reverse order) */
static void img_mirror(img *im)
{
	if (!im || im->kind != 1) return;
	uint8_t *p = im->data, *end = im->data + im->size, *tmp = malloc(im->size + 2);
	double c = 400;                                            /* (0823:1447: its cost, from the code) */
	for (int y = 0; y < im->h && p + 2 <= end; y++) {
		int len = ru16(p); uint8_t *q = p + 2, *e = q + len, *d = tmp + len;
		if (e > end) break;
		c += 25 + len;
		while (q < e && d > tmp) {
			uint8_t c8 = *q++;
			if (c8 & 0x80) { d -= 2; d[0] = c8; d[1] = *q++; c += 11; }
			else { int n = c8 + 1; d -= n + 1; d[0] = c8; for (int i = 0; i < n; i++) d[n - i] = *q++; c += 10 + 4 * n; }
		}
		memcpy(p + 2, tmp, len);
		p = e;
	}
	free(tmp);
	cpu(c);
}
/* 2D7D:03A2: mirror the images 0x191..0x1B4 of an anim (none in the 400-image list of FINAL.DAT) */
static void anim_mirror(anim *a) { for (int n = 0x191; n <= 0x1B4; n++) img_mirror(anim_img(a, n)); }
/* 0823:1414: mirror a one-byte-a-pixel image (rows of its width) */
static void mirror_chunky(img *im)
{
	if (!im || im->kind != 2) return;
	for (int y = 0; y < im->h; y++) {
		uint8_t *p = im->data + y * im->stride, *q = p + im->w - 1;
		while (p < q) { uint8_t t = *p; *p++ = *q; *q-- = t; }
	}
	cpu(100 + im->h * (8 + 3.0 * (im->w + 1)));                /* (0823:1414: 6 instructions a pair of pixels) */
}
/* 26BC:08B2: draw image n of a list (mirrored first by 0823:1414 when flip) with 194C:511C (-> 6D42, DS:247C) */
static void draw_shape_flip(const shplist *l, int n, int x, int y, int mode, int flip)
{
	img *im = shape_get(l, n);
	if (!im) return;
	if (flip) mirror_chunky(im);
	draw_img(im, x, y, mode);
	img_free(im);
}
/* 2D7D:127E: the frame of the final pictures (images 0x158, 0x159, 0x15C twice of DS:68FA) */
static void final_frame(void)
{
	draw_shape(&trans_list, 0x158, 0, -7, 0);
	draw_shape(&trans_list, 0x159, 0, 0x5D, 0);
	draw_shape(&trans_list, 0x15C, 0x22, 0x4F, 10);
	draw_shape(&trans_list, 0x15C, 0xDF, 0x4F, 10);
}
/* 2D7D:18E4: the callback of the script FINAL.DAT 25002: 0 mirror + sound 25004, 1 the palette at once, else a
 * fade of banks 0..2 to DS:692A (not set in this scene) */
static int final_cb(anim *a, int arg)
{
	if (arg == 0) { anim_mirror(a); sound_play(25004); }
	else if (arg == 1) { uint8_t pal[768]; palt_get(trans_palt, pal); setpal_banks(pal, 0xFFFF); }   /* 2D7D:1870 */
	else { if (a->fade) fade_free(a->fade); a->fade = fade_new(1, 7, dac); }   /* (the script never asks for it) */
	return 0;
}
/* 2D7D:466B: the prologue of the ending, from FINAL.DAT (the last level's file): the Prince's return */
static int scene_final_intro(void)
{
	setpal(0, 256, NULL, 1);
	/* 2D7D:4A6A: the scene's key handler (DS:1F32) */
	trans_list = shpl_load(25000, 0xFFFF);
	final_frame();
	shplist l = shpl_load(25001, 1);
	draw_shape_flip(&l, 15, 0xA6, 0x8B, 10, 0);
	trans_palt = 25000;
	int r = play_anim(&trans_list, 25000, 2, 1, final_cb, 0, 0, 1);
	if (!r) r = timer_wait(0, 0x3C);
	if (!r) r = fade_to(0, 0xFFFF, 0);
	return r;
}
/* 2D7D:49B6: a caption over the saved bar (DS:13CC) with its narration base+off */
static void end_say(int base, int t, int off)
{
	port *s = the_port; the_port = &screen_port;
	sound_play(base + off);
	text_saved = 1; show_text(base, t); text_saved = 0;
	the_port = s;
}
/* 2D7D:474C: the callback of the ending's scripts 30001 / 30002 */
static int end_cb(anim *a, int arg)
{
	int r = 0;
	port *s = the_port;
	switch (arg) {
	case 0:
		r = wait_cue(0x65);
		if (r) break;
		if (a->fade) fade_free(a->fade);
		a->fade = fade_new(1, 0xFFFF, end_pal[0]);
		end_say(30000, 4, 4);
		break;
	case 1: if (a->fade) fade_free(a->fade); a->fade = fade_new(1, 7, end_pal[1]); break;
	case 2:
		the_port = &screen_port;
		copy_bits(&screen_port, saved_bar_bottom, &saved_bar_bottom->portRect, &saved_bar_bottom->portRect);
		the_port = s;
		if (a->fade) fade_free(a->fade);
		a->fade = fade_new(2, 0xFFF8, end_pal[2]);
		break;
	case 3:
		/* 194C:25C0: the image is purgeable (194C:1870) and has been purged by now (the anim's images and the preloaded
		 * sounds fill the heap: the oracle spends ~4 ticks here), so it is loaded again */
		img_free(end_img); end_img = shape_get(&end_list, 10);
		anim_swap_bg(a);
		draw_rowrun(0, -a->scr.l, 8 - a->scr.t, end_img);            /* 2583:0006 into the background */
		anim_swap_bg(a);
		the_port = &screen_port;
		fill_rect(&(rect){0, 0, 8, 320}, 0);                         /* DS:13C4 */
		draw_rowrun(10, 0, 8, end_img);
		the_port = s;
		sound_play(30020);
		if (a->fade) fade_free(a->fade);
		a->fade = fade_new(1, 0xFFFF, end_pal[3]);
		break;
	case 4: if (a->fade) fade_free(a->fade); a->fade = fade_new(1, 0xFFFF, NULL); break;    /* 2D7D:1885 */
	case 5: case 6: case 7: case 8: end_say(30000, arg, arg); break;
	case 0x62:
		anim_swap_bg(a);
		draw_shape(&end_list, 2, 0x28 - a->scr.l, 0x20 - a->scr.t, 0);
		anim_swap_bg(a);
		break;
	case 0x63: {                   /* a blink on the screen */
		the_port = &screen_port;
		rect R = shape_rect(&end_list, 0x2C, 0xA1, 0x40);
		port *S = save_rect(&R);
		draw_shape(&end_list, 0x2C, 0xA1, 0x40, 10);
		countdown[0] = 6;
		while (countdown[0]) yield();
		restore_rect(S);
		the_port = s;
		break; }
	}
	return r;
}
/* 2D7D:41B1: the ending (the dispatcher 2D7D:407F leaves NIS.DAT to it); the prologue's result is not looked at */
static int scene_end(void)
{
	scene_final_intro();
	res_open("NIS.DAT");
	port *P = port_new(R_PICTURE);
	list25000 = shpl_load(25000, 1);
	shplist L = end_list = shpl_load(30000, 0xFFFF);
	list25000 = shpl_load(25000, 1); font10 = font_load(10); font11 = font_load(11);   /* (2D7D:016B) */
	setpal(0, 256, NULL, 1);
	draw_shape(&list25000, 1, 0, 0, 0);
	rect sc = clip_push(&R_PICTURE);
	draw_shape(&L, 2, 0x28, 0x20, 0);
	draw_shape(&L, 5, 0x34, 0x5A, 10);
	clip_pop(sc);
	show_text(0, 0);
	copy_bits(P, the_port, &R_PICTURE, &R_PICTURE);
	story_palt = 30001;
	sound_play(30010);
	rect R;
	int r = fade_to(story_palt, 0xFFFF, 2);
	if (!r) r = timer_wait(0, 0x30);
	if (!r) {
		pic_draw(P, &L, 3, 0xA0, 0x2D, 10); R = shape_rect(&L, 3, 0xA0, 0x2D);
		r = wait_cue(0x61);
		if (!r) r = pic_dissolve(P, 0xF, &R);
		if (!r) r = play_anim(&L, 30000, 2, 1, end_cb, 0, 0, 1);
		if (!r) timer_wait(0, 0x28);
	}
	if (!r) r = wait_cue(0x62);
	if (!r) {
		extra_id = 7; extra_x = 0x4D; extra_y = 0x9F; extra_shape = 1;
		play_or_time(30001, 0xB5);
		show_text(30000, 1);
		pic_draw(P, &L, 7, 0x28, 0x20, 0);
		R = shape_rect(&L, 0x2C, 0xA1, 0x40);
		port *S = save_rect(&R);
		sound_preload(30002);
		draw_shape(&L, 0x2C, 0xA1, 0x40, 10);
		countdown[0] = 6;
		r = wait_countdown(0);
		restore_rect(S);
		if (!r) r = timer_wait(0, 0x3C);
		if (!r) r = pic_fade_in(P, 0);
		if (!r) r = wait_or_time(30001);
		if (!r) r = timer_wait(0, 0x28);
	}
	if (!r) r = wait_cue(0x63);
	if (!r) {
		play_or_time(30002, 0x116);
		show_text(30000, 2);
		if (!r) r = timer_wait(0, 0x5A);
	}
	if (!r) {
		show_text(30000, 3);
		r = wait_or_time(30002);
		if (!r) r = timer_wait(0, 0x5A);
	}
	if (!r) r = wait_cue(0x64);
	if (!r) r = fade_to(0, 0xFFFF, 3);
	if (!r) {
		fill_rect(&(rect){0, 0, 8, 320}, 0x30);                      /* DS:13C4 */
		draw_shape(&L, 8, 0, 6, 0);
		saved_bar_bottom = save_rect(&R_BAR_BOTTOM);                  /* DS:2B20 */
		for (int k = 0; k < 4; k++) palt_get(30002 + k, end_pal[k]);
		for (int k = 30004; k <= 30008; k++) sound_preload(k);
		end_img = shape_get(&L, 10);                                  /* (194C:1870: kept) */
		r = play_anim(&L, 30000, 1, 1, end_cb, 0, 0, 1);
		img_free(end_img); end_img = NULL;
		restore_rect(saved_bar_bottom); saved_bar_bottom = NULL;
	}
	fade_out_clear();
	if (r) sound_stop_all();
	/* (2D7D:0193 frees the caption list and fonts, 194C:4E36 disposes P) */
	port_free(P);
	res_close("NIS.DAT");
	return r;
}
/* 2D7D:407F: the story scenes 7..11 */
static int scene_story(int n)
{
	res_open("NISDIGI.DAT"); res_open("NISMIDI.DAT");
	if (n != 11) res_open("NIS.DAT");
	int r = 0;
	if (n == 7) r = scene_story1();
	else if (n == 8) r = scene_story2();
	else if (n == 9) r = scene_story3();
	else if (n == 10) r = scene_story4();
	else if (n == 11) r = scene_end();
	else fprintf(stderr, "nis: story scene %d not reconstructed\n", n);
	if (n != 11) res_close("NIS.DAT");
	return r;
}

/* ------------------------------------------------------------------ the transitions (TRANS.DAT, 2D7D:0708) */
static img *title_img1, *title_img2;   /* DS:68FE, 6900: the title's two lines (images 0x138, 0x139) */
static port *title_saved;              /* DS:6902 */
static shplist trans_list2;            /* DS:68FC: the shape list 3500 of a level file (RUINS.DAT, CAVERNS.DAT) */
static int trans_palt2;                /* DS:692A: the palette the callbacks fade banks 0..2 to (DS:6928: trans_palt) */
static rect r_13b8;                    /* DS:13B8 (126, 140, 175, 180): the window of transition 6 (moved by 37F0:01AA) */
/* a PALT resource as it is (setpal of a part of it) */
static const uint8_t *palt_raw(int id) { int size; return res_get("PALT", id, &size); }

/* ---- the game engine's parts the transitions call (hooks: see nis.h) */
static nis_room_fn room_fn; static void *room_user;
static nis_kid kid_state = { 0, 0, 0, 0 };
/* 0AAC:0376 (level, room): the engine loads the level and draws the room into the current port (the screen) */
static void engine_room(int level, int room)
{
	trace("engine_room", level, room);
	if (the_port->bits == screen_bits) beam(g_time);
	if (room_fn) room_fn(level, room, the_port->bits, the_port->rowbytes, room_user);
	else fprintf(stderr, "nis: engine hook: level %d room %d not drawn (0AAC:0376)\n", level, room);
	/* the time it takes (the oracle, cycles of the scene's own work, the interrupts' apart): loading the level and its
	 * images, drawing the room (0AAC:0376 to 03F6 and on to 042C: level 10 room 22 15.06 M, level 14 room 1 8.83 M),
	 * then 2A31:0D03 reloading the scenes' overlay the engine's code displaced (3.82 M) */
	cpu(level == 10 ? 15060000 : level == 14 ? 8830000 : 10000000);
	cpu(3820000);
}
/* 2D7D:03F1 / 0440: mirror the anim's images 0xFA..0x107 / 0x10 (packed pixels, 0823:1414); 2D7D:03A2 is anim_mirror */
static void anim_mirror_chunky(anim *a) { for (int n = 0xFA; n <= 0x107; n++) mirror_chunky(anim_img(a, n)); }
static void anim_mirror_16(anim *a) { mirror_chunky(anim_img(a, 0x10)); }
/* 0AAC:0442 (k): the engine draws the kid (image 0x50 of KID.DAT's list 25001, mask 2) where he stands (DS:5B38 x,
 * 5B3A y; mirrored when DS:5B37 = 0) and the item image 0xB of PRINCE.DAT's list 1000 (mask 0x8000) at (0xAA, 0xB5),
 * both transparent (194C:04BA, mode 10); k: then the colours 0xF0..0xFF from PALS 1000 (0FB3:2B1C(1000, 0xF0, 16, 0)) */
static void engine_kid(int k)
{
	trace("engine_kid", k, kid_state.x);
	shplist l = shpl_load(25001, 2);
	img *im = shape_get(&l, 0x50);
	if (im) {
		int x = kid_state.x - 0x89;
		if (!kid_state.facing) { mirror_chunky(im); x -= 0x13; }             /* (0823:14DB on the 4-bit image) */
		draw_img(im, x, kid_state.y - 10, 10);
		img_free(im);
	}
	shplist l2 = shpl_load(1000, 0x8000);
	draw_shape_flip(&l2, 0xB, 0xAA, 0xB5, 10, 0);
	if (k) {
		int size; const uint8_t *p = res_get("PALS", 1000, &size);
		if (p) setpal(0xF0, 16, p, 1);
	}
}
/* OVL14 37F0:01AA (id, rect): move the rect (DS:13B8) to the kid (DS:6116) and 4 pixels up, and make it _SCR id's */
static void ovl14_place(int id, rect *r)
{
	*r = offset_rect(*r, -4, kid_state.x6116 - r->l - 0x87);
	scr_patch_id = id; scr_patch = *r;
}

/* 2D7D:0370: a row of ten tiles (image 7 of the list) along the top */
static void trans_tiles(const shplist *l) { for (int i = 0; i < 10; i++) draw_shape(l, 7, i << 5, 0, 0); }
/* 2D7D:184C / 1885 (anim, delay): the anim fades (256 colours) to DS:6928's palette / to black along its redraws */
static void anim_fade_palt(anim *a, int delay)
{
	uint8_t pal[768]; palt_get(trans_palt, pal);
	if (a->fade) fade_free(a->fade);
	a->fade = fade_new(delay, 0xFFFF, pal);
}
static void anim_fade_black(anim *a, int delay) { if (a->fade) fade_free(a->fade); a->fade = fade_new(delay, 0xFFFF, NULL); }
/* 2631:03E6(DS:692A, 7, 1): banks 0..2 fade to DS:692A's palette */
static void anim_fade_palt2(anim *a)
{
	uint8_t pal[768]; palt_get(trans_palt2, pal);
	if (a->fade) fade_free(a->fade);
	a->fade = fade_new(1, 7, pal);
}
/* 2D7D:1870: DS:6928's palette at once (2631:0296) */
static void trans_setpal(void) { uint8_t pal[768]; palt_get(trans_palt, pal); setpal_banks(pal, 0xFFFF); }

/* the scripts' callbacks (anim, arg) */
static int cb_18A8(anim *a, int arg)           /* transition 1: 0 fade in and wait for cue 0x61, else fade out */
{
	int r = 0;
	if (arg == 0) { anim_fade_palt(a, 1); r = wait_cue(0x61); }
	else anim_fade_black(a, 1);
	return r;
}
static int cb_18E4(anim *a, int arg)           /* transition 2, anim 4 */
{
	if (arg == 0) { anim_mirror(a); sound_play(25004); }
	else if (arg == 1) trans_setpal();
	else anim_fade_palt2(a);
	return 0;
}
static int cb_192F(anim *a, int arg)           /* transition 2, anim 5: 0 the new place behind (the screen and the anim's background) */
{
	if (arg == 0) {
		anim_mirror(a);
		setpal(0, 256, NULL, 1);
		port *saved = the_port; the_port = &screen_port;
		fill_rect(&R_GAME, 0);
		trans_tiles(&trans_list2);
		rect r = R_GAME; r.t = 0x50;
		fill_rect(&r, 0xA2);
		draw_shape(&trans_list, 0x14C, 0, 0x23, 10);
		the_port = saved;
		rect d = offset_rect(R_SCREEN, -a->scr.t, -a->scr.l);
		if (a->bg) copy_bits(a->bg, &screen_port, &d, &R_SCREEN);
	} else if (arg == 1) trans_setpal();
	else setpal(0, 256, NULL, 1);
	return 0;
}
static int cb_1A1D(anim *a, int arg) { if (arg == 0) anim_mirror(a); else sound_play(25006); return 0; }   /* anim 6 */
static int cb_1A40(anim *a, int arg) { (void)arg; anim_mirror(a); return 0; }                                /* anim 7 */
static int cb_1A52(anim *a, int arg)           /* anim 8 */
{
	if (arg == 0) { anim_mirror_16(a); trans_setpal(); }
	else if (arg == 1) anim_fade_palt2(a);
	return 0;
}
static int cb_1A92(anim *a, int arg)           /* transition 3, anim 9: the music, the fade in, cue 0x61 */
{
	(void)a; (void)arg;
	sound_play(25009);
	trans_palt = 25008;
	int r = fade_to(trans_palt, 0xFFFF, 0);
	if (!r) r = wait_cue(0x61);
	return r;
}
static int cb_1AE7(anim *a, int arg) { if (arg == 0) anim_fade_palt(a, 1); else anim_fade_black(a, 1); return 0; }   /* anim 10 */
static int cb_1B10(anim *a, int arg) { if (arg == 0) anim_mirror_16(a); else anim_fade_palt2(a); return 0; }         /* anim 11 */
/* 2D7D:1BB4: image 0x16B into the anim's port and background */
static void t5_stamp(anim *a)
{
	port *saved = the_port; the_port = a->p;
	shplist l = shpl_load(25000, 0xFFFF); trans_list.mask = l.mask;
	draw_shape(&l, 0x16B, -6, -3, 0);
	if (a->bg) copy_bits(a->bg, a->p, &a->p->portRect, &a->p->portRect);
	the_port = saved;
}
static int cb_1B46(anim *a, int arg)           /* transition 5, anim 1: 0 mirror and stamp; else palette 25000 + arg now */
{
	if (arg == 0) { anim_mirror_chunky(a); t5_stamp(a); }
	else {
		if (arg == 1) sound_play(31031);
		const uint8_t *p = res_find("PALT", 25000 + arg, NULL);
		if (p) { uint8_t pal[768]; palt_get(25000 + arg, pal); setpal(0, 256, pal, 1); }
	}
	return 0;
}
static int cb_17DF(anim *a, int arg)           /* transition 6: dissolve the anim's window in, wait for cue 0x62 */
{
	(void)arg;
	port *p = port_new(r_13b8);
	copy_bits(p, a->p, &r_13b8, &a->p->portRect);
	the_port = &screen_port;
	int r = dissolve(0xB4, &r_13b8, p, 1);
	port_free(p);
	if (!r) r = wait_cue(0x62);
	return r;
}
/* 2D7D:12DD..172E: transition 6 (from OVL14, level 8): the ruined palace, the mother's spirit in a window (script 4210) and
 * her words (STRL 31000), the father's sword */
static void t6_172E(const shplist *l)
{
	draw_shape(l, 0x2BC, 0x1B, 0, 0);
	draw_shape(l, 0x2BD, 0x1C, 0x64, 0);
	draw_shape(l, 0x2BE, 0, 0, 10);
	draw_shape(l, 0x2BF, 0x121, 0x5A, 10);
	draw_shape(l, 0x2C0, 0x38, 0xB5, 10);
	draw_shape(l, 0x2C1, 0x38, 0xB9, 10);
	draw_shape(l, 0x2C3, 0x112, 0x5A, 10);
	engine_kid(1);
}
static int trans6(void)
{
	text_top = 1; saved_bar_top = NULL;
	font10 = font_load(10); font11 = font_load(11);
	port *P = port_new(R_GAME); the_port = P;
	shplist L = shpl_load(3500, 0xFFF0);
	draw_shape(&L, 0x2C4, 0, 0, 0);
	draw_shape(&L, 0x2C5, 0, 0x64, 0);
	engine_kid(0);
	const uint8_t *pal = palt_raw(4208); if (pal) setpal(0, 0xE0, pal, 1);
	int r = wait_sound(0xFE);
	if (!r) {
		sound_play(31020);
		the_port = &screen_port;
		r = dissolve(0xF0, &R_GAME, P, 1);
	}
	shplist L3 = {0, 0, 0, 0};
	if (!r) {
		pal = palt_raw(4210); if (pal) setpal(0x20, 0x10, pal, 1);
		L3 = shpl_load(3500, 4);
		L3.first = 0x1072; L3.count = 4;
		ovl14_place(0x1072, &r_13b8);                  /* (thunk 2A31:0E39) */
		r = wait_cue(0x61);
	}
	if (!r) {
		r = play_anim(&L3, 0x1072, 0, 1, cb_17DF, 0, 0, 1);
		port_free(P); P = NULL;
	}
	if (!r) { the_port = &screen_port; saved_bar_top = save_rect(&R_BAR_TOP); }
	if (!r) r = say_text(31000, 1, 1, 0x137);
	if (!r) r = say_text(31000, 2, 2, 0x116);
	if (!r) r = say_text(31000, 3, 3, 0x79);
	if (!r) r = say_text(31000, 4, 4, 0x146);
	if (!r) { show_text(0, 0); r = timer_wait(0, 0x3C); }
	if (!r) r = say_text(31000, 5, 5, 0xDE);
	if (!r) r = timer_wait(0, 0x3C);
	if (!r) r = say_text(31000, 6, 6, 0xE0);
	if (!r) { r = timer_wait(0, 0x3C); show_text(0, 0); }
	if (!r) r = wait_sound(31020);
	sound_play(10255);                                 /* (the level's music again) */
	if (saved_bar_top) { port_free(saved_bar_top); saved_bar_top = NULL; }
	if (!r) {
		P = port_new(r_13b8); the_port = P;
		L = shpl_load(3500, 0xFFF0);
		draw_shape(&L, 0x2C5, 0, 0x64, 0);
		the_port = &screen_port;
		r = dissolve(0xB4, &r_13b8, P, 1);
		port_free(P); P = NULL;
	}
	if (!r) {
		P = port_new(R_GAME); the_port = P;
		L = shpl_load(3500, 0xFFF0);
		t6_172E(&L);
	}
	if (!r) r = fade_to(4209, 0xFFFF, 1);
	if (!r) { copy_bits(&screen_port, P, &R_GAME, &R_GAME); r = fade_to(4211, 0xFFFF, 1); }
	if (!r) r = timer_wait(0, 0x3C);
	if (P) port_free(P);
	the_port = &screen_port;
	scr_patch_id = 0;                                  /* (the _SCR resource is freed: reloaded unpatched) */
	if (!sound_playing(10255)) sound_play(10255);
	text_top = 0;
	return r;
}
/* 2D7D:117A: transition 5: list 25000 and the kid from FINAL.DAT (the scene does not open TRANS.DAT) */
static int trans5(shplist *l)
{
	draw_limit = 0x109;
	final_frame();                                     /* 2D7D:127E */
	shplist k = shpl_load(25001, 2);
	draw_shape_flip(&k, 0xF9, 90, 137, 10, 1);       /* (DS:13C2, 13C0) */
	sound_play(31030);
	trans_palt = 25000;
	int r = fade_to(trans_palt, 0xFFFF, 0);
	if (!r) r = play_anim(l, 25000, 0, 1, NULL, 0, 0, 1);
	if (!r) r = timer_wait(0, 0x5A);
	if (!r) {
		l->mask = 0xFFFE;
		r = play_anim(l, 25000, 1, 1, cb_1B46, 0, 0, 1);
		fill_rect(&R_SCREEN, the_port->bg);            /* 194C:4D72 */
	}
	draw_limit = 0;
	return r;
}
/* 2D7D:536E: the title's callback: 0 from the script (the fade in), 2 after every redraw (the title lines, on the music's
 * cue points 0x65..0x73) */
static int title_cb(anim *a, int arg)
{
	int r = 0;
	port *saved = the_port;
	if (arg == 0) {
		rect R = shape_rect(&trans_list, 0x138, 0x2B, 0x35);
		the_port = &screen_port; title_saved = save_rect(&R); the_port = saved;
		uint8_t pal[768]; palt_get(trans_palt, pal);
		a->fade = fade_new(1, 0xFFFF, pal);                  /* 2D7D:184C */
		r = wait_cue(0x64);
	} else if (arg == 2 && cue >= 0x65 && cue - 0x65 <= 0xE) {
		switch (cue) {
		case 0x65:
			the_port = &screen_port; draw_img(title_img1, 0x2B, 0x44, 10); the_port = saved;
			cue = 0x72;
			draw_img(title_img1, 0x2B - a->scr.l, 0x44 - a->scr.t, 10);
			break;
		case 0x66: case 0x68:
			while (a->last + (uint32_t)a->delay > g_tick) yield();
			if (title_saved) { the_port = &screen_port; copy_bits(&screen_port, title_saved, &title_saved->portRect, &title_saved->portRect); the_port = saved; }
			a->dirty = a->p->portRect;
			{ int k = anim_cb_arg; anim_cb_arg = 0; anim_draw(a); anim_cb_arg = k; }
			cue = 0x78;
			break;
		case 0x67:
			the_port = &screen_port; draw_img(title_img2, 0x3A, 0x35, 10); the_port = saved;
			cue = 0x73;
			draw_img(title_img2, 0x3A - a->scr.l, 0x35 - a->scr.t, 10);
			break;
		case 0x69:
			a->fade = fade_new(1, 0xFFFF, NULL);                  /* 2D7D:1885 */
			cue = 0x62;
			break;
		case 0x72: draw_img(title_img1, 0x2B - a->scr.l, 0x44 - a->scr.t, 10); break;
		case 0x73: draw_img(title_img2, 0x3A - a->scr.l, 0x35 - a->scr.t, 10); break;
		}
	}
	if (cue == 0x62 && !a->fade) r = 1;
	return r;
}
static int scene_trans(int n)
{
	res_open("NISDIGI.DAT"); res_open("NISMIDI.DAT");          /* 2797:0260 */
	draw_limit = 0x109;
	if (n != 5) res_open("TRANS.DAT");
	if (n != 6) { setpal(0, 256, NULL, 1); fill_rect(&R_SCREEN, 0); }
	trans_list = shpl_load(25000, 0xFFFF);
	int r = 0;
	if (n == 4) {                                               /* 2D7D:0770: the title */
		if (!sound_playing(25011)) sound_play(25016);
		fill_rect(&R_SCREEN, 0x10);
		draw_shape(&trans_list, 0x140, 0x76, 0, 10);
		draw_shape(&trans_list, 0x141, 0x49, 0x3C, 10);
		draw_shape(&trans_list, 0x142, -0x29, 0x20, 10);
		title_img1 = shape_get(&trans_list, 0x138); title_img2 = shape_get(&trans_list, 0x139);
		trans_palt = 25002;
		title_saved = NULL; anim_cb_arg = 2;
		r = play_anim(&trans_list, 25000, 12, 1, title_cb, 0, 0, 1);
		anim_cb_arg = 0;
		port_free(title_saved); title_saved = NULL;
		img_free(title_img1); img_free(title_img2); title_img1 = title_img2 = NULL;
	} else if (n == 1) {                                        /* 2D7D:0888 */
		res_open("CAVERNS.DAT"); res_open("RUINS.DAT");
		fill_rect(&R_GAME, 0x30);
		draw_shape(&trans_list, 0x166, -0x7F, -0x22, 0);
		draw_shape(&trans_list, 0x162, 0, 0x69, 0);
		draw_shape(&trans_list, 0x171, 0xB9, 0x2E, 10);
		trans_palt = 25001;
		r = play_anim(&trans_list, 25000, 1, 1, cb_18A8, 0, 0, 1);
		if (!r) {
			setpal(0, 256, NULL, 1);
			fill_rect(&R_SCREEN, 0);
			fill_rect(&R_GAME, 0x10);
			draw_shape(&trans_list, 0x140, 0x75, 0, 10);
			draw_shape(&trans_list, 0x141, 0x49, 0x3C, 10);
			draw_shape(&trans_list, 0x142, -0x29, 0x22, 10);
			trans_palt = 25002;
			r = play_anim(&trans_list, 25000, 2, 1, cb_18A8, 0, 0, 1);
		}
		if (!r) {
			setpal(0, 256, NULL, 1);
			fill_rect(&R_SCREEN, 0);
			trans_list = shpl_load(25000, 0xFFFE);
			trans_list2 = shpl_load(3500, 0xFFF0);
			draw_shape(&trans_list2, 0x44C, 0, 0, 0);
			draw_shape(&trans_list2, 0x44D, 0xA, 0x64, 0);
			draw_shape(&trans_list2, 0x44E, 0, 0x64, 0);
			trans_palt = 25011;
			r = fade_to(trans_palt, 0xFFFF, 0);
		}
		if (!r) r = play_anim(&trans_list, 25000, 3, 1, NULL, 0, 0, 1);
		res_close("RUINS.DAT"); res_close("CAVERNS.DAT");
	} else if (n == 2) {                                        /* 2D7D:0AEC */
		res_open("RUINS.DAT");
		trans_list2 = shpl_load(3500, 0xFFF8);
		trans_tiles(&trans_list2);
		draw_shape(&trans_list2, 0x2FB, 0, 0x27, 10);
		draw_shape(&trans_list2, 0x2FC, 0, 0x63, 0);
		draw_shape(&trans_list, 0x13D, 0x4B, 0xA6, 10);
		trans_palt = 25003; trans_palt2 = 25004;
		r = play_anim(&trans_list, 25000, 4, 1, cb_18E4, 0, 0, 1);
		if (!r) { trans_palt = 25004; r = play_anim(&trans_list, 25000, 5, 1, cb_192F, 0, 0, 1); }
		if (!r) {
			setpal(0, 256, NULL, 1);
			trans_palt = 25004;
			trans_tiles(&trans_list2);
			draw_shape(&trans_list2, 0x2FE, 0, 0xB, 10);
			draw_shape(&trans_list2, 0x2FF, 0, 0x65, 0);
			uint8_t pal[768]; palt_get(trans_palt, pal); setpal(0, 256, pal, 1);
			r = play_anim(&trans_list, 25000, 6, 1, cb_1A1D, 0, 0, 1);
		}
		if (!r) { r = fade_to(0, 0xFFFF, 0); fill_rect(&R_SCREEN, 0); }
		if (!r) { draw_shape(&trans_list, 0x15B, 0, 0, 10); trans_palt = 25005; r = fade_to(trans_palt, 0xFFFF, 0); }
		if (!r) r = play_anim(&trans_list, 25000, 7, 1, cb_1A40, 0, 0, 1);
		if (!r) { r = fade_to(0, 0xFFFF, 0); fill_rect(&R_SCREEN, 0); }
		if (!r) {
			fill_rect(&R_GAME, 0);
			trans_list = shpl_load(25000, 0xFFFE);
			trans_palt = 25006; trans_palt2 = 25007;
			engine_room(10, 22);                                /* 0AAC:0376 */
			r = play_anim(&trans_list, 25000, 8, 1, cb_1A52, 0, 0, 1);
		}
	} else if (n == 3) {                                        /* 2D7D:0E0A */
		fill_rect(&R_GAME, 0x30);
		draw_shape(&trans_list, 0x16A, 0, 0x7E, 0);
		draw_shape(&trans_list, 0x16B, 0, 0xA0, 10);
		draw_shape(&trans_list, 0x16C, 0, 0xA8, 10);
		draw_shape(&trans_list, 0x16D, 0xB7, 0x61, 10);
		draw_shape(&trans_list, 0x170, 0xB, 0x4D, 10);
		draw_shape(&trans_list, 0x171, -0xC, 0x28, 10);
		draw_shape(&trans_list, 0x175, 0x1C, 0xD, 10);
		draw_shape(&trans_list, 0x175, 0x9A, 0x1F, 10);
		draw_shape(&trans_list, 0x176, 0x7E, 0x32, 10);
		draw_shape(&trans_list, 0x160, 0, 0, 10);
		r = play_anim(&trans_list, 25000, 9, 1, cb_1A92, 0, 0, 1);
		if (!r) {
			fade_to(0, 0xFFFF, 0);                              /* (its result unused) */
			fill_rect(&R_SCREEN, 0);
			fill_rect(&R_GAME, 0x30);
			draw_shape(&trans_list, 0x175, 0x99, 0x21, 0);
			draw_shape(&trans_list, 0x176, 0x81, 0x33, 0);
			draw_shape(&trans_list, 0x177, -8, 0x27, 0);
			draw_shape(&trans_list, 0x175, 2, 0xA, 0);
			draw_shape(&trans_list, 0x16A, 0, 0x87, 0);
			draw_shape(&trans_list, 0x16B, 0, 0xA8, 0);
			draw_shape(&trans_list, 0x16C, 0, 0xB0, 10);
			trans_palt = 25008;
			r = play_anim(&trans_list, 25000, 10, 1, cb_1AE7, 0, 0, 1);
		}
		if (!r) {
			setpal(0, 256, NULL, 1);
			fill_rect(&R_SCREEN, 0);
			trans_list = shpl_load(25000, 0xFFFE);
			engine_room(14, 1);                                 /* 0AAC:0376 */
			trans_palt = 25009;
			r = fade_to(trans_palt, 0xFFFF, 0);
		}
		if (!r) { trans_palt2 = 25010; r = play_anim(&trans_list, 25000, 11, 1, cb_1B10, 0, 0, 1); }
	} else if (n == 5) r = trans5(&trans_list);
	else if (n == 6) r = trans6();
	if (r) { fade_out_clear(); sound_stop_all(); }
	if (n != 5) res_close("TRANS.DAT");
	if (n != 6) res_close("RUINS.DAT");
	draw_limit = 0;
	return r;
}

/* ------------------------------------------------------------------ play_scene (0AAC:0274) and the driver */
/* 0AAC:0274 play_scene: the screen is blacked out before (0AAC:0080: not for 1, 100, 6) and erased after (0AAC:0050:
 * not for 1, 100, 2, 3, 6) */
static int play_scene(int n)
{
	int r = 0;
	event(NIS_EV_SCENE);
	if (n != 1 && n != 100 && n != 6) { setpal(0, 256, NULL, 1); fill_rect(&R_SCREEN, screen_port.bg); }
	if (n >= 20 && n <= 28) r = scene_tree(n - 19);
	else if (n >= 1 && n <= 6) r = scene_trans(n);
	else if (n >= 7 && n <= 11) r = scene_story(n);
	else fprintf(stderr, "nis: scene %d not reconstructed\n", n);
	if (n != 1 && n != 100 && n != 2 && n != 3 && n != 6) fill_rect(&R_SCREEN, screen_port.bg);
	return r;
}
static int scene_list[16], scene_count;
static const double nis_start_ofs = 114000, nis_intro_ofs = 92000, nis_c1_ofs = 120000;
static void scene_main(void)
{
	int r = 0;
	res_open("PRINCE.DAT"); res_open("KID.DAT"); res_open("DIGISND.DAT"); res_open("MIDISND.DAT");   /* (open since the start-up: 2D3E:019A, 2797:0260 on DS:12A8; KID.DAT: the ending draws its 25016) */
	if (cur_scene == 11) res_open("FINAL.DAT");     /* (the file of the last levels, DS:0531: open when the ending comes) */
	/* the NISn cheat (0AAC:01E0..026B) before each play: NIS1 opens CAVERNS.DAT and starts the music 10034, NIS5 (re)opens
	 * FINAL.DAT, the others RUINS.DAT */
	/* where the frame and the 60 Hz timer are when play_scene comes (the oracle's frames of its ticks: the NISn cheat
	 * calls it ~114000 cycles after a retrace, 6000 after a tick; the intro ~92000 after, 229000 before the next tick) */
	if (!tick_fn) { pit_base = g_time + (cur_scene == NIS_INTRO ? 320600 : 108000) - TICK_CYC; pit_k = 0; pit_div = TICK_DIV; pit_next(); }
	if (cur_scene >= 1 && cur_scene <= 6) {
		if (cur_scene == 1) { res_open("CAVERNS.DAT"); cpu(nis_c1_ofs); nis_event_fn e = event_fn; event_fn = NULL; sound_play(10034); event_fn = e; }
		else if (cur_scene == 5) { res_close("FINAL.DAT"); res_open("FINAL.DAT"); }
		else res_open("RUINS.DAT");
	}
	if (cur_scene != 1) cpu(cur_scene == NIS_INTRO ? nis_intro_ofs : nis_start_ofs);
	/* 0823:01CA: the intro plays 7, 4, 8 while no key stops it */
	for (int i = 0; i < scene_count && r != 2; i++) { r = play_scene(scene_list[i]); if (r) r = 2; }
	scene_result = r;
	scene_done = 1;
}
static void reset_state(void)
{
	nfiles = 0; g_frame = 0; g_tick = 0; g_time = 0; g_abort = 0; scene_done = 0; scene_result = 0;
	isr_music = 0; isr_acc = 0; pit_base = 0; pit_k = 0; pit_div = TICK_DIV; pit_next();
	if (getenv("NIS_FADE_CYC")) fade_alloc_cyc = atof(getenv("NIS_FADE_CYC"));
	if (getenv("NIS_CPU")) cpu_scale = atof(getenv("NIS_CPU"));
	memset(countdown, 0, sizeof countdown); memset(&mus, 0, sizeof mus); memset(&digi, 0, sizeof digi); cue = 0;
	for (int i = 0; i < 16; i++) mus.map[i] = (uint8_t)i;
	memset(&fm, 0, sizeof fm); fm.count = fm_bank_count; fm_reset(); opl_writes = 0; mus_events_sent = 0; isr_lost = 0;
	memset(screen_bits, 0, sizeof screen_bits);
	if (init_dac_set) memcpy(dac, init_dac, sizeof dac); else { memset(dac, 0, sizeof dac); memcpy(dac + 0xE0 * 3, bios_e0, sizeof bios_e0); }
	screen_port.clip = (rect){0, 0, H, W}; the_port = &screen_port;
	draw_limit = 0; anim_cb_arg = 0; timer_target = 0; n_anim_frames = 0; n_snd_loaded = 0;
	r_13b8 = (rect){126, 140, 175, 180}; scr_patch_id = 0; text_top = 0; n_res_mem = 0;
}
int nis_open(const char *dir, int scene)
{
	snprintf(g_dir, sizeof g_dir, "%s", dir);
	{ char p[640]; snprintf(p, sizeof p, "%s/PRESETS.DEF", dir); FILE *f = fopen(p, "rb"); int c = f ? fgetc(f) : EOF; if (f) fclose(f); fm_bank_count = c > 0 ? c : 1; }   /* (MIDI.DRV function 6: the bank) */
	if (!dat_load("PRINCE.DAT") || !dat_load("NIS.DAT")) { fprintf(stderr, "nis: no game files in %s\n", dir); return 0; }
	reset_state();
	memset(&dis_cache, 0, sizeof dis_cache);
	cur_scene = scene;
	if (scene == NIS_INTRO) { scene_list[0] = 7; scene_list[1] = 4; scene_list[2] = 8; scene_count = 3; }
	else { scene_list[0] = scene; scene_count = 1; }
	coro_destroy(scene_coro); scene_coro = coro_create(scene_main, 1 << 20);
	return 1;
}
static void do_tick(void)              /* the 60 Hz tick (194C:7EE7) */
{
	g_tick++;
	for (int k = 0; k < 4; k++) if (countdown[k]) countdown[k]--;
}
int nis_step(uint8_t *screen, uint8_t *pal)
{
	if (scene_done) { if (screen) memcpy(screen, screen_bits, sizeof screen_bits); if (pal) memcpy(pal, dac, 768); return 0; }
	scan_row = 0;
	g_frame++;
	uint64_t start = fstart(g_frame);
	if (tick_fn) { uint32_t t = tick_fn(g_frame, tick_user); while (g_tick < t) do_tick(); if (g_time < start) g_time = start; }
	else advance(start);
	/* what the monitor shows during this frame: the pixels as the frame begins (a change the scene makes during the
	 * frame shows from the next one), the palette as set at the retrace that begins it (the scenes set the palette
	 * right after a retrace wait) */
	coro_resume(scene_coro);
	beam(fstart(g_frame + 1));
	if (screen) memcpy(screen, scan_bits, sizeof scan_bits);
	if (pal) memcpy(pal, dac, 768);
	return !scene_done;
}
void nis_close(void) { coro_destroy(scene_coro); scene_coro = NULL; }
void nis_set_sound_callback(nis_sound_fn fn, void *user) { sound_fn = fn; sound_user = user; }
void nis_set_tick_source(nis_tick_fn fn, void *user) { tick_fn = fn; tick_user = user; }
void nis_abort(void) { g_abort = 1; }
void nis_set_room_hook(nis_room_fn fn, void *user) { room_fn = fn; room_user = user; }
void nis_set_kid(const nis_kid *k) { if (k) kid_state = *k; }
void nis_set_palette(const uint8_t *pal) { init_dac_set = pal != NULL; if (pal) for (int i = 0; i < 768; i++) init_dac[i] = pal[i] & 0x3F; }
void nis_set_event_callback(nis_event_fn fn, void *user) { event_fn = fn; event_user = user; }
uint32_t nis_frame(void) { return g_frame; }
double nis_frame_pos(void) { double x = (double)((int64_t)(g_time - fstart(g_frame))) / FRAME_CYC; return x < 0 ? 0 : x; }
uint32_t nis_tick(void) { return g_tick; }
uint32_t nis_anim_frames(void) { return n_anim_frames; }

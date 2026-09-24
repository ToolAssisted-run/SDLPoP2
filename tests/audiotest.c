/* Sound driver test: replays an oracle capture's sound requests and interrupts through source/audio.c and compares what the
 * drivers write (OPL registers, the sequencer's MIDI events, DSP bytes) with what the DOS drivers wrote.
 *   audiotest PRINCE2_DIR CAPTURE-snap.txt [-v]
 * The capture (oracle script lines, see tests/README.md / docs/AUDIO.md):
 *   probe 1611 053C a_req SS0000 10       probe 194C 840E a_reqid SS0000 10   probe 194C 83D2 a_stop SS0000 10
 *   probe 194C 3380 a_vol SS0000 8        probe 194C 8396 a_rel SS0000 10     probe 194C 2F10 a_isr
 *   probe 194C 3422 a_digiend             probe 194C 34BA a_midiend           probe 194C 3055 a_ev
 *   probe 4BD9 08AF a_opl                 probe 4B5C 02D2 a_dsp               probe 4B5C 0526 a_irq
 * (4BD9 / 4B5C: where MIDI.DRV / DIGI.DRV sit in the oracle's memory.) Inputs (requests, stops, volume, the MIDI timer
 * interrupt, the digitized sound's end) drive the C driver; the outputs between two inputs must match in order.
 * DSP transfers that DIGI.DRV splits at 64 KiB DMA pages are merged into one (the C driver has no physical addresses). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../source/audio.h"

typedef struct { int kind; int a, b; int frame; int line; } ev;   /* kind: 1 input, 2 output */
enum { IN_REQ = 1, IN_REQID, IN_STOP, IN_VOL, IN_REL, IN_ISR, IN_DIGIEND, IN_SPKISR };
static ev *evs; static int nev, cap;
static void add(int kind, int t, int a, int b, int frame, int line)
{
	if (nev == cap) { cap = cap ? cap * 2 : 4096; evs = realloc(evs, cap * sizeof *evs); }
	evs[nev++] = (ev){kind * 100 + t, a, b, frame, line};
}
static unsigned hexw(const char *s, int word) { unsigned v = 0; if (sscanf(s + 4 * word, "%4x", &v) != 1) return 0; return (v >> 8 | v << 8) & 0xFFFF; }
static unsigned reg(const char *l, const char *name) { const char *p = strstr(l, name); unsigned v = 0; if (p) sscanf(p + strlen(name), "%x", &v); return v; }

/* the C side's outputs, collected per input */
static struct { int what, a, b; } out[65536]; static int nout;
static void trace(int what, int a, int b)
{
	if (what == AUDIO_T_TICK || what == AUDIO_T_DIGI_END) return;
	if (nout < 65536) { out[nout].what = what; out[nout].a = a; out[nout].b = b; nout++; }
}
static const char *wname(int w) { return w == AUDIO_T_OPL ? "opl" : w == AUDIO_T_MIDI ? "midi" : w == AUDIO_T_DSP ? "dsp" : w == AUDIO_T_MIDI_END ? "midiend" : w == AUDIO_T_SPEAKER ? "speaker" : "?"; }
static int same(int w, int a1, int b1, int a2, int b2)
{
	if (w == AUDIO_T_MIDI) {
		if (a1 != a2) return (a1 & 0xF0) == 0xF0 && (a2 & 0xF0) == 0xF0;
		if ((a1 & 0xF0) == 0xF0) return 1;
		if ((a1 & 0xF0) == 0xC0 || (a1 & 0xF0) == 0xD0) return (b1 & 0xFF) == (b2 & 0xFF);
		return b1 == b2;
	}
	if (w == AUDIO_T_DSP && b2 == -1) return 1;       /* a length the oracle could not show */
	return a1 == a2 && (w == AUDIO_T_MIDI_END || b1 == b2);
}

/* audiotest PRINCE2_DIR --wav OUT.wav SECONDS ID...: render sounds (started together) to a 44.1 kHz WAV */
static int wav(int argc, char **argv)
{
	if (!audio_init(argv[1], AUDIO_CAP_DIGI | AUDIO_CAP_MIDI)) return 2;
	char p[1024]; snprintf(p, sizeof p, "%s/NISDIGI.DAT", argv[1]); audio_add_file(p); snprintf(p, sizeof p, "%s/NISMIDI.DAT", argv[1]); audio_add_file(p);
	int rate = 44100, n = (int)(atof(argv[4]) * rate);
	for (int i = 5; i < argc; i++) if (!audio_request((uint16_t)strtol(argv[i], 0, 0))) fprintf(stderr, "%s: not started\n", argv[i]);
	int16_t *pcm = calloc(n, 2); audio_render(pcm, n, rate);
	FILE *f = fopen(argv[3], "wb"); if (!f) return 2;
	uint32_t h[11] = {0x46464952, 36 + 2 * (uint32_t)n, 0x45564157, 0x20746D66, 16, 1 | 1 << 16, (uint32_t)rate, (uint32_t)rate * 2, 2 | 16 << 16, 0x61746164, 2 * (uint32_t)n};
	fwrite(h, 4, 11, f); fwrite(pcm, 2, n, f); fclose(f);
	printf("%s: %d samples; still playing: %d\n", argv[3], n, audio_playing(0));
	return 0;
}

static void wav_write(const char *path, const int16_t *pcm, int n, int rate)
{
	FILE *f = fopen(path, "wb"); if (!f) return;
	uint32_t h[11] = {0x46464952, 36 + 2 * (uint32_t)n, 0x45564157, 0x20746D66, 16, 1 | 1 << 16, (uint32_t)rate, (uint32_t)rate * 2, 2 | 16 << 16, 0x61746164, 2 * (uint32_t)n};
	fwrite(h, 4, 11, f); fwrite(pcm, 2, n, f); fclose(f);
}
/* audiotest PRINCE2_DIR --pcm CAPTURE-snap.txt AUDIO.raw [OUT_PREFIX]: the capture's requests (placed at their sample
 * position in the oracle's mixer output, oracle-run `audio F1 F2 PATH`) rendered by the C driver in real time; compares
 * 10 ms RMS envelopes with the DOSBox-X output and writes both as WAVs */
static int pcm_compare(int argc, char **argv)
{
	const int rate = 44100;
	char p[1024]; snprintf(p, sizeof p, "%s.frames", argv[4]);
	FILE *f = fopen(p, "r"); if (!f) { perror(p); return 2; }
	static long long cum[100000], iend[100000]; static int np[100000]; int fr, n, maxf = 0; unsigned long long ie; long long c = 0;
	while (fscanf(f, "%d %d %llu", &fr, &n, &ie) == 3 && fr < 100000) { np[fr] = n; cum[fr] = c += n; iend[fr] = (long long)ie; maxf = fr; }
	fclose(f);
	f = fopen(argv[4], "rb"); if (!f) { perror(argv[4]); return 2; }
	int16_t *st = malloc(c * 4); c = (long long)fread(st, 4, c, f); fclose(f);
	int16_t *ora = malloc(c * 2), *mine = calloc(c, 2);
	for (long long i = 0; i < c; i++) ora[i] = (int16_t)((st[2 * i] + st[2 * i + 1]) / 2);
	int caps = getenv("AUDIO_CAPS") ? atoi(getenv("AUDIO_CAPS")) : AUDIO_CAP_DIGI | AUDIO_CAP_MIDI;
	if (!audio_init(argv[1], caps)) return 2;
	if (caps) { snprintf(p, sizeof p, "%s/NISDIGI.DAT", argv[1]); audio_add_file(p); snprintf(p, sizeof p, "%s/NISMIDI.DAT", argv[1]); audio_add_file(p); }
	else { snprintf(p, sizeof p, "%s/NISIBM.DAT", argv[1]); audio_add_file(p); }
	if (getenv("GAIN_FM")) audio_gain_fm = atoi(getenv("GAIN_FM"));
	if (getenv("GAIN_DIGI")) audio_gain_digi = atoi(getenv("GAIN_DIGI"));
	f = fopen(argv[3], "r"); if (!f) { perror(argv[3]); return 2; }
	static char l[1 << 20]; long long pos = 0; int started = 0;
	while (fgets(l, sizeof l, f)) {
		const char *q = strstr(l, "frame="), *qn = strstr(l, " n="), *pr = strstr(l, " probe="); if (!q || !qn || !pr) continue;
		int ef = atoi(q + 6); long long en = atoll(qn + 3); char lab[64]; if (sscanf(strchr(pr + 1, ' ') + 1, "%63s", lab) != 1) continue;
		const char *stk = strstr(l, "stack="); stk = stk ? stk + 6 : "";
		int t = !strcmp(lab, "a_req") ? 1 : !strcmp(lab, "a_reqid") ? 2 : !strcmp(lab, "a_stop") ? 3 : !strcmp(lab, "a_vol") ? 4 : !strcmp(lab, "a_rel") ? 5 : 0;
		if (!t) continue;
		if (t == 4 && !started) { started = 1; continue; }
		if (ef > maxf || ef < 1) continue;
		long long a = cum[ef - 1], span = iend[ef] - iend[ef - 1];
		long long at = a + (span > 0 ? (long long)np[ef] * (en - iend[ef - 1]) / span : 0);
		if (at > pos && at <= c) { audio_render(mine + pos, (int)(at - pos), rate); pos = at; }
		unsigned v = hexw(stk, 2);
		if (t == 1) audio_request((uint16_t)(10000 + v)); else if (t == 2) audio_request((uint16_t)v); else if (t == 3) audio_stop((uint16_t)v); else if (t == 4) audio_volume(v & 0xFF); else audio_release((uint16_t)v);
	}
	fclose(f);
	if (pos < c) audio_render(mine + pos, (int)(c - pos), rate);
	/* 10 ms envelopes (DC removed per window), their correlation and level ratio */
	int w = rate / 100; long nw = (long)(c / w); double sxy = 0, sxx = 0, syy = 0, sx = 0, sy = 0; long act = 0;
	for (long k = 0; k < nw; k++) {
		double mo = 0, mc = 0; for (int i = 0; i < w; i++) { mo += ora[k * w + i]; mc += mine[k * w + i]; } mo /= w; mc /= w;
		double eo = 0, ec = 0; for (int i = 0; i < w; i++) { double a1 = ora[k * w + i] - mo, b1 = mine[k * w + i] - mc; eo += a1 * a1; ec += b1 * b1; }
		eo = sqrt(eo / w); ec = sqrt(ec / w);
		sx += eo; sy += ec; sxx += eo * eo; syy += ec * ec; sxy += eo * ec; if (eo > 100 || ec > 100) act++;
	}
	double cov = sxy / nw - (sx / nw) * (sy / nw), vx = sxx / nw - (sx / nw) * (sx / nw), vy = syy / nw - (sy / nw) * (sy / nw);
	printf("%s: %.1f s, %ld active 10 ms windows; envelope correlation %.3f, mean RMS oracle %.0f / C %.0f\n", argv[3], c / (double)rate, act, cov / sqrt(vx * vy + 1e-9), sx / nw, sy / nw);
	if (argc > 5) { snprintf(p, sizeof p, "%s_oracle.wav", argv[5]); wav_write(p, ora, (int)c, rate); snprintf(p, sizeof p, "%s_c.wav", argv[5]); wav_write(p, mine, (int)c, rate); }
	return 0;
}

int main(int argc, char **argv)
{
	if (argc >= 6 && !strcmp(argv[2], "--wav")) return wav(argc, argv);
	if (argc >= 5 && !strcmp(argv[2], "--pcm")) return pcm_compare(argc, argv);
	if (argc < 3) { fprintf(stderr, "usage: audiotest PRINCE2_DIR CAPTURE-snap.txt [-v]\n"); return 2; }
	int verbose = argc > 3 && !strcmp(argv[3], "-v");
	FILE *f = fopen(argv[2], "r"); if (!f) { perror(argv[2]); return 2; }
	static char l[1 << 20]; int line = 0, started = 0;
	while (fgets(l, sizeof l, f)) {
		line++;
		char lab[64] = ""; int fr = 0; const char *p = strstr(l, "frame="); if (p) fr = atoi(p + 6);
		p = strstr(l, " probe="); if (!p || sscanf(strchr(p + 1, ' ') + 1, "%63s", lab) != 1) continue;
		const char *st = strstr(l, "stack="); st = st ? st + 6 : "";
		unsigned ax = reg(l, " ax="), dx = reg(l, " dx=");
		if (!strcmp(lab, "a_vol")) { if (!started) { started = 1; continue; } add(1, IN_VOL, hexw(st, 2) & 0xFF, 0, fr, line); }
		if (!started) continue;
		int had_input = 0; for (int q = nev - 1; q >= 0 && !had_input; q--) had_input = evs[q].kind / 100 == 1;
		if (!had_input && strcmp(lab, "a_req") && strcmp(lab, "a_reqid") && strcmp(lab, "a_stop") && strcmp(lab, "a_rel") && strcmp(lab, "a_isr") && strcmp(lab, "a_digiend") && strcmp(lab, "a_spkisr")) continue;   /* the start's own volume call */
		if (!strcmp(lab, "a_req")) add(1, IN_REQ, hexw(st, 2), 0, fr, line);
		else if (!strcmp(lab, "a_reqid")) add(1, IN_REQID, hexw(st, 2), 0, fr, line);
		else if (!strcmp(lab, "a_stop")) add(1, IN_STOP, hexw(st, 2), 0, fr, line);
		else if (!strcmp(lab, "a_rel")) add(1, IN_REL, hexw(st, 2), 0, fr, line);
		else if (!strcmp(lab, "a_isr")) add(1, IN_ISR, 0, 0, fr, line);
		else if (!strcmp(lab, "a_digiend")) add(1, IN_DIGIEND, 0, 0, fr, line);
		else if (!strcmp(lab, "a_spkisr")) { if (nev && evs[nev - 1].kind == 301) continue; add(1, IN_SPKISR, 0, 0, fr, line); }   /* (the start's own int 8 is part of the start) */
		else if (!strcmp(lab, "a_gate")) add(2, AUDIO_T_SPEAKER, 0, ax & 3, fr, line);
		else if (!strcmp(lab, "a_div")) add(2, AUDIO_T_SPEAKER, 1, ax, fr, line);
		else if (!strcmp(lab, "a_opl")) add(2, AUDIO_T_OPL, ax >> 8, ax & 0xFF, fr, line);
		else if (!strcmp(lab, "a_ev")) add(2, AUDIO_T_MIDI, (ax & 0xFF) | ((ax & 0xFF) < 0xF0 ? (ax >> 8) & 0x0F : 0), dx, fr, line);
		else if (!strcmp(lab, "a_dsp")) add(2, AUDIO_T_DSP, ax & 0xFF, 0, fr, line);
		else if (!strcmp(lab, "a_midiend")) add(2, AUDIO_T_MIDI_END, 0, 0, fr, line);
		else if (!strcmp(lab, "a_irq")) add(3, 0, 0, 0, fr, line);
		else if (!strcmp(lab, "a_digistart") || !strcmp(lab, "a_midistart") || !strcmp(lab, "a_spkstart")) add(3, 1, 0, 0, fr, line);
	}
	fclose(f);
	/* merge DMA-page continuation blocks (DSP 14 lo hi written in the IRQ) into the transfer's first block */
	int last14 = -1;
	for (int i = 0; i < nev; i++) {
		if (evs[i].kind == 200 + AUDIO_T_DSP && evs[i].a == 0x14 && i + 2 < nev && evs[i + 1].kind == 200 + AUDIO_T_DSP) {
			int prev_irq = 0; for (int j = i - 1; j >= 0 && evs[j].kind != 200 + AUDIO_T_DSP; j--) if (evs[j].kind == 300) { prev_irq = 1; break; }
			if (prev_irq && last14 >= 0) {
				unsigned add_len = (evs[i + 1].a | evs[i + 2].a << 8) + 1, len = (evs[last14 + 1].a | evs[last14 + 2].a << 8) + add_len;
				evs[last14 + 1].a = len & 0xFF; evs[last14 + 2].a = (len >> 8) & 0xFF;
				for (int k = 0; k < 3; k++) evs[i + k].kind = 0;
				i += 2; continue;
			}
			last14 = i;
		}
	}
	/* a transfer stopped before its first DMA block ended shows only that block's length: its length is not compared */
	for (int i = 0; i + 2 < nev; i++) {
		if (evs[i].kind != 200 + AUDIO_T_DSP || evs[i].a != 0x14 || evs[i + 1].kind != 200 + AUDIO_T_DSP) continue;
		int done = 0;
		for (int j = i + 3; j < nev; j++) {
			if (evs[j].kind == 100 + IN_DIGIEND || (evs[j].kind == 300 && 0)) { done = 1; break; }
			if (evs[j].kind == 200 + AUDIO_T_DSP && (evs[j].a == 0x40 || evs[j].a == 0xD0)) break;
		}
		if (!done) evs[i + 1].b = evs[i + 2].b = -1;
	}
	/* a request whose resource must first be read from disk starts its sound only after the load, while timer
	 * interrupts keep coming: the request moves to its channel start (194C:35E5 / 3668 / 370A) when there is one */
	for (int i = 0; i < nev; i++) {
		int t = evs[i].kind; if (t != 100 + IN_REQ && t != 100 + IN_REQID) continue;
		for (int j = i + 1; j < nev; j++) {
			int u = evs[j].kind;
			if (u == 100 + IN_REQ || u == 100 + IN_REQID || u == 100 + IN_STOP || u == 100 + IN_VOL || u == 100 + IN_REL) break;
			if (u == 301) {
				int ins = 0; for (int k = i + 1; k < j; k++) if (evs[k].kind / 100 == 1) ins++;
				if (!ins) break;                           /* no interrupt in between: already in place */
				ev r = evs[i]; memmove(&evs[i], &evs[i + 1], (j - i - 1) * sizeof *evs); evs[j - 1] = r; break;
			}
		}
	}
	int caps = getenv("AUDIO_CAPS") ? atoi(getenv("AUDIO_CAPS")) : AUDIO_CAP_DIGI | AUDIO_CAP_MIDI;   /* 0 for a PC speaker capture */
	if (!audio_init(argv[1], caps)) { fprintf(stderr, "no sound files in %s\n", argv[1]); return 2; }
	char p2[1024];
	if (caps) { snprintf(p2, sizeof p2, "%s/NISDIGI.DAT", argv[1]); audio_add_file(p2); snprintf(p2, sizeof p2, "%s/NISMIDI.DAT", argv[1]); audio_add_file(p2); }
	else { snprintf(p2, sizeof p2, "%s/NISIBM.DAT", argv[1]); audio_add_file(p2); }
	audio_manual_clock = 1; audio_trace = trace;
	/* every input through the C driver; then each kind of output compared in order with the oracle's (an interrupt may
	 * land inside a request's driver calls in the DOS run, so outputs are not compared per input) */
	typedef struct { int a, b, frame, line; } rec;
	static rec cs[8][1 << 17], os[8][1 << 17]; int nc[8] = {0}, no[8] = {0}, inputs = 0, last_in = 0;
	for (int i = 0; i < nev; i++) {
		int t = evs[i].kind;
		if (t / 100 == 2) { int w = t % 100; if (no[w] < (1 << 17)) os[w][no[w]++] = (rec){evs[i].a, evs[i].b, evs[i].frame, evs[i].line}; continue; }
		if (t / 100 != 1) continue;
		nout = 0; inputs++;
		switch (t % 100) {
		case IN_REQ: audio_request((uint16_t)(10000 + evs[i].a)); break;
		case IN_REQID: audio_request((uint16_t)evs[i].a); break;
		case IN_STOP: audio_stop((uint16_t)evs[i].a); break;
		case IN_REL: audio_release((uint16_t)evs[i].a); break;
		case IN_VOL: audio_volume(evs[i].a); break;
		case IN_ISR: audio_midi_irq(); break;
		case IN_DIGIEND: audio_digi_irq(); break;
		case IN_SPKISR: audio_speaker_irq(); break;
		}
		for (int k = 0; k < nout; k++) { int w = out[k].what; if (nc[w] < (1 << 17)) cs[w][nc[w]++] = (rec){out[k].a, out[k].b, evs[i].frame, evs[i].line}; }
		last_in = i;
	}
	/* the capture's end may cut the last input's driver calls short */
	for (int w = 0; w < 8; w++) while (nc[w] > no[w] && cs[w][nc[w] - 1].line == evs[last_in].line) nc[w]--;
	int total_bad = 0, shown = 0; const int kinds[5] = {AUDIO_T_OPL, AUDIO_T_MIDI, AUDIO_T_DSP, AUDIO_T_MIDI_END, AUDIO_T_SPEAKER};
	printf("%s: %d inputs", argv[2], inputs);
	for (int q = 0; q < 5; q++) {
		int w = kinds[q], m = no[w] > nc[w] ? no[w] : nc[w], good = 0;
		for (int k = 0; k < m; k++) {
			if (k < no[w] && k < nc[w] && same(w, cs[w][k].a, cs[w][k].b, os[w][k].a, os[w][k].b)) { good++; continue; }
			total_bad++;
			if (shown++ < 30 || verbose) fprintf(stderr, "%s #%d: oracle %02X %04X (frame %d, line %d), C %02X %04X (input at frame %d)\n", wname(w), k,
				k < no[w] ? os[w][k].a : -1, k < no[w] ? os[w][k].b & 0xFFFF : 0, k < no[w] ? os[w][k].frame : -1, k < no[w] ? os[w][k].line : -1,
				k < nc[w] ? cs[w][k].a : -1, k < nc[w] ? cs[w][k].b & 0xFFFF : 0, k < nc[w] ? cs[w][k].frame : -1);
		}
		printf(", %s %d/%d%s", wname(w), good, no[w], nc[w] != no[w] ? " (C count differs)" : "");
	}
	printf(" -> %s\n", total_bad ? "DIFFERENT" : "identical");
	return total_bad != 0;
}

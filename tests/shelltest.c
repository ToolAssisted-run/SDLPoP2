/* shelltest: runs the shell (source/shell.c) frame by frame on an oracle-style script, as DOSBox runs the game:
 *   shelltest GAME_DIR SCRIPT [COMMAND LINE WORDS...]
 * Script lines (the oracle's syntax; other commands are ignored):
 *   key FRAME NAME 1|0      a key goes down / up (DOSBox key names: esc tab enter space a..z 0..9 left up f1 ...)
 *   poke FRAME PHYS HEX     writes a DS variable (PHYS = 0x3B250 + its DS offset), as the oracle's poke
 *   shot FRAME PATH         the screen as a 320x200 TGA (top-down, BGR)
 *   end FRAME
 * Prints the shell's mode changes, level starts and exits as "frame=N event ...". Keys typed while held repeat as
 * DOSBox's keyboard does (500 ms, then every 33 ms).
 * SHELL_CMP=SNAPFILE compares the game state at every tick start with the oracle's ds_tick probes (DS:2900..6C00) of
 * that capture, in order (SHELL_CMP_SKIP=n skips the first n samples), and prints the differing fields; with SHELL_LOAD=1
 * the game then goes on from the capture's state (the screens then test the drawing alone).
 * Also `probepoke ds_tick N PHYS HEX` (tools/plan2script.py): at the N-th tick's start, the key table (DS:1D00, phys
 * 3CF50; kept held for the frames after), the BIOS shift flags (phys 417) or a DS variable, as the oracle writes them.
 * SHELL_VRAM=FRAMES (a tools/framecap.py capture with VRAM_STEP, run with SHELL_CMP on its own ds_tick probes and
 * SHELL_SYNC): each VGA dump (or RGB shot, SHOT_STEP: the palette too, rows 0..191) compared with the screen the same distance after the same tick, and +-2 frames around.
 * SHELL_FOLLOW=1 follows the capture's platform timing as e2e does (the lateness meter; the ambient music's random draws
 * when the capture is up to 4 draws ahead). SHELL_OFFSET=n: the script's frames are n frames later than the shell's (DOS starting the program), SHELL_SEED=hex: the
 * clock's seed (DOSBox's time() at the start). SHELL_FILES=dir: where PRINCE.SAV/OPT/HOF go (default: the current
 * directory; the game directory is never written). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/shell.h"
#include "../source/render.h"
#include "../source/types.h"
#include "../source/globals.h"
#include "../source/state.h"
#include "snap.h"
#include "../source/loader.h"

typedef struct { const char *name; uint8_t scan, ascii, shifted; } keydef;
static const keydef keys[] = {
	{"esc",1,0x1B,0x1B},{"1",2,'1','!'},{"2",3,'2','@'},{"3",4,'3','#'},{"4",5,'4','$'},{"5",6,'5','%'},{"6",7,'6','^'},{"7",8,'7','&'},
	{"8",9,'8','*'},{"9",10,'9','('},{"0",11,'0',')'},{"minus",12,'-','_'},{"equals",13,'=','+'},{"backspace",14,8,8},{"tab",15,9,0},
	{"q",16,'q','Q'},{"w",17,'w','W'},{"e",18,'e','E'},{"r",19,'r','R'},{"t",20,'t','T'},{"y",21,'y','Y'},{"u",22,'u','U'},{"i",23,'i','I'},
	{"o",24,'o','O'},{"p",25,'p','P'},{"leftbracket",26,'[','{'},{"rightbracket",27,']','}'},{"enter",28,13,13},{"leftctrl",29,0,0},
	{"a",30,'a','A'},{"s",31,'s','S'},{"d",32,'d','D'},{"f",33,'f','F'},{"g",34,'g','G'},{"h",35,'h','H'},{"j",36,'j','J'},{"k",37,'k','K'},
	{"l",38,'l','L'},{"semicolon",39,';',':'},{"quote",40,'\'','"'},{"grave",41,'`','~'},{"leftshift",42,0,0},{"backslash",43,'\\','|'},
	{"z",44,'z','Z'},{"x",45,'x','X'},{"c",46,'c','C'},{"v",47,'v','V'},{"b",48,'b','B'},{"n",49,'n','N'},{"m",50,'m','M'},
	{"comma",51,',','<'},{"period",52,'.','>'},{"slash",53,'/','?'},{"rightshift",54,0,0},{"kpmultiply",55,'*','*'},{"leftalt",56,0,0},
	{"space",57,' ',' '},{"capslock",58,0,0},{"f1",59,0,0},{"f2",60,0,0},{"f3",61,0,0},{"f4",62,0,0},{"f5",63,0,0},{"f6",64,0,0},
	{"f7",65,0,0},{"f8",66,0,0},{"f9",67,0,0},{"f10",68,0,0},{"numlock",69,0,0},{"scrolllock",70,0,0},
	{"home",71,0,0},{"kp7",71,0,0},{"up",72,0,0},{"kp8",72,0,0},{"pageup",73,0,0},{"kp9",73,0,0},{"kpminus",74,'-','-'},
	{"left",75,0,0},{"kp4",75,0,0},{"kp5",76,0,0},{"right",77,0,0},{"kp6",77,0,0},{"kpplus",78,'+','+'},
	{"end",79,0,0},{"kp1",79,0,0},{"down",80,0,0},{"kp2",80,0,0},{"pagedown",81,0,0},{"kp3",81,0,0},{"insert",82,0,0},{"kp0",82,0,0},
	{"delete",83,0,0},{"kpperiod",83,0,0},{"rightctrl",29,0,0},{"rightalt",56,0,0},{"kpenter",28,13,13},
};
static const keydef *find_key(const char *n) { for (size_t i = 0; i < sizeof keys / sizeof *keys; i++) if (!strcmp(keys[i].name, n)) return &keys[i]; return NULL; }
typedef struct { int frame, key, level; char path[256]; int kind, done; unsigned phys; uint8_t bytes[16]; int nb; } ev_t;   /* kind 0 key, 1 shot, 2 poke */
static ev_t ev[20000]; static int nev;
static void write_tga(const char *path)
{
	FILE *f = fopen(path, "wb"); if (!f) return;
	uint8_t hdr[18] = {0}; hdr[2] = 2; hdr[12] = SCREEN_W & 255; hdr[13] = SCREEN_W >> 8; hdr[14] = SCREEN_H; hdr[16] = 24; hdr[17] = 0x20;
	fwrite(hdr, 1, 18, f);
	for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
		const uint8_t *c = render_palette + 3 * screen_buf[i];
		uint8_t p[3] = { (uint8_t)(c[2] << 2 | c[2] >> 4), (uint8_t)(c[1] << 2 | c[1] >> 4), (uint8_t)(c[0] << 2 | c[0] >> 4) };
		fwrite(p, 1, 3, f);
	}
	fclose(f);
}
/* ---- comparing with the oracle's per-tick samples ---- */
static uint8_t (*samples)[SNAP_MAX]; static int nsamples, cur_sample, ticks_seen, bad_ticks, cmp_frame;
static int sample_frame[4000], my_tick_frame[4000];   /* SHELL_SYNC: the oracle's and the shell's frame of each tick */
static int hexval(int c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; }
static void load_samples(const char *path, int skip)
{
	FILE *f = fopen(path, "r"); if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(2); }
	static char line[1 << 17]; samples = malloc(sizeof *samples * 4000);
	if (strlen(path) > 7 && !strcmp(path + strlen(path) - 7, ".frames")) {   /* a tools/framecap.py capture: its ds_tick records */
		uint8_t h[16];
		while (fread(h, 1, 16, f) == 16 && nsamples < 4000) {
			uint32_t len = h[12] | h[13] << 8 | h[14] << 16 | (uint32_t)h[15] << 24, fr = h[8] | h[9] << 8 | h[10] << 16 | (uint32_t)h[11] << 24;
			if (strcmp((const char *)h, getenv("SHELL_CMP_LABEL") ? getenv("SHELL_CMP_LABEL") : "ds_tick") || len > SNAP_MAX) { fseek(f, len, SEEK_CUR); continue; }   /* (SHELL_CMP_LABEL=pre_ds: the drawing's state, for the captures without ds_tick: timing only) */
			if (skip > 0) { skip--; fseek(f, len, SEEK_CUR); continue; }
			sample_frame[nsamples] = (int)fr; if (fread(samples[nsamples], 1, len, f) != len) break;
			nsamples++;
		}
		fclose(f); return;
	}
	while (fgets(line, sizeof line, f) && nsamples < 4000) {
		if (!strstr(line, " ds_tick at ")) continue;
		if (skip > 0) { skip--; continue; }
		sample_frame[nsamples] = atoi(line + 6);
		char *m = strstr(line, " mem="); if (!m) continue; m += 5;
		for (int i = 0; i < SNAP_MAX && m[2 * i] && m[2 * i + 1] && m[2 * i] != '\n'; i++) samples[nsamples][i] = (uint8_t)(hexval(m[2 * i]) << 4 | hexval(m[2 * i + 1]));
		nsamples++;
	}
	fclose(f);
}
static void tick_cmp(void)
{
	static uint8_t got[SNAP_MAX]; static const char *regions[200]; static int nr;
	static const char *const drawn[] = {"obj_x", "obj_y", "obj_id", "obj_chtab", "curr_tile", "curr_room", "tile_col", "tile_row", NULL};   /* the drawing pass's scratch (as e2e's frozen ticks) */
	if (!nr) for (int i = 0; i < snap_nfields && nr < 199; i++) { int d = 0; for (int j = 0; drawn[j]; j++) if (!strcmp(drawn[j], snap_fields[i].name)) d = 1; if (!d) regions[nr++] = snap_fields[i].name; }
	if (cur_sample >= nsamples) return;
	my_tick_frame[cur_sample] = cmp_frame;
	if (getenv("SHELL_FOLLOW")) {   /* the platform's timing, as e2e follows it: the lateness meter, and the ambient
	                                   music's draws (1611:03CC) when the capture drew up to 4 more */
		const uint8_t *m = samples[cur_sample];
		memcpy(tiles0 + 0xA, m + 0x2BA4 - SNAP_BASE, 2);
		uint32_t want = m[0x2B7A - SNAP_BASE] | m[0x2B7B - SNAP_BASE] << 8 | m[0x2B7C - SNAP_BASE] << 16 | (uint32_t)m[0x2B7D - SNAP_BASE] << 24, x = random_seed;
		uint32_t y = want; int near = 0;
		for (int k = 1; k <= 4 && random_seed != want && !near; k++) { x = x * 0x343FD + 0x269EC3; y = y * 0x343FD + 0x269EC3; near = x == want || y == random_seed; }
		if (near) { random_seed = want; memcpy(amb_state, m + 0x2B98 - SNAP_BASE, 2); memcpy(tiles0, m + 0x2B9A - SNAP_BASE, 2); }   /* (either side up to 4 draws ahead) */
	}
	memcpy(got, samples[cur_sample], SNAP_MAX); snap_store(got);
	int d = snap_diff(got, samples[cur_sample], regions, 0);
	ticks_seen++;
	if (d && getenv("SHELL_CMP_LIST")) { printf("sample %d differs:", cur_sample); for (int i = 0; regions[i]; i++) { const char *one[2] = {regions[i], NULL}; if (snap_diff(got, samples[cur_sample], one, 0)) printf(" %s", regions[i]); } printf("\n"); }
	if (d) { bad_ticks++; if (bad_ticks <= 5) { printf("frame=%d tick sample %d (tick %u): %d fields differ\n", cmp_frame, cur_sample, (unsigned)tick, d); snap_diff(got, samples[cur_sample], regions, 1); } }
	if (getenv("SHELL_LOAD")) snap_load(samples[cur_sample]);   /* (the capture's state from here: only the drawing is compared) */
	cur_sample++;
}
/* ---- probepoke ds_tick N (plans): applied at the N-th tick start ---- */
typedef struct { int tick; unsigned phys; uint8_t bytes[0x70]; int nb; } ppoke_t;
static ppoke_t *pp; static int npp, ticks_started;
static shell_input in;   /* (the held keys persist across frames) */
static void apply_ppokes(void)
{
	ticks_started++;
	for (int i = 0; i < npp; i++) if (pp[i].tick == ticks_started) {
		ppoke_t *q = &pp[i];
		if (q->phys == 0x3CF50) { for (int k = 0; k < q->nb && k < 0x70; k++) { key_table[k] = q->bytes[k]; if (k >= 0xD && k - 0xD < (int)sizeof in.down) in.down[k - 0xD] = q->bytes[k]; } }
		else if (q->phys == 0x417) { bios_shift_flags = q->bytes[0]; in.shift_flags = q->bytes[0]; }
		else for (int k = 0; k < q->nb; k++) { unsigned a = q->phys + k - 0x3B250; for (int f = 0; f < snap_nfields; f++) if (snap_fields[f].ds && a >= snap_fields[f].ds && a < (unsigned)snap_fields[f].ds + snap_fields[f].size) ((uint8_t *)snap_fields[f].p)[a - snap_fields[f].ds] = q->bytes[k]; }
	}
}
static void on_tick(void) { apply_ppokes(); if (samples) tick_cmp(); }
/* ---- SHELL_VRAM: the oracle's VGA dumps ---- */
typedef struct { int frame, rgb; uint8_t *px; } vdump_t;   /* rgb: a 'shot' record (320 x 200 x 3) */
static vdump_t *vd; static int nvd, vd_next, vd_n, vd_exact, vd_near, vd_bad; static long vd_px;
#define RING 16
static int vwin = 2;   /* SHELL_VRAM_WIN */
static uint8_t ring[RING][SCREEN_W * SCREEN_H], ring_pal[RING][768]; static int ring_fr[RING];
static int vd_cmp(const void *a, const void *b) { return ((const vdump_t *)a)->frame - ((const vdump_t *)b)->frame; }
static void load_vram(const char *p)
{
	FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(2); }
	uint8_t h[16];
	while (fread(h, 1, 16, f) == 16) {
		uint32_t len = h[12] | h[13] << 8 | h[14] << 16 | (uint32_t)h[15] << 24, fr = h[8] | h[9] << 8 | h[10] << 16 | (uint32_t)h[11] << 24;
		int rgb = !memcmp(h, "shot", 5) && len == 192000;
		if (!rgb && (memcmp(h, "vram", 5) || len != 64000)) { fseek(f, len, SEEK_CUR); continue; }
		vd = realloc(vd, sizeof *vd * (nvd + 1)); vd[nvd].frame = (int)fr; vd[nvd].rgb = rgb; vd[nvd].px = malloc(len);
		if (fread(vd[nvd].px, 1, len, f) != len) break;
		nvd++;
	}
	fclose(f);
	qsort(vd, nvd, sizeof *vd, vd_cmp);   /* (tools/framecap.py writes them in file-name order) */
}
static int target_of(int F);
static int vdiff(const uint8_t *a, const uint8_t *b, int rows) { int d = 0; for (int i = 0; i < SCREEN_W * rows; i++) d += a[i] != b[i]; return d; }
static int rgbdiff(const uint8_t *a, const uint8_t *pal, const uint8_t *b, int rows)   /* (DOSBox's shots: 6-bit c as c << 2 | c >> 4) */
{
	int d = 0;
	for (int i = 0; i < SCREEN_W * rows; i++) { const uint8_t *c = pal + 3 * a[i]; for (int k = 0; k < 3; k++) if ((uint8_t)(c[k] << 2 | c[k] >> 4) != b[3 * i + k]) { d++; break; } }
	return d;
}
static void vram_frame(int fr)
{
	memcpy(ring[fr % RING], screen_buf, sizeof ring[0]); memcpy(ring_pal[fr % RING], render_palette, 768); ring_fr[fr % RING] = fr;
	while (vd_next < nvd) {
		int t = target_of(vd[vd_next].frame);
		if (t < 0 || fr < t + vwin) return;
		int best = 1 << 30, at = -1, bestd = 0;
		for (int d = -vwin; d <= vwin; d++) { int k = t + d; if (k < 0 || ring_fr[k % RING] != k) continue; int x = vd[vd_next].rgb ? rgbdiff(ring[k % RING], ring_pal[k % RING], vd[vd_next].px, 192) : vdiff(ring[k % RING], vd[vd_next].px, 192); if (d == 0) at = x; if (x < best) { best = x; bestd = d; } }
		if (best == 1 << 30) { vd_next++; continue; }   /* (not in the ring any more) */
		vd_n++; if (at == 0) vd_exact++; else if (best == 0) vd_near++; else { vd_bad++; vd_px += best; if (getenv("SHELL_VRAM_V") || vd_bad <= 20) printf("%s oracle frame %d (shell %d): %d px differ (best %d at %+d)\n", vd[vd_next].rgb ? "shot" : "vram", vd[vd_next].frame, t, at, best, bestd); }
		if (getenv("SHELL_VRAM_OUT") && (best || getenv("SHELL_VRAM_ALL")) && vd[vd_next].rgb) {   /* PPMs: the shell's and the oracle's */
			char q[512]; snprintf(q, sizeof q, "%s/s%05d_mine.ppm", getenv("SHELL_VRAM_OUT"), vd[vd_next].frame);
			FILE *o = fopen(q, "wb"); if (o) { fprintf(o, "P6 320 200 255\n"); const uint8_t *a = ring[(t + bestd) % RING], *pl = ring_pal[(t + bestd) % RING]; for (int i = 0; i < 64000; i++) for (int k = 0; k < 3; k++) fputc((uint8_t)(pl[3 * a[i] + k] << 2 | pl[3 * a[i] + k] >> 4), o); fclose(o); }
			snprintf(q, sizeof q, "%s/s%05d_mine.idx", getenv("SHELL_VRAM_OUT"), vd[vd_next].frame);   /* (the indices and the palette) */
			o = fopen(q, "wb"); if (o) { fwrite(ring[(t + bestd) % RING], 1, 64000, o); fwrite(ring_pal[(t + bestd) % RING], 1, 768, o); fclose(o); }
			snprintf(q, sizeof q, "%s/s%05d_game.ppm", getenv("SHELL_VRAM_OUT"), vd[vd_next].frame);
			o = fopen(q, "wb"); if (o) { fprintf(o, "P6 320 200 255\n"); fwrite(vd[vd_next].px, 1, 192000, o); fclose(o); }
		}
		if (getenv("SHELL_VRAM_OUT") && best && !vd[vd_next].rgb) {
			char q[512]; snprintf(q, sizeof q, "%s/v%05d_mine.pgm", getenv("SHELL_VRAM_OUT"), vd[vd_next].frame);
			FILE *o = fopen(q, "wb"); if (o) { fprintf(o, "P5 320 200 255\n"); fwrite(ring[(t + bestd) % RING], 1, 64000, o); fclose(o); }
			snprintf(q, sizeof q, "%s/v%05d_game.pgm", getenv("SHELL_VRAM_OUT"), vd[vd_next].frame);
			o = fopen(q, "wb"); if (o) { fprintf(o, "P5 320 200 255\n"); fwrite(vd[vd_next].px, 1, 64000, o); fclose(o); }
		}
		vd_next++;
	}
}
extern void (*shell_tick_hook)(void);
/* the event at oracle frame F happens at shell frame fr: F itself, or with SHELL_SYNC the same distance after the same
 * tick (the last oracle tick before F) as in the capture */
static int sync_ticks, last_target = -1;
static int target_of(int F)   /* -1: not known yet */
{
	if (!sync_ticks || !nsamples) return F;
	int k = -1; for (int i = 0; i < nsamples && sample_frame[i] <= F; i++) k = i;
	if (k < 0) return F;
	if (k >= cur_sample) return -1;   /* (that tick has not come yet) */
	return my_tick_frame[k] + (F - sample_frame[k]);
}
static int at_frame(int F, int fr, int *done)   /* (key events keep their order: none before the previous one's frame) */
{
	if (*done) return 0;
	int t = target_of(F); if (t < 0) return 0;
	if (t < last_target) t = last_target;
	if (t > fr) return 0;   /* (a target already passed, its tick came late in the frame: now) */
	last_target = fr; *done = 1; return 1;
}
int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: shelltest GAME_DIR SCRIPT [WORDS...]\n"); return 2; }
	FILE *f = fopen(argv[2], "r"); if (!f) { fprintf(stderr, "cannot read %s\n", argv[2]); return 2; }
	char line[1024]; int end = 600;
	while (fgets(line, sizeof line, f)) {
		char a[64], b[256], c[256], d[64]; int n = sscanf(line, "%63s %255s %255s %63s", a, b, c, d);
		if (n >= 4 && !strcmp(a, "key")) { const keydef *k = find_key(c); if (!k) { fprintf(stderr, "unknown key %s\n", c); return 2; } ev[nev].frame = atoi(b); ev[nev].key = (int)(k - keys); ev[nev].level = atoi(d); ev[nev++].kind = 0; }
		else if (n >= 3 && !strcmp(a, "shot")) { ev[nev].frame = atoi(b); snprintf(ev[nev].path, sizeof ev[nev].path, "%s", c); ev[nev++].kind = 1; }
		else if (n >= 4 && !strcmp(a, "poke")) {   /* poke FRAME PHYS HEX: a DS variable (phys 0x3B250 + DS offset) */
			ev_t *e = &ev[nev++]; e->frame = atoi(b); e->kind = 2; e->phys = (unsigned)strtoul(c, NULL, 16);
			for (int i = 0; d[2 * i] && d[2 * i + 1] && i < 16; i++) { unsigned x; sscanf(d + 2 * i, "%2x", &x); e->bytes[e->nb++] = (uint8_t)x; }
		}
		else if (n >= 2 && !strcmp(a, "end")) end = atoi(b);
		else if (!strcmp(a, "probepoke")) {   /* probepoke ds_tick N PHYS HEX */
			char lab[64], hex[512]; int N; unsigned ph;
			if (sscanf(line, "probepoke %63s %d %x %511s", lab, &N, &ph, hex) == 4 && !strcmp(lab, "ds_tick")) {
				pp = realloc(pp, sizeof *pp * (npp + 1)); ppoke_t *q = &pp[npp++]; q->tick = N; q->phys = ph; q->nb = 0;
				for (int i = 0; hex[2 * i] && hex[2 * i + 1] && i < 0x70; i++) { unsigned x; sscanf(hex + 2 * i, "%2x", &x); q->bytes[q->nb++] = (uint8_t)x; }
			}
		}
	}
	fclose(f);
	if (getenv("SHELL_OFFSET")) { int o = atoi(getenv("SHELL_OFFSET")); for (int i = 0; i < nev; i++) ev[i].frame -= o; end -= o; }   /* the script's frames are the oracle's */
	snprintf(file_dir, sizeof file_dir, "%s", getenv("SHELL_FILES") ? getenv("SHELL_FILES") : ".");   /* PRINCE.SAV / OPT / HOF: never in the game's directory */
	if (getenv("SHELL_SEED")) shell_set_seed((uint32_t)strtoul(getenv("SHELL_SEED"), NULL, 16));
	if (!shell_init(argv[1], argc - 3, (const char **)argv + 3)) { fprintf(stderr, "cannot start from %s\n", argv[1]); return 1; }
	if (getenv("SHELL_SYNC")) sync_ticks = 1;
	if (getenv("SHELL_CMP")) { SNAP_BASE = 0x2900; SNAP_SIZE = 0x4300; load_samples(getenv("SHELL_CMP"), getenv("SHELL_CMP_SKIP") ? atoi(getenv("SHELL_CMP_SKIP")) : 0); }
	shell_tick_hook = on_tick;
	if (getenv("SHELL_VRAM")) load_vram(getenv("SHELL_VRAM"));
	if (getenv("SHELL_VRAM_WIN")) vwin = atoi(getenv("SHELL_VRAM_WIN"));
	memset(&in, 0, sizeof in);
	int held_since[0x60] = {0}; int last_mode = -1, last_level = -1;
	for (int fr = 0; fr <= end; fr++) {
		in.ntyped = 0;
		for (int i = 0; i < nev; i++) if (ev[i].kind == 0 && at_frame(ev[i].frame, fr, &ev[i].done)) {
			const keydef *k = &keys[ev[i].key];
			in.down[k->scan] = (uint8_t)ev[i].level;
			if (ev[i].level) held_since[k->scan] = fr;
			int shift = in.down[42] || in.down[54], ctrl = in.down[29], alt = in.down[56];
			in.shift_flags = (uint8_t)((in.down[54] ? 1 : 0) | (in.down[42] ? 2 : 0) | (ctrl ? 4 : 0) | (alt ? 8 : 0));
			if (ev[i].level && k->scan != 42 && k->scan != 54 && k->scan != 29 && k->scan != 56 && in.ntyped < 8) {
				int code = alt ? k->scan << 8 : ctrl && k->ascii >= 'a' && k->ascii <= 'z' ? k->ascii & 0x1F : k->ascii ? (shift ? k->shifted : k->ascii) : k->scan << 8;
				in.typed[in.ntyped++] = (uint16_t)code;
			}
			if (getenv("SHELL_KEYS")) printf("frame=%d key %s %d (oracle frame %d)%s\n", fr, k->name, ev[i].level, ev[i].frame, in.ntyped ? "" : "");
		}
		/* typematic repeat: 500 ms, then every 33 ms (the frames of a 70.086 Hz display) */
		for (int s = 1; s < 0x54; s++) if (in.down[s] && s != 42 && s != 54 && s != 29 && s != 56) {
			int t = fr - held_since[s];
			if (t >= 35 && (t - 35) % 2 == 0 && in.ntyped < 8) {
				const keydef *k = NULL; for (size_t i = 0; i < sizeof keys / sizeof *keys; i++) if (keys[i].scan == s) { k = &keys[i]; break; }
				int alt = in.down[56]; in.typed[in.ntyped++] = (uint16_t)(alt ? s << 8 : k && k->ascii ? k->ascii : s << 8);
			}
		}
		for (int i = 0; i < nev; i++) if (ev[i].kind == 2 && !ev[i].done) { int t = target_of(ev[i].frame); if (t >= 0 && t <= fr) {
			ev[i].done = 1;
			for (int k = 0; k < ev[i].nb; k++) { unsigned a = ev[i].phys + k - 0x3B250; for (int f = 0; f < snap_nfields; f++) if (snap_fields[f].ds && a >= snap_fields[f].ds && a < (unsigned)snap_fields[f].ds + snap_fields[f].size) ((uint8_t *)snap_fields[f].p)[a - snap_fields[f].ds] = ev[i].bytes[k]; }
		} }
		cmp_frame = fr;
		int r = shell_step(&in);
		if (nvd) vram_frame(fr);
		for (int i = 0; i < nev; i++) if (ev[i].kind == 1 && !ev[i].done) { int t = target_of(ev[i].frame); if (t >= 0 && t <= fr) { write_tga(ev[i].path); ev[i].done = 1; } }
		if (shell_mode() != last_mode) { printf("frame=%d mode %d\n", fr, shell_mode()); last_mode = shell_mode(); }
		if ((int8_t)word_32d8 != last_level) { printf("frame=%d level %d\n", fr, (int8_t)word_32d8); last_level = (int8_t)word_32d8; }
		if (r == SHELL_EXIT) { printf("frame=%d exit %d %s\n", fr, shell_exit_code(), shell_exit_message() ? shell_exit_message() : ""); break; }
	}
	if (samples) printf("compared %d ticks with %d samples: %d differ\n", ticks_seen, nsamples, bad_ticks);
	if (nvd) printf("vram: %d of %d dumps compared, %d exact, %d within %d frames, %d differ (%ld px)\n", vd_n, nvd, vd_exact, vd_near, vwin, vd_bad, vd_px);
	return 0;
}

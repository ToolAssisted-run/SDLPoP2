/* Story scenes (src/nis.c) against the oracle: the frames the real game showed (oracle `shot` TGAs, 640x400 BGR, the
 * 320x200 screen doubled) compared with ours, pixel by pixel in RGB.
 *
 *   nistest DATADIR SCENE EVENTS SHOTDIR [window [nth]]       (SCENE "intro": 7, 4, 8 in a row)
 *
 * EVENTS: the capture's -snap.txt (tools/nisoracle.py probes); nth: which play_scene of the capture (the NISn cheat
 * replays the scene forever; default 0). The game spends CPU and disk time the model does not have (unpacking images,
 * loading resources), so the frames are matched through checkpoints both sides log: the animation frames (32D4:0852),
 * fades (2631:037E), sounds started (194C:840E), texts (2D7D:01AF), dissolves (33B9:0000), animations started
 * (32D4:0BE4), the music's cue points (194C:314B), scenes (0AAC:0274). A shot taken d frames after the oracle's k-th checkpoint is compared with our frame d frames after our
 * k-th checkpoint ("synced"), and also with our frames up to +-window around it ("best"): a mismatch that vanishes
 * nearby is timing, one that stays is drawing. Summary: shots, exact synced, exact within the window. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "../src/nis.h"
#ifdef NIS_ENGINE   /* (built with all of src: transitions 2 and 3 draw their game room through the shell's 0AAC:0376) */
#include "../src/core.h"
#include "../src/shell.h"
void shell_nis_room(int lv, int room, uint8_t *pixels, int rowbytes);
static void room_hook(int lv, int room, uint8_t *pixels, int rowbytes, void *u) { (void)u; shell_nis_room(lv, room, pixels, rowbytes); }
#endif

#define MAXE 20000
typedef struct { int ev; long frame; } cp;
static cp ocp[MAXE], mcp[MAXE]; static int no, nm;
static long o_start = -1, o_end = -1;

static int ev_of(const char *name)
{
	if (!strcmp(name, "anim_frame")) return NIS_EV_ANIM_FRAME;
	if (!strcmp(name, "fade")) return NIS_EV_FADE;
	if (!strcmp(name, "music")) return NIS_EV_SOUND;
	if (!strcmp(name, "text")) return NIS_EV_TEXT;
	if (!strcmp(name, "dissolve")) return NIS_EV_DISSOLVE;
	if (!strcmp(name, "play_anim")) return NIS_EV_PLAY_ANIM;
	if (!strcmp(name, "play_scene")) return NIS_EV_SCENE;
	if (!strcmp(name, "cue")) return NIS_EV_CUE;
	/* (palette sets are not used: when a set shows relative to the probe varies by a frame) */
	/* (nor dissolve steps: their number depends on the machine's speed) */
	return 0;
}
/* the scene's span in the capture: from its play_scene (the nth) to the next play_scene; the intro (scene -1): from
 * scene 7's to the one after scene 8's */
static void load_events(const char *path, int scene, int nth)
{
	FILE *fp = fopen(path, "r"); if (!fp) { perror(path); exit(1); }
	char line[65536]; int seen = 0, intro = scene == NIS_INTRO, last_intro = 0;
	while (fgets(line, sizeof line, fp)) {
		long frame; char name[64];
		if (sscanf(line, "frame=%ld %*s %*s %63s", &frame, name) != 2) continue;
		if (!strcmp(name, "play_scene")) {
			char *ax = strstr(line, " ax="); int s = ax ? (int)strtol(ax + 4, NULL, 16) : -1;
			if (o_start >= 0 && o_end < 0 && (!intro || last_intro)) o_end = frame;
			if (s == (intro ? 7 : scene) && o_start < 0) { if (seen == nth) o_start = frame; seen++; }
			if (intro && s == 8) last_intro = 1;
			if (!intro) continue;
		}
		int ev = ev_of(name);
		if (ev && o_start >= 0 && o_end < 0 && no < MAXE) { ocp[no].ev = ev; ocp[no].frame = frame - o_start; no++; }
	}
	fclose(fp);
}
static int intro_mode;
static void on_event(int ev, uint32_t frame, void *u) { (void)u; if ((ev == NIS_EV_SCENE && !intro_mode) || ev == NIS_EV_SETPAL || ev == NIS_EV_DIS_STEP) return; if (nm < MAXE) { mcp[nm].ev = ev; mcp[nm].frame = frame; nm++; } }
static uint8_t *read_tga(const char *path)
{
	FILE *fp = fopen(path, "rb"); if (!fp) return NULL;
	uint8_t h[18]; if (fread(h, 1, 18, fp) != 18) { fclose(fp); return NULL; }
	int w = h[12] | h[13] << 8, hh = h[14] | h[15] << 8, bpp = h[16] / 8, desc = h[17];
	fseek(fp, h[0], SEEK_CUR);
	uint8_t *px = malloc((size_t)w * hh * bpp);
	if (fread(px, 1, (size_t)w * hh * bpp, fp) != (size_t)w * hh * bpp) { fclose(fp); free(px); return NULL; }
	fclose(fp);
	uint8_t *rgb = malloc(320 * 200 * 3);          /* the top-left pixel of each 2x2 */
	for (int y = 0; y < 200; y++) for (int x = 0; x < 320; x++) {
		int sy = (desc & 0x20) ? y * 2 : hh - 1 - y * 2;
		const uint8_t *p = px + ((size_t)sy * w + x * 2) * bpp;
		rgb[(y * 320 + x) * 3] = p[2]; rgb[(y * 320 + x) * 3 + 1] = p[1]; rgb[(y * 320 + x) * 3 + 2] = p[0];
	}
	free(px);
	return rgb;
}
static int dac8(int v) { return (v << 2) | (v >> 4); }     /* the VGA DAC's 6 bits as the capture shows them */
static int mismatch(const uint8_t *rgb, const uint8_t *scr, const uint8_t *pal, int *first)
{
	int n = 0; *first = -1;
	for (int i = 0; i < 320 * 200; i++) {
		const uint8_t *c = pal + scr[i] * 3;
		if (rgb[i * 3] != dac8(c[0]) || rgb[i * 3 + 1] != dac8(c[1]) || rgb[i * 3 + 2] != dac8(c[2])) { if (*first < 0) *first = i; n++; }
	}
	return n;
}
static void dump(const char *name, const uint8_t *rgb, const uint8_t *scr, const uint8_t *pal)
{
	FILE *fp = fopen(name, "wb"); if (!fp) return;
	fprintf(fp, "P6\n320 400\n255\n");
	fwrite(rgb, 1, 64000 * 3, fp);
	for (int i = 0; i < 64000; i++) { const uint8_t *c = pal + scr[i] * 3; uint8_t o[3] = { (uint8_t)dac8(c[0]), (uint8_t)dac8(c[1]), (uint8_t)dac8(c[2]) }; fwrite(o, 1, 3, fp); }
	fclose(fp);
}
static int cmpint(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }
/* our frame for oracle scene frame f: d frames after the checkpoint both sides reached last (paired by type and
 * ordinal, so that asynchronous ones like the music's cues may interleave differently) */
static long map_frame(long f, int *k_out)
{
	int k = -1;
	for (int i = 0; i < no && ocp[i].frame <= f; i++) k = i;
	for (; k >= 0; k--) {
		int nth = 0; for (int i = 0; i < k; i++) if (ocp[i].ev == ocp[k].ev) nth++;
		for (int j = 0; j < nm; j++) if (mcp[j].ev == ocp[k].ev && nth-- == 0) { *k_out = k; return mcp[j].frame + (f - ocp[k].frame); }
	}
	*k_out = -1;
	return f;
}
int main(int argc, char **argv)
{
	if (argc < 5) { fprintf(stderr, "usage: nistest DATADIR SCENE EVENTS SHOTDIR [window [nth]]\n"); return 2; }
	int scene = !strcmp(argv[2], "intro") ? NIS_INTRO : atoi(argv[2]), window = argc > 5 ? atoi(argv[5]) : 4, nth = argc > 6 ? atoi(argv[6]) : 0;
	load_events(argv[3], scene, nth); intro_mode = scene == NIS_INTRO;
	if (o_start < 0) { fprintf(stderr, "no play_scene %d in %s\n", scene, argv[3]); return 1; }
#ifdef NIS_ENGINE
	if (!pop2_init(argv[1])) { fprintf(stderr, "pop2_init failed\n"); return 2; }
	pop2_reset_state();
	{ extern uint16_t word_2ba6; word_2ba6 = 1; }   /* (0AAC:0274 sets DS:2BA6 before a scene: the extra pieces are off) */
	nis_set_room_hook(room_hook, NULL);
#endif
	/* pass 1: our checkpoints */
	nis_set_event_callback(on_event, NULL);
	nis_open(argv[1], scene);

	long total_frames = 0; while (nis_step(NULL, NULL) && total_frames < 400000) total_frames++;
	int co[16] = {0}, cm[16] = {0};
	for (int i = 0; i < no; i++) co[ocp[i].ev & 15]++;
	for (int i = 0; i < nm; i++) cm[mcp[i].ev & 15]++;
	printf("checkpoints (anim frames, fades, sounds, texts, dissolves, anims, scenes, cues): oracle");
	for (int e = 1; e <= 8; e++) printf(" %d", co[e]);
	printf(", ours"); for (int e = 1; e <= 8; e++) printf(" %d", cm[e]);
	printf("; our scene lasts %ld frames, the oracle's %ld\n", total_frames, o_end >= 0 ? o_end - o_start : -1);
	if (getenv("NIS_CP")) for (int i = 0; i < no || i < nm; i++) printf("  cp %3d: oracle %d @%ld   ours %d @%ld   (oracle-ours %ld)\n", i, i < no ? ocp[i].ev : 0, i < no ? ocp[i].frame : -1, i < nm ? mcp[i].ev : 0, i < nm ? mcp[i].frame : -1, (i < no ? ocp[i].frame : 0) - (i < nm ? mcp[i].frame : 0));
	/* the shots of this play */
	static int frames[400000]; int nf = 0;
	DIR *d = opendir(argv[4]); struct dirent *e;
	while (d && (e = readdir(d))) { int f; if (sscanf(e->d_name, "s%d.tga", &f) == 1 && f > o_start && (o_end < 0 || f < o_end) && nf < 400000) frames[nf++] = f; }
	if (d) closedir(d);
	qsort(frames, nf, sizeof(int), cmpint);
	/* pass 2: step again, comparing */
	nis_set_event_callback(NULL, NULL);
	nis_open(argv[1], scene);
	int ring = 2 * window + 1 + 400;    /* (the mapping can step back after a long load: keep 400 more frames) */
	uint8_t (*scr)[64000] = malloc((size_t)ring * 64000), (*pal)[768] = malloc((size_t)ring * 768);
	long f = 0; int alive = 1, exact = 0, near = 0, total = 0, sum = 0;
	const char *dumpat = getenv("NIS_DUMP");
	for (int k = 0; k < nf; k++) {
		int kk; long want = map_frame(frames[k] - o_start, &kk);
		if (want < 1) continue;
		while (f < want + window && alive) { f++; alive = nis_step(scr[f % ring], pal[f % ring]); }
		if (want > f) break;
		char p[1024]; snprintf(p, sizeof p, "%s/s%d.tga", argv[4], frames[k]);
		uint8_t *rgb = read_tga(p); if (!rgb) continue;
		if (want <= f - ring) { free(rgb); continue; }   /* (older than the frames kept) */
		int first, m0 = mismatch(rgb, scr[want % ring], pal[want % ring], &first);
		int best = m0, bestd = 0;
		for (int dd = -window; dd <= window && best; dd++) {
			long g = want + dd; if (g < 1 || g > f || g <= f - ring) continue;
			int fi, m = mismatch(rgb, scr[g % ring], pal[g % ring], &fi);
			if (m < best) { best = m; bestd = dd; }
		}
		if (dumpat && atoi(dumpat) == frames[k]) {
			char q[1100]; snprintf(q, sizeof q, "%s/nisdump_%d.ppm", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", frames[k]);
			dump(q, rgb, scr[want % ring], pal[want % ring]);
			fprintf(stderr, "dumped %s (top: oracle, bottom: ours)\n", q);
		}
		total++; sum += m0; if (!m0) exact++; if (!best) near++;
		if (m0 || getenv("NIS_ALL")) {
			printf("shot %d (scene frame %ld, after checkpoint %d; ours %ld): %d", frames[k], frames[k] - o_start, kk, want, m0);
			if (m0) printf(" (first at %d,%d)", first % 320, first / 320);
			if (bestd || best != m0) printf("  best %+d: %d", bestd, best);
			printf("\n");
		}
		free(rgb);
	}
	printf("scene %d: %d shots, %d exact synced, %d exact within +-%d frames, %d mismatching pixels in all\n", scene, total, exact, near, window, sum);
	return 0;
}
